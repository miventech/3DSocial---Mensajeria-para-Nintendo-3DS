// Captura adaptada de FBI/source/core/task/capturecam.c (licencia MIT).
// Aviso y licencia completos en THIRD_PARTY_FBI_LICENSE.txt.
// CAMU captura frames lineales en un hilo; la interfaz copia una versión
// consistente a textura Citro3D, cuyo almacenamiento usa orden Morton.
#include "CameraPhotos.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const u16 TEXTURA_ANCHO = 512;
static const u16 TEXTURA_ALTO = 256;
// Citro3D requiere texturas en mosaicos Morton; estas variables guardan la
// textura renderizable y los buffers lineales que se copian hacia la GPU.
static C3D_Tex texturaFoto;
static Tex3DS_SubTexture subTexturaFoto;
static C2D_Image imagenFoto;
static bool texturaFotoLista = false;
static u16* mosaicoFoto = NULL;
static C3D_Tex texturasMiniatura[3];
static Tex3DS_SubTexture subTexturasMiniatura[3];
static C2D_Image imagenesMiniatura[3];
static bool miniaturaTexturaLista[3] = {false, false, false};
static u16* mosaicosMiniatura[3] = {NULL, NULL, NULL};
static C3D_Tex texturasFotoChat[3];
static Tex3DS_SubTexture subTexturasFotoChat[3];
static C2D_Image imagenesFotoChat[3];
static bool fotoChatTexturaLista[3] = {false, false, false};
static u16* mosaicosFotoChat[3] = {NULL, NULL, NULL};
static long idsFotoChat[3] = {0, 0, 0};

// La camara escribe en su propio buffer. El hilo publica cuadros completos
// bajo mutex; la UI solo lee la copia estable en ultimoFotograma.
static u16* cuadroCompartido = NULL;
static u8* ultimoFotograma = NULL;
static Handle mutexCuadro = 0;
static Handle eventoCancelar = 0;
static volatile bool hiloTerminado = true;
static volatile u32 versionCuadro = 0;
static u32 versionVista = 0;
static Result resultadoCamara = 0;
static bool camaraActiva = false;
static u64 inicioCamaraMs = 0;

static bool cargarPixelsEnTextura(const u8* pixels);

