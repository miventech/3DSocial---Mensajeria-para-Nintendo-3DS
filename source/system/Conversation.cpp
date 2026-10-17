#include "AppState.hpp"

bool enviarMensajeServidor(const char* deID, const char* paraID, const char* mensaje) {
    // Envía texto por la ruta heredada POST /enviar para mantener compatibilidad.
    std::string cuerpo = "{\"de\":\"" + std::string(deID) + "\",\"para\":\"" +
        std::string(paraID) + "\",\"texto\":\"" + escaparJson(mensaje) + "\"}";
    RespuestaHttp respuesta;
    return pedirHttpDatos("POST", "/enviar", "application/json", cuerpo, respuesta, 1024) &&
        respuesta.estado >= 200 && respuesta.estado < 300;
}

void verificarMensajesNuevos() {
    // Consulta notificaciones pendientes y marca cada remitente con novedades.
    RespuestaHttp respuesta;
    if (!pedirHttp("GET", "/notificaciones/" + miID, NULL, "", respuesta, 512) ||
        respuesta.estado != 200) return;
    std::string idRemitente(respuesta.cuerpo.begin(), respuesta.cuerpo.end());
    while (!idRemitente.empty() && (idRemitente.back() == '\n' ||
           idRemitente.back() == '\r' || idRemitente.back() == ' ')) idRemitente.pop_back();
    if (idRemitente.empty() || idRemitente == "NINGUNO") return;
    bool hayNovedadNueva = false;
    for (auto& contacto : listaContactos) {
        if (contacto.id == idRemitente) {
            if (!contacto.tieneNovedad) { contacto.tieneNovedad = true; hayNovedadNueva = true; }
            break;
        }
    }
    if (hayNovedadNueva && !(estadoActual == PANTALLA_CHAT && contactoActivoID == idRemitente)) {
        if (g_ledActivado) encenderLEDBlanco();
        if (g_sonidoActivado) reproducirCampanita();
    }
}

void cargarChatDesdeSD(const std::string& friendID) {
    // Recupera la copia local del historial legado cuando se abre una conversación.
    historialChat.clear();
    std::string ruta = "sdmc:/3ds/Mensinger3DS/chat_" + friendID + ".txt";

    FILE* file = fopen(ruta.c_str(), "r");
    if (file != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
            if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
            if (strlen(line) > 0) {
                historialChat.push_back(std::string(line));
            }
        }
        fclose(file);
    }
}

void guardarHistorialEnSD(const std::string& friendID) {
    // Guarda el historial de texto recibido como caché por contacto.
    if (historialChat.empty()) return;

    std::string ruta = "sdmc:/3ds/Mensinger3DS/chat_" + friendID + ".txt";
    FILE* file = fopen(ruta.c_str(), "w");
    if (file != NULL) {
        for (const auto& msg : historialChat) {
            fprintf(file, "%s\n", msg.c_str());
        }
        fclose(file);
    }
}

std::string claveServidorCache() {
    // Separa cachés si el usuario cambia de servidor, aunque conserve el mismo ID.
    std::string origen = hostHttp();
    u32 hash = 2166136261u;
    for (unsigned char c : origen) hash = (hash ^ c) * 16777619u;
    char texto[12];
    snprintf(texto, sizeof(texto), "%08lx", (unsigned long)hash);
    return texto;
}

std::string rutaHistorialV2() {
    // Devuelve la ruta SD del historial paginado V2 de la conversación activa.
    return "sdmc:/3ds/Mensinger3DS/chat_v2_" + claveServidorCache() + "_" + miID + "_" + contactoActivoID + ".txt";
}

std::string rutaAudioV2(long id) {
    // Los WAV se almacenan por ID para reutilizar la descarga local.
    return "sdmc:/3ds/Mensinger3DS/audio_" + claveServidorCache() + "_" + std::to_string(id) + ".wav";
}

std::string rutaFotoV2(long id) {
    // Las fotos recibidas se guardan crudas en RGB565 bajo su ID de mensaje.
    return "sdmc:/3ds/Mensinger3DS/foto_" + claveServidorCache() + "_" + std::to_string(id) + ".bgr";
}

void guardarHistorialV2() {
    // Serializa filas como TSV; los medios se guardan por separado en la caché SD.
    FILE* archivo = fopen(rutaHistorialV2().c_str(), "wb");
    if (!archivo) return;
    for (const MensajeV2& m : g_historialV2) {
        const char* tipo = m.audio ? "audio" : m.foto ? "foto" : "texto";
        fprintf(archivo, "%ld\t%s\t%s\t%d\t%s\n", m.id, m.remitente.c_str(),
                tipo, m.duracionMs, m.texto.c_str());
    }
    fclose(archivo);
}

