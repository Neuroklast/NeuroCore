@echo off
setlocal

REM NeuroCore Release Build
REM Setzt JUCE_DIR auf E:\JUCE falls nicht bereits gesetzt
REM Aendern falls JUCE woanders liegt!

if not defined JUCE_DIR (
    set "JUCE_DIR=E:\JUCE"
)

echo ===========================================
echo  NeuroCore RELEASE Build
echo  JUCE_DIR=%JUCE_DIR%
echo ===========================================

REM CMake Cache loeschen fuer sauberen Build
if exist "out\build\x64-Release\CMakeCache.txt" (
    echo Loesche alten CMake Cache...
    del /f /q "out\build\x64-Release\CMakeCache.txt"
)

REM Konfigurieren
echo.
echo [1/2] Konfiguriere CMake...
cmake -B "out\build\x64-Release" -S . -DJUCE_DIR="%JUCE_DIR%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo FEHLER: CMake Konfiguration fehlgeschlagen!
    pause
    exit /b 1
)

REM Bauen
echo.
echo [2/2] Baue NeuroCore Release...
cmake --build "out\build\x64-Release" --config Release --target NeuroCore
if errorlevel 1 (
    echo FEHLER: Build fehlgeschlagen!
    pause
    exit /b 1
)

echo.
echo ===========================================
echo  Release Build erfolgreich!
echo  VST3: out\build\x64-Release\NeuroCore_artefacts\Release\VST3\
echo  Die VST3 wurde automatisch nach C:\Users\...\AppData\Roaming\VST3\ kopiert
echo ===========================================
pause
