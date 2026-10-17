#include "AppState.hpp"

// Composición de las pantallas superior e inferior. El dibujo es deliberadamente
// de solo presentación: Input.cpp modifica selección/estado y estas funciones
// reflejan ese estado en controles táctiles y visuales.
static void dibujarListaContactosRenovada(u32 fondo, u32 tarjeta, u32 texto, u32 secundario) {
    C2D_TargetClear(targetAbajo, fondo);
    C2D_SceneBegin(targetAbajo);
    u32 blanco = C2D_Color32(255, 255, 255, 255);
    u32 azul = C2D_Color32(28, 135, 205, 255);
    dibujarTextoUI("CONVERSACIONES", 13.0f, 8.0f, 0.43f, texto);
    dibujarTextoUI(std::to_string(listaContactos.size()) + " contactos", 230.0f, 10.0f, 0.32f, secundario);
    if (listaContactos.empty()) {
        dibujarRectanguloRedondeado(12.0f, 42.0f, 296.0f, 127.0f, 14.0f, tarjeta);
        C2D_DrawCircleSolid(160.0f, 85.0f, 0.6f, 24.0f, azul);
        dibujarTextoUI("+", 151.0f, 66.0f, 0.80f, blanco);
        dibujarTextoUI("Aun no tienes contactos", 64.0f, 113.0f, 0.46f, texto);
        dibujarTextoUI("Agrega uno o conoce gente nueva", 51.0f, 139.0f, 0.33f, secundario);
    } else {
        for (int slot = 0; slot < 3; ++slot) {
            int indice = scrollContactos + slot;
            if (indice >= (int)listaContactos.size()) break;
            const Contacto& contacto = listaContactos[indice];
            float x = 10.0f;
            float y = 37.0f + slot * 50.0f;
            bool elegido = indice == contactoSeleccionado;
            if (elegido) dibujarRectanguloRedondeado(x - 2.0f, y - 2.0f,
                262.0f, 48.0f, 11.0f, azul);
            dibujarRectanguloRedondeado(x, y, 258.0f, 44.0f, 9.0f, tarjeta);
            C2D_DrawCircleSolid(x + 27.0f, y + 22.0f, 0.6f, 17.0f, azul);
            if (contacto.tieneMiniatura)
                C2D_DrawImageAt(obtenerMiniaturaContacto(slot), x + 10.0f, y + 5.0f, 0.7f);
            else dibujarTextoUI("@", x + 20.0f, y + 11.0f, 0.43f, blanco);
            C2D_DrawCircleSolid(x + 43.0f, y + 36.0f, 0.8f, 5.0f,
                contacto.conectado ? C2D_Color32(42, 214, 133, 255) :
                C2D_Color32(135, 149, 166, 255));
            dibujarTextoUI(limitarTextoUtf8(contacto.usuarioPerfil.empty() ?
                contacto.nombre : contacto.usuarioPerfil, 22), x + 54.0f, y + 5.0f,
                0.45f, colorApodoTarjeta(contacto.colorNombre, g_modoOscuro));
            dibujarTextoUI(limitarTextoUtf8(contacto.apodoPerfil.empty() ?
                "ID " + contacto.id : "@" + contacto.apodoPerfil, 29),
                x + 54.0f, y + 26.0f, 0.32f,
                contacto.apodoPerfil.empty() ? secundario :
                colorApodoTarjeta(contacto.colorApodo, g_modoOscuro));
            if (contacto.tieneNovedad) {
                float pulso = (sinf((float)osGetTime() / 220.0f) + 1.0f) * 0.5f;
                C2D_DrawCircleSolid(x + 238.0f, y + 22.0f, 0.7f,
                    10.0f + 2.0f * pulso, C2D_Color32(39, 203, 122, 255));
                dibujarTextoUI(contacto.pendientes > 9 ? "9+" :
                    std::to_string(contacto.pendientes), x + 230.0f, y + 14.0f,
                    0.29f, blanco);
            } else if (elegido) dibujarTextoUI(">", x + 232.0f, y + 11.0f, 0.46f, azul);
        }
        if (listaContactos.size() > 3) {
            u32 flecha = g_modoOscuro ? C2D_Color32(63, 77, 94, 255) :
                C2D_Color32(207, 222, 234, 255);
            dibujarRectanguloRedondeado(278.0f, 37.0f, 30.0f, 67.0f, 8.0f, flecha);
            dibujarRectanguloRedondeado(278.0f, 110.0f, 30.0f, 67.0f, 8.0f, flecha);
            dibujarTextoUI("^", 286.0f, 57.0f, 0.64f, texto);
            dibujarTextoUI("v", 286.0f, 130.0f, 0.64f, texto);
        }
    }
    dibujarRectanguloRedondeado(10.0f, 192.0f, 69.0f, 39.0f, 9.0f, azul);
    dibujarRectanguloRedondeado(83.0f, 192.0f, 75.0f, 39.0f, 9.0f,
        C2D_Color32(29, 163, 139, 255));
    dibujarRectanguloRedondeado(162.0f, 192.0f, 72.0f, 39.0f, 9.0f,
        C2D_Color32(97, 105, 199, 255));
    dibujarRectanguloRedondeado(238.0f, 192.0f, 72.0f, 39.0f, 9.0f,
        C2D_Color32(202, 112, 51, 255));
    dibujarTextoUI("X ANADIR", 17.0f, 205.0f, 0.30f, blanco);
    dibujarTextoUI("BUSCAR", 94.0f, 205.0f, 0.32f, blanco);
    dibujarTextoUI("CONOCER", 166.0f, 205.0f, 0.30f, blanco);
    int pendientes = 0;
    for (const auto& item : g_desconocidos) pendientes += item.pendientes;
    dibujarTextoUI(pendientes > 0 ? "EXTRANOS " + std::to_string(pendientes) : "EXTRANOS",
        242.0f, 205.0f, 0.28f, blanco);
}

