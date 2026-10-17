# 3DSocial, de Miventech

**Mensajería homebrew para Nintendo 3DS: chat, notas de voz, fotos y perfiles mediante un servidor HTTP.**

3DSocial permite conversar usando un ID generado en la primera ejecución, sin registrar una cuenta de correo. Incluye un servidor Python de ejemplo para usarlo en una red local. Es un proyecto creado para aprender, experimentar y divertirse con el desarrollo homebrew de 3DS.

> **Antes de probar el audio:** recomendamos **Luma3DS 11.0 o superior**, con Rosalina, y realizar una vez el volcado del firmware DSP de **tu propia consola**. Sigue los pasos de [Preparar el audio con Rosalina](#preparar-el-audio-con-rosalina).

## Qué puedes hacer

- Enviar y recibir mensajes privados entre IDs, añadir contactos y consultar el historial por páginas.
- Grabar notas de voz de hasta 30 segundos y reproducir las recibidas.
- Tomar fotos con la cámara de la 3DS, revisarlas antes de enviarlas y abrir las fotos recibidas.
- Editar nombre, apodo, biografía, descripción y foto de perfil; ver las miniaturas de los contactos.
- Cambiar entre tema claro y oscuro, configurar el servidor y activar o desactivar sonido, LED y búsqueda de actualizaciones.

El texto funciona también con servidores que implementan las rutas antiguas. Las notas de voz, fotos, perfiles e historial paginado necesitan las rutas adicionales que documenta [ServerExample](ServerExample/README.md).

## La aplicación en funcionamiento

Las siguientes fotos muestran 3DSocial ejecutándose en una consola real. Las marcas rojas de algunas capturas ocultan IDs personales.

### Conversación y controles del chat

<img src="img/5086840346787908777_121.jpg" alt="3DSocial en una Nintendo 3DS: mensajes en la pantalla superior y controles de chat, voz, fotos e historial en la inferior" width="720">

La pantalla superior muestra la conversación. Abajo están **Escribir**, **MIC** y **Foto**, además de los controles para cargar mensajes anteriores, reproducir notas de voz y abrir fotos recibidas.

### Cámara y vista previa

<img src="img/5086840346787908778_121.jpg" alt="Vista previa de la cámara de 3DSocial con los botones Tomar foto y Cancelar" width="720">

La cámara presenta la imagen en la pantalla superior. En la inferior puedes tomar la foto con **A** o cancelar con **B**; después de capturarla, la app permite revisarla antes de enviarla.

### Conexión con un servidor

<img src="img/5086840346787908774_121.jpg" alt="Pestaña Conexión de 3DSocial con host HTTP, puerto, intervalo de consulta y botón Probar" width="720">

La pestaña **Conexión** permite escribir la dirección del servidor y el puerto, cambiar el intervalo de consulta, probar la conexión y guardar los ajustes. La dirección `192.168.1.35` de la foto es solo un ejemplo de red local.

### Opciones generales y sonido

<img src="img/5086840346787908776_121.jpg" alt="Pestaña General de 3DSocial con interruptores de modo oscuro, LED, sonido y actualizaciones" width="720">

Aquí se activan el modo oscuro, el LED, el sonido de notificación y la búsqueda de actualizaciones. Para oír la campanita, **Sonido de notificación** debe estar activado y el DSP debe estar preparado como se explica más abajo.

### Perfil de usuario

<img src="img/5086840346787908782_121.jpg" alt="Pestaña Perfil de 3DSocial con nombre, apodo, biografía, descripción, foto y botón Sincronizar" width="720">

La pestaña **Perfil** permite editar los datos visibles para tus contactos, cambiar la foto con la cámara y sincronizar los cambios con el servidor.

## Requisitos

- Una Nintendo 3DS/2DS capaz de ejecutar homebrew. Para instalar el `.cia` necesitas un instalador de títulos, como FBI.
- Tarjeta SD y conexión de red entre la consola y el servidor elegido.
- **Para la reproducción de audio:** el componente DSP disponible para `ndsp`. El método recomendado en esta guía es generar `sd:/3ds/dspfirm.cdc` desde Rosalina.
- Para ejecutar el servidor de ejemplo: Python 3.8 o posterior en un equipo de la misma red.

## Preparar el audio con Rosalina

3DSocial usa la biblioteca de audio **NDSP** de libctru para la campanita y las notas de voz. Si falta el componente DSP, la inicialización de NDSP puede fallar y el audio no funcionará. libctru busca el archivo `sd:/3ds/dspfirm.cdc` en la tarjeta SD; Rosalina puede generarlo directamente desde la consola. [Código de NDSP en libctru](https://github.com/devkitPro/libctru/blob/master/libctru/source/ndsp/ndsp.c), [opción de volcado en Luma3DS](https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/rosalina/source/menus/miscellaneous.c).

**Recomendamos Luma3DS 11.0 o posterior** para seguir estas instrucciones. Rosalina es el menú integrado en Luma3DS, no un programa con versión 11 independiente. La función de volcado apareció ya en Luma3DS 10.3; por tanto, el requisito técnico para este audio es disponer del componente DSP, no un número de versión concreto. [Notas oficiales de Luma3DS 10.3](https://github.com/LumaTeam/Luma3DS/releases/tag/v10.3).

Haz el volcado una sola vez en tu propia 3DS:

1. Enciende la consola y abre el menú HOME.
2. Pulsa **L + Abajo en la cruceta + Select** al mismo tiempo para abrir **Rosalina**. Es la combinación predeterminada; si la cambiaste, usa la que figure en tu configuración de Luma3DS.
3. Entra en **Miscellaneous options**.
4. Selecciona **Dump DSP firmware** y espera el mensaje de éxito.
5. Pulsa **B** para volver y pulsa **B** de nuevo para cerrar Rosalina.
6. Comprueba, si tienes acceso a la SD, que existe `sd:/3ds/dspfirm.cdc`. Vuelve a abrir 3DSocial y prueba una nota de voz o el sonido de notificación.

El [3DS Hacks Guide](https://3ds.hacks.guide/finalizing-setup) documenta la misma combinación de botones y la opción de volcado. Si Rosalina no abre, revisa la versión de Luma3DS y su combinación configurada; **Select al encender** abre la configuración de Luma3DS, mientras que **L + Abajo + Select con la consola encendida** abre Rosalina. [Uso básico de Luma3DS](https://github.com/LumaTeam/Luma3DS#basic-usage).

El archivo DSP procede de tu consola. No hace falta incluirlo en este repositorio ni distribuirlo con la aplicación. Si el audio sigue sin funcionar, confirma que el volcado terminó correctamente, que el archivo está en la SD utilizada por la app y que **Sonido** está activado en los ajustes de 3DSocial.

## Instalación de 3DSocial

Los binarios compilados de esta copia local están en la raíz. Como `.gitignore` excluye `.cia` y `.3dsx`, al publicar el repositorio conviene adjuntarlos a una [Release de GitLab](https://docs.gitlab.com/user/project/releases/) para que otras personas puedan descargarlos. Elige uno de los formatos:

| Formato | Instalación | Inicio |
| --- | --- | --- |
| `3DSocial.cia` | Cópialo a la SD e instálalo con FBI | Icono de 3DSocial en el menú HOME |
| `3DSocial.3dsx` | Cópialo a `sd:/3ds/3DSocial/3DSocial.3dsx` | Homebrew Launcher |

El CIA usa el Title ID `000400000F9F8F01`, el código de producto `CTR-P-3DSC` y el código de compañía `MV` (Miventech), distintos de los de Mensinger 3DS. Si instalas ambos, pueden aparecer como aplicaciones separadas.

## Primer uso y conexión al servidor

1. Conecta la 3DS a la misma red que el servidor. Al abrir la app por primera vez se genera tu ID; compártelo con la persona que quieras agregar.
2. Para una prueba local, inicia [el servidor de ejemplo](ServerExample/README.md) en tu computadora. En Windows, abre `ServerExample/IniciarServidor.bat`; desde una terminal también puedes ejecutar:

   ```powershell
   cd ServerExample
   py -3 server.py --port 8080
   ```

3. Averigua la dirección IPv4 local de esa computadora, por ejemplo con `ipconfig` en Windows. En 3DSocial abre **Configuración → Conexión** y escribe solo el host, por ejemplo `192.168.1.50`, sin `http://` ni barra final. Usa el puerto `8080` para el servidor de ejemplo.
4. Pulsa **Probar** y guarda los ajustes. Mantén abierta la terminal del servidor mientras uses el chat. Si el firewall pide permiso para Python, permite el acceso en la red privada.
5. Pulsa **Añadir contacto**, escribe su ID numérico y un nombre. Selecciona el contacto para abrir la conversación.

La app trae configurado inicialmente `gabxriel.pythonanywhere.com:80`. Puedes sustituirlo por tu propio servidor; para las funciones multimedia este debe implementar las rutas descritas en [ServerExample](ServerExample/README.md). `127.0.0.1` en la 3DS apunta a la propia consola, no a tu computadora.

## Controles principales

| Pantalla | Acción |
| --- | --- |
| Contactos | Toca un contacto o selecciónalo con la cruceta y **A** para abrir el chat; **X** añade un contacto; **Y** abre su perfil; **L** abre tu perfil. |
| Chat | **Escribir** envía texto. Mantén pulsado **MIC** o **Y** para grabar y suelta para enviar; **B** cancela la grabación. **Foto** abre la cámara. |
| Historial | Usa **Anteriores** y **Recientes** para navegar. Selecciona una nota y pulsa **Reproducir**, o selecciona una foto y pulsa **Ver grande**. |
| Cámara | **A** toma la foto; en la vista de confirmación, **A** la envía, **X** repite y **B** descarta. |
| Perfil y ajustes | **L/R** cambia de pestaña en Ajustes. **A** edita o confirma, **X** borra el campo seleccionado y **B** vuelve. |

La pantalla táctil muestra las acciones disponibles. **Start** cierra la aplicación fuera de la pantalla inicial. Las notas de voz no enviadas pueden conservarse como pendientes para reintentar el envío.

## Compilar desde el código fuente

Necesitas devkitPro con **devkitARM**, **libctru**, **citro2d**, las herramientas `3dsxtool`, `smdhtool`, `bannertool` y `makerom`, además de la biblioteca `ctr-led-brary` incluida en este proyecto. El Makefile compila la biblioteca LED desde su código fuente; el ejecutable local `bannertool.exe` y los demás binarios generados no se publican en el repositorio.

Coloca el proyecto en una ruta **sin espacios**: el Makefile actual invoca una compilación interna mediante la ruta de trabajo y no escapa los espacios de esa ruta. Con el entorno de devkitPro configurado:

```sh
make
make cia
```

`make` genera `3DSocial.3dsx`, `3DSocial.elf` y `3DSocial.smdh`. `make cia` también genera `3DSocial.cia` usando el icono, el banner y la configuración [app.rsf](app.rsf). Los archivos de salida y las carpetas `build/` están excluidos por `.gitignore`.

## Organización del repositorio

| Ruta | Contenido |
| --- | --- |
| `source/app/` | Punto de entrada, estado compartido, inicio y controles. |
| `source/gui/` | Textos, componentes y pantallas. |
| `source/system/` | Contactos, configuración e historial en la SD. |
| `source/network/` | Conexión HTTP, mensajes y perfiles. |
| `source/media/` | Cámara, fotos, grabación, reproducción y sonido de notificación. |
| `ServerExample/` | Servidor Python de ejemplo y pruebas; guarda mensajes y medios en SQLite. |
| `ctr-led-brary/` | Biblioteca usada para las notificaciones LED. |

El servidor de ejemplo usa HTTP sin cifrado y los IDs no autentican usuarios. Úsalo en una red local de confianza; para exponerlo a Internet harían falta autenticación y transporte cifrado compatibles con el cliente. [Detalles del servidor](ServerExample/README.md).

## Origen y agradecimientos

3DSocial nació de la base y la idea de [Mensinger3DS-Backend, de GABXRIEL](https://github.com/GABXRIEL/Mensinger3DS-Backend). Es el proyecto padre y la inspiración de esta herramienta. Gracias a **GABXRIEL** por ese trabajo original y por abrir el camino para seguir experimentando con mensajería en Nintendo 3DS.

Gracias también a las comunidades y herramientas de **devkitPro/libctru**, **Luma3DS/Rosalina**, **PabloMK7** por `ctr-led-brary` (licencia en [su README](ctr-led-brary/README.md)) y al proyecto **FBI**, cuyo flujo de captura de cámara se reconoce en [THIRD_PARTY_FBI_LICENSE.txt](THIRD_PARTY_FBI_LICENSE.txt).

Este proyecto se comparte con fines **educativos y de diversión**. No está afiliado a Nintendo.

## Licencia y créditos al reutilizar el código

El material sobre el que Miventech tiene derechos o autorización para licenciar se ofrece bajo la [Licencia Miventech de uso libre con atribución](LICENSE). Puedes usarlo, modificarlo y distribuirlo, también en proyectos comerciales y con otra licencia para tus propias modificaciones. Al publicar un proyecto que reutilice una parte sustancial, conserva la licencia e incluye un crédito visible como **«Basado en 3DSocial de Miventech»** en el README, los créditos, la pantalla «Acerca de» o la página de descarga.

Esta licencia es propia del proyecto; no debe presentarse como MIT ni como otra licencia estándar. Los derechos de terceros siguen sujetos a sus respectivas condiciones. Conserva también los avisos correspondientes, incluido [el aviso del código adaptado de FBI](THIRD_PARTY_FBI_LICENSE.txt), y acredita a GABXRIEL cuando corresponda por la base original.
