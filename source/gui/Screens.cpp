#include "AppState.hpp"

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
    u32 colTextoTarjeta= g_modoOscuro ? C2D_Color32(240, 240, 240, 255): C2D_Color32(30, 30, 30, 255);

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
                125.0f, 112.0f, 0.61f, colTextoMain);
            dibujarTextoUI(apodoTarjeta.empty() ? (g_perfilPropio ? "" : "Sin apodo configurado") :
                "@" + limitarTextoUtf8(apodoTarjeta, 24), 125.0f, 137.0f, 0.39f, colTextoSub);
            dibujarTextoUI("ID " + (g_perfilPropio ? miID :
                (contactoPerfil ? contactoPerfil->id : "")), 125.0f, 157.0f, 0.38f, colTextoSub);
        }
        C2D_TargetClear(targetAbajo, colFondoAbajo);
        C2D_SceneBegin(targetAbajo);
        if (g_perfilPropio) {
            const char* etiquetas[] = {"USUARIO", "APODO", "BIOGRAFIA", "DESCRIPCION", "FOTO DE PERFIL", "GUARDAR EN SERVIDOR"};
            const std::string valores[] = {g_perfilNombre, g_perfilApodo, g_perfilBio, g_perfilDescripcion, "A / TOMAR FOTO", "A / SINCRONIZAR"};
            for (int i = 0; i < 6; i++) {
                float y = 12.0f + i * 36.0f;
                if (focoPerfil == i)
                    dibujarRectanguloRedondeado(8.0f, y - 2.0f, 304.0f, 36.0f, 9.0f,
                        C2D_Color32(32, 129, 222, 255));
                dibujarRectanguloRedondeado(10.0f, y, 300.0f, 32.0f, 8.0f, colTarjeta);
                dibujarTextoUI(etiquetas[i], 20.0f, y + 3.0f, 0.31f, colTextoSub);
                dibujarTextoUI(limitarTextoUtf8(valores[i].empty() ? "Toca para editar" : valores[i], 36),
                    20.0f, y + 16.0f, 0.40f, colTextoMain);
            }
            if (g_perfilError)
                dibujarTextoUI("Sin conexion. Guardado en SD; A para reintentar.", 12.0f, 231.0f,
                    0.30f, C2D_Color32(210, 70, 75, 255));
            else dibujarTextoUI("CRUCETA  |  A editar  |  X borrar  |  B volver", 20.0f, 231.0f,
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

    // ==========================================
    // 3. PANTALLA CONTACTOS
    // ==========================================
    else if (estadoActual == PANTALLA_CONTACTOS) {
        // Pantalla Superior
        C2D_TargetClear(targetArriba, colFondoArriba);
        C2D_SceneBegin(targetArriba);

        dibujarBarraEstadoSuperior();

        C2D_Text txtPart1;
        C2D_TextParse(&txtPart1, bufTexto, getText(TXT_CONTACTOS));
        C2D_DrawText(&txtPart1, C2D_WithColor, 24.0f, 39.0f, 0.5f, 0.85f, 0.85f, colTextoMain);
        dibujarTextoUI(std::to_string(listaContactos.size()) +
            (listaContactos.size() == 1 ? " contacto" : " contactos"),
            27.0f, 65.0f, 0.42f, colTextoSub);

        std::string strID = std::string(getText(TXT_TU_ID)) + miID;
        u32 colorTarjetaID = g_modoOscuro ? C2D_Color32(42, 50, 63, 255) : C2D_Color32(236, 246, 253, 255);
        dibujarRectanguloRedondeado(18.0f, 76.0f, 364.0f, 54.0f, 12.0f, colorTarjetaID);
        C2D_Text txtMiID;
        C2D_TextParse(&txtMiID, bufTexto, strID.c_str());
        C2D_DrawText(&txtMiID, C2D_WithColor, 32.0f, 94.0f, 0.5f, 0.62f, 0.62f,
                     g_modoOscuro ? C2D_Color32(95, 190, 255, 255) : C2D_Color32(0, 125, 205, 255));
        dibujarTextoUI("- AJUSTES   L TU PERFIL   Y CONTACTO", 32.0f, 114.0f,
            0.32f, colTextoSub);

        if (!listaContactos.empty()) {
            const Contacto& elegido = listaContactos[contactoSeleccionado];
            dibujarRectanguloRedondeado(18.0f, 144.0f, 364.0f, 63.0f, 12.0f,
                g_modoOscuro ? C2D_Color32(35, 83, 110, 255) : C2D_Color32(220, 241, 250, 255));
            dibujarTextoUI("SELECCIONADO", 31.0f, 152.0f, 0.34f, colTextoSub);
            dibujarTextoUI(limitarTextoUtf8(elegido.nombre, 29), 31.0f, 169.0f,
                0.59f, colTextoMain);
            std::string apodoSeleccionado = elegido.apodoPerfil.empty() ?
                ("ID: " + elegido.id) : ("@" + elegido.apodoPerfil);
            dibujarTextoUI(limitarTextoUtf8(apodoSeleccionado, 26), 31.0f, 188.0f,
                0.37f, colTextoSub);
            dibujarTextoUI("A CHAT  Y PERFIL", 281.0f, 174.0f, 0.35f,
                g_modoOscuro ? C2D_Color32(139, 220, 255, 255) : C2D_Color32(23, 116, 177, 255));
        } else {
            dibujarTextoUI("Pulsa X para anadir un contacto", 28.0f, 160.0f,
                0.47f, colTextoSub);
        }

        C2D_Text txtVerMain;
        C2D_TextParse(&txtVerMain, bufTexto, VERSION_ACTUAL.c_str());
        C2D_DrawText(&txtVerMain, C2D_WithColor, 10.0f, 220.0f, 0.5f, 0.45f, 0.45f, colTextoSub);

        // Pantalla Inferior
        C2D_TargetClear(targetAbajo, colFondoAbajo);
        C2D_SceneBegin(targetAbajo);

        C2D_Text txtHeader;
        C2D_TextParse(&txtHeader, bufTexto, getText(TXT_LISTA_CONTACTOS));
        dibujarRectanguloRedondeado(10.0f, 5.0f, 300.0f, 24.0f, 7.0f, colTarjeta);
        C2D_DrawText(&txtHeader, C2D_WithColor, 19.0f, 9.0f, 0.5f, 0.55f, 0.55f, colTextoMain);
        dibujarTextoUI("CRUCETA + A", 219.0f, 12.0f, 0.32f, colTextoSub);

        if (listaContactos.empty()) {
            C2D_Text txtVacio;
            C2D_TextParse(&txtVacio, bufTexto, getText(TXT_SIN_CONTACTOS));
            dibujarRectanguloRedondeado(20.0f, 68.0f, 280.0f, 58.0f, 12.0f, colTarjeta);
            C2D_DrawCircleSolid(48.0f, 97.0f, 0.5f, 13.0f, C2D_Color32(0, 150, 220, 255));
            C2D_DrawText(&txtVacio, C2D_WithColor, 70.0f, 88.0f, 0.5f, 0.55f, 0.55f, colTextoTarjeta);
        } else {
            float y = 35.0f;
            for (size_t i = scrollContactos; i < listaContactos.size() && i < (size_t)(scrollContactos + 3); i++) {
                bool seleccionado = (int)i == contactoSeleccionado;
                u32 acento = seleccionado ? C2D_Color32(23, 142, 211, 255) :
                    C2D_Color32(91, 158, 191, 255);
                if (seleccionado)
                    dibujarRectanguloRedondeado(8.0f, y - 2.0f, 264.0f, 46.0f,
                        11.0f, acento);
                dibujarRectanguloRedondeado(10.0f, y, 260.0f, 42.0f, 9.0f, colTarjeta);
                dibujarRectanguloRedondeado(10.0f, y + 9.0f, 4.0f, 24.0f, 2.0f, acento);
                C2D_DrawCircleSolid(34.0f, y + 21.0f, 0.5f, 13.0f, acento);

                std::string strNom = limitarTextoUtf8(listaContactos[i].nombre, 22);
                std::string strIDSub = listaContactos[i].apodoPerfil.empty() ?
                    "ID: " + listaContactos[i].id : "@" + listaContactos[i].apodoPerfil;

                C2D_Text txtAvatar, txtNombre, txtIDSub;
                C2D_TextParse(&txtAvatar, bufTexto, "@");
                C2D_TextParse(&txtNombre, bufTexto, strNom.c_str());
                C2D_TextParse(&txtIDSub, bufTexto, strIDSub.c_str());

                if (listaContactos[i].tieneMiniatura)
                    C2D_DrawImageAt(obtenerMiniaturaContacto((int)i - scrollContactos),
                        18.0f, y + 5.0f, 0.4f);
                else
                    C2D_DrawText(&txtAvatar, C2D_WithColor, 29.0f, y + 11.0f, 0.4f, 0.45f, 0.45f, C2D_Color32(255, 255, 255, 255));
                C2D_DrawText(&txtNombre, C2D_WithColor, 55.0f, y + 4.0f, 0.6f, 0.55f, 0.55f, colTextoTarjeta);
                C2D_DrawText(&txtIDSub, C2D_WithColor, 55.0f, y + 23.0f, 0.6f, 0.40f, 0.40f, colTextoSub);

                if (listaContactos[i].tieneNovedad) {
                    C2D_DrawCircleSolid(245.0f, y + 21.0f, 0.7f, 7.0f, C2D_Color32(50, 205, 50, 255));
                }
                y += 48.0f;
            }

                if (listaContactos.size() > 3) {
                u32 colBtnScroll = g_modoOscuro ? C2D_Color32(60, 60, 75, 255) : C2D_Color32(180, 180, 190, 255);
                dibujarRectanguloRedondeado(280.0f, 35.0f, 30.0f, 65.0f, 8.0f, colBtnScroll);
                C2D_Text txtSubir;
                C2D_TextParse(&txtSubir, bufTexto, "^");
                C2D_DrawText(&txtSubir, C2D_WithColor, 289.0f, 55.0f, 0.5f, 0.8f, 0.8f, C2D_Color32(255, 255, 255, 255));

                dibujarRectanguloRedondeado(280.0f, 115.0f, 30.0f, 65.0f, 8.0f, colBtnScroll);
                C2D_Text txtBajar;
                C2D_TextParse(&txtBajar, bufTexto, "v");
                C2D_DrawText(&txtBajar, C2D_WithColor, 289.0f, 135.0f, 0.5f, 0.8f, 0.8f, C2D_Color32(255, 255, 255, 255));
            }
        }

        // Acciones accesibles tambien con SELECT y X.
        u32 colorGris = g_modoOscuro ? C2D_Color32(60, 60, 75, 255) : C2D_Color32(120, 120, 130, 255);
        dibujarRectanguloRedondeado(10.0f, 190.0f, 60.0f, 40.0f, 10.0f, colorGris);
        C2D_Text txtVolverMenu;
        C2D_TextParse(&txtVolverMenu, bufTexto, "- AJ.");
        C2D_DrawText(&txtVolverMenu, C2D_WithColor, 18.0f, 200.0f, 0.5f, 0.45f, 0.45f, C2D_Color32(255, 255, 255, 255));

        // Botón Añadir Contacto
        u32 colorAzul = C2D_Color32(0, 150, 220, 255);
        dibujarRectanguloRedondeado(80.0f, 190.0f, 145.0f, 40.0f, 10.0f, colorAzul);
        dibujarRectanguloRedondeado(235.0f, 190.0f, 75.0f, 40.0f, 10.0f,
            C2D_Color32(29, 177, 145, 255));
        C2D_Text txtBtnAdd;
        C2D_TextParse(&txtBtnAdd, bufTexto, "X  ANADIR");
        C2D_DrawText(&txtBtnAdd, C2D_WithColor, 103.0f, 201.0f, 0.5f, 0.45f, 0.45f, C2D_Color32(255, 255, 255, 255));
        dibujarTextoUI("Y PERFIL", 245.0f, 203.0f, 0.35f, C2D_Color32(255, 255, 255, 255));
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
        C2D_TargetClear(targetArriba, colFondoArriba);
        C2D_SceneBegin(targetArriba);

        u32 colorCabeceraChat = g_modoOscuro ? C2D_Color32(26, 91, 150, 255) : C2D_Color32(0, 128, 210, 255);
        dibujarRectanguloRedondeado(0.0f, 0.0f, 400.0f, 38.0f, 8.0f, colorCabeceraChat);
        C2D_DrawCircleSolid(25.0f, 19.0f, 0.5f, 12.0f, C2D_Color32(255, 255, 255, 45));

        C2D_Text txtAvatarChat;
        C2D_TextParse(&txtAvatarChat, bufTexto, "@");
        C2D_DrawText(&txtAvatarChat, C2D_WithColor, 20.0f, 11.0f, 0.4f, 0.45f, 0.45f, C2D_Color32(255, 255, 255, 255));

        std::string tituloChat = limitarTextoUtf8(contactoActivoNombre, 27);
        C2D_Text txtTitChat;
        C2D_TextParse(&txtTitChat, bufTexto, tituloChat.c_str());
        C2D_DrawText(&txtTitChat, C2D_WithColor, 46.0f, 5.0f, 0.5f, 0.58f, 0.58f, C2D_Color32(255, 255, 255, 255));

        std::string subtituloChat = "ID: " + contactoActivoID;
        C2D_Text txtSubtituloChat;
        C2D_TextParse(&txtSubtituloChat, bufTexto, subtituloChat.c_str());
        C2D_DrawText(&txtSubtituloChat, C2D_WithColor, 46.0f, 21.0f, 0.5f, 0.36f, 0.36f, C2D_Color32(225, 239, 250, 255));

        bool mostrarV2 = g_servidorV2 || (!g_v2Determinado && !g_historialV2.empty());
        if (mostrarV2 && !g_historialV2.empty()) {
            dibujarHistorialV2(colTextoMain, colTextoSub);
        } else if (historialChat.empty()) {
            dibujarRectanguloRedondeado(56.0f, 84.0f, 288.0f, 64.0f, 12.0f, colTarjeta);
            C2D_Text txtSinMsg;
            C2D_TextParse(&txtSinMsg, bufTexto, getText(TXT_SIN_MENSAJES));
            C2D_DrawText(&txtSinMsg, C2D_WithColor, 72.0f, 105.0f, 0.5f, 0.48f, 0.48f, colTextoSub);
        } else {
            dibujarHistorialChat(colTextoMain, colTextoSub);
        }

        C2D_TargetClear(targetAbajo, colFondoAbajo);
        C2D_SceneBegin(targetAbajo);

        if (!g_confirmarBorrarContacto) {
            u32 azulChat = C2D_Color32(29, 132, 213, 255);
            u32 grisChat = g_modoOscuro ? C2D_Color32(59, 68, 83, 255) : C2D_Color32(101, 112, 128, 255);
            u32 blancoChat = C2D_Color32(255, 255, 255, 255);
            dibujarRectanguloRedondeado(20.0f, 26.0f, 161.0f, 48.0f, 11.0f, azulChat);
            dibujarTextoUI("A  ESCRIBIR", 39.0f, 41.0f, 0.49f, blancoChat);
            dibujarRectanguloRedondeado(187.0f, 26.0f, 57.0f, 48.0f, 11.0f,
                notaVozActiva() ? C2D_Color32(208, 66, 78, 255) :
                g_servidorV2 ? C2D_Color32(29, 172, 150, 255) : grisChat);
            dibujarRectanguloRedondeado(249.0f, 26.0f, 51.0f, 48.0f, 11.0f,
                g_fotoPendiente ? C2D_Color32(220, 142, 43, 255) :
                g_servidorV2 ? C2D_Color32(88, 115, 213, 255) : grisChat);
            if (notaVozActiva()) {
                dibujarTextoUI(std::to_string((duracionNotaVozMs() + 999) / 1000) + "s", 201.0f, 40.0f, 0.45f, blancoChat);
                dibujarTextoUI(getText(TXT_CANCELAR_VOZ), 188.0f, 78.0f, 0.30f, colTextoSub);
            } else {
                dibujarTextoUI(g_audioPendiente ? "Y ENV." : "Y MIC", 191.0f, 41.0f, 0.33f, blancoChat);
                if (g_audioPendiente) dibujarTextoUI(getText(TXT_DESCARTAR_VOZ), 20.0f, 78.0f, 0.31f, colTextoSub);
                else if (g_servidorV2) dibujarTextoUI(getText(TXT_GRABAR_VOZ), 188.0f, 78.0f, 0.30f, colTextoSub);
            }
            dibujarTextoUI(g_fotoPendiente ? "X VER" : "X FOTO", 252.0f, 41.0f, 0.30f, blancoChat);
            C2D_DrawRectSolid(263.0f, 52.0f, 0.5f, 22.0f, 12.0f, blancoChat);
            C2D_DrawRectSolid(268.0f, 49.0f, 0.5f, 12.0f, 3.0f, blancoChat);
            C2D_DrawRectSolid(271.0f, 53.0f, 0.5f, 8.0f, 8.0f, blancoChat);

            dibujarRectanguloRedondeado(20.0f, 94.0f, 80.0f, 38.0f, 9.0f, grisChat);
            dibujarRectanguloRedondeado(106.0f, 94.0f, 96.0f, 38.0f, 9.0f, grisChat);
            dibujarRectanguloRedondeado(208.0f, 94.0f, 92.0f, 38.0f, 9.0f, grisChat);
            dibujarTextoUI("<", 51.0f, 102.0f, 0.62f, blancoChat);
            dibujarTextoUI("L ANTERIORES", 111.0f, 105.0f, 0.34f, blancoChat);
            dibujarTextoUI("R RECIENTES", 213.0f, 105.0f, 0.34f, blancoChat);

            dibujarRectanguloRedondeado(20.0f, 145.0f, 80.0f, 35.0f, 8.0f, grisChat);
            dibujarRectanguloRedondeado(106.0f, 145.0f, 108.0f, 35.0f, 8.0f,
                g_audioSeleccionadoId > 0 ? azulChat : grisChat);
            dibujarRectanguloRedondeado(220.0f, 145.0f, 80.0f, 35.0f, 8.0f, grisChat);
            dibujarTextoUI("< Audio", 27.0f, 155.0f, 0.39f, blancoChat);
            dibujarTextoUI(g_audioSeleccionadoId > 0 ? getText(TXT_REPRODUCIR) : "Audio",
                118.0f, 155.0f, 0.40f, blancoChat);
            dibujarTextoUI("Audio >", 231.0f, 155.0f, 0.39f, blancoChat);
            dibujarRectanguloRedondeado(20.0f, 184.0f, 80.0f, 33.0f, 8.0f, grisChat);
            dibujarRectanguloRedondeado(106.0f, 184.0f, 108.0f, 33.0f, 8.0f,
                g_fotoSeleccionadaId > 0 ? C2D_Color32(88, 115, 213, 255) : grisChat);
            dibujarRectanguloRedondeado(220.0f, 184.0f, 80.0f, 33.0f, 8.0f, grisChat);
            dibujarTextoUI("< FOTO", 30.0f, 193.0f, 0.39f, blancoChat);
            dibujarTextoUI(g_fotoSeleccionadaId > 0 ? "VER GRANDE" : "FOTO",
                           116.0f, 193.0f, 0.37f, blancoChat);
            dibujarTextoUI("FOTO >", 228.0f, 193.0f, 0.39f, blancoChat);

            dibujarRectanguloRedondeado(266.0f, 220.0f, 36.0f, 17.0f, 4.0f,
                                        C2D_Color32(177, 61, 71, 255));
            C2D_DrawRectSolid(278.0f, 224.0f, 0.5f, 12.0f, 2.0f, blancoChat);
            C2D_DrawRectSolid(281.0f, 222.0f, 6.0f, 2.0f, 6.0f, blancoChat);
            C2D_DrawRectSolid(280.0f, 227.0f, 8.0f, 8.0f, 10.0f, blancoChat);

            float fx = (float)CHAT_FOCO_X[focoChat] - 2.0f;
            float fy = (float)CHAT_FOCO_Y[focoChat] - 2.0f;
            float fw = (float)CHAT_FOCO_ANCHO[focoChat] + 4.0f;
            float fh = (float)CHAT_FOCO_ALTO[focoChat] + 4.0f;
            u32 focoColor = C2D_Color32(255, 211, 75, 255);
            C2D_DrawRectSolid(fx, fy, 0.9f, fw, 2.0f, focoColor);
            C2D_DrawRectSolid(fx, fy + fh - 2.0f, 0.9f, fw, 2.0f, focoColor);
            C2D_DrawRectSolid(fx, fy, 0.9f, 2.0f, fh, focoColor);
            C2D_DrawRectSolid(fx + fw - 2.0f, fy, 0.9f, 2.0f, fh, focoColor);

            if (g_estadoEnvio != ENVIO_NINGUNO && osGetTime() - g_tiempoNotificacion < 3000)
                dibujarTextoUI(getText(g_estadoEnvio == ENVIO_EXITO ? TXT_ENVIADO : TXT_ERROR),
                    20.0f, 216.0f, 0.36f, g_estadoEnvio == ENVIO_EXITO ?
                    C2D_Color32(0, 175, 105, 255) : C2D_Color32(220, 50, 50, 255));
            else if (g_audioError || g_fotoError) {
                char errorCamara[48];
                snprintf(errorCamara, sizeof(errorCamara), "CAMARA / FOTO ERROR %08lX",
                         (unsigned long)obtenerCodigoErrorCamara());
                dibujarTextoUI(g_fotoError ? errorCamara : getText(TXT_AUDIO_NO_DISPONIBLE),
                    20.0f, 216.0f, 0.36f, C2D_Color32(220, 50, 50, 255));
            }
            else dibujarTextoUI("CRUCETA+A  |  B VOLVER  |  - AJUSTES",
                20.0f, 220.0f, 0.29f, colTextoSub);
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
