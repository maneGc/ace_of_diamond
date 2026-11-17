@echo off
rem demo.bat - compila (si hace falta) y ejecuta el demo del Avance 1.
rem Uso:  demo.bat                       Barry Bonds, 2001
rem       demo.bat "Aaron Judge" 2022
rem       doble clic en el archivo tambien funciona.
setlocal
chcp 65001 >nul
cd /d "%~dp0"

if exist "%~dp0demo.exe" goto run

where gcc >nul 2>nul || set "PATH=C:\msys64\ucrt64\bin;%PATH%"
where gcc >nul 2>nul || (
    echo No encontre gcc. Instala MSYS2 con el paquete mingw-w64-ucrt-x86_64-gcc.
    goto end
)
echo Compilando demo.exe ...
gcc -std=c11 -O2 -Iengine/include engine/src/data.c engine/src/prior.c engine/src/aging.c engine/src/talent.c engine/src/demo/demo.c -lm -o demo.exe || goto end

:run
"%~dp0demo.exe" %*

:end
rem Si algo falló, deja la ventana abierta para leer el error.
if errorlevel 1 pause
endlocal
