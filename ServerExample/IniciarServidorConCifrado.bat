@echo off
REM Inicia el servidor con HTTPS. Se pueden agregar opciones como --tls-host IP.
call "%~dp0IniciarServidor.bat" %* --cifrado-conexion si
