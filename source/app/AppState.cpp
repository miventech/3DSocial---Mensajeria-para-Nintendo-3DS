#include "AppState.hpp"

// Punto de entrada y coordinador de la interfaz de dos pantallas de 3DS.
// Las solicitudes HTTP y los medios se delegan a MessagingV2, VoiceNotes y CameraPhotos.

extern const char* const SERVIDOR_PREDETERMINADO = "gabxriel.pythonanywhere.com";
extern const int PUERTO_PREDETERMINADO = 80;


// 🔴 VARIABLES DE ACTUALIZACIÓN
bool comprobadoUpdate = false;
bool hayNuevaVersion = false;
std::string ultimaVersionDisponible = "";
extern const std::string VERSION_ACTUAL = "v1.2.4";


// --- OPCIONES DE CONFIGURACIÓN ---
bool g_modoOscuro = false;
bool g_ledActivado = true;
bool g_sonidoActivado = true;
bool g_buscarActualizaciones = true;
std::string g_servidorHost = SERVIDOR_PREDETERMINADO;
int g_servidorPuerto = PUERTO_PREDETERMINADO;
int g_intervaloConsultaMs = 2000;
int g_pestanaConfig = 0; // General, Conexion, Perfil
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
// Estado de la conversación y datos sincronizados que se comparten entre pantallas.
std::string contactoActivoID = "";
std::string contactoActivoNombre = "";
std::vector<Contacto> listaContactos;
std::vector<std::string> historialChat;
bool g_confirmarBorrarContacto = false;
std::vector<MensajeV2> g_historialV2;
bool g_servidorV2 = false;
bool g_v2Determinado = false;
bool g_masAntiguos = true;
int g_desplazamientoChat = 0;
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
EstadoApp g_pantallaRetornoFotoPerfil = PANTALLA_CONFIGURACION;
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
int focoConfiguracion = 0;
int focoChat = 0;
// Rectángulos de navegación táctil: cada índice de foco apunta a una zona del chat.
const int CHAT_FOCO_X[13] = {20,187,249,20,106,208,20,106,220,20,106,220,266};
const int CHAT_FOCO_Y[13] = {26,26,26,94,94,94,145,145,145,184,184,184,220};
const int CHAT_FOCO_ANCHO[13] = {161,57,51,80,96,92,80,108,80,80,108,80,36};
const int CHAT_FOCO_ALTO[13] = {48,48,48,38,38,38,35,35,35,33,33,33,17};

// Lista de todos los textos de la app

