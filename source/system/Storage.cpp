#include "AppState.hpp"

// Formato persistente local de identidad/contactos/preferencias. La SD conserva
// estos archivos sin cifrar, por lo que las fotos y el historial local también
// deben considerarse accesibles para quien pueda leer la tarjeta.
std::string obtenerOGenerarID() {
    // El ID se conserva localmente para que reinstalar o reiniciar no cambie la identidad.
    mkdir("sdmc:/3ds/Mensinger3DS", 0777);
    FILE* file = fopen("sdmc:/3ds/Mensinger3DS/Mensinger3DS_ID.txt", "r");
    if (file != NULL) {
        char buffer[16] = {0};
        fgets(buffer, sizeof(buffer), file);
        fclose(file);
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') buffer[len - 1] = '\0';
        return std::string(buffer);
    }

    srand(time(NULL) ^ osGetTime());
    int numRand = 10000000 + (rand() % 89999999);
    char nuevaID[16];
    snprintf(nuevaID, sizeof(nuevaID), "%d", numRand);

    file = fopen("sdmc:/3ds/Mensinger3DS/Mensinger3DS_ID.txt", "w");
    if (file != NULL) {
        fputs(nuevaID, file);
        fclose(file);
    }
    return std::string(nuevaID);
}

// 📁 2. Cargar Contactos
void cargarContactosSD() {
    // Lee una línea por contacto con formato id:nombre; acepta líneas antiguas solo con ID.
    listaContactos.clear();
    mkdir("sdmc:/3ds/Mensinger3DS", 0777);
    FILE* file = fopen("sdmc:/3ds/Mensinger3DS/Mensinger3DS_Contactos.txt", "r");
    if (file != NULL) {
        char line[64];
        while (fgets(line, sizeof(line), file)) {
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
            if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';

            std::string strLinea(line);
            if (!strLinea.empty()) {
                size_t posDosPuntos = strLinea.find(':');
                Contacto c;
                if (posDosPuntos != std::string::npos) {
                    c.id = strLinea.substr(0, posDosPuntos);
                    c.nombre = strLinea.substr(posDosPuntos + 1);
                } else {
                    c.id = strLinea;
                    c.nombre = "Amigo";
                }
                listaContactos.push_back(c);
            }
        }
        fclose(file);
    }
}

void guardarContactosSD() {
    // Reescribe el archivo local completo para mantenerlo sincronizado con la lista en memoria.
    FILE* file = fopen("sdmc:/3ds/Mensinger3DS/Mensinger3DS_Contactos.txt", "w");
    if (file != NULL) {
        for (const auto& c : listaContactos) {
            fprintf(file, "%s:%s\n", c.id.c_str(), c.nombre.c_str());
        }
        fclose(file);
    }
}

void guardarContactoSD(const std::string& nuevaID, const std::string& nuevoNombre) {
    // Evita duplicados y asigna el nombre genérico si el usuario no escribió uno.
    for (const auto& c : listaContactos) {
        if (c.id == nuevaID) return;
    }

    Contacto nuevoC;
    nuevoC.id = nuevaID;
    nuevoC.nombre = nuevoNombre.empty() ? "Amigo" : nuevoNombre;
    listaContactos.push_back(nuevoC);

    guardarContactosSD();
}

void borrarContactoActual(const std::string& idBorrar) {
    // Quitar un contacto solo modifica la lista local; no borra el historial del servidor.
    for (auto it = listaContactos.begin(); it != listaContactos.end(); ++it) {
        if (it->id == idBorrar) {
            listaContactos.erase(it);
            break;
        }
    }
    guardarContactosSD();
}


bool servidorValido(const std::string& servidor) {
    // Acepta un hostname o IPv4 sin esquema, puerto, ruta ni caracteres especiales.
    if (servidor.empty() || servidor.size() > 120 || servidor.front() == '-' || servidor.back() == '-') return false;
    for (char c : servidor) {
        if (!std::isalnum((unsigned char)c) && c != '.' && c != '-') return false;
    }
    return true;
}

std::string hostHttp() {
    // El encabezado Host omite el puerto solo cuando HTTP usa el puerto estándar 80.
    if (g_servidorPuerto == 80) return g_servidorHost;
    return g_servidorHost + ":" + std::to_string(g_servidorPuerto);
}

