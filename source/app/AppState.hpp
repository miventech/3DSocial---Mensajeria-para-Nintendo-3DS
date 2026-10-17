#pragma once
// Interfaz compartida del cliente: modelos, estados globales y contratos entre
// pantallas, almacenamiento, red y medios. El almacenamiento de los `extern`
// vive en AppState.cpp; los headers específicos exponen las APIs de medios/red.
#include <3ds.h>
#include <citro2d.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>
#include <sstream>
#include <cctype>
#include "ctr-led-brary.hpp"
#include <netdb.h>
#include <cmath>
#include "MessagingV2.hpp"
#include "VoiceNotes.hpp"
#include "CameraPhotos.hpp"


// Estado de navegación principal. FOTO es el visor; cámara y confirmación son
// estados distintos para separar captura, revisión y envío explícito.
enum EstadoApp {
    PANTALLA_CONTACTOS,
    PANTALLA_CHAT,
    PANTALLA_FOTO,
    PANTALLA_CAMARA,
    PANTALLA_CONFIRMAR_FOTO,
    PANTALLA_CONFIGURACION,
    PANTALLA_PERFIL,
    PANTALLA_BUSQUEDA,
    PANTALLA_CONOCER,
    PANTALLA_EXTRANOS
};
// Máquina de estados previa a la interfaz: servidor, acceso, actualización o error.
enum EstadoInicio {
    // Secuencia visible al abrir: conexión, búsqueda opcional de versión y acceso a contactos.
    INICIO_CONECTANDO,
    INICIO_BUSCANDO_UPDATE,
    INICIO_ERROR_CONEXION,
    INICIO_UPDATE_DISPONIBLE,
    INICIO_COMPLETADO,
    INICIO_ERROR_ACCESO
};
// Resumen devuelto por /buscar para mostrar una tarjeta y agregar contacto.
struct ResultadoBusqueda {
    std::string id, nombre, apodo, bio;
};
// Conversaciones recibidas de IDs que no están en la lista local de contactos.
struct RemitenteDesconocido {
    std::string id;
    int pendientes = 0;
    int total = 0;
    long ultimoId = 0;
};
// ID/nombre locales más perfil, miniatura y estilo sincronizados del servidor.
struct Contacto {
    // Los campos base se guardan en la SD; los campos Perfil_* se refrescan desde el servidor.
    std::string id;
    std::string nombre;
    bool tieneNovedad = false;
    int pendientes = 0;
    bool conectado = false;
    std::string usuarioPerfil;
    std::string apodoPerfil;
    std::string bioPerfil;
    std::string descripcionPerfil;
    bool tienePerfil = false;
    bool tieneFotoPerfil = false;
    bool tieneMiniatura = false;
    int colorChat = 0;
    int colorFondo = 0;
    int colorTexto = 0;
    int colorApodo = 0;
    int colorNombre = 0;
};
// Resultado visual temporal de una acción de envío.
enum EstadoEnvio { ENVIO_NINGUNO, ENVIO_EXITO, ENVIO_ERROR };
// Índices estables de las tablas bilingües en Localization.cpp.
enum TextoID {
    TXT_TU_ID,
    TXT_INSTRUCCIONES_MAIN,
    TXT_LISTA_CONTACTOS,
    TXT_SIN_CONTACTOS,
    TXT_BTN_ADD_CONTACTO,
    TXT_SIN_MENSAJES,
    TXT_PREFIX_YO,
    TXT_BTN_ESCRIBIR,
    TXT_BTN_VOLVER,
    TXT_BTN_BORRAR,
    TXT_AVISO_UPDATE,
    TXT_INFO_UPDATE,
    TXT_BTN_UPDATE,
    TXT_CONECTANDO,
    TXT_BUSCANDO_UPDATE,
    TXT_ERROR_CONEXION,
    TXT_BOTON_CONTINUAR,
    TXT_PASO_1,
    TXT_PASO_2,
    TXT_ENVIADO,
    TXT_ERROR,
    TXT_MENU,
    TXT_INFO,
    TXT_ONLINE,
    TXT_CONTACTOS,
    TXT_CONFIG,
    TXT_AJUSTES,
    TXT_OSCURO,
    TXT_LED,
    TXT_SONIDO,
    TXT_GUARDAR,
    TXT_CONFIRMAR_BORRAR_TITULO,
    TXT_CONFIRMAR_BORRAR_CUERPO,
    TXT_CONFIRMAR_CANCELAR,
    TXT_CONFIRMAR_BORRAR,
    TXT_TU,
    TXT_AYUDA_ESCRIBIR,
    TXT_GENERAL,
    TXT_CONEXION,
    TXT_ACTUALIZACIONES,
    TXT_SERVIDOR,
    TXT_PUERTO,
    TXT_INTERVALO,
    TXT_PROBAR,
    TXT_RESTAURAR,
    TXT_CONEXION_OK,
    TXT_CONEXION_ERROR,
    TXT_CONFIG_INVALIDA,
    TXT_HTTP_INFO,
    TXT_ANTERIORES,
    TXT_RECIENTES,
    TXT_GRABAR_VOZ,
    TXT_GRABANDO_VOZ,
    TXT_REPRODUCIR,
    TXT_REINTENTAR,
    TXT_AUDIO_NO_DISPONIBLE,
    TXT_CANCELAR_VOZ,
    TXT_DESCARTAR_VOZ,
    TXT_CIFRAR_CONEXION,
    TOTAL_TEXTOS
};

