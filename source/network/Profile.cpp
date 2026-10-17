#include "AppState.hpp"

// Sincronización del perfil y descubrimiento. La copia local permite editar
// sin red; al guardar se sube y los perfiles visibles de contactos se refrescan
// en lotes pequeños para mantener acotada cada respuesta HTTP.
std::string escaparJson(const std::string& texto) {
    // Escapa comillas y caracteres de control antes de enviar campos JSON.
    std::string resultado;
    for (unsigned char c : texto) {
        if (c == '\"' || c == '\\') { resultado += '\\'; resultado += (char)c; }
        else if (c == '\n') resultado += "\\n";
        else if (c == '\r') resultado += "\\r";
        else if (c == '\t') resultado += "\\t";
        else if (c >= 32) resultado += (char)c;
    }
    return resultado;
}

bool guardarPerfilServidor() {
    // Sincroniza los campos de texto locales del perfil con POST /perfil/{id}.
    std::string cuerpo = "{\"username\":\"" + escaparJson(g_perfilNombre) +
        "\",\"nickname\":\"" + escaparJson(g_perfilApodo) +
        "\",\"bio\":\"" + escaparJson(g_perfilBio) +
        "\",\"description\":\"" + escaparJson(g_perfilDescripcion) +
        "\",\"show_in_search\":" + (g_mostrarEnBusqueda ? "true" : "false") +
        ",\"meet_people\":" + (g_quieroConocerGente ? "true" : "false") +
        ",\"chat_color\":" + std::to_string(g_colorChat) +
        ",\"background_color\":" + std::to_string(g_colorFondo) +
        ",\"text_color\":" + std::to_string(g_colorTexto) +
        ",\"nickname_color\":" + std::to_string(g_colorApodo) +
        ",\"name_color\":" + std::to_string(g_colorNombre) + "}";
    RespuestaHttp respuesta;
    return pedirHttpDatos("POST", "/perfil/" + miID, "application/json", cuerpo,
                          respuesta, 256) && respuesta.estado == 200;
}

bool iniciarSesion() {
    g_tokenSesion.clear();
    g_hostSesion.clear();
    RespuestaHttp respuesta;
    if (!pedirHttp("GET", "/acceso/" + miID, NULL, "", respuesta, 128)) return false;
    if (respuesta.estado == 404) {
        g_contrasenaActiva = false;
        g_hostSesion = g_servidorHost;
        g_puertoSesion = g_servidorPuerto;
        g_cifradoSesion = g_cifrarConexion;
        return true;
    }
    if (respuesta.estado != 200) return false;
    std::string estado(respuesta.cuerpo.begin(), respuesta.cuerpo.end());
    g_contrasenaActiva = estado == "PROTEGIDO";
    if (!g_contrasenaActiva) {
        if (estado == "LIBRE") { g_hostSesion = g_servidorHost; g_puertoSesion = g_servidorPuerto; g_cifradoSesion = g_cifrarConexion; }
        return estado == "LIBRE";
    }
    std::string contrasena = abrirTecladoContrasena("Contrasena de tu ID");
    if (contrasena.empty()) return false;
    std::string cuerpo = "{\"password\":\"" + escaparJson(contrasena) + "\"}";
    std::fill(contrasena.begin(), contrasena.end(), '\0');
    if (!pedirHttpDatos("POST", "/acceso/" + miID, "application/json", cuerpo,
                        respuesta, 128) || respuesta.estado != 200) return false;
    g_tokenSesion.assign(respuesta.cuerpo.begin(), respuesta.cuerpo.end());
    if (!g_tokenSesion.empty()) { g_hostSesion = g_servidorHost; g_puertoSesion = g_servidorPuerto; g_cifradoSesion = g_cifrarConexion; }
    return !g_tokenSesion.empty();
}

bool cambiarContrasena(const std::string& nueva) {
    RespuestaHttp respuesta;
    std::string cuerpo = "{\"password\":\"" + escaparJson(nueva) + "\"}";
    if (!pedirHttpDatos("POST", "/contrasena/" + miID, "application/json", cuerpo,
                        respuesta, 128) || respuesta.estado != 200) return false;
    std::string token(respuesta.cuerpo.begin(), respuesta.cuerpo.end());
    g_contrasenaActiva = !nueva.empty();
    g_tokenSesion = g_contrasenaActiva ? token : "";
    return true;
}

