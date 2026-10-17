#include "HttpSecure.hpp"
#include "monocypher.h"
#include <3ds/services/ps.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

extern int conectarAlServidor();
extern std::string hostHttp();
extern std::string miID;
extern std::string g_tokenSesion;
extern std::string g_hostSesion;
extern int g_puertoSesion;
extern bool g_cifradoSesion;
extern std::string g_servidorHost;
extern int g_servidorPuerto;
extern bool g_cifrarConexion;
extern bool g_claveServidorError;
extern bool g_cryptoDisponible;

static const char KDF_LABEL[] = "Mensinger3DS-channel-v1";
static const u8 REQUEST_AAD[] = {'M','3','D','S','1','Q'};
static const u8 RESPONSE_AAD[] = {'M','3','D','S','1','R'};
static const size_t MAX_BODY = 720000;

static std::string rutaClave() {
    return "sdmc:/3ds/Mensinger3DS/server-key-" + g_servidorHost + "-" +
           std::to_string(g_servidorPuerto) + ".bin";
}

bool olvidarClaveServidor() {
    return remove(rutaClave().c_str()) == 0;
}

static bool verificarOGuardarClave(const u8 clave[32]) {
    // Un archivo existente, aunque corrupto, nunca se sustituye silenciosamente.
    std::string ruta = rutaClave();
    FILE* entrada = fopen(ruta.c_str(), "rb");
    if (entrada) {
        u8 guardada[33];
        size_t bytes = fread(guardada, 1, sizeof(guardada), entrada);
        fclose(entrada);
        bool coincide = bytes == 32 && crypto_verify32(guardada, clave) == 0;
        g_claveServidorError = !coincide;
        return coincide;
    }
    g_claveServidorError = true;
    std::string temporal = ruta + ".tmp";
    FILE* salida = fopen(temporal.c_str(), "wb");
    if (!salida) return false;
    bool valido = fwrite(clave, 1, 32, salida) == 32;
    valido = fclose(salida) == 0 && valido;
    if (!valido || rename(temporal.c_str(), ruta.c_str()) != 0) {
        remove(temporal.c_str());
        return false;
    }
    g_claveServidorError = false;
    return true;
}

static bool enviarTodo(int socket, const void* datos, size_t longitud) {
    const u8* actual = static_cast<const u8*>(datos);
    while (longitud) {
        int enviados = send(socket, actual, longitud, 0);
        if (enviados <= 0) return false;
        actual += enviados;
        longitud -= enviados;
    }
    return true;
}

static bool httpBinario(const char* metodo, const char* ruta,
                        const std::vector<u8>& cuerpo, RespuestaHttp& respuesta,
                        size_t limite) {
    int socket = conectarAlServidor();
    if (socket < 0) return false;
    std::string cabecera = std::string(metodo) + " " + ruta + " HTTP/1.1\r\nHost: " +
        hostHttp() + "\r\nConnection: close\r\n";
    if (!cuerpo.empty()) cabecera += "Content-Type: application/octet-stream\r\nContent-Length: " +
        std::to_string(cuerpo.size()) + "\r\n";
    cabecera += "\r\n";
    bool bien = enviarTodo(socket, cabecera.data(), cabecera.size()) &&
                (cuerpo.empty() || enviarTodo(socket, cuerpo.data(), cuerpo.size()));
    std::vector<u8> recibido;
    size_t finCabecera = std::string::npos;
    long longitudCuerpo = -1;
    u8 bloque[4096];
    while (bien) {
        fd_set disponibles;
        FD_ZERO(&disponibles);
        FD_SET(socket, &disponibles);
        timeval espera = {5, 0};
        if (select(socket + 1, &disponibles, NULL, NULL, &espera) <= 0) { bien = false; break; }
        int n = recv(socket, bloque, sizeof(bloque), 0);
        if (n < 0) { bien = false; break; }
        if (n == 0) break;
        recibido.insert(recibido.end(), bloque, bloque + n);
        if (finCabecera == std::string::npos) {
            if (recibido.size() > 8192) { bien = false; break; }
            const char* principio = reinterpret_cast<const char*>(recibido.data());
            const char* fin = std::search(principio, principio + recibido.size(),
                                          "\r\n\r\n", "\r\n\r\n" + 4);
            if (fin != principio + recibido.size()) {
                finCabecera = fin - principio + 4;
                std::string cab(principio, finCabecera);
                if (sscanf(cab.c_str(), "HTTP/%*d.%*d %d", &respuesta.estado) != 1) {
                    bien = false; break;
                }
                size_t clave = cab.find("Content-Length:");
                if (clave != std::string::npos)
                    longitudCuerpo = strtol(cab.c_str() + clave + 15, NULL, 10);
            }
        }
        if (finCabecera != std::string::npos) {
            if (recibido.size() - finCabecera > limite ||
                (longitudCuerpo >= 0 && (size_t)longitudCuerpo > limite)) {
                bien = false; break;
            }
            if (longitudCuerpo >= 0 && recibido.size() - finCabecera >= (size_t)longitudCuerpo)
                break;
        }
    }
    close(socket);
    if (!bien || finCabecera == std::string::npos || longitudCuerpo < 0 ||
        recibido.size() - finCabecera < (size_t)longitudCuerpo) return false;
    respuesta.cuerpo.assign(recibido.begin() + finCabecera,
                           recibido.begin() + finCabecera + longitudCuerpo);
    return true;
}

