@echo off
setlocal
REM Usa UTF-8 en los mensajes y ejecuta desde esta carpeta para localizar server.py.
chcp 65001 >nul
cd /d "%~dp0"
set "PYTHON="

echo Iniciando Mensinger 3DS...
echo Carpeta: %CD%
echo.

REM Comprueba que el launcher realmente encuentre Python 3; "where py" no basta.
py -3 -c "import sys; sys.exit(sys.version_info < (3, 8))" >nul 2>nul
if not errorlevel 1 set "PYTHON=py -3"
if not defined PYTHON (
    python -c "import sys; sys.exit(sys.version_info < (3, 8))" >nul 2>nul
    if not errorlevel 1 set "PYTHON=python"
)
if not defined PYTHON goto sin_python

%PYTHON% -c "import cryptography" >nul 2>nul
if errorlevel 1 (
    echo Falta cryptography en el Python que ejecuta el servidor.
    echo Instalando la dependencia con ese mismo Python...
    %PYTHON% -m pip install cryptography
    if errorlevel 1 goto error_dependencia
    %PYTHON% -c "import cryptography" >nul 2>nul
    if errorlevel 1 goto error_dependencia
)

echo Python: %PYTHON%
%PYTHON% server.py %*
goto fin

:sin_python
echo ERROR: No se encontro Python 3.8 o superior.
echo Instala Python 3 y vuelve a ejecutar este archivo.
goto fin

:error_dependencia
echo ERROR: No se pudo instalar cryptography.
echo Comprueba la conexion a Internet y ejecuta: %PYTHON% -m pip install cryptography
echo Despues vuelve a abrir IniciarServidor.bat.

:fin
REM Mantiene visible cualquier error de arranque hasta que el usuario cierre la ventana.
echo.
echo El servidor se detuvo. Pulsa una tecla para cerrar esta ventana.
pause >nul
endlocal
