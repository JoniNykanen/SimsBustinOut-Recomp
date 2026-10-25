@echo off
rem release.bat            SimsBustinOut.zip: the executable and the compiler, no game files; the player's
rem                        first start builds the game code from their own disc
rem release.bat with-game  SimsBustinOut-with-game-files.zip: ready to play, with assets\ and the built game
rem                        code, for moving your own copy to another PC of yours. Not for sharing.
set "GAME_EXE=SimsBustinOut"
cd /d "%~dp0"
set "OUT=out\build\release"
rem 7-Zip makes the zip: on the PATH, or where its installer puts it
set "SEVENZIP=7z"
where 7z >nul 2>nul || set "SEVENZIP=%ProgramFiles%\7-Zip\7z.exe"
if not "%SEVENZIP%"=="7z" if not exist "%SEVENZIP%" (echo 7-Zip not found: install it from https://www.7-zip.org & exit /b 1)
rem the Visual C++ runtime beside the executable, so a fresh Windows install needs nothing more
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSDIR=%%i"
for /d %%v in ("%VSDIR%\VC\Redist\MSVC\1*") do for /d %%c in ("%%v\x64\Microsoft.VC*.CRT") do set "VCRUNTIME=%%c"
if not exist "%VCRUNTIME%\vcruntime140.dll" (echo Visual C++ runtime not found: install Visual Studio with the C++ workload & exit /b 1)
if not exist "%OUT%\%GAME_EXE%.exe" call build.bat || exit /b 1
rem the configure step writes where the SDK is
if not exist "%OUT%\sdk_dir.txt" cmake --preset release >nul || exit /b 1
if not exist "%OUT%\sdk_dir.txt" echo %OUT%\sdk_dir.txt was not written, update the SDK. && exit /b 1
set /p SDK=<out\build\release\sdk_dir.txt
rem only the runtime DLLs the executables import; xbe_recomp.exe needs its own copy, Windows only looks beside the
rem exe it starts
del /q "%OUT%\compiler\*.dll" 2>nul
for %%f in (msvcp140 vcruntime140) do copy /y "%VCRUNTIME%\%%f.dll" "%OUT%\compiler\" >nul
if /i "%~1"=="with-game" goto :withgame

del /q "%GAME_EXE%.zip" 2>nul
rem an empty assets folder beside the exe, where the game (files or .iso) goes
rd /s /q "%OUT%\release" 2>nul
mkdir "%OUT%\release\assets"
rem the compiler folder without its work folder: no game code goes in the zip, the first start builds it
"%SEVENZIP%" a "%GAME_EXE%.zip" ".\%OUT%\%GAME_EXE%.exe" ".\%OUT%\release\assets" ".\%OUT%\compiler" .\README.md "%SDK%\THIRD_PARTY.txt" "%VCRUNTIME%\msvcp140.dll" "%VCRUNTIME%\vcruntime140.dll" "%VCRUNTIME%\vcruntime140_1.dll" -xr!work || exit /b 1
exit /b 0

:withgame
if not exist assets\default.xbe (echo assets\default.xbe is missing & exit /b 1)
if not exist "%OUT%\%GAME_EXE%.dll" (echo %OUT%\%GAME_EXE%.dll is missing: build the game first & exit /b 1)
set "STAGE=%OUT%\release-with-game\%GAME_EXE%"
rd /s /q "%OUT%\release-with-game" 2>nul
mkdir "%STAGE%"
for %%f in ("%OUT%\%GAME_EXE%.exe" "%OUT%\%GAME_EXE%.dll" README.md "%SDK%\THIRD_PARTY.txt" "%VCRUNTIME%\msvcp140.dll" "%VCRUNTIME%\vcruntime140.dll" "%VCRUNTIME%\vcruntime140_1.dll") do copy /y %%f "%STAGE%\" >nul || exit /b 1
rem robocopy's exit codes below 8 are success
robocopy assets "%STAGE%\assets" /e /njh /njs /nfl /ndl >nul
if errorlevel 8 (echo copying assets failed & exit /b 1)
rem the compiler stays as the fallback: an unzipper that drops file times makes the game build its code again
robocopy "%OUT%\compiler" "%STAGE%\compiler" /e /xd work /njh /njs /nfl /ndl >nul
if errorlevel 8 (echo copying the compiler failed & exit /b 1)
rem The runtime takes an unstamped game code DLL as current only while it is newer than the exe (game_code.cpp),
rem and a zip keeps times to 2 s: the DLL goes a minute past the exe so no rebuild starts on the other PC.
powershell -NoProfile -Command "$e = Get-Item '%STAGE%\%GAME_EXE%.exe'; (Get-Item '%STAGE%\%GAME_EXE%.dll').LastWriteTime = $e.LastWriteTime.AddMinutes(1)" || exit /b 1
del /q "%GAME_EXE%-with-game-files.zip" 2>nul
pushd "%OUT%\release-with-game"
"%SEVENZIP%" a "%~dp0%GAME_EXE%-with-game-files.zip" "%GAME_EXE%" || (popd & exit /b 1)
popd
echo %GAME_EXE%-with-game-files.zip: unzip anywhere writable and start %GAME_EXE%\%GAME_EXE%.exe
exit /b 0
