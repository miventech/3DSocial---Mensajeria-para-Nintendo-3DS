#include "MessagingV2.hpp"
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <sstream>
#include <cstring>

extern int conectarAlServidor();
extern std::string hostHttp();

static bool enviarTodo(int sock, const void* datos, size_t longitud) {
    // send() puede aceptar solo una parte; repetir hasta enviar el bloque completo.
    const u8* actual = static_cast<const u8*>(datos);
    while (longitud > 0) {
        int enviados = send(sock, actual, longitud, 0);
        if (enviados <= 0) return false;
        actual += enviados;
        longitud -= enviados;
    }
    return true;
}

static bool esperarLectura(int sock) {
    // Evita que una respuesta HTTP silenciosa bloquee indefinidamente la interfaz.
    fd_set lectura;
    FD_ZERO(&lectura);
    FD_SET(sock, &lectura);
    struct timeval espera = {5, 0};
    return select(sock + 1, &lectura, NULL, NULL, &espera) > 0;
}

static bool pedirHttpInterno(const std::string& metodo, const std::string& ruta, const char* tipo,
                            const std::string& archivo, const std::string* datos,
                            RespuestaHttp& respuesta, size_t maxCuerpo) {
    // Centraliza envíos desde archivo o memoria y limita el cuerpo recibido.
    respuesta = RespuestaHttp();
    FILE* entrada = NULL;
    long longitud = 0;
    if (!archivo.empty()) {
        entrada = fopen(archivo.c_str(), "rb");
        if (!entrada) return false;
        if (fseek(entrada, 0, SEEK_END) != 0 || (longitud = ftell(entrada)) < 0 ||
            fseek(entrada, 0, SEEK_SET) != 0) {
            fclose(entrada);
            return false;
        }
    }
    if (datos) longitud = datos->size();

    int sock = conectarAlServidor();
    if (sock < 0) { if (entrada) fclose(entrada); return false; }
    std::string cabecera = metodo + " " + ruta + " HTTP/1.1\r\nHost: " + hostHttp() +
        "\r\nConnection: close\r\n";
    if (entrada || datos) cabecera += std::string("Content-Type: ") + tipo + "\r\nContent-Length: " +
        std::to_string(longitud) + "\r\n";
    cabecera += "\r\n";
    bool correcto = enviarTodo(sock, cabecera.data(), cabecera.size());
    if (correcto && datos) correcto = enviarTodo(sock, datos->data(), datos->size());
    u8 bloque[4096];
    while (correcto && entrada) {
        size_t leidos = fread(bloque, 1, sizeof(bloque), entrada);
        if (leidos == 0) { correcto = !ferror(entrada); break; }
        correcto = enviarTodo(sock, bloque, leidos);
    }
    if (entrada) fclose(entrada);

    std::vector<u8> recibido;
    size_t finCabecera = std::string::npos;
    long longitudCuerpo = -1;
    // Lee hasta completar Content-Length o hasta que el servidor cierre la conexión.
    while (correcto && esperarLectura(sock)) {
        int leidos = recv(sock, bloque, sizeof(bloque), 0);
        if (leidos < 0) { correcto = false; break; }
        if (leidos == 0) break;
        recibido.insert(recibido.end(), bloque, bloque + leidos);
        if (finCabecera == std::string::npos) {
            if (recibido.size() > 8192) { correcto = false; break; }
            const char* inicio = reinterpret_cast<const char*>(recibido.data());
            const char* fin = std::search(inicio, inicio + recibido.size(), "\r\n\r\n", "\r\n\r\n" + 4);
            if (fin != inicio + recibido.size()) {
                finCabecera = fin - inicio + 4;
                std::string cab(inicio, finCabecera);
                if (sscanf(cab.c_str(), "HTTP/%*d.%*d %d", &respuesta.estado) != 1) { correcto = false; break; }
                size_t clave = cab.find("Content-Length:");
                if (clave != std::string::npos) longitudCuerpo = strtol(cab.c_str() + clave + 15, NULL, 10);
            }
        }
        if (finCabecera != std::string::npos) {
            if (recibido.size() - finCabecera > maxCuerpo) { correcto = false; break; }
            if (longitudCuerpo >= 0 && recibido.size() - finCabecera >= (size_t)longitudCuerpo) break;
        }
    }
    close(sock);
    if (!correcto || finCabecera == std::string::npos ||
        (longitudCuerpo >= 0 && recibido.size() - finCabecera < (size_t)longitudCuerpo)) return false;
    size_t longitudFinal = longitudCuerpo >= 0 ? (size_t)longitudCuerpo : recibido.size() - finCabecera;
    respuesta.cuerpo.assign(recibido.begin() + finCabecera,
                           recibido.begin() + finCabecera + longitudFinal);
    return true;
}

bool pedirHttp(const std::string& metodo, const std::string& ruta, const char* tipo,
               const std::string& archivo, RespuestaHttp& respuesta, size_t maxCuerpo) {
    return pedirHttpInterno(metodo, ruta, tipo, archivo, NULL, respuesta, maxCuerpo);
}