// Valores de conexión y versión compilados como predeterminados.
extern const char* const SERVIDOR_PREDETERMINADO;
extern const int PUERTO_PREDETERMINADO;
// Estado del aviso de actualización durante el arranque.
extern bool comprobadoUpdate;
extern bool hayNuevaVersion;
extern std::string ultimaVersionDisponible;
extern const std::string VERSION_ACTUAL;
// Preferencias editables que se guardan en config.ini.
extern bool g_modoOscuro;
extern bool g_ledActivado;
extern bool g_sonidoActivado;
extern bool g_buscarActualizaciones;
extern std::string g_servidorHost;
extern bool g_claveServidorError;
extern bool g_cryptoDisponible;
extern bool g_cifrarConexion;
extern int g_servidorPuerto;
extern int g_intervaloConsultaMs;
extern int g_pestanaConfig;
extern int g_estadoPruebaConexion;
extern bool g_errorConfig;
// Recursos globales de audio legacy/NDSP y buffers gráficos/sockets.
extern u8* bufferAudio;
extern u32 tamanoAudio;
extern ndspWaveBuf waveBuf;
// Navegación y recursos inicializados/liberados desde main.cpp.
extern EstadoApp estadoActual;
extern EstadoApp pantallaRetornoConfig;
extern EstadoInicio g_estadoInicio;
extern float g_anguloCarga;
extern u32* socBuffer;
extern C2D_TextBuf bufTexto;
extern C3D_RenderTarget* targetArriba;
extern C3D_RenderTarget* targetAbajo;
// Identidad local y sesión remota. El token se invalida si cambia host, puerto
// o modo; la contraseña nunca se persiste aquí en texto plano.
extern std::string miID;
extern std::string g_tokenSesion;
extern std::string g_hostSesion;
extern int g_puertoSesion;
extern bool g_cifradoSesion;
// Perfil propio y colores que otros clientes aplican al mostrar sus mensajes.
extern bool g_contrasenaActiva;
extern bool g_mostrarEnBusqueda;
extern bool g_quieroConocerGente;
extern int g_colorChat;
extern int g_colorFondo;
extern int g_colorTexto;
extern int g_colorApodo;
extern int g_colorNombre;
extern int g_scrollPerfil;
extern int g_slotMiniaturaChat;
// Resultados y scroll de las pantallas Buscar y Conocer gente.
extern std::vector<ResultadoBusqueda> g_resultadosBusqueda;
extern std::string g_consultaBusqueda;
extern bool g_modoConocer;
extern int g_scrollBusqueda;
// Lista local de contactos y selección del chat abierto.
extern std::string contactoActivoID;
extern std::string contactoActivoNombre;
extern std::vector<Contacto> listaContactos;
extern std::vector<RemitenteDesconocido> g_desconocidos;
extern int g_scrollDesconocidos;
extern int g_seleccionDesconocidos;
extern EstadoApp g_pantallaRetornoChat;
extern long g_ultimoAvisoRecibido;
extern u64 g_ultimoLatidoPresencia;
extern u64 g_ultimaConsultaPresencias;
extern std::vector<std::string> historialChat;
extern bool g_confirmarBorrarContacto;
// Historial paginado moderno. Los IDs permiten fusionar páginas y animar una
// llegada específica sin depender de la posición que el usuario está viendo.
extern std::vector<MensajeV2> g_historialV2;
extern std::vector<long> g_idsMensajesVisibles;
extern bool g_servidorV2;
extern bool g_v2Determinado;
extern bool g_masAntiguos;
extern int g_desplazamientoChat;
extern int g_totalMensajesChat;
// Medio seleccionado y archivos locales pendientes o fallidos de enviar.
extern long g_audioSeleccionadoId;
extern bool g_audioPendiente;
extern std::string g_rutaAudioPendiente;
extern bool g_audioError;
extern long g_fotoSeleccionadaId;
extern bool g_fotoPendiente;
extern std::string g_rutaFotoPendiente;
extern bool g_fotoError;
// Campos de perfil, rutas de foto y foco del editor desplazable.
extern std::string g_perfilNombre;
extern std::string g_perfilApodo;
extern std::string g_perfilBio;
extern std::string g_perfilDescripcion;
extern std::string g_rutaFotoPerfil;
extern std::string g_rutaFotoPerfilPendiente;
extern bool g_fotoPerfilEnCurso;
extern EstadoApp g_pantallaRetornoFotoPerfil;
extern bool g_fotoPerfilPendiente;
extern bool g_perfilFotoCargada;
extern bool g_perfilError;
extern int focoPerfil;
extern bool g_perfilPropio;
extern int g_indicePerfilContacto;
extern u64 g_ultimaCargaPerfiles;
extern std::string g_idsMiniaturasCargadas;
extern u64 g_ultimoIntentoMiniaturas;
// Avisos temporales y selección visible de contactos/chat.
extern EstadoEnvio g_estadoEnvio;
extern u64 g_tiempoNotificacion;
extern int scrollContactos;
extern int contactoSeleccionado;
extern u64 g_ultimoMensajeAnimacion;
extern long g_ultimoMensajeAnimadoId;
extern int g_ultimoMensajeAnimadoIndice;
extern bool g_chatMedios;
extern int focoConfiguracion;
extern int focoChat;
extern const int CHAT_FOCO_X[13];
extern const int CHAT_FOCO_Y[13];
extern const int CHAT_FOCO_ANCHO[13];
extern const int CHAT_FOCO_ALTO[13];

