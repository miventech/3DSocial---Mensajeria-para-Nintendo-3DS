#include "AppState.hpp"

// Nivel de socket y utilidades de conectividad. El protocolo HTTP y las
// operaciones de mensajes se implementan en MessagingV2.cpp.
void encenderLEDBlanco() {
    // Usa ctr-led-brary para indicar que el cliente está activo.
    RGBLedPattern pattern = LED::GeneratePattern(
        LED_Color(0xFF, 0xFF, 0xFF), // Blanco puro (R:255, G:255, B:255)
        LED_PatType::WAVE_DESC,
        2.0f,
        0.5f,
        0
    );
    LED::PlayLEDPattern(pattern);
}

void apagarLED() {
    // Detiene el patrón LED al salir o cuando la preferencia está desactivada.
    LED::StopLEDPattern();
}

// ⚡ CONEXIÓN CON TIMEOUT NO BLOQUEANTE
int conectarAlServidor() {
    // Abre un socket no bloqueante con tiempo límite para no congelar el bucle de la UI.
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_servidorPuerto);

    struct hostent *host = gethostbyname(g_servidorHost.c_str());
    if (host == NULL || host->h_addr_list[0] == NULL) {
        close(sock);
        return -1;
    }

    memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int resultado = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (resultado < 0 && errno != EINPROGRESS && errno != EALREADY && errno != EWOULDBLOCK) {
        close(sock);
        return -1;
    }
    if (resultado < 0) {
        bool conectado = false;
        for (int intento = 0; intento < 20 && !conectado; intento++) {
            fd_set escritura;
            FD_ZERO(&escritura);
            FD_SET(sock, &escritura);
            struct timeval espera = { 0, 100000 };
            int listo = select(sock + 1, NULL, &escritura, NULL, &espera);
            if (listo < 0) break;
            if (listo == 0) continue;

            // libctru puede conservar EINPROGRESS en SO_ERROR aun después de conectar.
            // Una segunda llamada a connect distingue EISCONN de una conexión fallida.
            int estado = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
            if (estado == 0 || errno == EISCONN) {
                conectado = true;
            } else if (errno != EINPROGRESS && errno != EALREADY && errno != EWOULDBLOCK) {
                break;
            }
        }
        if (!conectado) {
            close(sock);
            return -1;
        }
    }
    fcntl(sock, F_SETFL, flags);
    return sock;
}

std::string obtenerVersionRemota() {
    // Consulta GET /version y devuelve el texto simple que publica el servidor.
    RespuestaHttp respuesta;
    if (!pedirHttp("GET", "/version", NULL, "", respuesta, 512) || respuesta.estado != 200) return "";
    std::string version(respuesta.cuerpo.begin(), respuesta.cuerpo.end());
    version.erase(std::remove_if(version.begin(), version.end(),
        [](char c) { return c == '\r' || c == '\n' || c == ' ' || c == '\t'; }), version.end());
    return version;
}

bool comprobarActualizaciones() {
    // Compara la versión publicada con VERSION_ACTUAL y prepara el aviso inicial.
    std::string vRemota = obtenerVersionRemota();
    int rm = 0, rn = 0, rp = 0, lm = 0, ln = 0, lp = 0;
    bool versionesValidas = sscanf(vRemota.c_str(), "v%d.%d.%d", &rm, &rn, &rp) == 3 &&
                            sscanf(VERSION_ACTUAL.c_str(), "v%d.%d.%d", &lm, &ln, &lp) == 3;
    if (versionesValidas && (rm > lm || (rm == lm && rn > ln) ||
        (rm == lm && rn == ln && rp > lp))) {
        hayNuevaVersion = true;
        ultimaVersionDisponible = vRemota;
        return true;
    }
    return false;
}