static void agregarU16(std::vector<u8>& v, size_t numero) {
    v.push_back((u8)(numero >> 8));
    v.push_back((u8)numero);
}

static void agregarU32(std::vector<u8>& v, size_t numero) {
    v.push_back((u8)(numero >> 24));
    v.push_back((u8)(numero >> 16));
    v.push_back((u8)(numero >> 8));
    v.push_back((u8)numero);
}

static void agregarTexto(std::vector<u8>& v, const std::string& texto) {
    v.insert(v.end(), texto.begin(), texto.end());
}

bool pedirHttpSeguro(const std::string& metodo, const std::string& ruta,
                     const char* tipo, const std::string& archivo,
                     const std::string* datos, RespuestaHttp& respuesta,
                     size_t maxCuerpo) {
    respuesta = RespuestaHttp();
    if (!g_cryptoDisponible || (metodo != "GET" && metodo != "POST") ||
        ruta.empty() || ruta.size() > 1024) return false;
    // El único GET visible contiene clave pública y desafío; ningún dato de
    // usuario sale antes de comparar la clave con la guardada en la SD.
    RespuestaHttp inicio;
    if (!httpBinario("GET", "/clave-servidor", {}, inicio, 52) ||
        inicio.estado != 200 || inicio.cuerpo.size() != 52 ||
        memcmp(inicio.cuerpo.data(), "M3K1", 4) != 0 ||
        !verificarOGuardarClave(inicio.cuerpo.data() + 4)) {
        g_claveServidorError = true;
        return false;
    }
    const u8* claveServidor = inicio.cuerpo.data() + 4;
    const u8* desafio = inicio.cuerpo.data() + 36;
    std::string tipoTexto = tipo ? tipo : "";
    std::string token = (!g_tokenSesion.empty() && g_hostSesion == g_servidorHost &&
        g_puertoSesion == g_servidorPuerto && g_cifradoSesion == g_cifrarConexion) ?
        g_tokenSesion : "";
    std::vector<u8> contenido;
    if (datos) contenido.assign(datos->begin(), datos->end());
    else if (!archivo.empty()) {
        FILE* entrada = fopen(archivo.c_str(), "rb");
        if (!entrada) return false;
        long longitud = 0;
        bool valido = fseek(entrada, 0, SEEK_END) == 0 &&
            (longitud = ftell(entrada)) >= 0 && (size_t)longitud <= MAX_BODY &&
            fseek(entrada, 0, SEEK_SET) == 0;
        if (valido) {
            contenido.resize((size_t)longitud);
            valido = fread(contenido.data(), 1, contenido.size(), entrada) == contenido.size();
        }
        fclose(entrada);
        if (!valido) return false;
    }
    if (contenido.size() > MAX_BODY || tipoTexto.size() > 96 ||
        miID.size() > 10 || token.size() > 128) return false;
    std::vector<u8> plano;
    plano.reserve(10 + ruta.size() + tipoTexto.size() + miID.size() + token.size() +
                  contenido.size());
    plano.push_back(metodo == "GET" ? 0 : 1);
    agregarU16(plano, ruta.size());
    plano.push_back((u8)tipoTexto.size());
    plano.push_back((u8)miID.size());
    plano.push_back((u8)token.size());
    agregarU32(plano, contenido.size());
    agregarTexto(plano, ruta);
    agregarTexto(plano, tipoTexto);
    agregarTexto(plano, miID);
    agregarTexto(plano, token);
    plano.insert(plano.end(), contenido.begin(), contenido.end());
    if (!contenido.empty()) crypto_wipe(contenido.data(), contenido.size());
    if (!token.empty()) std::fill(token.begin(), token.end(), '\0');

    u8 secreto[32], clavePublica[32], compartido[32], clave[32];
    if (R_FAILED(PS_GenerateRandomBytes(secreto, sizeof(secreto)))) {
        crypto_wipe(plano.data(), plano.size());
        return false;
    }
    crypto_x25519_public_key(clavePublica, secreto);
    crypto_x25519(compartido, secreto, claveServidor);
    crypto_wipe(secreto, sizeof(secreto));
    const u8 cero[32] = {};
    if (crypto_verify32(compartido, cero) == 0) {
        crypto_wipe(compartido, 32);
        crypto_wipe(plano.data(), plano.size());
        return false;
    }
    std::vector<u8> material;
    material.insert(material.end(), compartido, compartido + 32);
    material.insert(material.end(), clavePublica, clavePublica + 32);
    material.insert(material.end(), claveServidor, claveServidor + 32);
    material.insert(material.end(), desafio, desafio + 16);
    material.insert(material.end(), KDF_LABEL, KDF_LABEL + strlen(KDF_LABEL));
    crypto_blake2b(clave, 32, material.data(), material.size());
    crypto_wipe(compartido, 32);
    crypto_wipe(material.data(), material.size());
    const u8 noncePeticion[12] = {};
    u8 nonceRespuesta[12] = {};
    nonceRespuesta[11] = 1;
    std::vector<u8> sobre;
    sobre.insert(sobre.end(), {'M','3','Q','1'});
    sobre.insert(sobre.end(), desafio, desafio + 16);
    sobre.insert(sobre.end(), clavePublica, clavePublica + 32);
    size_t lugarMac = sobre.size();
    sobre.resize(sobre.size() + 16);
    size_t lugarCifrado = sobre.size();
    sobre.resize(lugarCifrado + plano.size());
    crypto_aead_ctx contexto;
    crypto_aead_init_ietf(&contexto, clave, noncePeticion);
    crypto_aead_write(&contexto, sobre.data() + lugarCifrado, sobre.data() + lugarMac,
                      REQUEST_AAD, sizeof(REQUEST_AAD), plano.data(), plano.size());
    crypto_wipe(&contexto, sizeof(contexto));
    crypto_wipe(plano.data(), plano.size());
    RespuestaHttp externa;
    bool bien = httpBinario("POST", "/canal", sobre, externa, maxCuerpo + 26);
    if (!bien || externa.estado != 200 || externa.cuerpo.size() < 26 ||
        memcmp(externa.cuerpo.data(), "M3R1", 4) != 0) {
        crypto_wipe(clave, 32);
        return false;
    }
    size_t cifrado = externa.cuerpo.size() - 20;
    std::vector<u8> abierto(cifrado);
    crypto_aead_init_ietf(&contexto, clave, nonceRespuesta);
    bien = crypto_aead_read(&contexto, abierto.data(), externa.cuerpo.data() + 4,
                            RESPONSE_AAD, sizeof(RESPONSE_AAD),
                            externa.cuerpo.data() + 20, cifrado) == 0;
    crypto_wipe(&contexto, sizeof(contexto));
    crypto_wipe(clave, 32);
    if (!bien || abierto.size() < 6) {
        if (!abierto.empty()) crypto_wipe(abierto.data(), abierto.size());
        return false;
    }
    int estado = ((int)abierto[0] << 8) | abierto[1];
    size_t longitud = ((size_t)abierto[2] << 24) | ((size_t)abierto[3] << 16) |
                      ((size_t)abierto[4] << 8) | abierto[5];
    if (longitud != abierto.size() - 6 || longitud > maxCuerpo || estado < 100 || estado > 599) {
        crypto_wipe(abierto.data(), abierto.size());
        return false;
    }
    respuesta.estado = estado;
    respuesta.cuerpo.assign(abierto.begin() + 6, abierto.end());
    crypto_wipe(abierto.data(), abierto.size());
    return true;
}
