#include "AppState.hpp"
#include "sonido_campanita.h"

void extraerCampanitaAuto() {
    // Copia a la SD el sonido incluido si todavía no hay un archivo personalizado.
    mkdir("sdmc:/3ds/Mensinger3DS", 0777);

    FILE* check = fopen("sdmc:/3ds/Mensinger3DS/campanita.wav", "rb");
    if (check) {
        fclose(check);
        return;
    }

    FILE* f = fopen("sdmc:/3ds/Mensinger3DS/campanita.wav", "wb");
    if (f) {
        fwrite(sonido_campanita_wav, 1, sonido_campanita_wav_len, f);
        fclose(f);
    }
}

// --- CARGAR SONIDO ---
void cargarSonidoCampanita() {
    // Lee el WAV de aviso y configura el canal NDSP para reproducirlo una vez.
    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    FILE* f = fopen("sdmc:/3ds/Mensinger3DS/campanita.wav", "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    tamanoAudio = ftell(f) - 44;
    fseek(f, 44, SEEK_SET);

    bufferAudio = (u8*)linearAlloc(tamanoAudio);
    if (bufferAudio) {
        fread(bufferAudio, 1, tamanoAudio, f);
        DSP_FlushDataCache(bufferAudio, tamanoAudio);

        memset(&waveBuf, 0, sizeof(ndspWaveBuf));
        waveBuf.data_pcm16 = (int16_t*)bufferAudio;
        waveBuf.nsamples   = tamanoAudio / 2;
        waveBuf.looping    = false;
        waveBuf.status     = NDSP_WBUF_DONE; // Corregido el nombre de la constante
    }
    fclose(f);

    ndspChnReset(0);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
    ndspChnSetRate(0, 22050.0f);
}

// --- REPRODUCIR SONIDO ---
void reproducirCampanita() {
    // Añade el buffer al canal si el sonido está habilitado y se cargó correctamente.
    if (!g_sonidoActivado || !bufferAudio) return;
    ndspChnWaveBufAdd(0, &waveBuf);
}

// --- CERRAR AUDIO ---
void cerrarAudio() {
    // Libera NDSP y memoria lineal reservada para la campanita.
    ndspExit();
    if (bufferAudio) {
        linearFree(bufferAudio);
        bufferAudio = NULL;
    }
}