bool buscarPerfiles() {
    std::string ruta = "/buscar?modo=" + std::string(estadoActual == PANTALLA_CONOCER ? "conocer" : "nombre") + "&q=";
    static const char* hex = "0123456789ABCDEF";
    for (unsigned char c : g_consultaBusqueda) {
        if (std::isalnum(c) && c < 128) ruta += (char)c;
        else { ruta += '%'; ruta += hex[c >> 4]; ruta += hex[c & 15]; }
    }
    RespuestaHttp respuesta;
    if (!pedirHttp("GET", ruta, NULL, "", respuesta, 8192) || respuesta.estado != 200) {
        g_resultadosBusqueda.clear();
        return false;
    }
    g_resultadosBusqueda.clear();
    g_scrollBusqueda = 0;
    std::stringstream lineas(std::string(respuesta.cuerpo.begin(), respuesta.cuerpo.end()));
    std::string linea;
    while (std::getline(lineas, linea)) {
        size_t a = linea.find('\t'), b = linea.find('\t', a + 1), c = linea.find('\t', b + 1);
        if (a == std::string::npos || b == std::string::npos || c == std::string::npos) continue;
        ResultadoBusqueda item;
        item.id = linea.substr(0, a);
        item.nombre = linea.substr(a + 1, b - a - 1);
        item.apodo = linea.substr(b + 1, c - b - 1);
        item.bio = linea.substr(c + 1);
        if (item.id != miID) g_resultadosBusqueda.push_back(item);
    }
    return true;
}

bool actualizarPerfilesContactos() {
    // Descarga de una vez los datos de perfil de los contactos configurados.
    g_ultimaCargaPerfiles = osGetTime();
    if (listaContactos.empty()) return true;
    for (size_t desde = 0; desde < listaContactos.size(); desde += 50) {
        std::string ruta = "/perfiles?ids=";
        size_t fin = std::min(listaContactos.size(), desde + 50);
        for (size_t i = desde; i < fin; ++i) {
            if (i > desde) ruta += ",";
            ruta += listaContactos[i].id;
        }
        RespuestaHttp respuesta;
        if (!pedirHttp("GET", ruta, NULL, "", respuesta, 32768) || respuesta.estado != 200)
            return false;
        std::string cuerpo(respuesta.cuerpo.begin(), respuesta.cuerpo.end());
        std::stringstream lineas(cuerpo);
        std::string linea;
        while (std::getline(lineas, linea)) {
            std::string campos[11];
            size_t inicio = 0;
            int cantidad = 0;
            for (; cantidad < 10; ++cantidad) {
                size_t tab = linea.find('\t', inicio);
                if (tab == std::string::npos) break;
                campos[cantidad] = linea.substr(inicio, tab - inicio);
                inicio = tab + 1;
            }
            if (cantidad < 5) continue;
            campos[cantidad] = linea.substr(inicio);
            for (Contacto& contacto : listaContactos) {
                if (contacto.id == campos[0]) {
                    contacto.usuarioPerfil = campos[1];
                    contacto.apodoPerfil = campos[2];
                    contacto.bioPerfil = campos[3];
                    contacto.descripcionPerfil = campos[4];
                    contacto.tieneFotoPerfil = campos[5] == "1";
                    if (cantidad >= 9) {
                        contacto.colorChat = std::max(0, std::min(5, atoi(campos[6].c_str())));
                        contacto.colorFondo = std::max(0, std::min(5, atoi(campos[7].c_str())));
                        contacto.colorTexto = std::max(0, std::min(5, atoi(campos[8].c_str())));
                        contacto.colorApodo = std::max(0, std::min(5, atoi(campos[9].c_str())));
                    }
                    contacto.colorNombre = cantidad >= 10 ?
                        std::max(0, std::min(5, atoi(campos[10].c_str()))) : 0;
                    contacto.tienePerfil = true;
                    break;
                }
            }
        }
    }
    g_idsMiniaturasCargadas.clear();
    return true;
}

const Contacto* contactoActivo() {
    for (const Contacto& contacto : listaContactos)
        if (contacto.id == contactoActivoID) return &contacto;
    return NULL;
}

