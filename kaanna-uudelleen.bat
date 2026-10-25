@echo off
rem Kääntää pelin uudelleen (config\game.toml muuttunut tai lähdekoodi muuttunut). Sama ympäristö kuin build.bat.
rem Visual Studio haetaan vswherella kuten build.bat:ssa (mika tahansa painos, jossa C++-tyokalut)
set "VSWHERE_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
rem varapolku, jos ProgramFiles(x86)-muuttujaa ei ole
if not exist "%VSWHERE_EXE%" set "VSWHERE_EXE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE_EXE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (echo Visual Studio C++-tyokaluineen puuttuu: aja build.bat & exit /b 1)
call "%VSDIR%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul
set "VSCMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "PATH=%VSCMAKE%\CMake\bin;%VSCMAKE%\Ninja;%VSDIR%\VC\Tools\Llvm\x64\bin;%PATH%"
cd /d "%~dp0"
cmake --build out\build\release || exit /b 1
rem Runtime pitää DLL:ää vanhentuneena, jos exe on sitä uudempi (game_code.cpp: "current when newer than the executable"),
rem ja linkittäisi sen itse uudelleen käynnistyksessä. Pelkkä runtime-muutos linkittää vain exen, joten kosketetaan DLL:ää.
if exist out\build\release\SimsBustinOut.dll copy /b out\build\release\SimsBustinOut.dll+,, out\build\release\SimsBustinOut.dll >nul