static void dibujarControlesChatRenovados(u32 texto, u32 secundario) {
    u32 blanco = C2D_Color32(255, 255, 255, 255);
    u32 azul = C2D_Color32(28, 135, 205, 255);
    u32 verde = C2D_Color32(25, 160, 139, 255);
    u32 violeta = C2D_Color32(97, 105, 199, 255);
    u32 neutro = g_modoOscuro ? C2D_Color32(61, 72, 89, 255) :
        C2D_Color32(188, 204, 219, 255);
    u32 inactivo = g_modoOscuro ? C2D_Color32(69, 75, 84, 255) :
        C2D_Color32(135, 145, 155, 255);
    dibujarRectanguloRedondeado(10.0f, 5.0f, 146.0f, 35.0f, 9.0f,
        g_chatMedios ? neutro : azul);
    dibujarRectanguloRedondeado(164.0f, 5.0f, 146.0f, 35.0f, 9.0f,
        g_chatMedios ? azul : neutro);
    dibujarTextoUI("MENSAJES", 39.0f, 16.0f, 0.43f, g_chatMedios ? texto : blanco);
    dibujarTextoUI("MULTIMEDIA", 184.0f, 16.0f, 0.40f, g_chatMedios ? blanco : texto);
    if (!g_chatMedios) {
        dibujarRectanguloRedondeado(12.0f, 50.0f, 205.0f, 60.0f, 12.0f, azul);
        dibujarTextoUI("A  ESCRIBIR MENSAJE", 29.0f, 72.0f, 0.47f, blanco);
        dibujarRectanguloRedondeado(228.0f, 50.0f, 80.0f, 60.0f, 12.0f,
            notaVozActiva() ? C2D_Color32(205, 61, 74, 255) :
            g_servidorV2 ? verde : inactivo);
        dibujarTextoUI(notaVozActiva() ? std::to_string((duracionNotaVozMs() + 999) / 1000) + " s" :
            g_audioPendiente ? "Y ENVIAR" : "Y AUDIO", 238.0f, 72.0f, 0.34f, blanco);
        dibujarRectanguloRedondeado(12.0f, 123.0f, 90.0f, 44.0f, 9.0f,
            g_servidorV2 ? violeta : inactivo);
        dibujarRectanguloRedondeado(111.0f, 123.0f, 95.0f, 44.0f, 9.0f, neutro);
        dibujarRectanguloRedondeado(215.0f, 123.0f, 93.0f, 44.0f, 9.0f, neutro);
        dibujarTextoUI(g_fotoPendiente ? "X VER" : "X FOTO", 26.0f, 138.0f, 0.38f, blanco);
        dibujarTextoUI("L ANTES", 123.0f, 138.0f, 0.36f, texto);
        dibujarTextoUI("R NUEVOS", 224.0f, 138.0f, 0.35f, texto);
        dibujarRectanguloRedondeado(12.0f, 188.0f, 90.0f, 40.0f, 9.0f, neutro);
        dibujarTextoUI("B VOLVER", 25.0f, 202.0f, 0.36f, texto);
        dibujarTextoUI("HISTORIAL EN PANTALLA SUPERIOR", 115.0f, 191.0f,
            0.31f, secundario);
        dibujarTextoUI("L anteriores   R recientes", 115.0f, 211.0f,
            0.30f, secundario);
        if (g_estadoEnvio != ENVIO_NINGUNO && osGetTime() - g_tiempoNotificacion < 3000)
            dibujarTextoUI(getText(g_estadoEnvio == ENVIO_EXITO ? TXT_ENVIADO : TXT_ERROR),
                12.0f, 229.0f, 0.30f, g_estadoEnvio == ENVIO_EXITO ? verde :
                C2D_Color32(213, 67, 76, 255));
        else if (g_audioError || g_fotoError)
            dibujarTextoUI(g_fotoError ? "Error de camara/foto" :
                getText(TXT_AUDIO_NO_DISPONIBLE), 12.0f, 229.0f, 0.30f,
                C2D_Color32(213, 67, 76, 255));
    } else {
        dibujarRectanguloRedondeado(10.0f, 48.0f, 300.0f, 78.0f, 11.0f,
            g_modoOscuro ? C2D_Color32(44, 51, 63, 255) : C2D_Color32(248, 250, 253, 255));
        dibujarTextoUI("NOTAS DE VOZ", 19.0f, 55.0f, 0.38f, texto);
        dibujarRectanguloRedondeado(12.0f, 82.0f, 55.0f, 38.0f, 8.0f, neutro);
        dibujarRectanguloRedondeado(75.0f, 82.0f, 170.0f, 38.0f, 8.0f,
            g_audioSeleccionadoId ? verde : inactivo);
        dibujarRectanguloRedondeado(253.0f, 82.0f, 55.0f, 38.0f, 8.0f, neutro);
        dibujarTextoUI("<", 32.0f, 91.0f, 0.53f, texto);
        dibujarTextoUI(g_audioSeleccionadoId ? "A REPRODUCIR" : "SIN AUDIO",
            101.0f, 94.0f, 0.36f, blanco);
        dibujarTextoUI(">", 275.0f, 91.0f, 0.53f, texto);
        dibujarRectanguloRedondeado(10.0f, 132.0f, 300.0f, 72.0f, 11.0f,
            g_modoOscuro ? C2D_Color32(44, 51, 63, 255) : C2D_Color32(248, 250, 253, 255));
        dibujarTextoUI("FOTOS", 19.0f, 136.0f, 0.38f, texto);
        dibujarRectanguloRedondeado(12.0f, 160.0f, 55.0f, 38.0f, 8.0f, neutro);
        dibujarRectanguloRedondeado(75.0f, 160.0f, 170.0f, 38.0f, 8.0f,
            g_fotoSeleccionadaId ? violeta : inactivo);
        dibujarRectanguloRedondeado(253.0f, 160.0f, 55.0f, 38.0f, 8.0f, neutro);
        dibujarTextoUI("<", 32.0f, 169.0f, 0.53f, texto);
        dibujarTextoUI(g_fotoSeleccionadaId ? "A VER FOTO" : "SIN FOTOS",
            108.0f, 171.0f, 0.36f, blanco);
        dibujarTextoUI(">", 275.0f, 169.0f, 0.53f, texto);
        dibujarRectanguloRedondeado(12.0f, 207.0f, 130.0f, 27.0f, 7.0f,
            g_pantallaRetornoChat == PANTALLA_EXTRANOS ? verde : C2D_Color32(177, 61, 71, 255));
        dibujarTextoUI(g_pantallaRetornoChat == PANTALLA_EXTRANOS ? "AGREGAR CONTACTO" :
            "BORRAR CONTACTO", 17.0f, 216.0f, 0.29f, blanco);
        dibujarTextoUI("B VOLVER AL CHAT", 178.0f, 216.0f, 0.31f, secundario);
    }
    if ((g_chatMedios && focoChat >= 6) || (!g_chatMedios && focoChat < 6)) {
        float fx = (float)CHAT_FOCO_X[focoChat] - 2.0f;
        float fy = (float)CHAT_FOCO_Y[focoChat] - 2.0f;
        float fw = (float)CHAT_FOCO_ANCHO[focoChat] + 4.0f;
        float fh = (float)CHAT_FOCO_ALTO[focoChat] + 4.0f;
        float pulso = (sinf((float)osGetTime() / 250.0f) + 1.0f) * 0.5f;
        u32 focoColor = C2D_Color32(255, 191 + (u8)(pulso * 45), 52, 255);
        C2D_DrawRectSolid(fx, fy, 0.9f, fw, 2.0f, focoColor);
        C2D_DrawRectSolid(fx, fy + fh - 2.0f, 0.9f, fw, 2.0f, focoColor);
        C2D_DrawRectSolid(fx, fy, 0.9f, 2.0f, fh, focoColor);
        C2D_DrawRectSolid(fx + fw - 2.0f, fy, 0.9f, 2.0f, fh, focoColor);
    }
}

