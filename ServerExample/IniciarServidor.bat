@echo off
setlocal
REM Usa UTF-8 en los mensajes y ejecuta desde esta carpeta para localizar server.py.
chcp 65001 >nul
cd /d "%~dp0"

echo Iniciando Mensinger 3DS...
echo Carpeta: %CD%
echo.

where py >nul 2>nul
if not errorlevel 1 (
    REM El launcher py permite elegir explícitamente Python 3.
    py -3 server.py %*
    goto fin
)

where python >nul 2>nul
if not errorlevel 1 (
    REM Alternativa para equipos que no tienen instalado el launcher py.
    python server.py %*
    goto fin
)

echo ERROR: No se encontro Python.
echo Instala Python 3 y vuelve a ejecutar este archivo.

:fin
REM Mantiene visible cualquier error de arranque hasta que el usuario cierre la ventana.
echo.
echo El servidor se detuvo. Pulsa una tecla para cerrar esta ventana.
pause >nul
endlocal
