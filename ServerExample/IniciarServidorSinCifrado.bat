@echo off
REM Inicia el servidor con HTTP. La base de datos sigue cifrada.
call "%~dp0IniciarServidor.bat" %* --cifrado-conexion no