// UI: idioma, textos localizados, primitivas de dibujo y renderizado de vistas.
u8 obtenerIdiomaSistema();
const char* getText(TextoID id);
void dibujarRectanguloRedondeado(float x, float y, float ancho, float alto, float radio, u32 color, float profundidad = 0.5f);
bool esMensajePropio(const std::string& mensaje);
std::string extraerTextoMensaje(const std::string& mensaje);
// Limita por puntos UTF-8 completos y envuelve el texto para el ancho de burbuja.
std::string limitarTextoUtf8(const std::string& texto, size_t maxCaracteres);
std::string envolverTextoMensaje(const std::string& mensaje, size_t maxCaracteres);
void dibujarHistorialChat(u32 colorTextoPrincipal, u32 colorTextoSecundario);
void dibujarHistorialV2(u32 colorTextoPrincipal, u32 colorTextoSecundario);
void dibujarAnimacionCarga(float centroX, float centroY, float angulo);
// Persistencia: identidad, contactos y preferencias de la tarjeta SD.
// Lee el ID estable existente o genera y guarda uno para esta instalación SD.
std::string obtenerOGenerarID();
void cargarContactosSD();
void guardarContactosSD();
void guardarContactoSD(const std::string& nuevaID, const std::string& nuevoNombre);
void borrarContactoActual(const std::string& idBorrar);
// Acepta solo host DNS/IPv4 sin esquema ni puerto; hostHttp añade el puerto.
bool servidorValido(const std::string& servidor);
std::string hostHttp();
void guardarAjustesSD();
void cargarAjustesSD();
// Indicadores LED, transporte al servidor y comprobación de versión remota.
void encenderLEDBlanco();
void apagarLED();
// Conecta al host configurado con timeout; -1 indica que no pudo establecer socket.
int conectarAlServidor();
std::string obtenerVersionRemota();
bool comprobarActualizaciones();
std::string escaparJson(const std::string& texto);
// Perfil: sincroniza campos, autentica ID y consulta directorios públicos.
bool guardarPerfilServidor();
bool iniciarSesion();
bool cambiarContrasena(const std::string& nueva);
bool buscarPerfiles();
const Contacto* contactoActivo();
u32 colorBurbujaPerfil(int indice);
u32 colorFondoPerfil(int indice, bool oscuro);
u32 colorTextoPerfil(int indice);
u32 colorApodoPerfil(int indice);
u32 colorApodoTarjeta(int indice, bool oscuro);
void dibujarIconoEngranaje(float x, float y, u32 color);
const char* nombreColorPerfil(int indice);
std::string abrirTecladoContrasena(const char* hint);
// Descarga detalles de contactos en grupos de hasta 50 y miniaturas visibles.
bool actualizarPerfilesContactos();
bool guardarMiniaturaDesdeFoto(const std::vector<u8>& foto, const std::string& ruta);
void actualizarMiniaturasContactos();
bool cargarFotoPerfilContacto(const Contacto& contacto);
bool subirFotoPerfilServidor(const std::string& ruta);
bool sincronizarPerfilServidor();
void abrirPerfil();
void abrirPerfilContacto(int indice);
// Conversación: envío de texto, notificación y caché local del protocolo legacy.
bool enviarMensajeServidor(const char* deID, const char* paraID, const char* mensaje);
void verificarMensajesNuevos();
void actualizarPresencias();
void cargarChatDesdeSD(const std::string& friendID);
void guardarHistorialEnSD(const std::string& friendID);
std::string claveServidorCache();
std::string rutaHistorialV2();
std::string rutaAudioV2(long id);
std::string rutaFotoV2(long id);
void guardarHistorialV2();
void cargarHistorialV2();
void fusionarHistorialV2(const std::vector<MensajeV2>& nuevos);
// Cursor cero trae recientes; otro ID trae mensajes previos y actualiza el total.
bool cargarPaginaV2(long antes);
// Chat: caché paginada, foco de mensaje, medios y confirmación/reintento.
void seleccionarAudio(int direccion);
void seleccionarFoto(int direccion);
void actualizarMiniaturasFotosChat();
bool mostrarFotoSeleccionada();
bool reproducirAudioSeleccionado();
void cargarMensajesAnteriores();
// Archivos pendientes se conservan si la subida falla para permitir reintento.
void enviarNotaPendiente();
void enviarFotoPendiente();
void cerrarCapturaFoto(bool descartarPendiente);
void abrirCamaraFoto();
void revisarFotoPendiente();
void tomarFotoVista();
void repetirFotoVista();
void confirmarEnvioFoto();
void actualizarChatDesdeServidor(const char* myID, const char* friendID);
void extraerCampanitaAuto();
void cargarSonidoCampanita();
void reproducirCampanita();
void cerrarAudio();
std::string abrirTeclado(const char* hint);
void dibujarBarraEstadoSuperior();
void dibujarTextoUI(const std::string& texto, float x, float y, float escala, u32 color);
void dibujarOpcionAjustes(int fila, const char* titulo, bool activo, u32 colorTarjeta, u32 colorTexto);
void dibujarPantallaConfiguracion();
bool procesarInicio(u32 kDown, touchPosition touch, bool& updateYaComprobado);
// Devuelve false solo cuando se solicita terminar la app con Start.
bool procesarEntrada(u32 kDown, u32 kNav, u32 kUp, touchPosition touch, u64& ultimoCheckTiempo);
// Dibuja el estadoActual en los targets superior e inferior de Citro2D.
void dibujarPantallaActual();
