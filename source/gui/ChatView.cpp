#include "AppState.hpp"

// Renderizado de conversación. Separa el formato legado del modelo paginado,
// calcula burbujas visibles y aplica avisos de llegada a un solo mensaje.
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

static void dibujarAvisoMensajeReciente(float x, float y, float ancho, float alto,
                                       bool esMensajeNuevo) {
    if (!esMensajeNuevo) return;
    u64 inicio = g_ultimoMensajeAnimacion;
    u64 ahora = osGetTime();
    if (!inicio || ahora < inicio || ahora - inicio >= 850) return;
    float avance = (float)(ahora - inicio) / 850.0f;
    float intensidad = (1.0f - avance) * (0.75f + 0.25f * sinf(avance * 13.0f));
    u32 color = C2D_Color32(255, 215, 89, (u8)(intensidad * 220.0f));
    C2D_DrawRectSolid(x + 12.0f, y + alto - 5.0f, 0.8f,
        std::min(44.0f, ancho - 24.0f), 2.0f, color);
}

void dibujarHistorialChat(u32 colorTextoPrincipal, u32 colorTextoSecundario) {
    // Dibuja el historial legado de texto, desde el mensaje más reciente hacia arriba.
    g_idsMensajesVisibles.clear();
    const float escalaTexto = 0.42f;
    float yInferior = 234.0f;
    const Contacto* estilo = contactoActivo();

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

        if (y < 106.0f) break;

        bool entradaOscura = estilo && (estilo->colorChat > 0 || estilo->colorFondo > 0);
        u32 colorBurbuja = propio ? colorBurbujaPerfil(g_colorChat) :
            estilo && estilo->colorChat > 0 ? colorBurbujaPerfil(estilo->colorChat) :
            entradaOscura || g_modoOscuro ? C2D_Color32(55, 61, 73, 255) :
            C2D_Color32(248, 249, 252, 255);
        u32 colorTextoBurbuja = propio
            ? colorTextoPerfil(g_colorTexto)
            : entradaOscura || g_modoOscuro ?
              colorTextoPerfil(estilo ? estilo->colorTexto : 0) : colorTextoPrincipal;
        u32 colorEtiqueta = propio
            ? colorApodoPerfil(g_colorNombre)
            : entradaOscura || g_modoOscuro ?
              colorApodoPerfil(estilo ? estilo->colorNombre : 0) :
              estilo && estilo->colorNombre > 0 ?
              colorApodoTarjeta(estilo->colorNombre, false) : colorTextoSecundario;

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
        dibujarAvisoMensajeReciente(x, y, anchoBurbuja, altoBurbuja,
            i == g_ultimoMensajeAnimadoIndice);

        yInferior = y - 6.0f;
    }
}

