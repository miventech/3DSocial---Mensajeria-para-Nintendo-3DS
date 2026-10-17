#pragma once

// Contrato para cámara, visor y miniaturas RGB565. La captura se actualiza de
// forma asíncrona; el llamador detiene la sesión al abandonar el estado cámara.

#include <3ds.h>
#include <citro2d.h>
#include <string>

// Resolución que entrega la cámara y que espera el protocolo del servidor.
static const u16 FOTO_CAMARA_ANCHO = 400;
static const u16 FOTO_CAMARA_ALTO = 240;
static const u32 FOTO_CAMARA_BYTES = FOTO_CAMARA_ANCHO * FOTO_CAMARA_ALTO * 2;
// Miniaturas cuadradas usadas en la lista de contactos.
static const u16 PERFIL_MINIATURA_DIMENSION = 32;
static const u32 PERFIL_MINIATURA_BYTES = PERFIL_MINIATURA_DIMENSION * PERFIL_MINIATURA_DIMENSION * 2;
// Vista previa pequeña de fotos dentro de las burbujas del chat.
static const u16 CHAT_FOTO_MINI_ANCHO = 80;
static const u16 CHAT_FOTO_MINI_ALTO = 48;
static const u32 CHAT_FOTO_MINI_BYTES = CHAT_FOTO_MINI_ANCHO * CHAT_FOTO_MINI_ALTO * 2;

// Inicia captura asíncrona. Devuelve false si no se pudo configurar la cámara.
bool iniciarVistaCamara();
// Publica en la textura el fotograma completo más reciente, si hay uno nuevo.
bool actualizarVistaCamara();
// Indica si ya está disponible el primer fotograma de la sesión actual.
bool hayFotogramaCamara();
// Devuelve el código Result de CAMU para mostrar o registrar un fallo.
u32 obtenerCodigoErrorCamara();
// Guarda el fotograma actual en el formato RGB565 de 400×240.
bool guardarFotogramaCamara(const std::string& ruta);
// Detiene el hilo de captura y libera los servicios de cámara.
void detenerVistaCamara();
// Carga una foto de perfil RGB565 desde la SD para verla en pantalla.
bool cargarFotoVista(const std::string& ruta);
// Libera la textura de la foto de vista completa.
void cerrarFotoVista();
// Devuelve la imagen Citro2D actualmente preparada para dibujarse.
C2D_Image obtenerFotoVista();
// Carga en el slot 0..2 la miniatura RGB565 de un contacto.
bool cargarMiniaturaContacto(int slot, const std::string& ruta);
// Devuelve la imagen de miniatura preparada para un slot.
C2D_Image obtenerMiniaturaContacto(int slot);
// Libera todas las miniaturas y sus buffers de mosaico.
void cerrarMiniaturasContactos();
// Texturas independientes para hasta tres fotos de la página visible del chat.
bool cargarMiniaturaFotoChat(int slot, long id, const std::string& ruta);
long idMiniaturaFotoChat(int slot);
C2D_Image obtenerMiniaturaFotoChat(int slot);
void cerrarMiniaturasFotoChat();
