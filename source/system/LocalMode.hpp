#ifndef LOCAL_MODE_HPP
#define LOCAL_MODE_HPP

#include <3ds.h>

// Inicializa los servicios de red local de la 3DS
bool initLocalMode();

// Bucle o actualización frame a frame del modo local
void updateLocalMode();

// Limpia el estado y cierra los servicios al salir
void exitLocalMode();

#endif // LOCAL_MODE_HPP