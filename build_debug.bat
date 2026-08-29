@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM Windows Debug. Same tree as AGENTS.md (`build/`), Debug config.

set "VERSION_LABEL=0.6.4-beta"
set "FILE_STEM=NEUROKORE-%VERSION_LABEL%"
set "BUILD_DIR=build"
set "CONFIG=Debug"
set "TARGET=NeuroKore_All"
if /i "%~1"=="/nopause" set "NOPAUSE=1"

echo ===========================================
echo  NEUROKORE DEBUG
echo  %VERSION_LABEL%  target %TARGET%
echo ===========================================

call :find_cmake
if errorlevel 1 exit /b 1

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo.
    echo [1/2] cmake -B %BUILD_DIR% -S .
    "%CMAKE%" -B "%BUILD_DIR%" -S .
    if errorlevel 1 (
        echo FEHLER: CMake configure fehlgeschlagen.
        call :maybe_pause
        exit /b 1
    )
) else (
    echo [1/2] %BUILD_DIR%\CMakeCache.txt vorhanden — configure uebersprungen.
)

echo.
echo [2/2] cmake --build %BUILD_DIR% --config %CONFIG% --target %TARGET%
"%CMAKE%" --build "%BUILD_DIR%" --config %CONFIG% --target %TARGET%
if errorlevel 1 (
    echo FEHLER: Debug-Build fehlgeschlagen.
    call :maybe_pause
    exit /b 1
)

set "VST3=%BUILD_DIR%\NeuroKore_artefacts\%CONFIG%\VST3\%FILE_STEM%.vst3"
set "VST3_BUNDLE=%BUILD_DIR%\NeuroKore_artefacts\%CONFIG%\VST3\NEUROKORE.vst3"
set "EXE=%BUILD_DIR%\NeuroKore_artefacts\%CONFIG%\Standalone\%FILE_STEM%.exe"

echo.
echo ===========================================
echo  Debug OK
if exist "%VST3%" echo  VST3: %VST3%
if exist "%VST3_BUNDLE%" echo  VST3: %VST3_BUNDLE%
if exist "%EXE%" echo  EXE:  %EXE%
echo ===========================================
call :maybe_pause
exit /b 0

:find_cmake
where cmake >nul 2>&1
if not errorlevel 1 (
    set "CMAKE=cmake"
    exit /b 0
)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -find CMake\bin\cmake.exe`) do (
        if exist "%%I" (
            set "CMAKE=%%I"
            exit /b 0
        )
    )
)
echo FEHLER: cmake nicht im PATH und nicht in Visual Studio gefunden.
call :maybe_pause
exit /b 1

:maybe_pause
if defined NOPAUSE exit /b 0
if defined CI exit /b 0
pause
exit /b 0
