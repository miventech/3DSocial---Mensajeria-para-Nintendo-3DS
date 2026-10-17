#pragma once
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


enum EstadoApp {
    // La cámara y la confirmación son estados separados para no enviar una foto accidentalmente.
    PANTALLA_CONTACTOS,
    PANTALLA_CHAT,
    PANTALLA_FOTO,
    PANTALLA_CAMARA,
    PANTALLA_CONFIRMAR_FOTO,
    PANTALLA_CONFIGURACION,
    PANTALLA_PERFIL
};
enum EstadoInicio {
    // Secuencia visible al abrir: conexión, búsqueda opcional de versión y acceso a contactos.
    INICIO_CONECTANDO,
    INICIO_BUSCANDO_UPDATE,
    INICIO_ERROR_CONEXION,
    INICIO_UPDATE_DISPONIBLE,
    INICIO_COMPLETADO
};
struct Contacto {
    // Los campos base se guardan en la SD; los campos Perfil_* se refrescan desde el servidor.
    std::string id;
    std::string nombre;
    bool tieneNovedad = false;
    std::string usuarioPerfil;
    std::string apodoPerfil;
    std::string bioPerfil;
    std::string descripcionPerfil;
    bool tienePerfil = false;
    bool tieneFotoPerfil = false;
    bool tieneMiniatura = false;
};
enum EstadoEnvio { ENVIO_NINGUNO, ENVIO_EXITO, ENVIO_ERROR };
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
    TOTAL_TEXTOS
};

extern const char* const SERVIDOR_PREDETERMINADO;
extern const int PUERTO_PREDETERMINADO;
extern bool comprobadoUpdate;
extern bool hayNuevaVersion;
extern std::string ultimaVersionDisponible;
extern const std::string VERSION_ACTUAL;
extern bool g_modoOscuro;
extern bool g_ledActivado;
extern bool g_sonidoActivado;
extern bool g_buscarActualizaciones;
extern std::string g_servidorHost;
extern int g_servidorPuerto;
extern int g_intervaloConsultaMs;
extern int g_pestanaConfig;
extern int g_estadoPruebaConexion;
extern bool g_errorConfig;
extern u8* bufferAudio;
extern u32 tamanoAudio;
extern ndspWaveBuf waveBuf;
extern EstadoApp estadoActual;
extern EstadoApp pantallaRetornoConfig;
extern EstadoInicio g_estadoInicio;
extern float g_anguloCarga;
extern u32* socBuffer;
extern C2D_TextBuf bufTexto;
extern C3D_RenderTarget* targetArriba;
extern C3D_RenderTarget* targetAbajo;
extern std::string miID;
extern std::string contactoActivoID;
extern std::string contactoActivoNombre;
extern std::vector<Contacto> listaContactos;
extern std::vector<std::string> historialChat;
extern bool g_confirmarBorrarContacto;
extern std::vector<MensajeV2> g_historialV2;
extern bool g_servidorV2;
extern bool g_v2Determinado;
extern bool g_masAntiguos;
extern int g_desplazamientoChat;
extern long g_audioSeleccionadoId;
extern bool g_audioPendiente;
extern std::string g_rutaAudioPendiente;
extern bool g_audioError;
extern long g_fotoSeleccionadaId;
extern bool g_fotoPendiente;
extern std::string g_rutaFotoPendiente;
extern bool g_fotoError;
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
extern EstadoEnvio g_estadoEnvio;
extern u64 g_tiempoNotificacion;
extern int scrollContactos;
extern int contactoSeleccionado;
extern int focoConfiguracion;
extern int focoChat;
extern const int CHAT_FOCO_X[13];
extern const int CHAT_FOCO_Y[13];
extern const int CHAT_FOCO_ANCHO[13];
extern const int CHAT_FOCO_ALTO[13];

u8 obtenerIdiomaSistema();
const char* getText(TextoID id);
void dibujarRectanguloRedondeado(float x, float y, float ancho, float alto, float radio, u32 color, float profundidad = 0.5f);
bool esMensajePropio(const std::string& mensaje);
std::string extraerTextoMensaje(const std::string& mensaje);
std::string limitarTextoUtf8(const std::string& texto, size_t maxCaracteres);
std::string envolverTextoMensaje(const std::string& mensaje, size_t maxCaracteres);
void dibujarHistorialChat(u32 colorTextoPrincipal, u32 colorTextoSecundario);
void dibujarHistorialV2(u32 colorTextoPrincipal, u32 colorTextoSecundario);
void dibujarAnimacionCarga(float centroX, float centroY, float angulo);
std::string obtenerOGenerarID();
void cargarContactosSD();
void guardarContactosSD();
void guardarContactoSD(const std::string& nuevaID, const std::string& nuevoNombre);
void borrarContactoActual(const std::string& idBorrar);
bool servidorValido(const std::string& servidor);
std::string hostHttp();
void guardarAjustesSD();
void cargarAjustesSD();
void encenderLEDBlanco();
void apagarLED();
int conectarAlServidor();
std::string obtenerVersionRemota();
bool comprobarActualizaciones();
std::string escaparJson(const std::string& texto);
bool guardarPerfilServidor();
bool actualizarPerfilesContactos();
bool guardarMiniaturaDesdeFoto(const std::vector<u8>& foto, const std::string& ruta);
void actualizarMiniaturasContactos();
bool cargarFotoPerfilContacto(const Contacto& contacto);
bool subirFotoPerfilServidor(const std::string& ruta);
bool sincronizarPerfilServidor();
void abrirPerfil();
void abrirPerfilContacto(int indice);
bool enviarMensajeServidor(const char* deID, const char* paraID, const char* mensaje);
void verificarMensajesNuevos();
void cargarChatDesdeSD(const std::string& friendID);
void guardarHistorialEnSD(const std::string& friendID);
std::string claveServidorCache();
std::string rutaHistorialV2();
std::string rutaAudioV2(long id);
std::string rutaFotoV2(long id);
void guardarHistorialV2();
void cargarHistorialV2();
void fusionarHistorialV2(const std::vector<MensajeV2>& nuevos);
bool cargarPaginaV2(long antes);
void seleccionarAudio(int direccion);
void seleccionarFoto(int direccion);
bool mostrarFotoSeleccionada();
bool reproducirAudioSeleccionado();
void cargarMensajesAnteriores();
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
bool procesarEntrada(u32 kDown, u32 kNav, u32 kUp, touchPosition touch, u64& ultimoCheckTiempo);
void dibujarPantallaActual();
