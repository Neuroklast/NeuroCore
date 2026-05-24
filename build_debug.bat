@echo off
setlocal

REM NeuroCore Debug Build - verwendet Ninja (in VS2022 Build Tools enthalten)
REM JUCE_DIR anpassen falls noetig!

if not defined JUCE_DIR (
    set "JUCE_DIR=D:\JUCE"
    echo JUCE_DIR nicht gesetzt, verwende Default: D:\JUCE
    echo (bei Bedarf JUCE_DIR als Umgebungsvariable setzen oder direkt im Skript aendern)
)
REM Projekt-Default: D:\JUCE (Issue-Vorgabe; via JUCE_DIR-Umgebungsvariable ueberschreibbar)

echo ===========================================
echo  NeuroCore DEBUG Build
echo  JUCE_DIR=%JUCE_DIR%
echo ===========================================

REM Ninja-Pfad aus VS2022 Build Tools
set "VS_NINJA=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "VS_CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
set "VS_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VS_NINJA%\ninja.exe" (
    echo FEHLER: Ninja wurde nicht unter "%VS_NINJA%\ninja.exe" gefunden.
    echo Passe VS_NINJA im Skript an, falls VS Build Tools an einem anderen Ort installiert sind.
    pause
    exit /b 1
)

if not exist "%VS_CMAKE%\cmake.exe" (
    echo FEHLER: CMake wurde nicht unter "%VS_CMAKE%\cmake.exe" gefunden.
    echo Passe VS_CMAKE im Skript an, falls VS Build Tools an einem anderen Ort installiert sind.
    pause
    exit /b 1
)

REM PATH um Ninja erweitern
set "PATH=%VS_NINJA%;%VS_CMAKE%;%PATH%"

REM Pruefe ob Ninja vorhanden
where ninja >nul 2>&1
if errorlevel 1 (
    echo FEHLER: Ninja nicht gefunden!
    echo Stelle sicher dass VS2022 Build Tools installiert sind.
    pause
    exit /b 1
)

REM Build-Verzeichnis
set "BUILD_DIR=out\build\x64-Debug-Ninja"

REM Cache loeschen fuer sauberen Build
if exist "%BUILD_DIR%\CMakeCache.txt" (
    echo Loesche alten CMake Cache...
    del /f /q "%BUILD_DIR%\CMakeCache.txt"
)

REM VS2022 Developer-Umgebung aktivieren fuer cl.exe
if not exist "%VS_VCVARS%" (
    echo FEHLER: vcvars64.bat nicht gefunden: %VS_VCVARS%
    echo Passe VS_VCVARS im Skript an, falls VS Build Tools an einem anderen Ort installiert sind.
    pause
    exit /b 1
)
call "%VS_VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo FEHLER: Konnte VS2022 Developer-Umgebung nicht initialisieren (vcvars64.bat).
    pause
    exit /b 1
)

REM Konfigurieren mit Ninja
echo.
echo [1/2] Konfiguriere CMake mit Ninja...
cmake -B "%BUILD_DIR%" -S . -G "Ninja Multi-Config" -DJUCE_DIR="%JUCE_DIR%"
if errorlevel 1 (
    echo FEHLER: CMake Konfiguration fehlgeschlagen!
    pause
    exit /b 1
)

REM Bauen
echo.
echo [2/2] Baue NeuroCore Debug...
cmake --build "%BUILD_DIR%" --config Debug --target NeuroCore
if errorlevel 1 (
    echo FEHLER: Build fehlgeschlagen!
    pause
    exit /b 1
)

echo.
echo ===========================================
echo  Build erfolgreich!
echo  VST3: %BUILD_DIR%\NeuroCore_artefacts\Debug\VST3\NeuroCore.vst3
echo ===========================================
pause
