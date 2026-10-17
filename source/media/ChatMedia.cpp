#include "AppState.hpp"

void seleccionarAudio(int direccion) {
    // Mueve la selección al siguiente audio visible para reproducirlo o descargarlo.
    std::vector<long> ids;
    for (const MensajeV2& m : g_historialV2) if (m.audio) ids.push_back(m.id);
    if (ids.empty()) { g_audioSeleccionadoId = 0; return; }
    auto it = std::find(ids.begin(), ids.end(), g_audioSeleccionadoId);
    int indice = it == ids.end() ? (int)ids.size() - 1 : (int)(it - ids.begin());
    indice = std::max(0, std::min((int)ids.size() - 1, indice + direccion));
    g_audioSeleccionadoId = ids[indice];
}

void seleccionarFoto(int direccion) {
    // Mueve la selección entre fotos para abrir una de ellas a pantalla completa.
    std::vector<long> ids;
    for (const MensajeV2& m : g_historialV2) if (m.foto) ids.push_back(m.id);
    if (ids.empty()) { g_fotoSeleccionadaId = 0; return; }
    auto it = std::find(ids.begin(), ids.end(), g_fotoSeleccionadaId);
    int indice = it == ids.end() ? (int)ids.size() - 1 : (int)(it - ids.begin());
    indice = std::max(0, std::min((int)ids.size() - 1, indice + direccion));
    g_fotoSeleccionadaId = ids[indice];
}

bool mostrarFotoSeleccionada() {
    // Descarga si hace falta y abre la foto seleccionada en el estado de vista completa.
    if (g_fotoSeleccionadaId <= 0) return false;
    std::string ruta = rutaFotoV2(g_fotoSeleccionadaId);
    FILE* archivo = fopen(ruta.c_str(), "rb");
    if (archivo) fclose(archivo);
    else if (!g_servidorV2 || !descargarFoto(g_fotoSeleccionadaId, ruta)) return false;
    if (!cargarFotoVista(ruta)) {
        remove(ruta.c_str());
        return false;
    }
    estadoActual = PANTALLA_FOTO;
    return true;
}

bool reproducirAudioSeleccionado() {
    // Reproduce desde caché o descarga el WAV antes de entregarlo a VoiceNotes.
    if (g_audioSeleccionadoId <= 0) return false;
    std::string ruta = rutaAudioV2(g_audioSeleccionadoId);
    FILE* archivo = fopen(ruta.c_str(), "rb");
    if (archivo) fclose(archivo);
    else if (!g_servidorV2 || !descargarAudio(g_audioSeleccionadoId, ruta)) return false;
    if (reproducirNotaVoz(ruta)) return true;
    remove(ruta.c_str());
    return g_servidorV2 && descargarAudio(g_audioSeleccionadoId, ruta) && reproducirNotaVoz(ruta);
}

void cargarMensajesAnteriores() {
    // Solicita la página anterior al mensaje más antiguo ya cargado.
    if (g_desplazamientoChat + 3 < (int)g_historialV2.size()) {
        g_desplazamientoChat += 3;
        return;
    }
    if (g_servidorV2 && g_masAntiguos && !g_historialV2.empty()) {
        size_t anteriores = g_historialV2.size();
        if (cargarPaginaV2(g_historialV2.front().id) && g_historialV2.size() > anteriores)
            g_desplazamientoChat = std::min((int)g_historialV2.size() - 1, g_desplazamientoChat + 3);
    }
}

void enviarNotaPendiente() {
    // Completa la subida de una grabación confirmada y añade su ID al chat.
    if (!g_audioPendiente) return;
    if (!g_servidorV2) {
        g_estadoEnvio = ENVIO_ERROR;
        g_tiempoNotificacion = osGetTime();
        return;
    }
    long id = 0;
    bool enviado = enviarAudio(miID, contactoActivoID, g_rutaAudioPendiente, id);
    g_audioError = !enviado;
    g_estadoEnvio = enviado ? ENVIO_EXITO : ENVIO_ERROR;
    g_tiempoNotificacion = osGetTime();
    if (enviado) {
        remove(g_rutaAudioPendiente.c_str());
        g_audioPendiente = false;
        g_audioSeleccionadoId = id;
        g_desplazamientoChat = 0;
        cargarPaginaV2(0);
    }
}

void enviarFotoPendiente() {
    // Completa la subida de una captura confirmada y actualiza el historial.
    if (!g_fotoPendiente) return;
    if (!g_servidorV2) {
        g_fotoError = true;
        return;
    }
    long id = 0;
    bool enviado = enviarFoto(miID, contactoActivoID, g_rutaFotoPendiente, id);
    g_fotoError = !enviado;
    g_estadoEnvio = enviado ? ENVIO_EXITO : ENVIO_ERROR;
    g_tiempoNotificacion = osGetTime();
    if (enviado) {
        remove(g_rutaFotoPendiente.c_str());
        g_fotoPendiente = false;
        g_fotoSeleccionadaId = id;
        g_desplazamientoChat = 0;
        cargarPaginaV2(0);
    }
}

