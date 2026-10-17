#include "AppState.hpp"

bool procesarInicio(u32 kDown, touchPosition touch, bool& updateYaComprobado) {
        // --- PANTALLA DE CARGA Y VERIFICACIÓN AL INICIO ---
        if (g_estadoInicio != INICIO_COMPLETADO) {
            if (g_estadoInicio == INICIO_CONECTANDO) {
                int testSock = conectarAlServidor();
                if (testSock >= 0) {
                    close(testSock);
                    g_estadoInicio = g_buscarActualizaciones ? INICIO_BUSCANDO_UPDATE : INICIO_COMPLETADO;
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
            else if (g_estadoInicio == INICIO_ERROR_CONEXION || g_estadoInicio == INICIO_UPDATE_DISPONIBLE) {
                if ((kDown & KEY_A) || (kDown & KEY_START) ||
                    ((kDown & KEY_TOUCH) && touch.px >= 80 && touch.px <= 240 && touch.py >= 160 && touch.py <= 200)) {
                    g_estadoInicio = INICIO_COMPLETADO;
                }
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

                C2D_DrawRectangle(80.0f, 160.0f, 0.5f, 160.0f, 40.0f, C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255), C2D_Color32(0, 150, 220, 255));
                C2D_Text txtBtn;
                C2D_TextParse(&txtBtn, bufTexto, getText(TXT_BOTON_CONTINUAR));
                C2D_DrawText(&txtBtn, C2D_WithColor, 120.0f, 170.0f, 0.5f, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));
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
