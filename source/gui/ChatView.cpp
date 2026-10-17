#include "AppState.hpp"

void dibujarRectanguloRedondeado(float x, float y, float ancho, float alto, float radio, u32 color, float profundidad) {
    // Forma una tarjeta redondeada con rectángulos centrales y círculos en las esquinas.
    if (ancho <= 0.0f || alto <= 0.0f) return;
    float r = std::min(radio, std::min(ancho * 0.5f, alto * 0.5f));

    C2D_DrawRectSolid(x + r, y, profundidad, ancho - (r * 2.0f), alto, color);
    C2D_DrawRectSolid(x, y + r, profundidad, ancho, alto - (r * 2.0f), color);
    C2D_DrawCircleSolid(x + r, y + r, profundidad, r, color);
    C2D_DrawCircleSolid(x + ancho - r, y + r, profundidad, r, color);
    C2D_DrawCircleSolid(x + r, y + alto - r, profundidad, r, color);
    C2D_DrawCircleSolid(x + ancho - r, y + alto - r, profundidad, r, color);
}

bool esMensajePropio(const std::string& mensaje) {
    // El formato legado identifica lo enviado desde esta consola con la etiqueta [Yo].
    return mensaje.find("[Yo]") == 0;
}

std::string extraerTextoMensaje(const std::string& mensaje) {
    // Quita el prefijo [remitente]: del historial antiguo antes de dibujar la burbuja.
    size_t cierreEtiqueta = mensaje.find(']');
    if (mensaje.empty() || mensaje[0] != '[' || cierreEtiqueta == std::string::npos) {
        return mensaje;
    }

    size_t inicioTexto = cierreEtiqueta + 1;
    while (inicioTexto < mensaje.size() && (mensaje[inicioTexto] == ':' || mensaje[inicioTexto] == ' ')) {
        inicioTexto++;
    }
    return mensaje.substr(inicioTexto);
}

std::string limitarTextoUtf8(const std::string& texto, size_t maxCaracteres) {
    // Cuenta secuencias UTF-8 para no cortar un carácter acentuado por la mitad.
    size_t posicion = 0;
    size_t caracteres = 0;

    while (posicion < texto.size() && caracteres < maxCaracteres) {
        unsigned char primerByte = (unsigned char)texto[posicion];
        size_t bytesCaracter = (primerByte & 0x80) == 0 ? 1 :
                               (primerByte & 0xE0) == 0xC0 ? 2 :
                               (primerByte & 0xF0) == 0xE0 ? 3 :
                               (primerByte & 0xF8) == 0xF0 ? 4 : 1;
        if (posicion + bytesCaracter > texto.size()) break;
        posicion += bytesCaracter;
        caracteres++;
    }

    if (posicion >= texto.size()) return texto;
    return texto.substr(0, posicion) + "...";
}

std::string envolverTextoMensaje(const std::string& mensaje, size_t maxCaracteres) {
    // Ajusta por palabras para que el texto quepa dentro de la burbuja del chat.
    std::istringstream palabras(mensaje);
    std::string palabra;
    std::string linea;
    std::string resultado;

    while (palabras >> palabra) {
        if (!linea.empty() && linea.size() + palabra.size() + 1 > maxCaracteres) {
            resultado += linea + "\n";
            linea = palabra;
        } else if (linea.empty()) {
            linea = palabra;
        } else {
            linea += " " + palabra;
        }
    }

    if (!linea.empty()) resultado += linea;
    return resultado;
}

void dibujarHistorialChat(u32 colorTextoPrincipal, u32 colorTextoSecundario) {
    // Dibuja el historial legado de texto, desde el mensaje más reciente hacia arriba.
    const float escalaTexto = 0.42f;
    float yInferior = 234.0f;

    for (int i = (int)historialChat.size() - 1; i >= 0; i--) {
        bool propio = esMensajePropio(historialChat[i]);
        std::string contenido = envolverTextoMensaje(extraerTextoMensaje(historialChat[i]), 34);
        if (contenido.empty()) continue;

        C2D_Text textoMensaje;
        C2D_TextParse(&textoMensaje, bufTexto, contenido.c_str());

        float anchoTexto = 0.0f;
        float altoTexto = 0.0f;
        C2D_TextGetDimensions(&textoMensaje, escalaTexto, escalaTexto, &anchoTexto, &altoTexto);

        float anchoBurbuja = std::max(96.0f, std::min(282.0f, anchoTexto + 24.0f));
        float altoBurbuja = std::max(38.0f, altoTexto + 20.0f);
        float x = propio ? (386.0f - anchoBurbuja) : 14.0f;
        float y = yInferior - altoBurbuja;

        if (y < 40.0f) break;

        u32 colorBurbuja = propio
            ? C2D_Color32(25, 125, 215, 255)
            : (g_modoOscuro ? C2D_Color32(55, 61, 73, 255) : C2D_Color32(248, 249, 252, 255));
        u32 colorTextoBurbuja = propio
            ? C2D_Color32(255, 255, 255, 255)
            : colorTextoPrincipal;
        u32 colorEtiqueta = propio
            ? C2D_Color32(220, 239, 255, 255)
            : colorTextoSecundario;

        if (propio) {
            C2D_DrawTriangle(x + anchoBurbuja - 18.0f, y + altoBurbuja - 11.0f, colorBurbuja,
                             x + anchoBurbuja + 3.0f, y + altoBurbuja + 1.0f, colorBurbuja,
                             x + anchoBurbuja - 28.0f, y + altoBurbuja - 1.0f, colorBurbuja, 0.5f);
        } else {
            C2D_DrawTriangle(x + 18.0f, y + altoBurbuja - 11.0f, colorBurbuja,
                             x - 3.0f, y + altoBurbuja + 1.0f, colorBurbuja,
                             x + 28.0f, y + altoBurbuja - 1.0f, colorBurbuja, 0.5f);
        }
        dibujarRectanguloRedondeado(x, y, anchoBurbuja, altoBurbuja, 9.0f, colorBurbuja);

        C2D_Text etiqueta;
        std::string nombreEtiqueta = propio ? getText(TXT_TU) : limitarTextoUtf8(contactoActivoNombre, 24);
        C2D_TextParse(&etiqueta, bufTexto, nombreEtiqueta.c_str());
        C2D_DrawText(&etiqueta, C2D_WithColor, x + 10.0f, y + 4.0f, 0.5f, 0.36f, 0.36f, colorEtiqueta);
        C2D_DrawText(&textoMensaje, C2D_WithColor, x + 10.0f, y + 15.0f, 0.5f, escalaTexto, escalaTexto, colorTextoBurbuja);

        yInferior = y - 6.0f;
    }
}