bool guardarMiniaturaDesdeFoto(const std::vector<u8>& foto, const std::string& ruta) {
    if (foto.size() != FOTO_CAMARA_BYTES) return false;
    u8 miniatura[PERFIL_MINIATURA_BYTES];
    const int recorteIzquierdo = (FOTO_CAMARA_ANCHO - FOTO_CAMARA_ALTO) / 2;
    for (int y = 0; y < PERFIL_MINIATURA_DIMENSION; ++y) {
        int origenY = ((2 * y + 1) * FOTO_CAMARA_ALTO) / (2 * PERFIL_MINIATURA_DIMENSION);
        for (int x = 0; x < PERFIL_MINIATURA_DIMENSION; ++x) {
            int origenX = recorteIzquierdo + ((2 * x + 1) * FOTO_CAMARA_ALTO) / (2 * PERFIL_MINIATURA_DIMENSION);
            size_t origen = (origenY * FOTO_CAMARA_ANCHO + origenX) * 2;
            size_t destino = (y * PERFIL_MINIATURA_DIMENSION + x) * 2;
            miniatura[destino] = foto[origen];
            miniatura[destino + 1] = foto[origen + 1];
        }
    }
    FILE* archivo = fopen(ruta.c_str(), "wb");
    if (!archivo) return false;
    bool escrito = fwrite(miniatura, 1, sizeof(miniatura), archivo) == sizeof(miniatura);
    escrito = fclose(archivo) == 0 && escrito;
    if (!escrito) remove(ruta.c_str());
    return escrito;
}

void actualizarMiniaturasContactos() {
    // Solicita miniaturas solo para los contactos visibles y evita repetir la descarga.
    std::string claveLista;
    std::string idsConsulta;
    std::vector<int> visibles;
    std::vector<int> filas;
    size_t fin = std::min(listaContactos.size(), (size_t)scrollContactos + 3);
    for (size_t i = scrollContactos; i < fin; ++i) {
        int fila = (int)i - scrollContactos;
        if (!idsConsulta.empty()) idsConsulta += ",";
        idsConsulta += listaContactos[i].id;
        claveLista += std::to_string(fila) + ":" + listaContactos[i].id + ";";
        visibles.push_back((int)i);
        filas.push_back(fila);
    }
    bool cambioLista = claveLista != g_idsMiniaturasCargadas;
    if (!cambioLista && osGetTime() - g_ultimoIntentoMiniaturas < 10000) return;
    g_idsMiniaturasCargadas = claveLista;
    g_ultimoIntentoMiniaturas = osGetTime();
    if (visibles.empty()) return;

    if (cambioLista) {
        for (size_t fila = 0; fila < visibles.size(); ++fila) {
            Contacto& contacto = listaContactos[visibles[fila]];
            std::string ruta = "sdmc:/3ds/Mensinger3DS/contacto_mini_" + contacto.id + ".bgr";
            contacto.tieneMiniatura = cargarMiniaturaContacto(filas[fila], ruta);
        }
    }

    RespuestaHttp respuesta;
    bool loteDisponible = pedirHttp("GET", "/perfil-miniaturas?ids=" + idsConsulta, NULL, "", respuesta, 8192) &&
        respuesta.estado == 200 && !respuesta.cuerpo.empty();
    if (loteDisponible) {
        size_t cursor = 1;
        size_t cantidad = respuesta.cuerpo[0];
        for (size_t item = 0; item < cantidad; ++item) {
            if (cursor >= respuesta.cuerpo.size()) break;
            size_t largoID = respuesta.cuerpo[cursor++];
            if (largoID == 0 || largoID > 10 || cursor + largoID + PERFIL_MINIATURA_BYTES > respuesta.cuerpo.size()) break;
            std::string id(respuesta.cuerpo.begin() + cursor, respuesta.cuerpo.begin() + cursor + largoID);
            cursor += largoID;
            auto it = std::find_if(visibles.begin(), visibles.end(), [&](int indice) {
                return listaContactos[indice].id == id;
            });
            if (it != visibles.end()) {
                Contacto& contacto = listaContactos[*it];
                std::string ruta = "sdmc:/3ds/Mensinger3DS/contacto_mini_" + contacto.id + ".bgr";
                FILE* archivo = fopen(ruta.c_str(), "wb");
                if (archivo) {
                    bool escrito = fwrite(respuesta.cuerpo.data() + cursor, 1, PERFIL_MINIATURA_BYTES, archivo) == PERFIL_MINIATURA_BYTES;
                    escrito = fclose(archivo) == 0 && escrito;
                    if (!escrito) remove(ruta.c_str());
                    else {
                        int slot = filas[(size_t)(it - visibles.begin())];
                        contacto.tieneMiniatura = cargarMiniaturaContacto(slot, ruta);
                        contacto.tieneFotoPerfil = contacto.tieneMiniatura;
                    }
                }
            }
            cursor += PERFIL_MINIATURA_BYTES;
        }
    }
    for (size_t fila = 0; fila < visibles.size(); ++fila) {
        Contacto& contacto = listaContactos[visibles[fila]];
        if (contacto.tieneMiniatura || (loteDisponible && !contacto.tieneFotoPerfil)) continue;
        RespuestaHttp foto;
        if (!pedirHttp("GET", "/perfil/" + contacto.id + "/foto", NULL, "", foto, FOTO_CAMARA_BYTES) ||
            foto.estado != 200) continue;
        std::string ruta = "sdmc:/3ds/Mensinger3DS/contacto_mini_" + contacto.id + ".bgr";
        if (guardarMiniaturaDesdeFoto(foto.cuerpo, ruta)) {
            contacto.tieneMiniatura = cargarMiniaturaContacto(filas[fila], ruta);
            contacto.tieneFotoPerfil = contacto.tieneMiniatura;
        }
    }
}