// Hilo de fondo: configura CAMU, espera fotogramas y publica copias completas.
// El evento 0 cancela, 1 indica que llegó un buffer y 2 señala error de cámara.
static void hiloCapturaCamara(void*) {
    Handle eventos[3] = {eventoCancelar, 0, 0};
    u16* receptor = (u16*)calloc(1, FOTO_CAMARA_BYTES);
    Result res = receptor ? 0 : (Result)-1;

    if (receptor && R_SUCCEEDED(res = camInit())) {
        const u32 camara = SELECT_OUT1;
        if (R_SUCCEEDED(res = CAMU_SetSize(camara, SIZE_CTR_TOP_LCD, CONTEXT_A)) &&
            R_SUCCEEDED(res = CAMU_SetOutputFormat(camara, OUTPUT_RGB_565, CONTEXT_A)) &&
            R_SUCCEEDED(res = CAMU_SetFrameRate(camara, FRAME_RATE_30)) &&
            R_SUCCEEDED(res = CAMU_SetNoiseFilter(camara, true)) &&
            R_SUCCEEDED(res = CAMU_SetAutoExposure(camara, true)) &&
            R_SUCCEEDED(res = CAMU_SetAutoWhiteBalance(camara, true)) &&
            R_SUCCEEDED(res = CAMU_Activate(camara))) {
            u32 unidad = 0;
            if (R_SUCCEEDED(res = CAMU_GetBufferErrorInterruptEvent(&eventos[2], PORT_CAM1)) &&
                R_SUCCEEDED(res = CAMU_SetTrimming(PORT_CAM1, true)) &&
                R_SUCCEEDED(res = CAMU_SetTrimmingParamsCenter(PORT_CAM1,
                    FOTO_CAMARA_ANCHO, FOTO_CAMARA_ALTO, 400, 240)) &&
                R_SUCCEEDED(res = CAMU_GetMaxBytes(&unidad, FOTO_CAMARA_ANCHO, FOTO_CAMARA_ALTO)) &&
                R_SUCCEEDED(res = CAMU_SetTransferBytes(PORT_CAM1, unidad,
                    FOTO_CAMARA_ANCHO, FOTO_CAMARA_ALTO)) &&
                R_SUCCEEDED(res = CAMU_ClearBuffer(PORT_CAM1)) &&
                R_SUCCEEDED(res = CAMU_SetReceiving(&eventos[1], receptor, PORT_CAM1,
                    FOTO_CAMARA_BYTES, (s16)unidad)) &&
                R_SUCCEEDED(res = CAMU_StartCapture(PORT_CAM1))) {
                bool cancelar = false;
                while (!cancelar && R_SUCCEEDED(res)) {
                    s32 indice = 0;
                    res = svcWaitSynchronizationN(&indice, eventos, 3, false, U64_MAX);
                    if (R_FAILED(res)) break;
                    switch (indice) {
                        case 0:
                            cancelar = true;
                            break;
                        case 1:
                            svcCloseHandle(eventos[1]);
                            eventos[1] = 0;
                            svcWaitSynchronization(mutexCuadro, U64_MAX);
                            memcpy(cuadroCompartido, receptor, FOTO_CAMARA_BYTES);
                            GSPGPU_FlushDataCache(cuadroCompartido, FOTO_CAMARA_BYTES);
                            ++versionCuadro;
                            svcReleaseMutex(mutexCuadro);
                            res = CAMU_SetReceiving(&eventos[1], receptor, PORT_CAM1,
                                FOTO_CAMARA_BYTES, (s16)unidad);
                            break;
                        case 2:
                            if (eventos[1]) svcCloseHandle(eventos[1]);
                            eventos[1] = 0;
                            if (R_SUCCEEDED(res = CAMU_ClearBuffer(PORT_CAM1)) &&
                                R_SUCCEEDED(res = CAMU_SetReceiving(&eventos[1], receptor,
                                    PORT_CAM1, FOTO_CAMARA_BYTES, (s16)unidad)))
                                res = CAMU_StartCapture(PORT_CAM1);
                            break;
                    }
                }
                CAMU_StopCapture(PORT_CAM1);
                bool ocupada = false;
                while (R_SUCCEEDED(CAMU_IsBusy(&ocupada, PORT_CAM1)) && ocupada)
                    svcSleepThread(1000000);
                CAMU_ClearBuffer(PORT_CAM1);
            }
            CAMU_Activate(SELECT_NONE);
        }
        camExit();
    }

    if (eventos[1]) svcCloseHandle(eventos[1]);
    if (eventos[2]) svcCloseHandle(eventos[2]);
    free(receptor);
    if (resultadoCamara == 0) resultadoCamara = res;
    hiloTerminado = true;
}

void detenerVistaCamara() {
    // Espera al hilo antes de liberar mutex, eventos y memoria compartida.
    camaraActiva = false;
    if (!hiloTerminado) {
        if (eventoCancelar) svcSignalEvent(eventoCancelar);
        while (!hiloTerminado) svcSleepThread(1000000);
    }
    if (eventoCancelar) svcCloseHandle(eventoCancelar);
    if (mutexCuadro) svcCloseHandle(mutexCuadro);
    eventoCancelar = 0;
    mutexCuadro = 0;
    free(cuadroCompartido);
    free(ultimoFotograma);
    cuadroCompartido = NULL;
    ultimoFotograma = NULL;
    versionCuadro = 0;
    versionVista = 0;
}

bool iniciarVistaCamara() {
    // La captura usa buffers separados para que la UI nunca lea una imagen a medias.
    cerrarFotoVista();
    cuadroCompartido = (u16*)calloc(1, FOTO_CAMARA_BYTES);
    ultimoFotograma = (u8*)malloc(FOTO_CAMARA_BYTES);
    if (!cuadroCompartido || !ultimoFotograma ||
        R_FAILED(svcCreateEvent(&eventoCancelar, RESET_STICKY)) ||
        R_FAILED(svcCreateMutex(&mutexCuadro, false))) {
        detenerVistaCamara();
        return false;
    }
    hiloTerminado = false;
    resultadoCamara = 0;
    if (!threadCreate(hiloCapturaCamara, NULL, 0x10000, 0x1A, 0, true)) {
        hiloTerminado = true;
        detenerVistaCamara();
        return false;
    }
    inicioCamaraMs = osGetTime();
    camaraActiva = true;
    return true;
}

