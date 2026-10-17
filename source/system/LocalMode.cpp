#include "LocalMode.hpp"
#include <stdio.h>

bool initLocalMode() {
    // Este modo aún es una pantalla de estado: no abre sockets ni descubre consolas.
    // Las secuencias ANSI colocan las instrucciones en la pantalla de consola libctru.
    // Aquí inicializaremos el socket local o servicio UDS de la 3DS más adelante
    printf("\x1b[10;2H[MODO LOCAL EN DESARROLLO]");
    printf("\x1b[12;2HPresiona B para regresar al menú.");
    return true;
}

void updateLocalMode() {
    // Aquí se actualizará la búsqueda local una vez implementado el transporte.
    // Lógica para buscar consolas o enviar mensajes en red local
}

void exitLocalMode() {
    // Reservado para cerrar los servicios que initLocalMode añada en el futuro.
    // Limpieza de memoria y servicios
}