bool cargarFotoPerfilContacto(const Contacto& contacto) {
    // Descarga y prepara la foto original del contacto para la vista de perfil.
    cerrarFotoVista();
    g_perfilFotoCargada = false;
    if (!contacto.tieneFotoPerfil) return false;
    std::string ruta = "sdmc:/3ds/Mensinger3DS/contacto_perfil_" + contacto.id + ".bgr";
    RespuestaHttp respuesta;
    if (!pedirHttp("GET", "/perfil/" + contacto.id + "/foto", NULL, "",
                   respuesta, FOTO_CAMARA_BYTES) || respuesta.estado != 200 ||
        respuesta.cuerpo.size() != FOTO_CAMARA_BYTES) return false;
    FILE* foto = fopen(ruta.c_str(), "wb");
    if (!foto) return false;
    bool escrito = fwrite(respuesta.cuerpo.data(), 1, respuesta.cuerpo.size(), foto) == respuesta.cuerpo.size();
    escrito = fclose(foto) == 0 && escrito;
    if (!escrito) { remove(ruta.c_str()); return false; }
    g_perfilFotoCargada = cargarFotoVista(ruta);
    return g_perfilFotoCargada;
}

bool subirFotoPerfilServidor(const std::string& ruta) {
    // Sube la imagen de perfil RGB565; el servidor produce su miniatura cuadrada.
    RespuestaHttp respuesta;
    return pedirHttp("POST", "/perfil/" + miID + "/foto",
        "application/x-mensinger-bgr565", ruta, respuesta, 256) && respuesta.estado == 200;
}

bool sincronizarPerfilServidor() {
    // Sincroniza texto y, si existe una foto pendiente, la carga después.
    bool texto = guardarPerfilServidor();
    FILE* foto = fopen(g_rutaFotoPerfil.c_str(), "rb");
    if (!foto) return texto;
    fclose(foto);
    return subirFotoPerfilServidor(g_rutaFotoPerfil) && texto;
}

void abrirPerfil() {
    // Abre el perfil propio dentro de Ajustes y selecciona la pestaña Perfil.
    cerrarFotoVista();
    g_perfilFotoCargada = false;
    g_perfilPropio = true;
    g_indicePerfilContacto = -1;
    g_fotoPerfilEnCurso = false;
    g_perfilError = false;
    focoPerfil = 0;
    g_scrollPerfil = 0;
    FILE* foto = fopen(g_rutaFotoPerfil.c_str(), "rb");
    if (foto) {
        fclose(foto);
        g_perfilFotoCargada = cargarFotoVista(g_rutaFotoPerfil);
    }
    estadoActual = PANTALLA_PERFIL;
}

void abrirPerfilContacto(int indice) {
    // Abre la ficha del contacto elegido y carga sus datos/foto si están disponibles.
    if (indice < 0 || indice >= (int)listaContactos.size()) return;
    if (g_ultimaCargaPerfiles == 0 || osGetTime() - g_ultimaCargaPerfiles > 30000)
        actualizarPerfilesContactos();
    g_perfilPropio = false;
    g_indicePerfilContacto = indice;
    g_perfilError = false;
    focoPerfil = 0;
    const Contacto& contacto = listaContactos[indice];
    cargarFotoPerfilContacto(contacto);
    estadoActual = PANTALLA_PERFIL;
}

