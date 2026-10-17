#include "AppState.hpp"

// Único almacenamiento de los globales declarados en AppState.hpp. Mantener
// aquí sus valores iniciales evita definiciones duplicadas entre módulos.
// Punto de entrada y coordinador de la interfaz de dos pantallas de 3DS.
// Las solicitudes HTTP y los medios se delegan a MessagingV2, VoiceNotes y CameraPhotos.

extern const char* const SERVIDOR_PREDETERMINADO = "gabxriel.pythonanywhere.com";
extern const int PUERTO_PREDETERMINADO = 80;


// 🔴 VARIABLES DE ACTUALIZACIÓN
bool comprobadoUpdate = false;
bool hayNuevaVersion = false;
std::string ultimaVersionDisponible = "";
extern const std::string VERSION_ACTUAL = "v1.4.4";


// --- OPCIONES DE CONFIGURACIÓN ---
bool g_modoOscuro = false;
bool g_ledActivado = true;
bool g_sonidoActivado = true;
bool g_buscarActualizaciones = true;
std::string g_servidorHost = SERVIDOR_PREDETERMINADO;
bool g_claveServidorError = false;
bool g_cryptoDisponible = false;
bool g_cifrarConexion = true;
int g_servidorPuerto = PUERTO_PREDETERMINADO;
int g_intervaloConsultaMs = 2000;
int g_pestanaConfig = 0; // General, Conexion
int g_estadoPruebaConexion = 0; // 0: sin probar, 1: conectado, -1: error
bool g_errorConfig = false;

// --- VARIABLES GLOBALES DE AUDIO ---
u8* bufferAudio = NULL;
u32 tamanoAudio = 0;
ndspWaveBuf waveBuf;


// Estructura para guardar ID y Nombre

EstadoApp estadoActual = PANTALLA_CONTACTOS;
EstadoApp pantallaRetornoConfig = PANTALLA_CONTACTOS;
EstadoInicio g_estadoInicio = INICIO_CONECTANDO;
float g_anguloCarga = 0.0f;

u32* socBuffer = NULL;
C2D_TextBuf bufTexto;
C3D_RenderTarget* targetArriba;
C3D_RenderTarget* targetAbajo;

std::string miID = "";
std::string g_tokenSesion;
std::string g_hostSesion;
int g_puertoSesion = 0;
bool g_cifradoSesion = true;
bool g_contrasenaActiva = false;
bool g_mostrarEnBusqueda = false;
bool g_quieroConocerGente = false;
int g_colorChat = 0;
int g_colorFondo = 0;
int g_colorTexto = 0;
int g_colorApodo = 0;
int g_colorNombre = 0;
int g_scrollPerfil = 0;
int g_slotMiniaturaChat = -1;
std::vector<ResultadoBusqueda> g_resultadosBusqueda;
std::string g_consultaBusqueda;
bool g_modoConocer = false;
int g_scrollBusqueda = 0;
// Estado de la conversación y datos sincronizados que se comparten entre pantallas.
std::string contactoActivoID = "";
std::string contactoActivoNombre = "";
std::vector<Contacto> listaContactos;
std::vector<RemitenteDesconocido> g_desconocidos;
int g_scrollDesconocidos = 0;
int g_seleccionDesconocidos = 0;
EstadoApp g_pantallaRetornoChat = PANTALLA_CONTACTOS;
long g_ultimoAvisoRecibido = 0;
u64 g_ultimoLatidoPresencia = 0;
u64 g_ultimaConsultaPresencias = 0;
std::vector<std::string> historialChat;
bool g_confirmarBorrarContacto = false;
std::vector<MensajeV2> g_historialV2;
std::vector<long> g_idsMensajesVisibles;
bool g_servidorV2 = false;
bool g_v2Determinado = false;
bool g_masAntiguos = true;
int g_desplazamientoChat = 0;
int g_totalMensajesChat = 0;
long g_audioSeleccionadoId = 0;
bool g_audioPendiente = false;
std::string g_rutaAudioPendiente;
bool g_audioError = false;
long g_fotoSeleccionadaId = 0;
bool g_fotoPendiente = false;
std::string g_rutaFotoPendiente;
bool g_fotoError = false;
std::string g_perfilNombre;
std::string g_perfilApodo;
std::string g_perfilBio;
std::string g_perfilDescripcion;
std::string g_rutaFotoPerfil;
std::string g_rutaFotoPerfilPendiente;
bool g_fotoPerfilEnCurso = false;
EstadoApp g_pantallaRetornoFotoPerfil = PANTALLA_PERFIL;
bool g_fotoPerfilPendiente = false;
bool g_perfilFotoCargada = false;
bool g_perfilError = false;
int focoPerfil = 0;
bool g_perfilPropio = true;
int g_indicePerfilContacto = -1;
u64 g_ultimaCargaPerfiles = 0;
std::string g_idsMiniaturasCargadas;
u64 g_ultimoIntentoMiniaturas = 0;

EstadoEnvio g_estadoEnvio = ENVIO_NINGUNO;
u64 g_tiempoNotificacion = 0;

int scrollContactos = 0;
int contactoSeleccionado = 0;
u64 g_ultimoMensajeAnimacion = 0;
long g_ultimoMensajeAnimadoId = 0;
int g_ultimoMensajeAnimadoIndice = -1;
bool g_chatMedios = false;
int focoConfiguracion = 0;
int focoChat = 0;
// Rectángulos de navegación táctil: cada índice de foco apunta a una zona del chat.
const int CHAT_FOCO_X[13] = {12,228,12,12,111,215,12,75,253,12,75,253,12};
const int CHAT_FOCO_Y[13] = {50,50,123,188,123,123,82,82,82,160,160,160,207};
const int CHAT_FOCO_ANCHO[13] = {205,80,90,90,95,93,55,170,55,55,170,55,130};
const int CHAT_FOCO_ALTO[13] = {60,60,44,40,44,44,38,38,38,38,38,38,27};

// Lista de todos los textos de la app