bool actualizarVistaCamara() {
    // Se llama desde el bucle de renderizado; no espera por la cámara más de lo necesario.
    if (!camaraActiva || hiloTerminado) return false;
    if ((osGetTime() - inicioCamaraMs > 8000) && !versionCuadro) {
        resultadoCamara = (Result)0xFFFFFFFF; // La camara no entrego el primer cuadro.
        return false;
    }
    svcWaitSynchronization(mutexCuadro, U64_MAX);
    bool nuevo = versionVista != versionCuadro;
    if (nuevo) {
        memcpy(ultimoFotograma, cuadroCompartido, FOTO_CAMARA_BYTES);
        versionVista = versionCuadro;
    }
    svcReleaseMutex(mutexCuadro);
    if (nuevo && !cargarPixelsEnTextura(ultimoFotograma)) return false;
    return true;
}

bool hayFotogramaCamara() { return camaraActiva && versionVista != 0; }
u32 obtenerCodigoErrorCamara() { return (u32)resultadoCamara; }

bool guardarFotogramaCamara(const std::string& ruta) {
    // El formato del archivo es el bloque crudo de píxeles acordado con el servidor.
    if (!hayFotogramaCamara()) return false;
    FILE* archivo = fopen(ruta.c_str(), "wb");
    if (!archivo) return false;
    bool guardada = fwrite(ultimoFotograma, 1, FOTO_CAMARA_BYTES, archivo) == FOTO_CAMARA_BYTES;
    guardada = fclose(archivo) == 0 && guardada;
    if (guardada) CAMU_PlayShutterSound(SHUTTER_SOUND_TYPE_NORMAL);
    else remove(ruta.c_str());
    return guardada;
}

static u32 indiceMorton(u32 x, u32 y) {
    // Reordena las coordenadas 8×8 al índice que usa el layout de textura de 3DS.
    return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) |
           ((x & 4) << 2) | ((y & 4) << 3);
}

bool cargarFotoVista(const std::string& ruta) {
    // La cámara y las texturas GPU_RGB565 usan el mismo orden de canales.
    FILE* archivo = fopen(ruta.c_str(), "rb");
    if (!archivo) return false;
    u8* pixels = (u8*)malloc(FOTO_CAMARA_BYTES);
    if (!pixels) { fclose(archivo); return false; }
    bool leida = fread(pixels, 1, FOTO_CAMARA_BYTES, archivo) == FOTO_CAMARA_BYTES;
    leida = fclose(archivo) == 0 && leida;
    if (!leida) { free(pixels); return false; }
    bool cargada = cargarPixelsEnTextura(pixels);
    free(pixels);
    return cargada;
}

static bool cargarPixelsEnTextura(const u8* pixels) {
    // Empaqueta imagen lineal de 400×240 en mosaicos compatibles con GPU_RGB565.
    const u32 bytesTextura = TEXTURA_ANCHO * TEXTURA_ALTO * 2;
    if (!mosaicoFoto) mosaicoFoto = (u16*)linearAlloc(bytesTextura);
    if (!mosaicoFoto) return false;
    if (!texturaFotoLista) {
        texturaFotoLista = C3D_TexInit(&texturaFoto, TEXTURA_ANCHO, TEXTURA_ALTO, GPU_RGB565);
        if (!texturaFotoLista) return false;
        C3D_TexSetFilter(&texturaFoto, GPU_LINEAR, GPU_LINEAR);
        subTexturaFoto.width = FOTO_CAMARA_ANCHO;
        subTexturaFoto.height = FOTO_CAMARA_ALTO;
        subTexturaFoto.left = 0.0f;
        subTexturaFoto.top = 1.0f;
        subTexturaFoto.right = (float)FOTO_CAMARA_ANCHO / TEXTURA_ANCHO;
        subTexturaFoto.bottom = 1.0f - (float)FOTO_CAMARA_ALTO / TEXTURA_ALTO;
        imagenFoto.tex = &texturaFoto;
        imagenFoto.subtex = &subTexturaFoto;
    }
    memset(mosaicoFoto, 0, bytesTextura);
    const u16* origen = (const u16*)pixels;
    for (u32 y = 0; y < FOTO_CAMARA_ALTO; y++) {
        for (u32 x = 0; x < FOTO_CAMARA_ANCHO; x++) {
            u32 tile = (y / 8) * (TEXTURA_ANCHO / 8) + (x / 8);
            u32 destino = tile * 64 + indiceMorton(x & 7, y & 7);
            mosaicoFoto[destino] = origen[y * FOTO_CAMARA_ANCHO + x];
        }
    }
    memcpy(texturaFoto.data, mosaicoFoto, bytesTextura);
    C3D_TexFlush(&texturaFoto);
    return true;
}

