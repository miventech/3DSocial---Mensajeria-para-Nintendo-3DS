#pragma once

#include "MessagingV2.hpp"

// Sobre cifrado sobre HTTP; no envía ID, token, ruta ni contenido en claro.
bool pedirHttpSeguro(const std::string& metodo, const std::string& ruta,
                     const char* tipo, const std::string& archivo,
                     const std::string* datos, RespuestaHttp& respuesta,
                     size_t maxCuerpo);

// El borrado solo debe hacerse tras verificar fuera de banda que cambió el servidor.
bool olvidarClaveServidor();
