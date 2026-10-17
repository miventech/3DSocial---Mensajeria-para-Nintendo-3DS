#include "AppState.hpp"

// Tabla de idioma por índice TextoID. Mantener las dos filas alineadas con el
// enum en AppState.hpp evita que una clave termine mostrando otra traducción.
u8 obtenerIdiomaSistema() {
    // Si CFGU no responde se mantiene inglés, idioma inicial de respaldo.
    u8 lang = CFG_LANGUAGE_EN;
    if (R_SUCCEEDED(cfguInit())) {
        CFGU_GetSystemLanguage(&lang);
        cfguExit();
    }
    return lang;
}

const char* getText(TextoID id) {
    // Los IDs del enum indexan ambas tablas; todo texto persistente debe añadirse a las dos.
    static const char* TABLA[2][TOTAL_TEXTOS] = {
        // --- 0: ESPAÑOL ---
        {
            "Tu ID: ",
            "Toca un contacto abajo para abrir el chat.\nUsa las flechas o el D-Pad (Arriba/Abajo) para navegar.",
            "Lista de Contactos:",
            "Sin contactos. ¡Añade uno!",
            "+ Anadir contacto",
            "Aún no hay mensajes.\n¡Inicia la conversación!",
            "[Yo]:",
            "Escribir mensaje",
            "Volver a contactos",
            "Borrar",
            "Nueva version disponible: ",
            "Consulta el repositorio de 3DSocial de Miventech para descargar el archivo .cia.",
            "",
            "Conectando con el servidor...",
            "Buscando actualizaciones...",
            "No se ha podido conectar con el servidor.\nIgualmente podra usar la app sin conexion.",
            "Continuar",
            "Paso 1/2: Ingresa ID numerica del Amigo",
            "Paso 2/2: Ingresa el Nombre o Apodo",
            "Mensaje enviado",
            "No se ha podido enviar el mensaje",
            "Menú principal",
            "Pantalla de selección",
            "Contactos",
            "Tus contactos",
            "Configuración",
            "Ajustes de la app",
            "Modo oscuro",
            "LED de notificación",
            "Sonido de notificación",
            "Guardar y volver",
            "¿Borrar contacto?",
            "Se borrará de tu lista.\nEl historial guardado se conservará.",
            "Cancelar",
            "Borrar",
            "Tú",
            "Toca para escribir",
            "General",
            "Conexión",
            "Buscar actualizaciones",
            "Servidor",
            "Puerto",
            "Consultar mensajes",
            "Probar",
            "Restaurar",
            "Conexión correcta",
            "No se pudo conectar",
            "Servidor o puerto inválido",
            "HTTP con datos cifrados",
            "Anteriores",
            "Recientes",
            "Mantén para voz",
            "Grabando",
            "Reproducir",
            "Reintentar",
            "Audio no disponible",
            "B: cancelar",
            "B: descartar nota pendiente",
            "Cifrar datos",
        },
        // --- 1: INGLÉS ---
        {
            "Your ID: ",
            "Tap a contact below to open chat.\nUse D-Pad (Up/Down) to navigate.",
            "Contact List:",
            "No contacts. Add one!",
            "+ Add contact",
            "No messages yet.\nStart the conversation!",
            "[Me]:",
            "Write a message",
            "Back to contacts",
            "Delete",
            "New version available: ",
            "Visit Miventech's 3DSocial repository to download the .cia file.",
            "",
            "Connecting to server...",
            "Checking for updates...",
            "Unable to connect to server.\nYou can still use the app offline.",
            "Continue",
            "Step 1/2: Enter Friend's numeric ID",
            "Step 2/2: Enter Name or Nickname",
            "Message sent",
            "The message could not be sent",
            "Main menu",
            "Selection screen",
            "Contacts",
            "your contacts",
            "Configuration",
            "App settings",
            "Dark mode",
            "Notification LED",
            "Notification sound",
            "Save and return",
            "Delete contact?",
            "It will be deleted from your list.\nSaved chat history will remain.",
            "Cancel",
            "Delete",
            "You",
            "Tap to write",
            "General",
            "Connection",
            "Check for updates",
            "Server",
            "Port",
            "Check messages",
            "Test",
            "Reset",
            "Connection successful",
            "Could not connect",
            "Invalid server or port",
            "HTTP with encrypted data",
            "Older",
            "Latest",
            "Hold for voice",
            "Recording",
            "Play",
            "Retry",
            "Audio unavailable",
            "B: cancel",
            "B: discard pending note",
            "Encrypt data",
        }
    };

    u8 lang = obtenerIdiomaSistema();
    int idx = (lang == CFG_LANGUAGE_ES) ? 0 : 1;
    return TABLA[idx][id];
}