static void dibujarProgresoChatSuperior(u32 texto, u32 secundario) {
    int cargados = g_servidorV2 ? (int)g_historialV2.size() : (int)historialChat.size();
    int total = g_servidorV2 ? std::max(g_totalMensajesChat, cargados) : cargados;
    int desplazamiento = std::max(0, std::min(g_desplazamientoChat, std::max(0, total - 1)));
    bool reciente = desplazamiento == 0;
    u32 blanco = C2D_Color32(255, 255, 255, 255);
    if (reciente) {
        float pulso = (sinf((float)osGetTime() / 430.0f) + 1.0f) * 0.5f;
        u32 borde = C2D_Color32(34, 188, 124, (u8)(175 + pulso * 80));
        dibujarRectanguloRedondeado(12.0f, 42.0f, 376.0f, 37.0f, 10.0f, borde);
        dibujarRectanguloRedondeado(15.0f, 45.0f, 370.0f, 31.0f, 8.0f,
            C2D_Color32(23, 132, 94, 255));
        C2D_DrawCircleSolid(33.0f, 60.0f, 0.7f, 10.0f,
            C2D_Color32(72, 220, 151, 255));
        dibujarTextoUI("✓", 27.0f, 50.0f, 0.51f, blanco);
        dibujarTextoUI("AL DIA - MENSAJES MAS RECIENTES", 49.0f, 51.0f,
            0.39f, blanco);
    } else {
        dibujarRectanguloRedondeado(13.0f, 43.0f, 374.0f, 33.0f, 9.0f,
            g_modoOscuro ? C2D_Color32(45, 64, 83, 255) :
            C2D_Color32(225, 239, 249, 255));
        dibujarTextoUI("HISTORIAL", 24.0f, 52.0f, 0.39f, texto);
        dibujarTextoUI("R: volver a recientes", 188.0f, 52.0f,
            0.35f, secundario);
    }
    int paginas = std::max(1, total);
    int pagina = std::min(paginas, desplazamiento + 1);
    dibujarTextoUI(std::to_string(total) + " mensajes  |  pagina " +
        std::to_string(pagina) + "/" + std::to_string(paginas),
        18.0f, 83.0f, 0.32f, secundario);
    float progreso = total <= 1 ? 1.0f :
        1.0f - (float)desplazamiento / (float)(total - 1);
    C2D_DrawRectSolid(17.0f, 98.0f, 0.5f, 366.0f, 4.0f,
        g_modoOscuro ? C2D_Color32(60, 78, 97, 255) : C2D_Color32(198, 215, 228, 255));
    C2D_DrawRectSolid(17.0f, 98.0f, 0.6f, 366.0f * progreso, 4.0f,
        reciente ? C2D_Color32(45, 203, 132, 255) : C2D_Color32(33, 141, 213, 255));
}

