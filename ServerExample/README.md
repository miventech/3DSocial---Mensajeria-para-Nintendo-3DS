# ServerExample

Servidor HTTP de ejemplo para Mensinger 3DS. Usa únicamente la biblioteca estándar de Python y guarda **todo el historial de texto, audio y fotos** en `messages.sqlite3` mediante SQLite. Los registros no se borran automáticamente al quitar un contacto ni al reiniciar el servidor.

## Iniciar

En Windows, puedes iniciar el servidor haciendo doble clic en `IniciarServidor.bat`. La ventana de consola debe permanecer abierta mientras se usa el chat.

También puedes iniciarlo desde esta carpeta, con Python 3.8 o posterior:

```powershell
py -3 server.py
```

En macOS o Linux:

```sh
python3 server.py
```

Por defecto escucha en todas las interfaces de red, puerto `8080`. Para elegir otro puerto o la ruta de datos:

```powershell
py -3 server.py --port 8080 --db messages.sqlite3
```

La consola debe permanecer abierta mientras se usa el chat. `Ctrl+C` detiene el servidor.

### Interpretar la consola

Al iniciar, verás la dirección HTTP y la ruta absoluta del archivo SQLite que está usando el proceso. Cada petición agrega una línea estándar de Python con la IP del cliente, fecha, método/ruta HTTP y código de respuesta. Por ejemplo:

```text
192.168.1.42 - - [16/Oct/2026 12:30:00] "GET /notificaciones/12345678 HTTP/1.1" 200 -
```

Esto significa que el equipo `192.168.1.42` consultó notificaciones y el servidor respondió `200` (correcto). Otros códigos habituales son `400` para datos inválidos, `404` para una ruta o medio inexistente, `415` para un formato de contenido no admitido y `500` para un fallo interno. El registro contiene la ruta y el resultado; no imprime el texto de los mensajes ni el contenido de fotos o audios. Los errores de base de datos pueden producir además una línea explicativa en la consola.

## Conectar la 3DS

1. Conecta la computadora y la 3DS a la misma red local.
2. Busca la dirección IPv4 de la computadora con `ipconfig` en Windows o `ip addr` en Linux.
3. En la app de 3DS abre **Configuración → Conexión**. En **Servidor HTTP** escribe solo la dirección IPv4, por ejemplo `192.168.1.50`, sin `http://` ni barra final. En **Puerto** escribe `8080`.
4. Pulsa **Probar** y después **Guardar y volver**. Si Windows solicita acceso para Python en la red privada, permítelo.

La dirección `127.0.0.1` solo sirve para probar desde la misma computadora; en la 3DS debes usar la dirección de la computadora en la red local. Si la computadora cambia de dirección IP, actualiza el ajuste en la app.

## Rutas compatibles

| Ruta | Respuesta |
| --- | --- |
| `GET /version` | Texto simple, por defecto `v1.2.1` |
| `POST /enviar` | Acepta JSON con `de`, `para` y `texto`; devuelve `OK` |
| `GET /notificaciones/<id>` | ID del remitente pendiente o `NINGUNO` |
| `GET /obtener_chat/<mi_id>/<contacto_id>` | Líneas `[id_remitente]: mensaje` en orden cronológico |
| `GET /historial/<mi_id>/<contacto_id>?before=<id>&limit=20` | Página de hasta 20 mensajes de texto, audio y fotos; `before=0` trae los más recientes |
| `POST /audio/<de>/<para>` | Recibe un WAV en el cuerpo (`Content-Type: audio/wav`) y devuelve el ID del mensaje |
| `GET /audio/<id>` | Devuelve el WAV guardado para ese mensaje |
| `POST /foto/<de>/<para>` | Recibe una foto capturada por la cámara en RGB565 400x240 y devuelve el ID del mensaje |
| `GET /foto/<id>` | Devuelve la foto RGB565 guardada para ese mensaje |
| `POST /perfil/<id>` | Guarda `username`, `nickname`, `bio` y `description` como JSON |
| `GET /perfil/<id>` | Devuelve los datos del perfil como JSON |
| `GET /perfiles?ids=<id1,id2,...>` | Devuelve como TSV los perfiles de hasta 50 contactos |
| `POST /perfil/<id>/foto` | Guarda la foto de perfil BGR565 de 400×240 píxeles |
| `GET /perfil/<id>/foto` | Devuelve la foto de perfil BGR565 |
| `GET /perfil-miniaturas?ids=<id1,id2,id3>` | Devuelve miniaturas cuadradas 32×32 para hasta tres contactos visibles |

Ejemplo de envío desde otra terminal:

```sh
curl -X POST http://127.0.0.1:8080/enviar -H "Content-Type: application/json" -d '{"de":"12345678","para":"87654321","texto":"Hola"}'
```

```text
GET /notificaciones/87654321  ->  12345678
GET /obtener_chat/87654321/12345678  ->  [12345678]: Hola
```

Los IDs deben ser numéricos de 1 a 10 dígitos. El texto admite hasta 120 caracteres y una sola línea. `/obtener_chat` conserva el formato para clientes antiguos y muestra solo los textos recientes; `/historial` permite cargar todo el registro por páginas. Cada línea del protocolo nuevo contiene `id`, `remitente`, `tipo` (`texto`, `audio` o `foto`), `duración_ms` y `texto`, separados por tabuladores. Un cursor `before` excluye ese ID y devuelve mensajes anteriores.

Las notas de voz usan WAV PCM mono de 16 bits a 10 910 Hz, con un límite de 30 segundos (654 644 bytes incluido el encabezado). El servidor comprueba el encabezado, la duración y el tamaño antes de guardarlas. La nueva app graba manteniendo pulsado **MIC**, envía al soltar, cancela con **B** y descarga el audio recibido al pulsar **Reproducir**. En el chat, **Anteriores** y **Recientes** recorren el historial.

Las fotos solo se pueden crear desde el botón **FOTO** de la app. La cámara exterior muestra una vista previa en vivo; después de tomar la foto, la app permite repetirla, descartarla o confirmar el envío. La imagen capturada mide 400x240 píxeles en RGB565 (192 000 bytes por foto). No hay una ruta para seleccionar archivos de la SD. Las fotos aparecen en el historial, se guardan en SQLite y la app las descarga a su caché de la SD al elegir **Ver grande**.

Desde **L / Mi perfil** en contactos o **Y** en ajustes, la app permite editar nombre de usuario, apodo, biografía y descripción. **X** borra el campo seleccionado. La foto de perfil se toma con la misma cámara y se confirma antes de subirla. El texto se guarda también en `config.ini` de la SD y se puede volver a sincronizar desde la última fila de la pantalla de perfil. Los campos admiten hasta 32, 32, 120 y 120 caracteres, respectivamente. El servidor conserva la foto cuando se actualiza el texto, y conserva el texto cuando se cambia la foto.

Al recibir una foto de perfil, el servidor conserva el original 400×240 y genera una miniatura BGR565 de 32×32 mediante recorte centrado cuadrado. La app solicita las miniaturas de las filas visibles y las muestra junto a los nombres; las miniaturas se guardan en caché en la SD.

Una base creada con la versión anterior se amplía automáticamente al iniciar esta versión. Los textos existentes se conservan. Para hacer una copia completa del historial, detén el servidor y copia `messages.sqlite3`.

Pruebas locales del servidor:

```powershell
py -3 -m unittest -v test_server.py
```

Este ejemplo usa HTTP sin cifrado y los IDs no autentican usuarios. Úsalo en una red local de confianza; antes de publicarlo en Internet harían falta autenticación y transporte cifrado compatibles con el cliente.
