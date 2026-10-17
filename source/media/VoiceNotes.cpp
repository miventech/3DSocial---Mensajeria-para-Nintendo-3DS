#include "VoiceNotes.hpp"
#include <3ds.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

// Captura incremental del micrófono y reproducción NDSP. El WAV se escribe a
// medida que llegan muestras para evitar reservar hasta 30 segundos en RAM.
static const u32 FRECUENCIA_VOZ = 10910;
// PCM mono de 16 bits: 10 910 muestras/s × 30 s × 2 bytes por muestra.
static const u32 MAX_DATOS_VOZ = FRECUENCIA_VOZ * 30 * 2;
static const u32 BUFFER_MICROFONO = 0x30000;
static u8* memoriaMicrofono = NULL;
static u32 tamanoMuestras = 0;
static u32 posicionLectura = 0;
static u32 datosGrabados = 0;
static bool grabacionFallida = false;
static FILE* archivoGrabacion = NULL;
static std::string rutaGrabacion;
static u64 inicioGrabacion = 0;
static u8* datosReproduccion = NULL;
static ndspWaveBuf buferReproduccion;

static void escribir16(FILE* archivo, u16 valor) {
    u8 bytes[2] = {(u8)valor, (u8)(valor >> 8)};
    fwrite(bytes, 1, 2, archivo);
}

static void escribir32(FILE* archivo, u32 valor) {
    u8 bytes[4] = {(u8)valor, (u8)(valor >> 8), (u8)(valor >> 16), (u8)(valor >> 24)};
    fwrite(bytes, 1, 4, archivo);
}

static void escribirCabecera(FILE* archivo, u32 datos) {
    // El encabezado WAV se reescribe al cerrar, cuando ya se conoce el tamaño real.
    fseek(archivo, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, archivo); escribir32(archivo, 36 + datos);
    fwrite("WAVEfmt ", 1, 8, archivo); escribir32(archivo, 16);
    escribir16(archivo, 1); escribir16(archivo, 1);
    escribir32(archivo, FRECUENCIA_VOZ); escribir32(archivo, FRECUENCIA_VOZ * 2);
    escribir16(archivo, 2); escribir16(archivo, 16);
    fwrite("data", 1, 4, archivo); escribir32(archivo, datos);
}

static bool copiarMuestras() {
    // El micrófono escribe en un anillo; se copian solo las muestras aún no guardadas.
    if (!archivoGrabacion) return false;
    u32 ultima = micGetLastSampleOffset();
    if (ultima >= tamanoMuestras) return false;
    while (posicionLectura != ultima && datosGrabados < MAX_DATOS_VOZ) {
        u32 disponibles = ultima > posicionLectura ? ultima - posicionLectura : tamanoMuestras - posicionLectura;
        disponibles = std::min(disponibles, MAX_DATOS_VOZ - datosGrabados);
        disponibles &= ~1u;
        if (disponibles == 0) break;
        if (fwrite(memoriaMicrofono + posicionLectura, 1, disponibles, archivoGrabacion) != disponibles) {
            grabacionFallida = true;
            return false;
        }
        datosGrabados += disponibles;
        posicionLectura = (posicionLectura + disponibles) % tamanoMuestras;
    }
    return true;
}

bool iniciarNotaVoz(const std::string& archivo) {
    // Inicializa micrófono, WAV provisional y contador antes de empezar a muestrear.
    if (archivoGrabacion) return false;
    memoriaMicrofono = (u8*)memalign(0x1000, BUFFER_MICROFONO);
    if (!memoriaMicrofono) return false;
    if (R_FAILED(micInit(memoriaMicrofono, BUFFER_MICROFONO))) {
        free(memoriaMicrofono); memoriaMicrofono = NULL; return false;
    }
    tamanoMuestras = micGetSampleDataSize();
    if (tamanoMuestras == 0) {
        micExit(); free(memoriaMicrofono); memoriaMicrofono = NULL; return false;
    }
    archivoGrabacion = fopen(archivo.c_str(), "wb+");
    if (!archivoGrabacion) {
        micExit(); free(memoriaMicrofono); memoriaMicrofono = NULL; return false;
    }
    rutaGrabacion = archivo;
    datosGrabados = 0;
    grabacionFallida = false;
    posicionLectura = 0;
    escribirCabecera(archivoGrabacion, 0);
    if (R_FAILED(MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_10910,
                                    0, tamanoMuestras, true))) {
        fclose(archivoGrabacion); archivoGrabacion = NULL;
        remove(archivo.c_str()); micExit(); free(memoriaMicrofono); memoriaMicrofono = NULL;
        return false;
    }
    inicioGrabacion = osGetTime();
    return true;
}