void dibujarHistorialV2(u32 colorTextoPrincipal, u32 colorTextoSecundario) {
    // Dibuja mensajes paginados V2, incluyendo indicadores de audio y foto seleccionados.
    float yInferior = 234.0f;
    int indice = (int)g_historialV2.size() - 1 - g_desplazamientoChat;
    for (; indice >= 0; indice--) {
        const MensajeV2& mensaje = g_historialV2[indice];
        bool propio = mensaje.remitente == miID;
        std::string contenido;
        if (mensaje.audio) {
            int segundos = (mensaje.duracionMs + 999) / 1000;
            contenido = std::string("♪ Audio 0:") + (segundos < 10 ? "0" : "") + std::to_string(segundos);
        } else if (mensaje.foto) contenido = "FOTO  400 x 240";
        else contenido = envolverTextoMensaje(mensaje.texto, 34);
        if (contenido.empty()) continue;
        C2D_Text textoMensaje;
        C2D_TextParse(&textoMensaje, bufTexto, contenido.c_str());
        float anchoTexto = 0.0f, altoTexto = 0.0f;
        C2D_TextGetDimensions(&textoMensaje, 0.42f, 0.42f, &anchoTexto, &altoTexto);
        float ancho = std::max(112.0f, std::min(282.0f, anchoTexto + 24.0f));
        float alto = std::max(40.0f, altoTexto + 20.0f);
        float x = propio ? 386.0f - ancho : 14.0f;
        float y = yInferior - alto;
        if (y < 40.0f) break;
        u32 burbuja = propio ? C2D_Color32(25, 125, 215, 255) :
            (g_modoOscuro ? C2D_Color32(55, 61, 73, 255) : C2D_Color32(248, 249, 252, 255));
        if ((mensaje.audio && mensaje.id == g_audioSeleccionadoId) ||
            (mensaje.foto && mensaje.id == g_fotoSeleccionadaId))
            burbuja = propio ? C2D_Color32(19, 158, 168, 255) : C2D_Color32(75, 152, 171, 255);
        dibujarRectanguloRedondeado(x, y, ancho, alto, 10.0f, burbuja);
        C2D_Text etiqueta;
        std::string nombre = propio ? getText(TXT_TU) : limitarTextoUtf8(contactoActivoNombre, 24);
        C2D_TextParse(&etiqueta, bufTexto, nombre.c_str());
        bool seleccionado = (mensaje.audio && mensaje.id == g_audioSeleccionadoId) ||
                            (mensaje.foto && mensaje.id == g_fotoSeleccionadaId);
        u32 colorPrincipal = propio || seleccionado
            ? C2D_Color32(255, 255, 255, 255) : colorTextoPrincipal;
        C2D_DrawText(&etiqueta, C2D_WithColor, x + 10.0f, y + 4.0f, 0.5f, 0.35f, 0.35f,
                     propio ? C2D_Color32(220, 239, 255, 255) : colorTextoSecundario);
        C2D_DrawText(&textoMensaje, C2D_WithColor, x + 10.0f, y + 16.0f, 0.5f, 0.42f, 0.42f, colorPrincipal);
        yInferior = y - 6.0f;
    }
}

void dibujarAnimacionCarga(float centroX, float centroY, float angulo) {
    // Ocho puntos con distinta opacidad forman el indicador giratorio de espera.
    const int numPuntos = 8;
    const float radio = 18.0f;
    const float radioPuntoBase = 3.0f;

    for (int i = 0; i < numPuntos; i++) {
        float anguloPunto = angulo + (i * (2.0f * M_PI / numPuntos));
        float x = centroX + radio * cosf(anguloPunto);
        float y = centroY + radio * sinf(anguloPunto);

        u8 alpha = (u8)(255 * ((float)(i + 1) / numPuntos));
        u32 color = C2D_Color32(255, 255, 255, alpha);

        C2D_DrawCircleSolid(x, y, 0.5f, radioPuntoBase, color);
    }
}

// 🆔 1. Obtener ID de la Consola
