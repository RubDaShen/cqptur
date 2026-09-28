@echo off
rem Builds from a plain command prompt, no IDE needed.
rem Usage: build.cmd [release|debug]   (default: release)
setlocal
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=release"

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo Visual Studio with the "Desktop development with C++" workload was not found.
    exit /b 1
)

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || exit /b 1
cmake --preset %PRESET% || exit /b 1
cmake --build --preset %PRESET% || exit /b 1
echo.
echo Built: %~dp0build\%PRESET%\cqptur.exe