void cerrarFotoVista() {
    detenerVistaCamara();
    if (texturaFotoLista) C3D_TexDelete(&texturaFoto);
    texturaFotoLista = false;
    if (mosaicoFoto) linearFree(mosaicoFoto);
    mosaicoFoto = NULL;
}

C2D_Image obtenerFotoVista() { return imagenFoto; }

bool cargarMiniaturaContacto(int slot, const std::string& ruta) {
    // Solo hay tres slots porque la lista visible enseña hasta tres contactos a la vez.
    if (slot < 0 || slot >= 3) return false;
    FILE* archivo = fopen(ruta.c_str(), "rb");
    if (!archivo) return false;
    u8 pixels[PERFIL_MINIATURA_BYTES];
    bool leida = fread(pixels, 1, sizeof(pixels), archivo) == sizeof(pixels);
    leida = fclose(archivo) == 0 && leida;
    if (!leida) return false;

    const u32 bytes = PERFIL_MINIATURA_BYTES;
    if (!mosaicosMiniatura[slot])
        mosaicosMiniatura[slot] = (u16*)linearAlloc(bytes);
    if (!mosaicosMiniatura[slot]) return false;
    if (!miniaturaTexturaLista[slot]) {
        miniaturaTexturaLista[slot] = C3D_TexInit(&texturasMiniatura[slot],
            PERFIL_MINIATURA_DIMENSION, PERFIL_MINIATURA_DIMENSION, GPU_RGB565);
        if (!miniaturaTexturaLista[slot]) return false;
        C3D_TexSetFilter(&texturasMiniatura[slot], GPU_LINEAR, GPU_LINEAR);
        subTexturasMiniatura[slot].width = PERFIL_MINIATURA_DIMENSION;
        subTexturasMiniatura[slot].height = PERFIL_MINIATURA_DIMENSION;
        subTexturasMiniatura[slot].left = 0.0f;
        subTexturasMiniatura[slot].top = 1.0f;
        subTexturasMiniatura[slot].right = 1.0f;
        subTexturasMiniatura[slot].bottom = 0.0f;
        imagenesMiniatura[slot].tex = &texturasMiniatura[slot];
        imagenesMiniatura[slot].subtex = &subTexturasMiniatura[slot];
    }

    const u16* origen = (const u16*)pixels;
    for (u32 y = 0; y < PERFIL_MINIATURA_DIMENSION; ++y) {
        for (u32 x = 0; x < PERFIL_MINIATURA_DIMENSION; ++x) {
            u32 tile = (y / 8) * (PERFIL_MINIATURA_DIMENSION / 8) + (x / 8);
            u32 destino = tile * 64 + indiceMorton(x & 7, y & 7);
            mosaicosMiniatura[slot][destino] =
                origen[y * PERFIL_MINIATURA_DIMENSION + x];
        }
    }
    memcpy(texturasMiniatura[slot].data, mosaicosMiniatura[slot], bytes);
    C3D_TexFlush(&texturasMiniatura[slot]);
    return true;
}

C2D_Image obtenerMiniaturaContacto(int slot) {
    if (slot < 0 || slot >= 3) return C2D_Image();
    return imagenesMiniatura[slot];
}