void dibujarHistorialV2(u32 colorTextoPrincipal, u32 colorTextoSecundario) {
    // Dibuja mensajes paginados V2, incluyendo indicadores de audio y foto seleccionados.
    g_idsMensajesVisibles.clear();
    float yInferior = 234.0f;
    int indice = (int)g_historialV2.size() - 1 - g_desplazamientoChat;
    int primerIndice = indice;
    const Contacto* estilo = contactoActivo();
    for (; indice >= 0; indice--) {
        const MensajeV2& mensaje = g_historialV2[indice];
        bool propio = mensaje.remitente == miID;
        std::string contenido;
        if (mensaje.audio) {
            int segundos = (mensaje.duracionMs + 999) / 1000;
            contenido = std::string("♪ Audio 0:") + (segundos < 10 ? "0" : "") + std::to_string(segundos);
        } else if (mensaje.foto) contenido = "FOTO";
        else contenido = envolverTextoMensaje(mensaje.texto, 34);
        if (contenido.empty()) continue;
        C2D_Text textoMensaje;
        C2D_TextParse(&textoMensaje, bufTexto, contenido.c_str());
        float anchoTexto = 0.0f, altoTexto = 0.0f;
        C2D_TextGetDimensions(&textoMensaje, 0.42f, 0.42f, &anchoTexto, &altoTexto);
        float ancho = mensaje.foto ? 186.0f :
            std::max(112.0f, std::min(282.0f, anchoTexto + 24.0f));
        float alto = mensaje.foto ? 94.0f : std::max(40.0f, altoTexto + 20.0f);
        float x = propio ? 386.0f - ancho : 14.0f;
        float y = yInferior - alto;
        if (y < 106.0f) break;
        g_idsMensajesVisibles.push_back(mensaje.id);
        bool entradaOscura = estilo && (estilo->colorChat > 0 || estilo->colorFondo > 0);
        u32 burbuja = propio ? colorBurbujaPerfil(g_colorChat) :
            estilo && estilo->colorChat > 0 ? colorBurbujaPerfil(estilo->colorChat) :
            entradaOscura || g_modoOscuro ? C2D_Color32(55, 61, 73, 255) :
            C2D_Color32(248, 249, 252, 255);
        if ((mensaje.audio && mensaje.id == g_audioSeleccionadoId) ||
            (mensaje.foto && mensaje.id == g_fotoSeleccionadaId))
            burbuja = propio ? C2D_Color32(19, 158, 168, 255) : C2D_Color32(75, 152, 171, 255);
        dibujarRectanguloRedondeado(x, y, ancho, alto, 10.0f, burbuja);
        C2D_Text etiqueta;
        std::string nombre = propio ? getText(TXT_TU) : limitarTextoUtf8(contactoActivoNombre, 24);
        C2D_TextParse(&etiqueta, bufTexto, nombre.c_str());
        bool seleccionado = (mensaje.audio && mensaje.id == g_audioSeleccionadoId) ||
                            (mensaje.foto && mensaje.id == g_fotoSeleccionadaId);
        u32 colorPrincipal = propio ? colorTextoPerfil(g_colorTexto) :
            seleccionado || entradaOscura || g_modoOscuro ?
            colorTextoPerfil(estilo ? estilo->colorTexto : 0) : colorTextoPrincipal;
        C2D_DrawText(&etiqueta, C2D_WithColor, x + 10.0f, y + 4.0f, 0.5f, 0.35f, 0.35f,
                     propio ? colorApodoPerfil(g_colorNombre) :
                     seleccionado || entradaOscura || g_modoOscuro ?
                     colorApodoPerfil(estilo ? estilo->colorNombre : 0) :
                     estilo && estilo->colorNombre > 0 ?
                     colorApodoTarjeta(estilo->colorNombre, false) : colorTextoSecundario);
        C2D_DrawText(&textoMensaje, C2D_WithColor, x + 10.0f, y + 16.0f, 0.5f, 0.42f, 0.42f, colorPrincipal);
        if (mensaje.foto) {
            int slot = primerIndice - indice;
            dibujarRectanguloRedondeado(x + 8.0f, y + 34.0f, 84.0f, 52.0f, 5.0f,
                C2D_Color32(235, 240, 245, 255));
            if (idMiniaturaFotoChat(slot) == mensaje.id)
                C2D_DrawImageAt(obtenerMiniaturaFotoChat(slot), x + 10.0f, y + 36.0f, 0.8f);
            else {
                C2D_DrawRectSolid(x + 20.0f, y + 47.0f, 0.8f, 58.0f, 28.0f,
                    C2D_Color32(177, 192, 207, 255));
                C2D_DrawCircleSolid(x + 49.0f, y + 60.0f, 0.9f, 8.0f,
                    C2D_Color32(232, 238, 244, 255));
            }
            C2D_Text textoMiniatura;
            C2D_TextParse(&textoMiniatura, bufTexto, "VISTA PREVIA\n80 x 48");
            C2D_DrawText(&textoMiniatura, C2D_WithColor, x + 100.0f, y + 47.0f,
                0.8f, 0.29f, 0.29f, colorPrincipal);
        }
        dibujarAvisoMensajeReciente(x, y, ancho, alto,
            g_desplazamientoChat == 0 && mensaje.id == g_ultimoMensajeAnimadoId);
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
