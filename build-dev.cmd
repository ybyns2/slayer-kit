@echo off
setlocal
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cmake -S "%~dp0." -B "%~dp0build-dev" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSK_DEV=ON || exit /b 1
cmake --build "%~dp0build-dev" || exit /b 1
echo OK : %~dp0build-dev\SlayerKit.exe
