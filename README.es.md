[English](README.md) | **Español**

# 3DSocial, de Miventech

**Mensajería homebrew para Nintendo 3DS: chat, notas de voz, fotos y perfiles mediante un servidor Python con cifrado de aplicación sobre HTTP.**

3DSocial permite conversar usando un ID generado en la primera ejecución, sin registrar una cuenta de correo. Incluye un servidor Python de ejemplo para usarlo en una red local. Es un proyecto creado para aprender, experimentar y divertirse con el desarrollo homebrew de 3DS.

> ## ⚠️ IMPORTANTE: por ahora solo funciona con servidores locales
>
> **3DSocial actualmente solo puede conectarse a servidores en tu red local** (por ejemplo, el [servidor de ejemplo](ServerExample/README.md) ejecutándose en tu propia computadora). **No hay un servidor público disponible todavía.**
>
> El motivo es económico: por ahora no puedo costear un servidor en la nube (Contabo o similares). **A finales de este mes evaluaré contratar un VPS** para ofrecer un servidor público para esta comunidad. Mientras tanto, puedes probar la app montando tu propio servidor local siguiendo la guía de [Primer uso y conexión al servidor](#primer-uso-y-conexión-al-servidor).

> **Antes de probar el audio:** recomendamos **Luma3DS 11.0 o superior**, con Rosalina, y realizar una vez el volcado del firmware DSP de **tu propia consola**. Sigue los pasos de [Preparar el audio con Rosalina](#preparar-el-audio-con-rosalina).

## Qué puedes hacer

- Enviar y recibir mensajes privados entre IDs, añadir contactos y consultar el historial por páginas.
- Revisar mensajes de personas fuera de contactos en **Extraños**, abrir sus chats y agregarlas. Ver avisos pendientes y el estado conectado/desconectado de cada contacto.
- Grabar notas de voz de hasta 30 segundos y reproducir las recibidas.
- Tomar fotos con la cámara de la 3DS, revisarlas antes de enviarlas y abrir las fotos recibidas.
- Editar nombre, apodo, biografía, descripción y foto de perfil; ver las miniaturas de los contactos.
- Proteger opcionalmente el ID con contraseña y decidir si el perfil aparece en búsqueda o en «Quiero conocer gente».
- Cambiar entre tema claro y oscuro, configurar el servidor y activar o desactivar sonido, LED y búsqueda de actualizaciones.

El texto funciona también con servidores que implementan las rutas antiguas. Las notas de voz, fotos, perfiles e historial paginado necesitan las rutas adicionales que documenta [ServerExample](ServerExample/README.md).

## La aplicación en funcionamiento

Las siguientes fotos muestran 3DSocial ejecutándose en una consola real. Las marcas rojas de algunas capturas ocultan IDs personales. Las primeras secciones corresponden a la **interfaz actual**; al final, la sección «Versiones anteriores» conserva fotos de builds previos para comparar la evolución.

### Contactos y conversaciones

<img src="img/5089159173861215455_121.jpg" alt="Pantalla de contactos de 3DSocial con la lista de conversaciones y los botones Añadir, Buscar, Conocer y Extraños" width="720">

Pantalla principal de contactos de la versión actual. La pantalla superior presenta el encabezado **3DSocial**, la tarjeta con **Tu ID** para compartirlo y la tarjeta del contacto con su foto de perfil y un punto de presencia (verde si hay actividad reciente, gris si no hay un latido válido). Los pies de pantalla indican los atajos: **L** abre tu perfil, **Y** el perfil del contacto y **A** abre el chat. La pantalla inferior muestra la lista **Conversaciones** con el número de contactos y cuatro botones de acción: **X Añadir** (agregar por ID), **Buscar** (encontrar usuarios por nombre o apodo), **Conocer** (personas abiertas a nuevos contactos) y **Extraños** (mensajes de remitentes fuera de tu lista).

### Chat: mensajes, multimedia e historial

<img src="img/5089159173861215460_121.jpg" alt="Historial del chat de 3DSocial paginado en la pantalla superior y controles de la pestaña Mensajes en la inferior" width="720">

El chat en modo **Historial**. La pantalla superior muestra el encabezado con el contacto, el contador «52 mensajes | página 17/32» con una barra de progreso del recorrido y los mensajes de esa página; la foto enviada aparece como burbuja **Tú / FOTO** con su vista previa en miniatura y la opción **Ver grande**. La pantalla inferior mantiene la pestaña **Mensajes** con **A Escribir mensaje**, **Y Audio**, **X Foto**, **L Antes** y **R Nuevos** para retroceder o avanzar por el historial, y **B Volver** para regresar a los mensajes recientes.

<img src="img/5089159173861215461_121.jpg" alt="Pestaña Multimedia del chat de 3DSocial con notas de voz y fotos, y el botón Borrar contacto" width="720">

La pestaña **Multimedia** del mismo chat. Arriba se ve una nota de voz propia («Tú / Audio 0:12») y un mensaje de texto recibido. Abajo, la pestaña reúne dos secciones: **Notas de voz** con **A Reproducir** y flechas para cambiar de audio, y **Fotos** con **A Ver foto**. El botón rojo **Borrar contacto** y **B Volver al chat** completan la pantalla; desde aquí también se accede al historial paginado de la foto anterior.

<img src="img/5086840346787908777_121.jpg" alt="3DSocial en una Nintendo 3DS: mensajes en la pantalla superior y controles de chat, voz, fotos e historial en la inferior" width="720">

Esta foto corresponde a una versión anterior de los controles del chat. Ya incluía la conversación en la pantalla superior con el progreso del historial y el aviso verde de mensajes al día, y los botones grandes para escribir, grabar audio, tomar fotos y recorrer el historial con **L/R**. La pestaña **Multimedia** ya reunía audio y fotos: al cambiar la selección, el chat muestra el mensaje correspondiente. Las fotos tienen una vista previa de 80×48 dentro de la burbuja y el mensaje nuevo recibe un breve aviso visual.

### Cámara y vista previa

<img src="img/5086840346787908778_121.jpg" alt="Vista previa de la cámara de 3DSocial con los botones Tomar foto y Cancelar" width="720">

La cámara presenta la imagen en la pantalla superior. En la inferior puedes tomar la foto con **A** o cancelar con **B**; después de capturarla, la app permite revisarla antes de enviarla.
La imagen en vivo, las fotos y sus miniaturas conservan los canales rojo y azul correctos del formato RGB565 de la cámara. Las fotos guardadas anteriormente se muestran con los colores corregidos al instalar esta versión.

### Búsqueda, Conocer gente y Extraños

<img src="img/5089159173861215459_121.jpg" alt="Pantalla Buscar usuarios de 3DSocial con el campo de búsqueda y los botones Volver, Por nombre y Conocer" width="720">

**Buscar usuarios** solo muestra los perfiles que activaron **Aparecer en búsquedas**. La pantalla superior indica «Toca Buscar para escribir» y el contador de resultados; la inferior ofrece **Volver**, **Por nombre** para abrir el teclado y **Conocer** para cambiar al otro modo de descubrimiento. La búsqueda encuentra coincidencias parciales del nombre o apodo sin distinguir mayúsculas ni acentos, y los resultados se limitan a 20.

<img src="img/5089159173861215458_121.jpg" alt="Pantalla Conocer gente de 3DSocial con personas abiertas a nuevos contactos" width="720">

**Conocer gente** es la otra cara de la búsqueda: aparecen únicamente los perfiles con la opción **Aparecer en Conocer** activada. La estructura es la misma —búsqueda opcional por nombre, contador de resultados y los mismos botones de navegación—. Desde un resultado puedes abrir el perfil y agregar a esa persona como contacto.

<img src="img/5089159173861215454_121.jpg" alt="Bandeja Mensajes de extraños de 3DSocial vacía, con contadores y botón Volver" width="720">

**Mensajes de extraños**: la bandeja de remitentes que no figuran en tu lista local. La pantalla superior resume «0 conversaciones / 0 mensajes pendientes» y la inferior muestra el estado vacío con **B Volver** y la paginación **Anteriores / Siguientes**. Con mensajes pendientes, cada conversación se abre con **A** para leer y responder sin agregar al remitente, y **X** lo guarda como contacto. Los contadores se actualizan sin consumir los mensajes; abrir el chat los marca leídos en el servidor. El servidor deja de mostrar una consola como conectada tras 60 segundos sin latido. Para usar esta bandeja y la presencia, actualiza el servidor y la consola a esta versión.

### Editor de perfil

<img src="img/5089159173861215457_121.jpg" alt="Editor de perfil de 3DSocial, primera página, con la vista previa de la cámara en la pantalla superior" width="720">

Primera página del editor de perfil (1/7 de 14 campos), al que se llega desde **Contactos → L / Mi perfil**. La pantalla superior sirve de visor en vivo de la cámara para la foto de perfil. La inferior edita **Usuario**, **Apodo**, **Biografía**, **Descripción**, **Aparecer en búsquedas**, **Aparecer en Conocer** y **Contraseña**; **A** cambia el campo elegido, **X** lo borra y **B** vuelve.

<img src="img/5089159173861215456_121.jpg" alt="Editor de perfil de 3DSocial, última página, con foto, colores personalizados y Sincronizar" width="720">

Última página del editor (campos 5–14 de 14): **Foto** con **A / Tomar foto** y la personalización visual del chat: **Color burbuja**, **Fondo del chat**, **Color del texto**, **Color del nombre** y **Color del apodo**. **Sincronizar / A Guardar** publica los cambios; tus contactos verán esos colores cuando sincronicen tu perfil.

<img src="img/5086840346787908782_121.jpg" alt="Pestaña Perfil de 3DSocial con nombre, apodo, biografía, descripción, foto y botón Sincronizar" width="720">

Esta foto corresponde a una versión anterior de los ajustes, cuando el perfil también se editaba desde la pestaña **Perfil** de la pantalla de ajustes, con nombre de usuario, apodo, biografía, descripción y foto de perfil con la cámara, y el botón **Sincronizar**. En la versión actual, el perfil se edita desde **Contactos → L**, como muestran las dos fotos anteriores.

### Ajustes de la aplicación

<img src="img/5086840346787908776_121.jpg" alt="Pestaña General de 3DSocial con interruptores de modo oscuro, LED, sonido y actualizaciones" width="720">

Aquí se activan el modo oscuro, el LED, el sonido de notificación y la búsqueda de actualizaciones. Para oír la campanita, **Sonido de notificación** debe estar activado y el DSP debe estar preparado como se explica más abajo.

<img src="img/5086840346787908774_121.jpg" alt="Pestaña Conexión de 3DSocial con host HTTP, puerto, intervalo de consulta y botón Probar" width="720">

La pestaña **Conexión** permite escribir la dirección del servidor y el puerto, cambiar el intervalo de consulta y activar o desactivar el cifrado de datos sobre HTTP, probar la conexión y guardar los ajustes. La dirección `192.168.1.35` de la foto es solo un ejemplo de red local.

### Versiones anteriores

Fotos de builds previos que se conservan en la biblioteca para comparar la evolución de la interfaz.

<img src="img/5086840346787908781_121.jpg" alt="Lista de contactos de la versión 1.2.4 de 3DSocial en una 3DS roja" width="720">

La lista de contactos de la **v1.2.4**: la pantalla superior mostraba el contacto seleccionado y tu ID, y la inferior la lista con los botones **Añadir**, **Perfil** y el acceso a ajustes. En la versión actual, esta pantalla se convirtió en la de contactos y conversaciones de la primera foto de esta guía.

<img src="img/5086840346787908779_121.jpg" alt="Primer plano de la barra de acciones del chat en una versión anterior de 3DSocial" width="720">

Primer plano de la barra de acciones del chat de esa misma época: **A Escribir**, **Y Mic** (mantén para voz), **X Foto**, la navegación **L Anteriores / R Recientes** y los selectores de audio y foto con **Reproducir** y **Ver grande**.

<img src="img/5086840346787908775_121.jpg" alt="Conversación en la pantalla superior de 3DSocial con burbujas de texto y foto" width="720">

La conversación en la pantalla superior de esa versión: las burbujas propias («Tú») en azul y la foto enviada como burbuja destacada con la etiqueta de sus dimensiones (400×240).

<img src="img/5086840346787908780_121.jpg" alt="Banner de 3DSocial en la pantalla superior y el icono en el menú HOME de una 3DS roja" width="720">

El banner de 3DSocial en la pantalla superior y el icono de la aplicación seleccionado en el menú HOME de una 3DS roja, tal como se ve tras instalar el `.cia` con FBI.

## Requisitos

- Una Nintendo 3DS/2DS capaz de ejecutar homebrew. Para instalar el `.cia` necesitas un instalador de títulos, como FBI.
- Tarjeta SD y conexión de red entre la consola y el servidor elegido.
- **Para la reproducción de audio:** el componente DSP disponible para `ndsp`. El método recomendado en esta guía es generar `sd:/3ds/dspfirm.cdc` desde Rosalina.
- Para ejecutar el servidor de ejemplo: Python 3.8 o posterior con `cryptography` en un equipo de la misma red.

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

Descarga los binarios desde las [Releases de GitHub](https://github.com/miventech/3DSocial---Mensajeria-para-Nintendo-3DS/releases). El repositorio excluye `.cia` y `.3dsx` del historial de código; estos archivos se adjuntan a la versión publicada. Elige uno de los formatos:

| Formato | Instalación | Inicio |
| --- | --- | --- |
| `3DSocial.cia` | Cópialo a la SD e instálalo con FBI | Icono de 3DSocial en el menú HOME |
| `3DSocial.3dsx` | Cópialo a `sd:/3ds/3DSocial/3DSocial.3dsx` | Homebrew Launcher |

El CIA usa el Title ID `000400000F9F8F01`, el código de producto `CTR-P-3DSC` y el código de compañía `MV` (Miventech), distintos de los de Mensinger 3DS. Si instalas ambos, pueden aparecer como aplicaciones separadas.

## Primer uso y conexión al servidor

1. Conecta la 3DS a la misma red que el servidor. Al abrir la app por primera vez se genera tu ID; compártelo con la persona que quieras agregar.
2. Para una prueba local, inicia [el servidor de ejemplo](ServerExample/README.md) en tu computadora. En Windows, abre `ServerExample/IniciarServidorConCifrado.bat` para HTTP con datos cifrados o `ServerExample/IniciarServidorSinCifrado.bat` para HTTP en claro; desde una terminal también puedes ejecutar:

   ```powershell
   cd ServerExample
   py -3 -m pip install cryptography
   py -3 server.py --port 8080
   ```

3. Averigua la dirección IPv4 local de esa computadora, por ejemplo con `ipconfig` en Windows. En 3DSocial abre **Configuración → Conexión** y escribe solo el host, por ejemplo `192.168.1.50`, sin esquema ni barra final. Usa el puerto `8080`. Si abriste el `.bat` con cifrado, deja **Cifrar conexión: SI (HTTP cifrado)**: la app guarda la clave pública del servidor automáticamente. Si abriste el `.bat` sin cifrado, selecciona **NO (HTTP)**.
4. Pulsa **Probar** y guarda los ajustes. Mantén abierta la terminal del servidor mientras uses el chat. Si el firewall pide permiso para Python, permite el acceso en la red privada.
5. Pulsa **Añadir contacto**, escribe su ID numérico y un nombre. Selecciona el contacto para abrir la conversación.

La app trae configurado inicialmente `gabxriel.pythonanywhere.com:80`, pero el canal cifrado está activado de forma predeterminada y requiere un servidor actualizado. Configura el servidor de ejemplo o selecciona **Cifrar conexión: NO (HTTP)** para un servidor antiguo compatible; en la pantalla inicial de error puedes pulsar **L** para alternar el modo. El modo en claro transmite contenido y contraseñas sin protección por la red. Para las funciones multimedia, el servidor debe implementar las rutas descritas en [ServerExample](ServerExample/README.md). `127.0.0.1` en la 3DS apunta a la propia consola, no a tu computadora.

## Controles principales

| Pantalla | Acción |
| --- | --- |
| Contactos | Toca un contacto o selecciónalo con la cruceta y **A** para abrir el chat; **X** añade un contacto; **Y** abre su perfil; **L** abre tu perfil. El engranaje de la pantalla superior indica **SELECT** para abrir Ajustes. |
| Chat, pestaña Mensajes | **A** escribe; mantén **Y** para grabar y suelta para enviar; **B** cancela la grabación o vuelve; **X** abre la cámara. **L/R** cambia entre mensajes anteriores y recientes. |
| Chat, pestaña Multimedia | Toca la pestaña para elegir y reproducir notas de voz o abrir fotos. La cruceta y **A** también recorren sus controles; **B** vuelve a Mensajes. |
| Cámara | **A** toma la foto; en la vista de confirmación, **A** la envía, **X** repite y **B** descarta. |
| Mi perfil | Desde Contactos, **L** abre el editor de perfil. **A** cambia la opción elegida, **X** borra el campo seleccionado y **B** vuelve. |
| Ajustes (SELECT) | Solo incluye **General** y **Conexión**. **L/R** cambia entre esas pestañas y **B** vuelve. |

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
| `source/crypto/` | Monocypher, biblioteca criptográfica portable incluida para 3DS. |
| `source/media/` | Cámara, fotos, grabación, reproducción y sonido de notificación. |
| `ServerExample/` | Servidor Python de ejemplo y pruebas; guarda mensajes y medios en SQLite. |
| `ctr-led-brary/` | Biblioteca usada para las notificaciones LED. |

El servidor de ejemplo usa HTTP con un sobre cifrado por defecto y permite HTTP en claro si se desactiva. La primera clave pública se obtiene automáticamente mediante confianza en el primer uso; esa primera conexión puede ser suplantada por alguien con control de la red. Los perfiles con contraseña requieren autenticación, pero los que dejan ese campo vacío permiten el acceso con su ID. [Detalles del servidor](ServerExample/README.md).

Para entender el recorrido completo de la app, los contratos HTTP, el cifrado de transporte y SQLite, la autenticación, los formatos multimedia y el monitor, consulta la [documentación técnica](docs/DOCUMENTACION_TECNICA.md).

## Origen y agradecimientos

3DSocial nació de la base y la idea de [Mensinger3DS-Backend](https://github.com/GABXRIEL/Mensinger3DS-Backend), creado por [GABXRIEL](https://github.com/GABXRIEL). Es el proyecto padre y la inspiración de esta herramienta. Gracias a GABXRIEL por ese trabajo original y por abrir el camino para seguir experimentando con mensajería en Nintendo 3DS.

Gracias también a las comunidades y herramientas de **devkitPro/libctru**, **Luma3DS/Rosalina**, **PabloMK7** por `ctr-led-brary` (licencia en [su README](ctr-led-brary/README.md)), **Monocypher** por la biblioteca criptográfica (licencia en [MONOCYPHER-LICENSE.md](source/crypto/MONOCYPHER-LICENSE.md)) y al proyecto **FBI**, cuyo flujo de captura de cámara se reconoce en [THIRD_PARTY_FBI_LICENSE.txt](THIRD_PARTY_FBI_LICENSE.txt).

Este proyecto se comparte con fines **educativos y de diversión**. No está afiliado a Nintendo.

## Licencia y créditos al reutilizar el código

El material sobre el que Miventech tiene derechos o autorización para licenciar se ofrece bajo la [Licencia Miventech de uso libre con atribución](LICENSE). Puedes usarlo, modificarlo y distribuirlo, también en proyectos comerciales y con otra licencia para tus propias modificaciones. Al publicar un proyecto que reutilice una parte sustancial, conserva la licencia e incluye un crédito visible como **«Basado en 3DSocial de Miventech»** en el README, los créditos, la pantalla «Acerca de» o la página de descarga.

Esta licencia es propia del proyecto; no debe presentarse como MIT ni como otra licencia estándar. Los derechos de terceros siguen sujetos a sus respectivas condiciones. Conserva también los avisos correspondientes, incluido [el aviso del código adaptado de FBI](THIRD_PARTY_FBI_LICENSE.txt), y acredita a GABXRIEL cuando corresponda por la base original.