// --- GUARDAR CONFIGURACIÓN ---
void guardarAjustesSD() {
    // Persiste preferencias, conexión y campos de perfil en formato clave=valor.
    FILE* f = fopen("sdmc:/3ds/Mensinger3DS/config.ini", "w");
    if (f) {
        fprintf(f, "modo_oscuro=%d\n", g_modoOscuro ? 1 : 0);
        fprintf(f, "led=%d\n", g_ledActivado ? 1 : 0);
        fprintf(f, "sonido=%d\n", g_sonidoActivado ? 1 : 0);
        fprintf(f, "actualizaciones=%d\n", g_buscarActualizaciones ? 1 : 0);
        fprintf(f, "servidor=%s\n", g_servidorHost.c_str());
        fprintf(f, "puerto=%d\n", g_servidorPuerto);
        fprintf(f, "cifrar_conexion=%d\n", g_cifrarConexion ? 1 : 0);
        fprintf(f, "intervalo_ms=%d\n", g_intervaloConsultaMs);
        fprintf(f, "perfil_nombre=%s\n", g_perfilNombre.c_str());
        fprintf(f, "perfil_apodo=%s\n", g_perfilApodo.c_str());
        fprintf(f, "perfil_bio=%s\n", g_perfilBio.c_str());
        fprintf(f, "perfil_descripcion=%s\n", g_perfilDescripcion.c_str());
        fprintf(f, "mostrar_en_busqueda=%d\n", g_mostrarEnBusqueda ? 1 : 0);
        fprintf(f, "quiero_conocer_gente=%d\n", g_quieroConocerGente ? 1 : 0);
        fprintf(f, "color_chat=%d\n", g_colorChat);
        fprintf(f, "color_fondo=%d\n", g_colorFondo);
        fprintf(f, "color_texto=%d\n", g_colorTexto);
        fprintf(f, "color_apodo=%d\n", g_colorApodo);
        fprintf(f, "color_nombre=%d\n", g_colorNombre);
        fclose(f);
    }
}

// --- CARGAR CONFIGURACIÓN ---
void cargarAjustesSD() {
    // Mantiene los valores por defecto para claves ausentes y crea el archivo si falta.
    FILE* f = fopen("sdmc:/3ds/Mensinger3DS/config.ini", "r");
    if (!f) {
        guardarAjustesSD(); // Si no existe, lo crea
        return;
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), f)) {
        char* separador = strchr(linea, '=');
        if (!separador) continue;
        *separador = '\0';
        std::string clave(linea);
        std::string valor(separador + 1);
        while (!valor.empty() && (valor.back() == '\n' || valor.back() == '\r')) valor.pop_back();
        if (clave == "modo_oscuro") g_modoOscuro = (valor == "1");
        else if (clave == "led") g_ledActivado = (valor == "1");
        else if (clave == "sonido") g_sonidoActivado = (valor == "1");
        else if (clave == "actualizaciones") g_buscarActualizaciones = (valor == "1");
        else if (clave == "servidor" && servidorValido(valor)) g_servidorHost = valor;
        else if (clave == "puerto") {
            char* fin = NULL;
            long puerto = strtol(valor.c_str(), &fin, 10);
            if (fin && *fin == '\0' && puerto >= 1 && puerto <= 65535) g_servidorPuerto = (int)puerto;
        }
        else if (clave == "cifrar_conexion" && (valor == "0" || valor == "1"))
            g_cifrarConexion = valor == "1";
        else if (clave == "intervalo_ms") {
            int intervalo = atoi(valor.c_str());
            if (intervalo == 1000 || intervalo == 2000 || intervalo == 5000 || intervalo == 10000) g_intervaloConsultaMs = intervalo;
        }
        else if (clave == "perfil_nombre") g_perfilNombre = valor;
        else if (clave == "perfil_apodo") g_perfilApodo = valor;
        else if (clave == "perfil_bio") g_perfilBio = valor;
        else if (clave == "perfil_descripcion") g_perfilDescripcion = valor;
        else if (clave == "mostrar_en_busqueda") g_mostrarEnBusqueda = valor == "1";
        else if (clave == "quiero_conocer_gente") g_quieroConocerGente = valor == "1";
        else if (clave == "color_chat") g_colorChat = std::max(0, std::min(5, atoi(valor.c_str())));
        else if (clave == "color_fondo") g_colorFondo = std::max(0, std::min(5, atoi(valor.c_str())));
        else if (clave == "color_texto") g_colorTexto = std::max(0, std::min(5, atoi(valor.c_str())));
        else if (clave == "color_apodo") g_colorApodo = std::max(0, std::min(5, atoi(valor.c_str())));
        else if (clave == "color_nombre") g_colorNombre = std::max(0, std::min(5, atoi(valor.c_str())));
    }
    fclose(f);
}