bool pedirHttpDatos(const std::string& metodo, const std::string& ruta, const char* tipo,
                    const std::string& datos, RespuestaHttp& respuesta, size_t maxCuerpo) {
    return pedirHttpInterno(metodo, ruta, tipo, "", &datos, respuesta, maxCuerpo);
}

bool pedirHistorial(const std::string& propio, const std::string& amigo, long antes,
                    std::vector<MensajeV2>& mensajes, int& estado) {
    // El protocolo TSV tiene cinco columnas: id, remitente, tipo, duración y texto.
    RespuestaHttp respuesta;
    std::string ruta = "/historial/" + propio + "/" + amigo + "?limit=20&before=" + std::to_string(antes);
    if (!pedirHttp("GET", ruta, NULL, "", respuesta, 16384)) return false;
    estado = respuesta.estado;
    if (estado != 200) return false;
    std::string cuerpo(respuesta.cuerpo.begin(), respuesta.cuerpo.end());
    std::istringstream lineas(cuerpo);
    std::string linea;
    mensajes.clear();
    while (std::getline(lineas, linea)) {
        size_t pos[4];
        size_t desde = 0;
        for (int i = 0; i < 4; i++) {
            pos[i] = linea.find('\t', desde);
            if (pos[i] == std::string::npos) return false;
            desde = pos[i] + 1;
        }
        MensajeV2 mensaje;
        mensaje.id = strtol(linea.c_str(), NULL, 10);
        mensaje.remitente = linea.substr(pos[0] + 1, pos[1] - pos[0] - 1);
        std::string tipo = linea.substr(pos[1] + 1, pos[2] - pos[1] - 1);
        mensaje.audio = tipo == "audio";
        mensaje.foto = tipo == "foto";
        mensaje.duracionMs = atoi(linea.substr(pos[2] + 1, pos[3] - pos[2] - 1).c_str());
        mensaje.texto = linea.substr(pos[3] + 1);
        if (mensaje.id <= 0 || mensaje.remitente.empty()) return false;
        mensajes.push_back(mensaje);
    }
    return true;
}

bool enviarAudio(const std::string& propio, const std::string& amigo,
                 const std::string& archivo, long& idMensaje) {
    // Una respuesta 200 contiene el ID numérico de la nueva fila de mensajes.
    RespuestaHttp respuesta;
    if (!pedirHttp("POST", "/audio/" + propio + "/" + amigo, "audio/wav", archivo, respuesta, 128) ||
        respuesta.estado != 200) return false;
    std::string cuerpo(respuesta.cuerpo.begin(), respuesta.cuerpo.end());
    idMensaje = strtol(cuerpo.c_str(), NULL, 10);
    return idMensaje > 0;
}

bool descargarAudio(long idMensaje, const std::string& archivo) {
    // El límite coincide con 30 segundos de PCM mono más el encabezado WAV.
    RespuestaHttp respuesta;
    if (!pedirHttp("GET", "/audio/" + std::to_string(idMensaje), NULL, "", respuesta, 654644) ||
        respuesta.estado != 200 || respuesta.cuerpo.size() < 44) return false;
    FILE* salida = fopen(archivo.c_str(), "wb");
    if (!salida) return false;
    bool correcto = fwrite(respuesta.cuerpo.data(), 1, respuesta.cuerpo.size(), salida) == respuesta.cuerpo.size();
    correcto = fclose(salida) == 0 && correcto;
    if (!correcto) remove(archivo.c_str());
    return correcto;
}

bool enviarFoto(const std::string& propio, const std::string& amigo,
                const std::string& archivo, long& idMensaje) {
    // El cliente envía RGB565 crudo; el servidor lo conserva en ese formato.
    RespuestaHttp respuesta;
    if (!pedirHttp("POST", "/foto/" + propio + "/" + amigo,
                   "application/x-mensinger-bgr565", archivo, respuesta, 128) ||
        respuesta.estado != 200) return false;
    std::string cuerpo(respuesta.cuerpo.begin(), respuesta.cuerpo.end());
    idMensaje = strtol(cuerpo.c_str(), NULL, 10);
    return idMensaje > 0;
}

bool descargarFoto(long idMensaje, const std::string& archivo) {
    // Una foto completa ocupa exactamente 400 × 240 píxeles × 2 bytes.
    const size_t bytesEsperados = 400 * 240 * 2;
    RespuestaHttp respuesta;
    if (!pedirHttp("GET", "/foto/" + std::to_string(idMensaje), NULL, "", respuesta,
                   bytesEsperados) || respuesta.estado != 200 ||
        respuesta.cuerpo.size() != bytesEsperados) return false;
    FILE* salida = fopen(archivo.c_str(), "wb");
    if (!salida) return false;
    bool correcto = fwrite(respuesta.cuerpo.data(), 1, respuesta.cuerpo.size(), salida) ==
                    respuesta.cuerpo.size();
    correcto = fclose(salida) == 0 && correcto;
    if (!correcto) remove(archivo.c_str());
    return correcto;
}
