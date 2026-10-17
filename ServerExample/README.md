# Servidor de ejemplo de 3DSocial

Este servidor usa **HTTP** en ambos modos. Por defecto, cifra cada petición y respuesta de la aplicación antes de enviarla por HTTP. Crea automáticamente una clave de transporte propia y una clave separada para el contenido de SQLite. La consola crea su clave efímera en cada petición y recuerda la clave pública del servidor; el jugador no instala certificados.

## Inicio rápido

En Windows, abre `IniciarServidorConCifrado.bat`. El archivo `IniciarServidor.bat` busca Python 3.8 o posterior e instala `cryptography` en ese mismo Python si falta. También puedes ejecutar:

```powershell
py -3 -m pip install cryptography
py -3 server.py --host 0.0.0.0 --port 8080
```

En la 3DS, abre **Configuración → Conexión**, escribe la IPv4 local de la computadora y el puerto `8080`, y deja **Cifrar conexión: SI (HTTP cifrado)**. Pulsa **Probar** y guarda. `127.0.0.1` solo sirve desde la computadora, no desde la 3DS. El servidor y la app deben usar el mismo modo.

Para una prueba con el protocolo antiguo en claro, abre `IniciarServidorSinCifrado.bat` y selecciona **NO (HTTP)** en la app. El modo en claro transmite ID, contraseña, token, mensajes y archivos legibles por la red. No abras ambos `.bat` sobre el mismo puerto a la vez.

## Qué se crea automáticamente

| Archivo | Función |
| --- | --- |
| `messages.sqlite3` | Perfiles, mensajes, fotos, audios y metadatos. |
| `messages.key` | Clave AES-256-GCM para contenido de SQLite; se crea una sola vez y se necesita para recuperar los mensajes. |
| `transport.key` | Clave privada X25519 del servidor para el canal sobre HTTP; se crea una sola vez en modo cifrado. |

Puedes elegir otras rutas con `--db`, `--key` y `--transport-key`. Guarda **juntos** la base y las dos claves en tus copias de seguridad. No publiques los archivos `.key`. Si falta una clave de una instalación existente, restaura la original; regenerar `transport.key` hará que las consolas rechacen la nueva identidad hasta que cada usuario compruebe el cambio y restablezca su clave fijada.

La consola guarda la clave pública en `sdmc:/3ds/Mensinger3DS/server-key-<host>-<puerto>.bin`. En el primer contacto la obtiene por HTTP sin enviar identidad, token ni contraseña y la fija para futuros contactos. Si cambia, detiene la conexión. La pantalla de error ofrece **R** para olvidar esa clave solo después de verificar con el administrador que el servidor legítimo cambió; la consola la obtiene de nuevo automáticamente. La consola no descarga certificados.

## Cifrado de transporte

1. La consola pide `GET /clave-servidor`. El servidor devuelve `M3K1`, su clave pública X25519 de 32 bytes y un desafío aleatorio de 16 bytes válido durante 180 segundos.
2. La consola compara la clave pública con la que guardó para ese host y puerto. Después crea 32 bytes aleatorios con el servicio `ps:ps` de 3DS y calcula una clave pública X25519 efímera.
3. Ambas partes derivan 32 bytes con BLAKE2b-256 de `secreto_compartido || clave_publica_cliente || clave_publica_servidor || desafio || "Mensinger3DS-channel-v1"`.
4. La consola cifra el método, ruta, ID, token, tipo de contenido y cuerpo con ChaCha20-Poly1305. Envía `M3Q1 || desafio || clave_publica_cliente || tag || ciphertext` a `POST /canal`. El desafío se consume una sola vez, incluso si el sobre falla, para impedir su repetición.
5. El servidor valida tamaños, autenticación y ruta, ejecuta las validaciones y permisos originales, y devuelve `M3R1 || tag || ciphertext` con el estado y cuerpo internos cifrados.

El nonce de petición son 12 bytes cero; el de respuesta termina en `01`. Son seguros aquí porque **cada desafío y clave efímera producen una clave diferente** y cada clave solo cifra una petición y una respuesta. Los AAD `M3DS1Q` y `M3DS1R` separan las direcciones. Los tamaños, el momento de los envíos, la dirección IP y las rutas exteriores `/clave-servidor` y `/canal` siguen visibles. El primer contacto usa confianza en el primer uso: alguien que controle la red en ese instante puede suplantar la clave inicial. Para una instalación que evite ese riesgo, preinstala el archivo público de 32 bytes por un canal confiable y comprueba la huella SHA-256 anunciada en la consola del servidor. Este canal tampoco ofrece secreto hacia adelante si se roba `transport.key` más tarde; para un servicio expuesto a Internet se recomienda TLS con una identidad certificada.

Esto cifra datos **entre la consola y el servidor**, no entre usuarios. El servidor descifra las peticiones y puede leer sus contenidos. La contraseña de perfil sigue siendo necesaria para impedir que alguien use un ID conocido. El modo de transporte no cambia el cifrado de SQLite: cuerpos de texto, audio, fotos y miniaturas se guardan con AES-256-GCM también cuando `--cifrado-conexion no`. Los IDs, fechas, tipos y textos de perfil siguen consultables en la base; las cachés de la SD no están cifradas.

## Configuración

```powershell
py -3 server.py --host 0.0.0.0 --port 8080 --cifrado-conexion si
py -3 server.py --host 0.0.0.0 --port 8080 --cifrado-conexion no
```

`si` es el valor predeterminado. `--version` cambia el texto de la ruta interna `/version`; `--db`, `--key` y `--transport-key` cambian las rutas de datos. En modo cifrado, las rutas habituales solo se aceptan dentro de `/canal`; las peticiones HTTP directas a mensajes, perfiles, contraseña y medios reciben 403. En modo en claro, se mantienen las rutas HTTP antiguas.

La [documentación técnica](../docs/DOCUMENTACION_TECNICA.md) enumera todas las rutas internas, formatos de audio/foto, controles de autorización, esquema SQLite y el monitor. Para comprobar servidor y compatibilidad criptográfica:

```powershell
py -3 -m unittest discover -s ServerExample -p "test_*.py" -v
```

El monitor lee `messages.sqlite3` con `messages.key` en modo solo lectura; no usa `transport.key` ni se conecta al canal HTTP.

## Bandeja de desconocidos y presencia

La consola consulta `GET /remitentes/{id}` para recibir hasta 80 remitentes recientes en filas TSV: `id`, mensajes pendientes, total recibido y último ID de mensaje. La consulta no cambia el estado de lectura. La lista de contactos vive en la SD de cada consola, por lo que el cliente separa esas filas entre contactos y personas desconocidas. Al abrir una conversación, `/historial` marca como leídos los mensajes de ese remitente; su conversación sigue visible en Extraños hasta que se agregue a contactos.

Cada consola envía `POST /presencia/{id}` aproximadamente cada 20 segundos mientras la app está activa. `GET /presencias/{id}?ids=id1,id2` consulta hasta 50 IDs. El servidor conserva los latidos solo en memoria y responde conectado si el último tiene menos de 60 segundos. Tras reiniciar el servidor todos aparecen desconectados hasta volver a enviar un latido. Las rutas privadas comprueban el token de sesión cuando el ID tiene contraseña. El monitor obtiene sus gráficas y galería directamente de SQLite y no altera la presencia ni los avisos.
