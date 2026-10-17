#include "AppState.hpp"
#include <3ds/services/ps.h>

// Composición raíz: los módulos se invocan desde este ciclo y comparten el
// estado declarado en AppState.hpp, sin crear bucles de interfaz separados.
int main(int argc, char **argv) {
    // Inicializa servicios 3DS, carga identidad/configuración y entra al bucle principal.
    gfxInitDefault();
    ptmuInit();

    socBuffer = (u32*)aligned_alloc(0x1000, 0x100000);
    if (socBuffer) socInit(socBuffer, 0x100000);
    g_cryptoDisponible = R_SUCCEEDED(psInit());

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    targetArriba = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    targetAbajo  = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    bufTexto = C2D_TextBufNew(4096);

    miID = obtenerOGenerarID();
    g_rutaFotoPerfil = "sdmc:/3ds/Mensinger3DS/perfil_" + miID + ".bgr";
    g_rutaFotoPerfilPendiente = "sdmc:/3ds/Mensinger3DS/perfil_pendiente_" + miID + ".bgr";
    cargarContactosSD();
    cargarAjustesSD();
    extraerCampanitaAuto();
    cargarSonidoCampanita();
    bool updateYaComprobado = false;

    u64 ultimoCheckTiempo = 0;

    while (aptMainLoop()) {
        // Cada vuelta lee controles, procesa red/estado y dibuja el estado activo.
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kNav = kDown | hidKeysDownRepeat();
        u32 kUp = hidKeysUp();
        touchPosition touch;
        hidTouchRead(&touch);

        if (procesarInicio(kDown, touch, updateYaComprobado)) continue;
        if (!procesarEntrada(kDown, kNav, kUp, touch, ultimoCheckTiempo)) break;
        if (g_hostSesion != g_servidorHost || g_puertoSesion != g_servidorPuerto ||
            g_cifradoSesion != g_cifrarConexion) {
            g_tokenSesion.clear();
            g_estadoInicio = INICIO_CONECTANDO;
            continue;
        }
        if (estadoActual == PANTALLA_CHAT) actualizarMiniaturasFotosChat();
        dibujarPantallaActual();
    }

    // Libera texturas y buffers antes de cerrar el RNG, sockets, gráficos, audio y
    // LED; así ningún recurso dependiente queda vivo al volver al menú HOME.
    cerrarFotoVista();
    cerrarMiniaturasContactos();
    cerrarMiniaturasFotoChat();
    if (bufTexto) C2D_TextBufDelete(bufTexto);
    C2D_Fini();
    C3D_Fini();
    if (g_cryptoDisponible) psExit();
    if (socBuffer) socExit();
    ptmuExit();
    gfxExit();
    cerrarNotasVoz();
    cerrarAudio();
    apagarLED();
    return 0;
}