void dibujarPantallaActual() {
    // --- RENDERIZADO APP PRINCIPAL ---
    if (estadoActual == PANTALLA_CAMARA && !actualizarVistaCamara()) {
        cerrarCapturaFoto(false);
        g_fotoError = true;
    }
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(bufTexto);

    // --- PALETA DE COLORES DINÁMICA ---
    u32 colFondoArriba = g_modoOscuro ? C2D_Color32(20, 20, 25, 255)  : C2D_Color32(255, 255, 255, 255);
    u32 colFondoAbajo  = g_modoOscuro ? C2D_Color32(30, 30, 35, 255)  : C2D_Color32(230, 230, 235, 255);
    u32 colTextoMain   = g_modoOscuro ? C2D_Color32(240, 240, 240, 255): C2D_Color32(0, 0, 0, 255);
    u32 colTextoSub    = g_modoOscuro ? C2D_Color32(160, 160, 170, 255): C2D_Color32(100, 100, 100, 255);
    u32 colTarjeta     = g_modoOscuro ? C2D_Color32(45, 45, 55, 255)  : C2D_Color32(255, 255, 255, 255);

    if (estadoActual == PANTALLA_CONFIGURACION) {
        dibujarPantallaConfiguracion();
    }
    else if (estadoActual == PANTALLA_PERFIL) {
        const Contacto* contactoPerfil = (!g_perfilPropio && g_indicePerfilContacto >= 0 &&
            g_indicePerfilContacto < (int)listaContactos.size()) ?
            &listaContactos[g_indicePerfilContacto] : NULL;
        C2D_TargetClear(targetArriba, colFondoArriba);
        C2D_SceneBegin(targetArriba);
        if (g_perfilFotoCargada) C2D_DrawImageAt(obtenerFotoVista(), 0.0f, 0.0f, 0.5f);
        else {
            dibujarBarraEstadoSuperior();
            dibujarTextoUI(g_perfilPropio ? "MI PERFIL" : "PERFIL DEL CONTACTO",
                25.0f, 48.0f, g_perfilPropio ? 0.85f : 0.65f, colTextoMain);
            dibujarRectanguloRedondeado(25.0f, 83.0f, 350.0f, 106.0f, 15.0f, colTarjeta);
            C2D_DrawCircleSolid(76.0f, 135.0f, 0.5f, 30.0f, C2D_Color32(32, 129, 222, 255));
            dibujarTextoUI("@", 64.0f, 116.0f, 0.85f, C2D_Color32(255, 255, 255, 255));
            std::string nombreTarjeta = g_perfilPropio ?
                (g_perfilApodo.empty() ? "Tu apodo" : g_perfilApodo) :
                (contactoPerfil ? contactoPerfil->nombre : "Contacto");
            std::string apodoTarjeta = g_perfilPropio ? g_perfilNombre :
                (contactoPerfil ? contactoPerfil->apodoPerfil : "");
            dibujarTextoUI(limitarTextoUtf8(nombreTarjeta, 24),
                125.0f, 112.0f, 0.61f,
                g_perfilPropio ? colorApodoTarjeta(g_colorNombre, g_modoOscuro) :
                contactoPerfil ? colorApodoTarjeta(contactoPerfil->colorNombre, g_modoOscuro) : colTextoMain);
            dibujarTextoUI(apodoTarjeta.empty() ? (g_perfilPropio ? "" : "Sin apodo configurado") :
                "@" + limitarTextoUtf8(apodoTarjeta, 24), 125.0f, 137.0f, 0.39f,
                g_perfilPropio ? colorApodoTarjeta(g_colorApodo, g_modoOscuro) :
                contactoPerfil ? colorApodoTarjeta(contactoPerfil->colorApodo, g_modoOscuro) : colTextoSub);
            dibujarTextoUI("ID " + (g_perfilPropio ? miID :
                (contactoPerfil ? contactoPerfil->id : "")), 125.0f, 157.0f, 0.38f, colTextoSub);
        }
        C2D_TargetClear(targetAbajo, colFondoAbajo);
        C2D_SceneBegin(targetAbajo);
        if (g_perfilPropio) {
            const char* etiquetas[] = {"USUARIO", "APODO", "BIOGRAFIA", "DESCRIPCION",
                "APARECER EN BUSQUEDAS", "APARECER EN CONOCER", "CONTRASENA", "FOTO",
                "COLOR BURBUJA", "FONDO DEL CHAT", "COLOR DEL TEXTO", "COLOR DEL NOMBRE",
                "COLOR DEL APODO", "SINCRONIZAR"};
            const std::string valores[] = {g_perfilNombre, g_perfilApodo, g_perfilBio,
                g_perfilDescripcion, g_mostrarEnBusqueda ? "SI" : "NO",
                g_quieroConocerGente ? "SI" : "NO",
                g_contrasenaActiva ? "ACTIVA (X para quitar)" : "SIN CONTRASENA",
                "A / TOMAR FOTO", nombreColorPerfil(g_colorChat), nombreColorPerfil(g_colorFondo),
                nombreColorPerfil(g_colorTexto), nombreColorPerfil(g_colorNombre),
                nombreColorPerfil(g_colorApodo), "A / GUARDAR"};
            for (int fila = 0; fila < 7; fila++) {
                int i = g_scrollPerfil + fila;
                if (i >= 14) break;
                float y = 8.0f + fila * 29.0f;
                if (focoPerfil == i)
                    dibujarRectanguloRedondeado(8.0f, y - 1.0f, 304.0f, 28.0f, 7.0f,
                        C2D_Color32(32, 129, 222, 255));
                dibujarRectanguloRedondeado(10.0f, y, 300.0f, 26.0f, 6.0f, colTarjeta);
                dibujarTextoUI(etiquetas[i], 16.0f, y + 7.0f, 0.29f, colTextoSub);
                dibujarTextoUI(limitarTextoUtf8(valores[i].empty() ? "Editar" : valores[i], 23),
                    153.0f, y + 6.0f, 0.34f, colTextoMain);
                if (i >= 8 && i <= 12) {
                    int indice = i == 8 ? g_colorChat : i == 9 ? g_colorFondo :
                                 i == 10 ? g_colorTexto : i == 11 ? g_colorNombre : g_colorApodo;
                    u32 muestra = i == 8 ? colorBurbujaPerfil(indice) :
                        i == 9 ? colorFondoPerfil(indice, true) :
                        i == 10 ? colorTextoPerfil(indice) : colorApodoPerfil(indice);
                    C2D_DrawCircleSolid(292.0f, y + 13.0f, 0.7f, 7.0f, muestra);
                }
            }
            dibujarTextoUI(std::to_string(g_scrollPerfil + 1) + "-" +
                std::to_string(std::min(14, g_scrollPerfil + 7)) + "/14", 256.0f,
                214.0f, 0.30f, colTextoSub);
            dibujarRectanguloRedondeado(174.0f, 211.0f, 30.0f, 18.0f, 4.0f, colTarjeta);
            dibujarRectanguloRedondeado(210.0f, 211.0f, 30.0f, 18.0f, 4.0f, colTarjeta);
            dibujarTextoUI("^", 184.0f, 213.0f, 0.35f, colTextoMain);
            dibujarTextoUI("v", 220.0f, 213.0f, 0.35f, colTextoMain);
            if (g_perfilError)
                dibujarTextoUI("Error al sincronizar", 12.0f, 228.0f,
                    0.30f, C2D_Color32(210, 70, 75, 255));
            else dibujarTextoUI("A cambiar  |  X borrar  |  B volver  |  ^v scroll", 11.0f, 228.0f,
                    0.29f, colTextoSub);
        } else {
            std::string etiquetas[] = {"USUARIO", "APODO", "BIOGRAFIA", "DESCRIPCION", "IDENTIFICADOR"};
            std::string valores[5];
            if (contactoPerfil && contactoPerfil->tienePerfil) {
                valores[0] = contactoPerfil->usuarioPerfil;
                valores[1] = contactoPerfil->apodoPerfil;
                valores[2] = contactoPerfil->bioPerfil;
                valores[3] = contactoPerfil->descripcionPerfil;
                valores[4] = contactoPerfil->id;
            } else {
                etiquetas[0] = "PERFIL";
                valores[0] = "Este contacto aun no configura su perfil";
                valores[4] = contactoPerfil ? contactoPerfil->id : "";
            }
            for (int i = 0; i < 5; i++) {
                float y = 13.0f + i * 40.0f;
                dibujarRectanguloRedondeado(10.0f, y, 300.0f, 36.0f, 8.0f, colTarjeta);
                dibujarTextoUI(etiquetas[i], 20.0f, y + 3.0f, 0.31f, colTextoSub);
                dibujarTextoUI(limitarTextoUtf8(valores[i].empty() ? "—" : valores[i], 40),
                    20.0f, y + 18.0f, 0.39f, colTextoMain);
            }
            dibujarTextoUI("A / B  VOLVER A CONTACTOS", 67.0f, 222.0f, 0.39f, colTextoSub);
        }
    }

    else if (estadoActual == PANTALLA_BUSQUEDA || estadoActual == PANTALLA_CONOCER) {
        bool conocer = estadoActual == PANTALLA_CONOCER;
        C2D_TargetClear(targetArriba, colFondoArriba);
        C2D_SceneBegin(targetArriba);
        dibujarBarraEstadoSuperior();
        dibujarTextoUI(conocer ? "CONOCER GENTE" : "BUSCAR USUARIOS",
                       22.0f, 42.0f, 0.72f, colTextoMain);
        dibujarTextoUI(conocer ? "Personas abiertas a nuevos contactos" :
                       "Solo aparecen perfiles visibles", 22.0f, 74.0f, 0.42f, colTextoSub);
        dibujarRectanguloRedondeado(18.0f, 105.0f, 364.0f, 52.0f, 10.0f, colTarjeta);
        dibujarTextoUI("BUSQUEDA", 30.0f, 114.0f, 0.36f, colTextoSub);
        dibujarTextoUI(g_consultaBusqueda.empty() ? (conocer ? "Explora o filtra por nombre" : "Toca Buscar para escribir") :
                       limitarTextoUtf8(g_consultaBusqueda, 38), 30.0f, 132.0f, 0.48f, colTextoMain);
        dibujarTextoUI(std::to_string(g_resultadosBusqueda.size()) + " resultados", 25.0f,
                       178.0f, 0.45f, colTextoSub);
        C2D_TargetClear(targetAbajo, colFondoAbajo);
        C2D_SceneBegin(targetAbajo);
        for (int i = 0; i < 3; ++i) {
            int indice = g_scrollBusqueda + i;
            if (indice >= (int)g_resultadosBusqueda.size()) break;
            const ResultadoBusqueda& item = g_resultadosBusqueda[indice];
            float y = 7.0f + i * 55.0f;
            dibujarRectanguloRedondeado(9.0f, y, 302.0f, 51.0f, 8.0f, colTarjeta);
            dibujarTextoUI(limitarTextoUtf8(item.nombre.empty() ? item.apodo : item.nombre, 21),
                           17.0f, y + 5.0f, 0.44f, colTextoMain);
            dibujarTextoUI(limitarTextoUtf8(item.bio, 37), 17.0f, y + 27.0f, 0.32f, colTextoSub);
            dibujarRectanguloRedondeado(232.0f, y + 8.0f, 69.0f, 34.0f, 7.0f,
                                        C2D_Color32(29, 151, 130, 255));
            dibujarTextoUI("AGREGAR", 238.0f, y + 18.0f, 0.31f,
                           C2D_Color32(255, 255, 255, 255));
        }
        dibujarRectanguloRedondeado(10.0f, 179.0f, 76.0f, 26.0f, 6.0f,
                                    C2D_Color32(29, 132, 213, 255));
        dibujarRectanguloRedondeado(91.0f, 179.0f, 102.0f, 26.0f, 6.0f,
                                    C2D_Color32(29, 132, 213, 255));
        dibujarRectanguloRedondeado(198.0f, 179.0f, 112.0f, 26.0f, 6.0f,
                                    C2D_Color32(29, 132, 213, 255));
        u32 blanco = C2D_Color32(255, 255, 255, 255);
        dibujarTextoUI("VOLVER", 19.0f, 187.0f, 0.33f, blanco);
        dibujarTextoUI("POR NOMBRE", 100.0f, 187.0f, 0.32f, blanco);
        dibujarTextoUI("CONOCER", 218.0f, 187.0f, 0.33f, blanco);
        dibujarTextoUI("BUSCAR", 22.0f, 216.0f, 0.39f, colTextoMain);
        dibujarTextoUI("ARRIBA / ABAJO: mas resultados", 134.0f, 218.0f, 0.29f, colTextoSub);
    }

    else if (estadoActual == PANTALLA_EXTRANOS) {
        int pendientes = 0;
        for (const auto& item : g_desconocidos) pendientes += item.pendientes;
        C2D_TargetClear(targetArriba, colFondoArriba);
        C2D_SceneBegin(targetArriba);
        dibujarBarraEstadoSuperior();
        dibujarTextoUI("MENSAJES DE EXTRANOS", 21.0f, 45.0f, 0.67f, colTextoMain);
        dibujarTextoUI("Personas que aun no agregaste a contactos", 22.0f, 78.0f, 0.40f, colTextoSub);
        dibujarRectanguloRedondeado(20.0f, 110.0f, 360.0f, 83.0f, 12.0f, colTarjeta);
        dibujarTextoUI(std::to_string(g_desconocidos.size()) + " conversaciones", 34.0f, 123.0f,
                       0.55f, colTextoMain);
        dibujarTextoUI(std::to_string(pendientes) + " mensajes pendientes", 34.0f, 157.0f,
                       0.43f, pendientes ? C2D_Color32(220, 126, 53, 255) : colTextoSub);
        dibujarTextoUI("A abrir   X agregar   B volver", 24.0f, 215.0f, 0.39f, colTextoSub);
        C2D_TargetClear(targetAbajo, colFondoAbajo);
        C2D_SceneBegin(targetAbajo);
        if (g_desconocidos.empty()) {
            dibujarRectanguloRedondeado(12.0f, 42.0f, 296.0f, 126.0f, 12.0f, colTarjeta);
            dibujarTextoUI("Sin mensajes de desconocidos", 37.0f, 96.0f, 0.42f, colTextoMain);
        }
        for (int slot = 0; slot < 3; ++slot) {
            int indice = g_scrollDesconocidos + slot;
            if (indice >= (int)g_desconocidos.size()) break;
            const auto& item = g_desconocidos[indice];
            float y = 37.0f + 50.0f * slot;
            if (indice == g_seleccionDesconocidos)
                dibujarRectanguloRedondeado(8.0f, y - 2.0f, 304.0f, 48.0f, 9.0f,
                    C2D_Color32(202, 112, 51, 255));
            dibujarRectanguloRedondeado(10.0f, y, 300.0f, 44.0f, 8.0f, colTarjeta);
            dibujarTextoUI("ID " + item.id, 18.0f, y + 4.0f, 0.43f, colTextoMain);
            dibujarTextoUI(std::to_string(item.total) + " mensajes  ·  " +
                std::to_string(item.pendientes) + " nuevos", 18.0f, y + 25.0f,
                0.31f, colTextoSub);
            dibujarRectanguloRedondeado(213.0f, y + 6.0f, 88.0f, 32.0f, 7.0f,
                C2D_Color32(29, 151, 130, 255));
            dibujarTextoUI("+ CONTACTO", 220.0f, y + 16.0f, 0.30f,
                C2D_Color32(255, 255, 255, 255));
        }
        dibujarRectanguloRedondeado(10.0f, 192.0f, 105.0f, 38.0f, 8.0f,
            C2D_Color32(28, 135, 205, 255));
        dibujarTextoUI("B VOLVER", 27.0f, 205.0f, 0.36f,
            C2D_Color32(255, 255, 255, 255));
        dibujarTextoUI("< ANTERIORES       SIGUIENTES >", 123.0f, 206.0f,
            0.30f, colTextoSub);
    }

    // ==========================================
    // 3. PANTALLA CONTACTOS
    // ==========================================
    else if (estadoActual == PANTALLA_CONTACTOS) {
        float latido = (sinf((float)osGetTime() / 370.0f) + 1.0f) * 0.5f;
        // Pantalla Superior
        C2D_TargetClear(targetArriba, colFondoArriba);
        C2D_SceneBegin(targetArriba);
        C2D_DrawRectangle(0.0f, 0.0f, 0.5f, 400.0f, 35.0f,
            C2D_Color32(16, 87, 151, 255), C2D_Color32(23, 122, 189, 255),
            C2D_Color32(16, 87, 151, 255), C2D_Color32(23, 122, 189, 255));
        dibujarTextoUI("3DSocial", 18.0f, 8.0f, 0.60f, C2D_Color32(255, 255, 255, 255));
        dibujarIconoEngranaje(318.0f, 6.0f, C2D_Color32(255, 255, 255, 255));
        dibujarTextoUI("SELECT", 348.0f, 12.0f, 0.35f, C2D_Color32(255, 255, 255, 255));
        dibujarTextoUI("Tus contactos", 20.0f, 48.0f, 0.76f, colTextoMain);
        dibujarTextoUI(std::to_string(listaContactos.size()) +
            (listaContactos.size() == 1 ? " conversacion" : " conversaciones"),
            22.0f, 75.0f, 0.40f, colTextoSub);
        int totalPendientes = 0;
        for (const auto& contacto : listaContactos) totalPendientes += contacto.pendientes;
        for (const auto& item : g_desconocidos) totalPendientes += item.pendientes;
        if (totalPendientes > 0)
            dibujarTextoUI(std::to_string(totalPendientes) + " nuevos", 280.0f, 76.0f,
                0.39f, C2D_Color32(218, 121, 57, 255));
        dibujarRectanguloRedondeado(20.0f, 96.0f, 360.0f, 39.0f, 10.0f,
            g_modoOscuro ? C2D_Color32(39, 58, 76, 255) : C2D_Color32(232, 243, 251, 255));
        dibujarTextoUI("TU ID", 31.0f, 106.0f, 0.35f, colTextoSub);
        dibujarTextoUI(miID, 86.0f, 103.0f, 0.53f,
            g_modoOscuro ? C2D_Color32(121, 207, 255, 255) : C2D_Color32(20, 105, 171, 255));
        if (!listaContactos.empty()) {
            const Contacto& elegido = listaContactos[contactoSeleccionado];
            u32 acento = C2D_Color32(31, 141, 209, (u8)(145 + latido * 90));
            dibujarRectanguloRedondeado(18.0f, 150.0f, 364.0f, 65.0f, 12.0f, acento);
            dibujarRectanguloRedondeado(21.0f, 153.0f, 358.0f, 59.0f, 10.0f, colTarjeta);
            int slot = contactoSeleccionado - scrollContactos;
            bool foto = elegido.tieneMiniatura && slot >= 0 && slot < 3;
            if (foto) C2D_DrawImageAt(obtenerMiniaturaContacto(slot), 31.0f, 166.0f, 0.7f);
            else {
                C2D_DrawCircleSolid(47.0f, 182.0f, 0.7f, 17.0f, C2D_Color32(31, 141, 209, 255));
                dibujarTextoUI("@", 41.0f, 172.0f, 0.50f, C2D_Color32(255, 255, 255, 255));
            }
            dibujarTextoUI(limitarTextoUtf8(elegido.usuarioPerfil.empty() ? elegido.nombre :
                elegido.usuarioPerfil, 21), 78.0f, 162.0f, 0.55f,
                colorApodoTarjeta(elegido.colorNombre, g_modoOscuro));
            dibujarTextoUI(limitarTextoUtf8(elegido.apodoPerfil.empty() ? "ID " + elegido.id :
                "@" + elegido.apodoPerfil, 28), 79.0f, 186.0f, 0.37f,
                colorApodoTarjeta(elegido.colorApodo, g_modoOscuro));
            C2D_DrawCircleSolid(285.0f, 180.0f, 0.7f, 5.0f,
                elegido.conectado ? C2D_Color32(42, 214, 133, 255) :
                C2D_Color32(135, 149, 166, 255));
            dibujarTextoUI("A ABRIR", 294.0f, 177.0f, 0.37f,
                g_modoOscuro ? C2D_Color32(139, 220, 255, 255) : C2D_Color32(23, 116, 177, 255));
        } else {
            dibujarRectanguloRedondeado(20.0f, 152.0f, 360.0f, 61.0f, 11.0f, colTarjeta);
            dibujarTextoUI("Tu lista esta vacia", 35.0f, 164.0f, 0.53f, colTextoMain);
            dibujarTextoUI("Pulsa X para agregar tu primer contacto", 35.0f, 189.0f, 0.38f, colTextoSub);
        }
        dibujarTextoUI("L Mi perfil    Y Ver perfil    A Abrir chat", 22.0f, 221.0f, 0.35f, colTextoSub);

        // Pantalla Inferior
        dibujarListaContactosRenovada(colFondoAbajo, colTarjeta, colTextoMain, colTextoSub);
    }

    // ==========================================
    // 4. PANTALLA CHAT
    // ==========================================
    else if (estadoActual == PANTALLA_FOTO) {
        C2D_TargetClear(targetArriba, C2D_Color32(8, 10, 14, 255));
        C2D_SceneBegin(targetArriba);
        C2D_DrawImageAt(obtenerFotoVista(), 0.0f, 0.0f, 0.5f);

        C2D_TargetClear(targetAbajo, colFondoAbajo);
        C2D_SceneBegin(targetAbajo);
        u32 botonVolverFoto = C2D_Color32(32, 126, 194, 255);
        dibujarRectanguloRedondeado(25.0f, 60.0f, 270.0f, 56.0f, 12.0f, botonVolverFoto);
        dibujarTextoUI("A / B   VOLVER AL CHAT", 51.0f, 80.0f, 0.48f,
                       C2D_Color32(255, 255, 255, 255));
        dibujarTextoUI("Foto " + std::to_string(g_fotoSeleccionadaId), 94.0f, 132.0f,
                       0.38f, colTextoSub);
    }
    else if (estadoActual == PANTALLA_CAMARA || estadoActual == PANTALLA_CONFIRMAR_FOTO) {
        bool vistaEnVivo = estadoActual == PANTALLA_CAMARA;
        C2D_TargetClear(targetArriba, C2D_Color32(8, 10, 14, 255));
        C2D_SceneBegin(targetArriba);
        if (!vistaEnVivo || hayFotogramaCamara()) {
            C2D_DrawImageAt(obtenerFotoVista(), 0.0f, 0.0f, 0.5f);
        } else {
            dibujarTextoUI("INICIANDO CAMARA...", 102.0f, 105.0f, 0.55f,
                           C2D_Color32(255, 255, 255, 255));
        }

        C2D_TargetClear(targetAbajo, C2D_Color32(22, 27, 38, 255));
        C2D_SceneBegin(targetAbajo);
        u32 textoFoto = C2D_Color32(246, 249, 252, 255);
        u32 textoSecundarioFoto = C2D_Color32(164, 176, 193, 255);
        if (vistaEnVivo) {
            dibujarTextoUI("VISTA PREVIA", 92.0f, 22.0f, 0.64f, textoFoto);
            dibujarTextoUI("Encuadra la foto en la pantalla superior", 33.0f, 54.0f,
                           0.40f, textoSecundarioFoto);
            dibujarRectanguloRedondeado(35.0f, 98.0f, 250.0f, 62.0f, 15.0f,
                hayFotogramaCamara() ? C2D_Color32(91, 188, 132, 255) :
                                      C2D_Color32(78, 91, 107, 255));
            dibujarTextoUI("A / TOMAR FOTO", 88.0f, 117.0f, 0.55f, textoFoto);
            dibujarTextoUI("B / CANCELAR", 101.0f, 190.0f, 0.42f, textoSecundarioFoto);
            if (g_fotoError)
                dibujarTextoUI("No se pudo guardar. Intenta otra vez.", 30.0f, 220.0f,
                               0.34f, C2D_Color32(255, 126, 126, 255));
        } else {
            dibujarTextoUI("REVISA TU FOTO", 75.0f, 22.0f, 0.64f, textoFoto);
            dibujarTextoUI(g_fotoPerfilEnCurso ? "Se guardara como foto de perfil" :
                           "Solo se enviara cuando la confirmes", 35.0f, 54.0f,
                           0.40f, textoSecundarioFoto);
            dibujarRectanguloRedondeado(16.0f, 102.0f, 138.0f, 60.0f, 13.0f,
                                        C2D_Color32(73, 85, 105, 255));
            dibujarRectanguloRedondeado(166.0f, 102.0f, 138.0f, 60.0f, 13.0f,
                                        C2D_Color32(53, 151, 118, 255));
            dibujarTextoUI("X / REPETIR", 36.0f, 122.0f, 0.46f, textoFoto);
            dibujarTextoUI(g_fotoPerfilEnCurso ? "A / GUARDAR" : "A / ENVIAR", 188.0f, 122.0f, 0.46f, textoFoto);
            dibujarTextoUI("B / DESCARTAR", 92.0f, 190.0f, 0.42f, textoSecundarioFoto);
            if (g_fotoError)
                dibujarTextoUI("Error al enviar. Puedes reintentar.", 35.0f, 220.0f,
                               0.34f, C2D_Color32(255, 126, 126, 255));
        }
    }

    // ==========================================
    // 4. PANTALLA CHAT
    // ==========================================
    else if (estadoActual == PANTALLA_CHAT) {
        const Contacto* estiloContacto = contactoActivo();
        int fondoChat = estiloContacto ? estiloContacto->colorFondo : 0;
        u32 fondoConversacion = colorFondoPerfil(fondoChat, g_modoOscuro);
        u32 textoConversacion = fondoChat == 0 ? colTextoMain : C2D_Color32(245, 247, 250, 255);
        u32 textoSecundarioConversacion = fondoChat == 0 ? colTextoSub : C2D_Color32(190, 203, 220, 255);
        C2D_TargetClear(targetArriba, fondoConversacion);
        C2D_SceneBegin(targetArriba);

        u32 colorCabeceraChat = g_modoOscuro ? C2D_Color32(26, 91, 150, 255) : C2D_Color32(0, 128, 210, 255);
        dibujarRectanguloRedondeado(0.0f, 0.0f, 400.0f, 38.0f, 8.0f, colorCabeceraChat);
        if (estiloContacto && estiloContacto->tieneMiniatura && g_slotMiniaturaChat >= 0)
            C2D_DrawImageAt(obtenerMiniaturaContacto(g_slotMiniaturaChat), 8.0f, 3.0f, 0.7f);
        else C2D_DrawCircleSolid(25.0f, 19.0f, 0.5f, 12.0f, C2D_Color32(255, 255, 255, 45));

        C2D_Text txtAvatarChat;
        C2D_TextParse(&txtAvatarChat, bufTexto, "@");
        if (!estiloContacto || !estiloContacto->tieneMiniatura || g_slotMiniaturaChat < 0)
            C2D_DrawText(&txtAvatarChat, C2D_WithColor, 20.0f, 11.0f, 0.4f, 0.45f, 0.45f, C2D_Color32(255, 255, 255, 255));

        std::string tituloChat = limitarTextoUtf8(contactoActivoNombre, 27);
        C2D_Text txtTitChat;
        C2D_TextParse(&txtTitChat, bufTexto, tituloChat.c_str());
        C2D_DrawText(&txtTitChat, C2D_WithColor, 46.0f, 5.0f, 0.5f, 0.58f, 0.58f, C2D_Color32(255, 255, 255, 255));

        std::string subtituloChat = "ID: " + contactoActivoID;
        if (estiloContacto) subtituloChat += estiloContacto->conectado ? "  ·  CONECTADO" : "  ·  DESCONECTADO";
        C2D_Text txtSubtituloChat;
        C2D_TextParse(&txtSubtituloChat, bufTexto, subtituloChat.c_str());
        C2D_DrawText(&txtSubtituloChat, C2D_WithColor, 46.0f, 21.0f, 0.5f, 0.36f, 0.36f, C2D_Color32(225, 239, 250, 255));

        dibujarProgresoChatSuperior(textoConversacion, textoSecundarioConversacion);

        bool mostrarV2 = g_servidorV2 || (!g_v2Determinado && !g_historialV2.empty());
        if (mostrarV2 && !g_historialV2.empty()) {
            dibujarHistorialV2(textoConversacion, textoSecundarioConversacion);
        } else if (historialChat.empty()) {
            dibujarRectanguloRedondeado(56.0f, 84.0f, 288.0f, 64.0f, 12.0f,
                fondoChat == 0 ? colTarjeta : C2D_Color32(62, 69, 83, 255));
            C2D_Text txtSinMsg;
            C2D_TextParse(&txtSinMsg, bufTexto, getText(TXT_SIN_MENSAJES));
            C2D_DrawText(&txtSinMsg, C2D_WithColor, 72.0f, 105.0f, 0.5f, 0.48f, 0.48f, textoSecundarioConversacion);
        } else {
            dibujarHistorialChat(textoConversacion, textoSecundarioConversacion);
        }

        C2D_TargetClear(targetAbajo, colFondoAbajo);
        C2D_SceneBegin(targetAbajo);
        if (!g_confirmarBorrarContacto) {
            dibujarControlesChatRenovados(colTextoMain, colTextoSub);
        } else {
            u32 colorPanel = g_modoOscuro ? C2D_Color32(45, 48, 58, 255) : C2D_Color32(255, 255, 255, 255);
            u32 colorTextoPanel = g_modoOscuro ? C2D_Color32(245, 245, 250, 255) : C2D_Color32(35, 38, 45, 255);
            dibujarRectanguloRedondeado(18.0f, 48.0f, 284.0f, 142.0f, 14.0f, colorPanel);

            C2D_Text txtTituloConfirmar, txtCuerpoConfirmar;
            C2D_TextParse(&txtTituloConfirmar, bufTexto, getText(TXT_CONFIRMAR_BORRAR_TITULO));
            C2D_TextParse(&txtCuerpoConfirmar, bufTexto, getText(TXT_CONFIRMAR_BORRAR_CUERPO));
            C2D_DrawText(&txtTituloConfirmar, C2D_WithColor, 34.0f, 62.0f, 0.5f, 0.62f, 0.62f, colorTextoPanel);
            C2D_DrawText(&txtCuerpoConfirmar, C2D_WithColor, 34.0f, 91.0f, 0.5f, 0.44f, 0.44f, colorTextoPanel);

            u32 colorCancelar = g_modoOscuro ? C2D_Color32(79, 83, 96, 255) : C2D_Color32(218, 222, 229, 255);
            dibujarRectanguloRedondeado(28.0f, 148.0f, 124.0f, 32.0f, 9.0f, colorCancelar);
            dibujarRectanguloRedondeado(168.0f, 148.0f, 124.0f, 32.0f, 9.0f, C2D_Color32(184, 55, 66, 255));

            C2D_Text txtCancelar, txtConfirmar;
            C2D_TextParse(&txtCancelar, bufTexto, "B CANCELAR");
            C2D_TextParse(&txtConfirmar, bufTexto, "A BORRAR");
            C2D_DrawText(&txtCancelar, C2D_WithColor, 57.0f, 157.0f, 0.5f, 0.48f, 0.48f,
                         g_modoOscuro ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(35, 38, 45, 255));
            C2D_DrawText(&txtConfirmar, C2D_WithColor, 207.0f, 157.0f, 0.5f, 0.48f, 0.48f, C2D_Color32(255, 255, 255, 255));
        }

    }
        C3D_FrameEnd(0);
}
