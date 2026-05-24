@echo off
setlocal

REM NeuroCore Debug Build
REM Setzt JUCE_DIR auf E:\JUCE falls nicht bereits gesetzt
REM Aendern falls JUCE woanders liegt!

if not defined JUCE_DIR (
    set "JUCE_DIR=E:\JUCE"
    echo JUCE_DIR war nicht gesetzt, verwende Default: E:\JUCE ^(bei Bedarf im Skript anpassen^)
)

echo ===========================================
echo  NeuroCore Debug Build
echo  JUCE_DIR=%JUCE_DIR%
echo ===========================================

REM CMake Cache loeschen fuer sauberen Build
if exist "out\build\x64-Debug\CMakeCache.txt" (
    echo Loesche alten CMake Cache...
    del /f /q "out\build\x64-Debug\CMakeCache.txt"
)

REM Konfigurieren
echo.
echo [1/2] Konfiguriere CMake...
cmake -B "out\build\x64-Debug" -S . -DJUCE_DIR="%JUCE_DIR%" -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 (
    echo FEHLER: CMake Konfiguration fehlgeschlagen!
    pause
    exit /b 1
)

REM Bauen
echo.
echo [2/2] Baue NeuroCore Debug...
cmake --build "out\build\x64-Debug" --config Debug --target NeuroCore
if errorlevel 1 (
    echo FEHLER: Build fehlgeschlagen!
    pause
    exit /b 1
)

echo.
echo ===========================================
echo  Build erfolgreich!
echo  VST3: out\build\x64-Debug\NeuroCore_artefacts\Debug\VST3\
echo ===========================================
pause
