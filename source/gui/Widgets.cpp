#include "AppState.hpp"

// Elementos visuales reutilizados por pantallas y perfiles. Los índices de
// estilo son valores de protocolo (0–5), no colores RGB guardados en la SD.
const char* nombreColorPerfil(int indice) {
    static const char* nombres[] = {"Predeterminado", "Verde", "Violeta", "Coral", "Dorado", "Grafito"};
    return nombres[std::max(0, std::min(5, indice))];
}

u32 colorBurbujaPerfil(int indice) {
    static const u32 colores[] = {
        C2D_Color32(25, 125, 215, 255), C2D_Color32(24, 125, 93, 255),
        C2D_Color32(111, 77, 174, 255), C2D_Color32(187, 81, 78, 255),
        C2D_Color32(144, 105, 19, 255), C2D_Color32(69, 82, 104, 255)
    };
    return colores[std::max(0, std::min(5, indice))];
}

u32 colorFondoPerfil(int indice, bool oscuro) {
    static const u32 colores[] = {
        C2D_Color32(0, 0, 0, 255), C2D_Color32(15, 49, 42, 255),
        C2D_Color32(40, 29, 66, 255), C2D_Color32(65, 30, 38, 255),
        C2D_Color32(58, 45, 22, 255), C2D_Color32(32, 39, 49, 255)
    };
    if (indice <= 0 || indice > 5)
        return oscuro ? C2D_Color32(20, 20, 25, 255) : C2D_Color32(255, 255, 255, 255);
    return colores[indice];
}

u32 colorTextoPerfil(int indice) {
    static const u32 colores[] = {
        C2D_Color32(255, 255, 255, 255), C2D_Color32(210, 255, 226, 255),
        C2D_Color32(229, 217, 255, 255), C2D_Color32(255, 220, 211, 255),
        C2D_Color32(255, 239, 173, 255), C2D_Color32(220, 230, 244, 255)
    };
    return colores[std::max(0, std::min(5, indice))];
}

u32 colorApodoPerfil(int indice) {
    static const u32 colores[] = {
        C2D_Color32(220, 239, 255, 255), C2D_Color32(146, 244, 186, 255),
        C2D_Color32(209, 178, 255, 255), C2D_Color32(255, 169, 151, 255),
        C2D_Color32(255, 223, 121, 255), C2D_Color32(185, 206, 234, 255)
    };
    return colores[std::max(0, std::min(5, indice))];
}

u32 colorApodoTarjeta(int indice, bool oscuro) {
    static const u32 claros[] = {
        C2D_Color32(80, 90, 108, 255), C2D_Color32(25, 110, 68, 255),
        C2D_Color32(93, 61, 153, 255), C2D_Color32(169, 70, 68, 255),
        C2D_Color32(137, 98, 20, 255), C2D_Color32(58, 75, 95, 255)
    };
    return oscuro ? colorApodoPerfil(indice) : claros[std::max(0, std::min(5, indice))];
}

void dibujarIconoEngranaje(float x, float y, u32 color) {
    C2D_DrawRectSolid(x + 9.0f, y, 0.5f, 6.0f, 24.0f, color);
    C2D_DrawRectSolid(x, y + 9.0f, 0.5f, 24.0f, 6.0f, color);
    C2D_DrawCircleSolid(x + 12.0f, y + 12.0f, 0.6f, 10.0f, color);
    C2D_DrawCircleSolid(x + 12.0f, y + 12.0f, 0.7f, 4.0f,
        g_modoOscuro ? C2D_Color32(26, 91, 150, 255) : C2D_Color32(0, 128, 210, 255));
}

