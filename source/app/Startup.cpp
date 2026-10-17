#include "AppState.hpp"

// Máquina de estados de arranque. Comprueba transporte y acceso antes de abrir
// la interfaz normal y concentra la recuperación de host, puerto y clave fijada.
bool procesarInicio(u32 kDown, touchPosition touch, bool& updateYaComprobado) {
        // --- PANTALLA DE CARGA Y VERIFICACIÓN AL INICIO ---
        if (g_estadoInicio != INICIO_COMPLETADO) {
            if ((g_estadoInicio == INICIO_ERROR_CONEXION || g_estadoInicio == INICIO_ERROR_ACCESO) &&
                (kDown & KEY_X)) {
                std::string host = abrirTeclado("Servidor (host o IPv4)");
                if (servidorValido(host)) {
                    g_servidorHost = host;
                    guardarAjustesSD();
                    g_estadoInicio = INICIO_CONECTANDO;
                }
            } else if ((g_estadoInicio == INICIO_ERROR_CONEXION ||
                        g_estadoInicio == INICIO_ERROR_ACCESO) && (kDown & KEY_Y)) {
                std::string entrada = abrirTeclado("Puerto del servidor");
                char* fin = NULL;
                long puerto = strtol(entrada.c_str(), &fin, 10);
                if (fin && *fin == '\0' && puerto > 0 && puerto <= 65535) {
                    g_servidorPuerto = (int)puerto;
                    guardarAjustesSD();
                    g_estadoInicio = INICIO_CONECTANDO;
                }
            } else if (g_estadoInicio == INICIO_ERROR_CONEXION && (kDown & KEY_L)) {
                g_cifrarConexion = !g_cifrarConexion;
                g_claveServidorError = false;
                guardarAjustesSD();
                g_estadoInicio = INICIO_CONECTANDO;
            } else if (g_estadoInicio == INICIO_ERROR_CONEXION && g_cifrarConexion &&
                       (kDown & KEY_R)) {
                olvidarClaveServidor();
                g_claveServidorError = false;
                g_estadoInicio = INICIO_CONECTANDO;
            }
            if (g_estadoInicio == INICIO_CONECTANDO) {
                RespuestaHttp pruebaTls;
                if (pedirHttp("GET", "/version", NULL, "", pruebaTls, 128) &&
                    pruebaTls.estado == 200) {
                    g_estadoInicio = iniciarSesion() ?
                        (g_buscarActualizaciones ? INICIO_BUSCANDO_UPDATE : INICIO_COMPLETADO) :
                        INICIO_ERROR_ACCESO;
                } else {
                    g_estadoInicio = INICIO_ERROR_CONEXION;
                }
            }
            else if (g_estadoInicio == INICIO_BUSCANDO_UPDATE) {
                if (!updateYaComprobado) {
                    updateYaComprobado = true;
                    if (comprobarActualizaciones()) {
                        g_estadoInicio = INICIO_UPDATE_DISPONIBLE;
                    } else {
                        g_estadoInicio = INICIO_COMPLETADO;
                    }
                }
            }
            else if (g_estadoInicio == INICIO_ERROR_CONEXION) {
                if ((kDown & KEY_A) || ((kDown & KEY_TOUCH) && touch.px >= 80 &&
                    touch.px <= 240 && touch.py >= 160 && touch.py <= 200))
                    g_estadoInicio = INICIO_CONECTANDO;
            }
            else if (g_estadoInicio == INICIO_UPDATE_DISPONIBLE) {
                if ((kDown & KEY_A) || (kDown & KEY_START) ||
                    ((kDown & KEY_TOUCH) && touch.px >= 80 && touch.px <= 240 && touch.py >= 160 && touch.py <= 200)) {
                    g_estadoInicio = INICIO_COMPLETADO;
                }
            }
            else if (g_estadoInicio == INICIO_ERROR_ACCESO &&
                     ((kDown & KEY_A) || ((kDown & KEY_TOUCH) && touch.py >= 160 && touch.py <= 200))) {
                g_estadoInicio = INICIO_CONECTANDO;
            }

            // RENDERIZADO DE LA PANTALLA DE CARGA
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            C2D_TextBufClear(bufTexto);

            // Pantalla Superior
            C2D_TargetClear(targetArriba, C2D_Color32(30, 30, 30, 255));
            C2D_SceneBegin(targetArriba);

            C2D_Text txtLogo;
            C2D_TextParse(&txtLogo, bufTexto, "3DSocial");
            C2D_DrawText(&txtLogo, C2D_WithColor, 120.0f, 90.0f, 0.5f, 0.9f, 0.9f, C2D_Color32(255, 255, 255, 255));

            C2D_Text txtVer;
            C2D_TextParse(&txtVer, bufTexto, VERSION_ACTUAL.c_str());
            C2D_DrawText(&txtVer, C2D_WithColor, 10.0f, 220.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(150, 150, 150, 255));

            if (g_estadoInicio == INICIO_UPDATE_DISPONIBLE) {
                C2D_DrawRectangle(20.0f, 130.0f, 0.5f, 360.0f, 80.0f, C2D_Color32(50, 50, 60, 255), C2D_Color32(50, 50, 60, 255), C2D_Color32(50, 50, 60, 255), C2D_Color32(50, 50, 60, 255));

                std::string msgUpd = std::string(getText(TXT_AVISO_UPDATE)) + ultimaVersionDisponible + "\n" + getText(TXT_INFO_UPDATE);
                C2D_Text txtMsg;
                C2D_TextParse(&txtMsg, bufTexto, msgUpd.c_str());
                C2D_DrawText(&txtMsg, C2D_WithColor, 30.0f, 135.0f, 0.5f, 0.42f, 0.42f, C2D_Color32(255, 215, 0, 255));
            }

            // Pantalla Inferior
            C2D_TargetClear(targetAbajo, C2D_Color32(40, 40, 45, 255));
            C2D_SceneBegin(targetAbajo);

            if (g_estadoInicio == INICIO_CONECTANDO) {
                C2D_Text txtEstado;
                C2D_TextParse(&txtEstado, bufTexto, getText(TXT_CONECTANDO));
                C2D_DrawText(&txtEstado, C2D_WithColor, 50.0f, 70.0f, 0.5f, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));
                dibujarAnimacionCarga(160.0f, 130.0f, g_anguloCarga);
            }
            else if (g_estadoInicio == INICIO_BUSCANDO_UPDATE) {
                C2D_Text txtEstado;
                C2D_TextParse(&txtEstado, bufTexto, getText(TXT_BUSCANDO_UPDATE));
                C2D_DrawText(&txtEstado, C2D_WithColor, 50.0f, 70.0f, 0.5f, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));
                dibujarAnimacionCarga(160.0f, 130.0f, g_anguloCarga);
            }
            else if (g_estadoInicio == INICIO_ERROR_CONEXION) {
                C2D_Text txtEstado;
                C2D_TextParse(&txtEstado, bufTexto, getText(TXT_ERROR_CONEXION));
                C2D_DrawText(&txtEstado, C2D_WithColor, 20.0f, 50.0f, 0.5f, 0.50f, 0.50f, C2D_Color32(255, 100, 100, 255));
                if (g_claveServidorError) {
                    C2D_Text avisoTls;
                    C2D_TextParse(&avisoTls, bufTexto, "Clave del servidor diferente o no guardada");
                    C2D_DrawText(&avisoTls, C2D_WithColor, 22.0f, 112.0f, 0.5f, 0.36f, 0.36f,
                                 C2D_Color32(255, 215, 120, 255));
                }
                C2D_Text modoConexion;
                C2D_TextParse(&modoConexion, bufTexto, g_cifrarConexion ?
                    "Cifrado de datos: SI (HTTP)" : "Cifrado de datos: NO (HTTP)");
                C2D_DrawText(&modoConexion, C2D_WithColor, 22.0f, 135.0f, 0.5f, 0.35f, 0.35f,
                             C2D_Color32(255, 215, 120, 255));

                C2D_DrawRectangle(80.0f, 160.0f, 0.5f, 160.0f, 40.0f, C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255));
                C2D_Text txtBtn;
                C2D_TextParse(&txtBtn, bufTexto, "REINTENTAR");
                C2D_DrawText(&txtBtn, C2D_WithColor, 120.0f, 170.0f, 0.5f, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));
                C2D_Text opciones;
                C2D_TextParse(&opciones, bufTexto, "X servidor | Y puerto | L cifrado | R nueva clave");
                C2D_DrawText(&opciones, C2D_WithColor, 12.0f, 215.0f, 0.5f, 0.29f, 0.29f,
                             C2D_Color32(220, 220, 220, 255));
            }
            else if (g_estadoInicio == INICIO_ERROR_ACCESO) {
                C2D_Text aviso;
                C2D_TextParse(&aviso, bufTexto, "Acceso denegado o sin conexion.\nA: reintentar contrasena");
                C2D_DrawText(&aviso, C2D_WithColor, 18.0f, 65.0f, 0.5f, 0.47f, 0.47f,
                             C2D_Color32(255, 130, 130, 255));
                C2D_DrawRectangle(80.0f, 160.0f, 0.5f, 160.0f, 40.0f,
                                  C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255),
                                  C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255));
                C2D_Text reintentar;
                C2D_TextParse(&reintentar, bufTexto, "A REINTENTAR");
                C2D_DrawText(&reintentar, C2D_WithColor, 109.0f, 172.0f, 0.5f, 0.47f, 0.47f,
                             C2D_Color32(255, 255, 255, 255));
                C2D_Text opciones;
                C2D_TextParse(&opciones, bufTexto, "X servidor  |  Y puerto");
                C2D_DrawText(&opciones, C2D_WithColor, 67.0f, 215.0f, 0.5f, 0.35f, 0.35f,
                             C2D_Color32(220, 220, 220, 255));
            }
            else if (g_estadoInicio == INICIO_UPDATE_DISPONIBLE) {
                C2D_DrawRectangle(80.0f, 160.0f, 0.5f, 160.0f, 40.0f, C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255));
                C2D_Text txtBtn;
                C2D_TextParse(&txtBtn, bufTexto, getText(TXT_BOTON_CONTINUAR));
                C2D_DrawText(&txtBtn, C2D_WithColor, 120.0f, 170.0f, 0.5f, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));
            }

            C3D_FrameEnd(0);
            return true;
        }
    return false;
}