bool continuarNotaVoz() {
    // El llamador consulta esto durante la pulsación de MIC para alcanzar el límite.
    return copiarMuestras() && duracionNotaVozMs() < 30000 && datosGrabados < MAX_DATOS_VOZ;
}

int detenerNotaVoz(bool conservar) {
    // Finaliza la captura y descarta audios fallidos, cancelados o menores de 0,5 s.
    if (!archivoGrabacion) return 0;
    bool copiaOk = copiarMuestras();
    MICU_StopSampling();
    copiaOk = copiarMuestras() && copiaOk;
    micExit();
    free(memoriaMicrofono); memoriaMicrofono = NULL;
    escribirCabecera(archivoGrabacion, datosGrabados);
    bool escrituraOk = fflush(archivoGrabacion) == 0;
    escrituraOk = fclose(archivoGrabacion) == 0 && escrituraOk;
    archivoGrabacion = NULL;
    int duracion = datosGrabados * 1000 / (FRECUENCIA_VOZ * 2);
    if (!conservar || !escrituraOk || !copiaOk || grabacionFallida || duracion < 500) {
        remove(rutaGrabacion.c_str());
        return 0;
    }
    return duracion;
}

bool notaVozActiva() { return archivoGrabacion != NULL; }
u64 duracionNotaVozMs() { return archivoGrabacion ? osGetTime() - inicioGrabacion : 0; }

void actualizarReproduccionVoz() {
    // NDSP conserva el buffer mientras reproduce; se libera cuando marca DONE.
    if (datosReproduccion && buferReproduccion.status == NDSP_WBUF_DONE) {
        linearFree(datosReproduccion);
        datosReproduccion = NULL;
    }
}

bool reproducirNotaVoz(const std::string& archivo) {
    // Comprueba la cabecera y límites antes de enviar PCM al canal NDSP 1.
    FILE* entrada = fopen(archivo.c_str(), "rb");
    if (!entrada) return false;
    u8 cabecera[44];
    if (fread(cabecera, 1, 44, entrada) != 44 || memcmp(cabecera, "RIFF", 4) != 0 ||
        memcmp(cabecera + 8, "WAVEfmt ", 8) != 0 || memcmp(cabecera + 36, "data", 4) != 0) {
        fclose(entrada); return false;
    }
    u32 tasa = cabecera[24] | (cabecera[25] << 8) | (cabecera[26] << 16) | (cabecera[27] << 24);
    u32 datos = cabecera[40] | (cabecera[41] << 8) | (cabecera[42] << 16) | (cabecera[43] << 24);
    if (tasa != FRECUENCIA_VOZ || datos == 0 || datos > MAX_DATOS_VOZ || datos % 2 != 0) {
        fclose(entrada); return false;
    }
    ndspChnWaveBufClear(1);
    if (datosReproduccion) { linearFree(datosReproduccion); datosReproduccion = NULL; }
    datosReproduccion = (u8*)linearAlloc(datos);
    if (!datosReproduccion || fread(datosReproduccion, 1, datos, entrada) != datos) {
        if (datosReproduccion) linearFree(datosReproduccion);
        datosReproduccion = NULL; fclose(entrada); return false;
    }
    fclose(entrada);
    DSP_FlushDataCache(datosReproduccion, datos);
    ndspChnReset(1);
    ndspChnSetFormat(1, NDSP_FORMAT_MONO_PCM16);
    ndspChnSetRate(1, (float)FRECUENCIA_VOZ);
    memset(&buferReproduccion, 0, sizeof(buferReproduccion));
    buferReproduccion.data_pcm16 = (s16*)datosReproduccion;
    buferReproduccion.nsamples = datos / 2;
    ndspChnWaveBufAdd(1, &buferReproduccion);
    return true;
}

void cerrarNotasVoz() {
    // Ruta de limpieza común para salir de la app sin dejar captura o audio activos.
    if (archivoGrabacion) detenerNotaVoz(false);
    ndspChnWaveBufClear(1);
    if (datosReproduccion) linearFree(datosReproduccion);
    datosReproduccion = NULL;
}