void cerrarMiniaturasContactos() {
    // Limpia texturas y referencias para permitir cargar otra ventana de contactos.
    for (int slot = 0; slot < 3; ++slot) {
        if (miniaturaTexturaLista[slot]) C3D_TexDelete(&texturasMiniatura[slot]);
        miniaturaTexturaLista[slot] = false;
        if (mosaicosMiniatura[slot]) linearFree(mosaicosMiniatura[slot]);
        mosaicosMiniatura[slot] = NULL;
        imagenesMiniatura[slot].tex = NULL;
        imagenesMiniatura[slot].subtex = NULL;
    }
}

bool cargarMiniaturaFotoChat(int slot, long id, const std::string& ruta) {
    if (slot < 0 || slot >= 3 || id <= 0) return false;
    FILE* archivo = fopen(ruta.c_str(), "rb");
    if (!archivo) return false;
    u8 pixels[CHAT_FOTO_MINI_BYTES];
    bool leida = fread(pixels, 1, sizeof(pixels), archivo) == sizeof(pixels);
    leida = fclose(archivo) == 0 && leida;
    if (!leida) return false;

    const u32 anchoTextura = 128;
    const u32 altoTextura = 64;
    const u32 bytesTextura = anchoTextura * altoTextura * 2;
    if (!mosaicosFotoChat[slot]) mosaicosFotoChat[slot] = (u16*)linearAlloc(bytesTextura);
    if (!mosaicosFotoChat[slot]) return false;
    if (!fotoChatTexturaLista[slot]) {
        fotoChatTexturaLista[slot] = C3D_TexInit(&texturasFotoChat[slot],
            anchoTextura, altoTextura, GPU_RGB565);
        if (!fotoChatTexturaLista[slot]) return false;
        C3D_TexSetFilter(&texturasFotoChat[slot], GPU_LINEAR, GPU_LINEAR);
        subTexturasFotoChat[slot].width = CHAT_FOTO_MINI_ANCHO;
        subTexturasFotoChat[slot].height = CHAT_FOTO_MINI_ALTO;
        subTexturasFotoChat[slot].left = 0.0f;
        subTexturasFotoChat[slot].top = 1.0f;
        subTexturasFotoChat[slot].right = (float)CHAT_FOTO_MINI_ANCHO / anchoTextura;
        subTexturasFotoChat[slot].bottom = 1.0f - (float)CHAT_FOTO_MINI_ALTO / altoTextura;
        imagenesFotoChat[slot].tex = &texturasFotoChat[slot];
        imagenesFotoChat[slot].subtex = &subTexturasFotoChat[slot];
    }
    memset(mosaicosFotoChat[slot], 0, bytesTextura);
    const u16* origen = (const u16*)pixels;
    for (u32 y = 0; y < CHAT_FOTO_MINI_ALTO; ++y) {
        for (u32 x = 0; x < CHAT_FOTO_MINI_ANCHO; ++x) {
            u32 tile = (y / 8) * (anchoTextura / 8) + (x / 8);
            u32 destino = tile * 64 + indiceMorton(x & 7, y & 7);
            mosaicosFotoChat[slot][destino] = origen[y * CHAT_FOTO_MINI_ANCHO + x];
        }
    }
    memcpy(texturasFotoChat[slot].data, mosaicosFotoChat[slot], bytesTextura);
    C3D_TexFlush(&texturasFotoChat[slot]);
    idsFotoChat[slot] = id;
    return true;
}

long idMiniaturaFotoChat(int slot) {
    return slot >= 0 && slot < 3 && fotoChatTexturaLista[slot] ? idsFotoChat[slot] : 0;
}

C2D_Image obtenerMiniaturaFotoChat(int slot) {
    if (slot < 0 || slot >= 3) return C2D_Image();
    return imagenesFotoChat[slot];
}

void cerrarMiniaturasFotoChat() {
    for (int slot = 0; slot < 3; ++slot) {
        if (fotoChatTexturaLista[slot]) C3D_TexDelete(&texturasFotoChat[slot]);
        fotoChatTexturaLista[slot] = false;
        if (mosaicosFotoChat[slot]) linearFree(mosaicosFotoChat[slot]);
        mosaicosFotoChat[slot] = NULL;
        imagenesFotoChat[slot].tex = NULL;
        imagenesFotoChat[slot].subtex = NULL;
        idsFotoChat[slot] = 0;
    }
}
