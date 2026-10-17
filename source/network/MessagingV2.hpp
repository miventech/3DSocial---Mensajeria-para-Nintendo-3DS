#pragma once

// Contrato de transporte del cliente. Las respuestas se limitan por maxCuerpo;
// las descargas se reciben en memoria y luego se guardan en disco.

#include <3ds.h>
#include <string>
#include <vector>

struct MensajeV2 {
    // Identificador persistente asignado por el servidor.
    long id = 0;
    // ID del remitente y texto; para medios, texto puede ser una etiqueta vacía.
    std::string remitente;
    std::string texto;
    // Duración solo para audio; las banderas distinguen audio y foto de texto.
    int duracionMs = 0;
    bool audio = false;
    bool foto = false;
};

struct RespuestaHttp {
    // Código HTTP recibido y bytes del cuerpo, que puede ser texto o medio binario.
    int estado = 0;
    std::vector<u8> cuerpo;
};

// Olvida la clave pública fijada para el servidor actual tras una rotación explícita.
bool olvidarClaveServidor();

// Ejecuta una solicitud y, si archivo no está vacío, envía ese archivo como cuerpo;
// el cuerpo de respuesta queda en memoria hasta el límite indicado.
bool pedirHttp(const std::string& metodo, const std::string& ruta, const char* tipo,
               const std::string& archivo, RespuestaHttp& respuesta, size_t maxCuerpo);
// Envía texto/binario en memoria y recoge una respuesta HTTP acotada.
bool pedirHttpDatos(const std::string& metodo, const std::string& ruta, const char* tipo,
                    const std::string& datos, RespuestaHttp& respuesta, size_t maxCuerpo);
// Lee una página anterior o reciente del historial con cursor antes y límite.
bool pedirHistorial(const std::string& propio, const std::string& amigo, long antes,
                    std::vector<MensajeV2>& mensajes, int& estado);
// Sube la nota WAV y devuelve el ID del mensaje creado.
bool enviarAudio(const std::string& propio, const std::string& amigo,
                 const std::string& archivo, long& idMensaje);
// Descarga el WAV identificado y lo guarda en la ruta de caché.
bool descargarAudio(long idMensaje, const std::string& archivo);
// Sube la foto RGB565 capturada y devuelve el ID del mensaje creado.
bool enviarFoto(const std::string& propio, const std::string& amigo,
                const std::string& archivo, long& idMensaje);
// Descarga la foto RGB565 identificada y la guarda en la ruta de caché.
bool descargarFoto(long idMensaje, const std::string& archivo);
// Descarga solo la vista previa 80×48 del mensaje de foto.
bool descargarMiniaturaFoto(long idMensaje, const std::string& archivo);