std::string abrirTeclado(const char* hint) {
    // Abre el teclado del sistema; devuelve texto aceptado o una cadena vacía al cancelar.
    C3D_FrameEnd(0);
    gspWaitForVBlank();

    SwkbdState swkbd;
    char buffer[128] = {0};

    swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, 120);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancelar", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Aceptar", true);

    if (hint != nullptr) {
        swkbdSetHintText(&swkbd, hint);
    }

    SwkbdButton button = swkbdInputText(&swkbd, buffer, sizeof(buffer));
    hidScanInput();

    if (button == SWKBD_BUTTON_RIGHT || button == SWKBD_BUTTON_CONFIRM) {
        return std::string(buffer);
    }
    return "";
}

std::string abrirTecladoContrasena(const char* hint) {
    C3D_FrameEnd(0);
    gspWaitForVBlank();
    SwkbdState swkbd;
    char buffer[129] = {0};
    swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, 120);
    swkbdSetPasswordMode(&swkbd, SWKBD_PASSWORD_HIDE);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancelar", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Aceptar", true);
    swkbdSetHintText(&swkbd, hint);
    SwkbdButton button = swkbdInputText(&swkbd, buffer, sizeof(buffer));
    hidScanInput();
    std::string result = (button == SWKBD_BUTTON_RIGHT || button == SWKBD_BUTTON_CONFIRM) ? buffer : "";
    memset(buffer, 0, sizeof(buffer));
    return result;
}

void dibujarBarraEstadoSuperior() {
    // Renderiza Wi-Fi, hora y batería en la pantalla superior de la consola.
    u32 barraInicio = g_modoOscuro ? C2D_Color32(27, 57, 83, 255) : C2D_Color32(25, 118, 171, 255);
    u32 barraFin = g_modoOscuro ? C2D_Color32(22, 43, 65, 255) : C2D_Color32(20, 87, 143, 255);
    C2D_DrawRectangle(0.0f, 0.0f, 0.5f, 400.0f, 25.0f,
                      barraInicio, barraFin, barraInicio, barraFin);

    u8 wifiSignal = osGetWifiStrength();
    float xWifi = 12.0f;
    for (int i = 0; i < 3; i++) {
        float altoBarra = 6.0f + (i * 4.0f);
        float yBarra = 18.0f - altoBarra;
        u32 colorBarra = (i < wifiSignal) ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(160, 160, 160, 255);
        C2D_DrawRectangle(xWifi + (i * 6.0f), yBarra, 0.5f, 4.0f, altoBarra, colorBarra, colorBarra, colorBarra, colorBarra);
    }

    time_t rawtime = time(NULL);
    struct tm* timeinfo = localtime(&rawtime);
    char bufferHora[10];
    strftime(bufferHora, sizeof(bufferHora), "%H:%M", timeinfo);

    C2D_Text txtHora;
    C2D_TextParse(&txtHora, bufTexto, bufferHora);
    C2D_DrawText(&txtHora, C2D_WithColor, 180.0f, 3.0f, 0.5f, 0.55f, 0.55f, C2D_Color32(255, 255, 255, 255));

    u8 batLevel = 5;
    PTMU_GetBatteryLevel(&batLevel);
    int pctBateria = batLevel * 20;

    char bufferBat[16];
    snprintf(bufferBat, sizeof(bufferBat), "Bat: %d%%", pctBateria);

    C2D_Text txtBat;
    C2D_TextParse(&txtBat, bufTexto, bufferBat);
    C2D_DrawText(&txtBat, C2D_WithColor, 310.0f, 3.0f, 0.5f, 0.55f, 0.55f, C2D_Color32(255, 255, 255, 255));
}

void dibujarTextoUI(const std::string& texto, float x, float y, float escala, u32 color) {
    // Envuelve el parseo de texto de Citro2D usado por las pantallas de la app.
    C2D_Text textoDibujado;
    C2D_TextParse(&textoDibujado, bufTexto, texto.c_str());
    C2D_DrawText(&textoDibujado, C2D_WithColor, x, y, 0.5f, escala, escala, color);
}