void cargarHistorialV2() {
    // Recupera las filas ya vistas antes de solicitar páginas nuevas al servidor.
    g_historialV2.clear();
    FILE* archivo = fopen(rutaHistorialV2().c_str(), "rb");
    if (!archivo) return;
    char linea[768];
    while (fgets(linea, sizeof(linea), archivo)) {
        std::string fila(linea);
        while (!fila.empty() && (fila.back() == '\n' || fila.back() == '\r')) fila.pop_back();
        size_t p1 = fila.find('\t'), p2 = fila.find('\t', p1 + 1);
        size_t p3 = fila.find('\t', p2 + 1), p4 = fila.find('\t', p3 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos ||
            p3 == std::string::npos || p4 == std::string::npos) continue;
        MensajeV2 m;
        m.id = strtol(fila.c_str(), NULL, 10);
        m.remitente = fila.substr(p1 + 1, p2 - p1 - 1);
        std::string tipo = fila.substr(p2 + 1, p3 - p2 - 1);
        m.audio = tipo == "audio";
        m.foto = tipo == "foto";
        m.duracionMs = atoi(fila.substr(p3 + 1, p4 - p3 - 1).c_str());
        m.texto = fila.substr(p4 + 1);
        if (m.id > 0) g_historialV2.push_back(m);
    }
    fclose(archivo);
}

void fusionarHistorialV2(const std::vector<MensajeV2>& nuevos) {
    // Añade mensajes por ID sin duplicar los que ya estaban en la caché.
    for (const MensajeV2& m : nuevos) {
        auto it = std::lower_bound(g_historialV2.begin(), g_historialV2.end(), m.id,
            [](const MensajeV2& actual, long id) { return actual.id < id; });
        if (it == g_historialV2.end() || it->id != m.id) g_historialV2.insert(it, m);
    }
    if (g_audioSeleccionadoId == 0) {
        for (auto it = g_historialV2.rbegin(); it != g_historialV2.rend(); ++it) {
            if (it->audio) { g_audioSeleccionadoId = it->id; break; }
        }
    }
    if (g_fotoSeleccionadaId == 0) {
        for (auto it = g_historialV2.rbegin(); it != g_historialV2.rend(); ++it) {
            if (it->foto) { g_fotoSeleccionadaId = it->id; break; }
        }
    }
    guardarHistorialV2();
}

bool cargarPaginaV2(long antes) {
    // Usa el ID más antiguo como cursor; cero significa empezar por los recientes.
    std::vector<MensajeV2> pagina;
    int estado = 0;
    if (!pedirHistorial(miID, contactoActivoID, antes, pagina, estado)) {
        if ((estado >= 300 && estado < 500) || estado == 200) {
            g_servidorV2 = false;
            g_v2Determinado = true;
        }
        return false;
    }
    g_servidorV2 = true;
    g_v2Determinado = true;
    if (antes > 0 && pagina.size() < 20) g_masAntiguos = false;
    if (antes > 0 && pagina.empty()) g_masAntiguos = false;
    fusionarHistorialV2(pagina);
    return true;
}

void actualizarChatDesdeServidor(const char* myID, const char* friendID) {
    // Detecta la capacidad V2 del servidor y actualiza texto y medios por páginas.
    if (!g_v2Determinado || g_servidorV2) {
        if (cargarPaginaV2(0)) return;
        if (g_servidorV2 || !g_v2Determinado) return;
    }
    RespuestaHttp respuesta;
    std::string ruta = "/obtener_chat/" + std::string(myID) + "/" + friendID;
    if (!pedirHttp("GET", ruta, NULL, "", respuesta, 8192) || respuesta.estado != 200) return;
    std::string contenido(respuesta.cuerpo.begin(), respuesta.cuerpo.end());
    std::stringstream lineas(contenido);
    std::string linea;
    std::vector<std::string> nuevosMensajes;
    std::string etiquetaYo = "[" + std::string(myID) + "]";
    std::string etiquetaAmigoID = "[" + std::string(friendID) + "]";
    std::string etiquetaAmigoNombre = "[" + contactoActivoNombre + "]";
    while (std::getline(lineas, linea)) {
        if (!linea.empty() && linea.back() == '\r') linea.pop_back();
        if (linea.empty()) continue;
        if (linea.find(etiquetaYo) == 0) linea.replace(0, etiquetaYo.length(), "[Yo]");
        else if (linea.find(etiquetaAmigoID) == 0)
            linea.replace(0, etiquetaAmigoID.length(), etiquetaAmigoNombre);
        nuevosMensajes.push_back(linea);
    }
    if (!nuevosMensajes.empty()) {
        historialChat = nuevosMensajes;
        guardarHistorialEnSD(friendID);
    }
}

// --- AUTO-EXTRACCIÓN DE CAMPANITA.WAV ---

