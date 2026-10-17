#pragma once

// Contrato de grabación/reproducción mono WAV. La llamada continuarNotaVoz se
// ejecuta durante la captura; detenerNotaVoz cierra el archivo y decide si
// conserva la grabación temporal para subirla.

#include <3ds.h>
#include <string>

// Prepara el micrófono y empieza a escribir una nota WAV en la ruta indicada.
bool iniciarNotaVoz(const std::string& archivo);
// Copia al archivo las muestras nuevas capturadas desde el último avance.
bool continuarNotaVoz();
// Finaliza la captura; conserva el WAV solo si conservar es true. Devuelve duración en ms o 0.
int detenerNotaVoz(bool conservar);
// Indica si hay una grabación abierta.
bool notaVozActiva();
// Duración transcurrida de la grabación activa, en milisegundos.
u64 duracionNotaVozMs();
// Carga y comienza a reproducir un WAV previamente guardado.
bool reproducirNotaVoz(const std::string& archivo);
// Actualiza el buffer de reproducción y detecta cuando termina.
void actualizarReproduccionVoz();
// Detiene grabación/reproducción y libera buffers y servicios de audio.
void cerrarNotasVoz();