void dibujarOpcionAjustes(int fila, const char* titulo, bool activo, u32 colorTarjeta, u32 colorTexto) {
    // Pinta una fila con foco y estado visual de interruptor.
    float y = 44.0f + fila * 36.0f;
    if (focoConfiguracion == fila)
        dibujarRectanguloRedondeado(8.0f, y - 2.0f, 304.0f, 36.0f, 10.0f,
            C2D_Color32(31, 154, 220, 255));
    dibujarRectanguloRedondeado(10.0f, y, 300.0f, 32.0f, 8.0f, colorTarjeta);
    dibujarTextoUI(titulo, 20.0f, y + 8.0f, 0.46f, colorTexto);
    dibujarRectanguloRedondeado(260.0f, y + 7.0f, 39.0f, 18.0f, 9.0f,
        activo ? C2D_Color32(29, 177, 145, 255) : C2D_Color32(133, 142, 155, 255));
    C2D_DrawCircleSolid(activo ? 288.0f : 271.0f, y + 16.0f, 0.5f, 7.0f, C2D_Color32(255, 255, 255, 255));
}

void dibujarPantallaConfiguracion() {
    // Renderiza General y Conexión; el perfil se edita desde L en Contactos.
    u32 fondo = g_modoOscuro ? C2D_Color32(21, 27, 41, 255) : C2D_Color32(237, 244, 250, 255);
    u32 tarjeta = g_modoOscuro ? C2D_Color32(39, 49, 68, 255) : C2D_Color32(255, 255, 255, 255);
    u32 texto = g_modoOscuro ? C2D_Color32(245, 248, 255, 255) : C2D_Color32(35, 48, 68, 255);
    u32 secundario = g_modoOscuro ? C2D_Color32(173, 191, 213, 255) : C2D_Color32(89, 109, 134, 255);
    u32 azul = C2D_Color32(32, 129, 222, 255);
    u32 blanco = C2D_Color32(255, 255, 255, 255);

    C2D_TargetClear(targetArriba, fondo);
    C2D_SceneBegin(targetArriba);
    dibujarBarraEstadoSuperior();
    dibujarTextoUI(getText(TXT_AJUSTES), 22.0f, 43.0f, 0.80f, texto);
    dibujarTextoUI(g_pestanaConfig == 1 ? getText(TXT_CONEXION) : getText(TXT_GENERAL), 23.0f, 74.0f, 0.44f, secundario);
    dibujarTextoUI("General y conexion  |  Perfil: L en contactos", 22.0f,
        216.0f, 0.36f, secundario);
    dibujarRectanguloRedondeado(19.0f, 102.0f, 362.0f, 104.0f, 13.0f, tarjeta);
    if (g_pestanaConfig == 1) {
        dibujarTextoUI(getText(TXT_SERVIDOR), 35.0f, 115.0f, 0.42f, secundario);
        dibujarTextoUI(limitarTextoUtf8(g_servidorHost, 39), 35.0f, 135.0f, 0.48f, texto);
        dibujarTextoUI(std::string(getText(TXT_PUERTO)) + ": " + std::to_string(g_servidorPuerto), 35.0f, 158.0f, 0.40f, secundario);
        const char* estado = g_errorConfig ? getText(TXT_CONFIG_INVALIDA) :
            g_estadoPruebaConexion > 0 ? getText(TXT_CONEXION_OK) :
            g_estadoPruebaConexion < 0 ? getText(TXT_CONEXION_ERROR) :
            g_cifrarConexion ? getText(TXT_HTTP_INFO) : "HTTP sin cifrado de conexion";
        dibujarTextoUI(estado, 35.0f, 183.0f, 0.39f,
            (g_errorConfig || g_estadoPruebaConexion < 0) ? C2D_Color32(218, 80, 86, 255) : secundario);
    } else {
        dibujarTextoUI(getText(TXT_OSCURO), 35.0f, 116.0f, 0.44f, texto);
        dibujarTextoUI(getText(TXT_LED), 35.0f, 138.0f, 0.44f, texto);
        dibujarTextoUI(getText(TXT_SONIDO), 35.0f, 160.0f, 0.44f, texto);
        dibujarTextoUI(getText(TXT_ACTUALIZACIONES), 35.0f, 182.0f, 0.44f, texto);
    }

    C2D_TargetClear(targetAbajo, fondo);
    C2D_SceneBegin(targetAbajo);
    const char* pestanas[] = {getText(TXT_GENERAL), getText(TXT_CONEXION)};
    for (int i = 0; i < 2; ++i) {
        float x = 8.0f + i * 154.0f;
        dibujarRectanguloRedondeado(x, 8.0f, 146.0f, 28.0f, 8.0f, g_pestanaConfig == i ? azul : tarjeta);
        dibujarTextoUI(pestanas[i], x + 35.0f, 15.0f, 0.39f, g_pestanaConfig == i ? blanco : texto);
    }

    if (g_pestanaConfig == 0) {
        dibujarOpcionAjustes(0, getText(TXT_OSCURO), g_modoOscuro, tarjeta, texto);
        dibujarOpcionAjustes(1, getText(TXT_LED), g_ledActivado, tarjeta, texto);
        dibujarOpcionAjustes(2, getText(TXT_SONIDO), g_sonidoActivado, tarjeta, texto);
        dibujarOpcionAjustes(3, getText(TXT_ACTUALIZACIONES), g_buscarActualizaciones, tarjeta, texto);
    } else if (g_pestanaConfig == 1) {
        const char* titulos[] = { getText(TXT_SERVIDOR), getText(TXT_PUERTO), getText(TXT_INTERVALO), getText(TXT_CIFRAR_CONEXION) };
        std::string valores[] = { limitarTextoUtf8(g_servidorHost, 24), std::to_string(g_servidorPuerto),
            std::to_string(g_intervaloConsultaMs / 1000) + " s", g_cifrarConexion ? "SI (HTTP cifrado)" : "NO (HTTP)" };
        for (int i = 0; i < 4; i++) {
            float y = 44.0f + i * 31.0f;
            if (focoConfiguracion == i)
                dibujarRectanguloRedondeado(8.0f, y - 1.0f, 304.0f, 31.0f, 8.0f, azul);
            dibujarRectanguloRedondeado(10.0f, y, 300.0f, 29.0f, 7.0f, tarjeta);
            dibujarTextoUI(titulos[i], 19.0f, y + 3.0f, 0.30f, secundario);
            dibujarTextoUI(valores[i], 19.0f, y + 14.0f, 0.36f, texto);
            dibujarTextoUI(">", 290.0f, y + 7.0f, 0.43f, secundario);
        }
        if (focoConfiguracion == 4)
            dibujarRectanguloRedondeado(8.0f, 167.0f, 149.0f, 31.0f, 10.0f, azul);
        if (focoConfiguracion == 5)
            dibujarRectanguloRedondeado(163.0f, 167.0f, 149.0f, 31.0f, 10.0f, azul);
        dibujarRectanguloRedondeado(10.0f, 169.0f, 145.0f, 27.0f, 8.0f, azul);
        dibujarRectanguloRedondeado(165.0f, 169.0f, 145.0f, 27.0f, 8.0f, tarjeta);
        dibujarTextoUI(getText(TXT_PROBAR), 60.0f, 175.0f, 0.45f, blanco);
        dibujarTextoUI(getText(TXT_RESTAURAR), 208.0f, 175.0f, 0.45f, texto);
    }
    if (focoConfiguracion == (g_pestanaConfig == 0 ? 4 : 6))
        dibujarRectanguloRedondeado(8.0f, 202.0f, 304.0f, 34.0f, 10.0f, blanco);
    dibujarRectanguloRedondeado(10.0f, 204.0f, 300.0f, 30.0f, 9.0f, azul);
    dibujarTextoUI("A GUARDAR  |  B VOLVER", 71.0f, 211.0f, 0.42f, blanco);
}