void cerrarCapturaFoto(bool descartarPendiente) {
    // Cierra la cámara y decide si conserva o borra el archivo temporal capturado.
    detenerVistaCamara();
    cerrarFotoVista();
    g_perfilFotoCargada = false;
    if (descartarPendiente) {
        if (g_fotoPerfilEnCurso && g_fotoPerfilPendiente) {
            remove(g_rutaFotoPerfilPendiente.c_str());
            g_fotoPerfilPendiente = false;
        } else if (!g_fotoPerfilEnCurso && g_fotoPendiente) {
            remove(g_rutaFotoPendiente.c_str());
            g_fotoPendiente = false;
        }
    }
    g_fotoError = false;
    estadoActual = g_fotoPerfilEnCurso ? g_pantallaRetornoFotoPerfil : PANTALLA_CHAT;
    if (g_fotoPerfilEnCurso) {
        FILE* foto = fopen(g_rutaFotoPerfil.c_str(), "rb");
        if (foto) { fclose(foto); if (estadoActual == PANTALLA_PERFIL) g_perfilFotoCargada = cargarFotoVista(g_rutaFotoPerfil); }
        g_fotoPerfilEnCurso = false;
    }
}

void abrirCamaraFoto() {
    // Inicia previsualización; el usuario todavía debe capturar y confirmar la foto.
    if (iniciarVistaCamara()) {
        g_fotoError = false;
        estadoActual = PANTALLA_CAMARA;
    } else {
        cerrarFotoVista();
        g_fotoError = true;
        estadoActual = g_fotoPerfilEnCurso ? g_pantallaRetornoFotoPerfil : PANTALLA_CHAT;
        if (g_fotoPerfilEnCurso) {
            FILE* foto = fopen(g_rutaFotoPerfil.c_str(), "rb");
            if (foto) { fclose(foto); if (estadoActual == PANTALLA_PERFIL) g_perfilFotoCargada = cargarFotoVista(g_rutaFotoPerfil); }
            g_fotoPerfilEnCurso = false;
        }
    }
}

void revisarFotoPendiente() {
    // Reintenta una carga pendiente tras recuperar conexión o al volver al chat.
    if (g_fotoPendiente) {
        cerrarFotoVista();
        if (cargarFotoVista(g_rutaFotoPendiente)) {
            g_fotoError = false;
            estadoActual = PANTALLA_CONFIRMAR_FOTO;
            return;
        }
        remove(g_rutaFotoPendiente.c_str());
        g_fotoPendiente = false;
    }
    abrirCamaraFoto();
}

void tomarFotoVista() {
    // Congela el fotograma actual en un archivo temporal para revisión.
    if (!hayFotogramaCamara()) return;
    if (!guardarFotogramaCamara(g_fotoPerfilEnCurso ? g_rutaFotoPerfilPendiente : g_rutaFotoPendiente)) {
        g_fotoError = true;
        return;
    }
    detenerVistaCamara();
    if (g_fotoPerfilEnCurso) g_fotoPerfilPendiente = true;
    else g_fotoPendiente = true;
    g_fotoError = false;
    estadoActual = PANTALLA_CONFIRMAR_FOTO;
}

void repetirFotoVista() {
    // Descarta la toma actual y regresa a la vista en vivo.
    if (g_fotoPerfilEnCurso) {
        if (g_fotoPerfilPendiente) remove(g_rutaFotoPerfilPendiente.c_str());
        g_fotoPerfilPendiente = false;
    } else {
        if (g_fotoPendiente) remove(g_rutaFotoPendiente.c_str());
        g_fotoPendiente = false;
    }
    abrirCamaraFoto();
}

void confirmarEnvioFoto() {
    // Marca la toma revisada como pendiente y comienza su envío al servidor.
    if (g_fotoPerfilEnCurso) {
        bool enviado = subirFotoPerfilServidor(g_rutaFotoPerfilPendiente);
        g_perfilError = !enviado;
        {
            if (rename(g_rutaFotoPerfilPendiente.c_str(), g_rutaFotoPerfil.c_str()) != 0) {
                remove(g_rutaFotoPerfil.c_str());
                if (rename(g_rutaFotoPerfilPendiente.c_str(), g_rutaFotoPerfil.c_str()) != 0)
                    g_perfilError = true;
                else g_fotoPerfilPendiente = false;
            } else g_fotoPerfilPendiente = false;
            cerrarFotoVista();
            estadoActual = g_pantallaRetornoFotoPerfil;
            g_perfilFotoCargada = estadoActual == PANTALLA_PERFIL && cargarFotoVista(g_rutaFotoPerfil);
            g_fotoPerfilEnCurso = false;
        }
        return;
    }
    enviarFotoPendiente();
    if (!g_fotoPendiente) {
        cerrarFotoVista();
        estadoActual = PANTALLA_CHAT;
    }
}


