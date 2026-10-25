# The Sims Bustin' Out - XBE Recomp

An unofficial native PC port of the original Xbox version of The Sims Bustin' Out (Europe / PAL release, title 45410043), statically recompiled to Windows and Direct3D 12.

This port was created using the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBERecomp) tool and follows the USA port [abilliontokens/bustinout-recomp](https://github.com/abilliontokens/bustinout-recomp), whose game hooks, keyboard layer, overlay, cheats and SDK fixes it carries over.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build.

## Installation

- Put your game files in the `assets` folder. This can be the disc image (`.iso`), or the extracted data files with `default.xbe` and the game's folders beside it.
- Run `SimsBustinOut.exe`. The first start compiles the game's code for your PC, this only happens once.
- Settings can be adjusted in `settings.toml` beside the exe, or in the game with F1.
- Extract the package to a folder you can write to (not `Program Files`): the compiled game code, `settings.toml` and the saves are written beside the exe.
- Without a `settings.toml` the game starts borderless at the desktop resolution, 60 fps with vsync, render scale 100 %, anisotropic filtering off, in English. `settings.toml` is written when you press Save in F1.
- Your saves are copied to `save-backups` beside the exe each time the game starts and they have changed; the ten newest copies are kept.

## Features

### Display
- Borderless, fullscreen or windowed, at the desktop resolution or one of your choice.
- Render scale from 50 % to 200 % of the output height; the game's 640x480 frame is rendered at that size.
- Vertical sync and a frame rate cap: a fixed rate, the monitor's refresh rate, or unlimited.
- The picture fills the window without the black bars the console left in the overscan rows, and is shown 16:9 on a widescreen output as the console did on a widescreen TV. The aspect ratio can also be set to 4:3 (`aspect` in `settings.toml`, or F1).
- Anisotropic texture filtering, forced where the game already filters across mip levels.

### Graphics
- The game's own 2x2 supersampled back buffer is honoured, so menus, text and the HUD are the size the game drew them.
- The gamma ramp the game blanks for its videos is kept, so the videos stay visible.
- The videos play from the disc's 29.97 fps NTSC cut instead of the 25 fps PAL cut, which judders on a 60 Hz display (`ntsc_videos = false` for the PAL ones).
- 2D sprites and text sample only inside their own part of a texture atlas, so no seam lines appear above 100 % render scale.

### General
- **Correct speed at any frame rate** - the console engine stepped the world a whole PAL tick (20 ms) a frame, so it ran 1.2x too fast at 60 fps and faster still uncapped. The frame step is replaced by a 1 ms quantiser that advances by the time that really passed.
- **Every language on the disc** - English, English (UK), French, German, Italian, Spanish, Dutch, Danish, Swedish, Norwegian and Finnish, chosen in F1 or with `language` in `settings.toml` (`auto`, `en-gb`, `fr`, `de`, `it`, `es`, `nl`, `da`, `sv`, `no`, `fi`). The console could only reach the first five through its system language. Takes effect at the next start.
- **F1 settings panel** - display, quality, input, language and sound, with Apply and Save.
- **F2 performance panel** - frame rate, frame time graph, render target and renderer counters.
- **Background mute** - the game goes quiet while another window is active (`mute_in_background`).
- **Cheats** - the game's own pad codes (cheat gnome, give money, fill motives, unlock all objects / locations / socials / skins, team picture), entered from the F1 panel exactly as the pad sequence would enter them. Nothing is saved.

### Controller
Supports Xbox, PlayStation and Switch controllers, with hot plugging and rumble. The console's buttons map as follows (the cheat codes use the console names):

| Console button | Xbox One / Series | PlayStation | Switch Pro |
|---|---|---|---|
| Black | RB | R1 | R |
| White | LB | L1 | L |
| L / R | LT / RT | L2 / R2 | ZL / ZR |
| A B X Y | by position, Xbox layout | | |

### Keyboard and mouse
The keyboard and the mouse are a controller on port 0, and merge with a real controller on that port. The mouse cursor is hidden over the picture; the camera's speed for the mouse drag is `mouse_camera_speed` (25-400 %, F1).

| Keys | Console input |
|---|---|
| W A S D | left stick: walk, menus that read the stick |
| arrow keys | D-pad: menus and dialogs |
| right mouse button held + drag | right stick: camera rotate and zoom |
| Q / E | rotate the camera |
| R / F, mouse wheel | zoom the camera |
| Space | A |
| Escape, Backspace | B |
| X / Y | X / Y |
| Z / C | left / right trigger |
| 1 / 2 | black / white |
| Enter | Start |
| Tab | Back |
| Shift / Ctrl, middle mouse button | left / right stick click |

### Miscellaneous
- **Boot Splash** - Place a `splash.png` (or `.bmp`) beside the exe to display a splash screen at launch.
- **Window icon** - the game's own dashboard image, read from your `default.xbe`.

## Troubleshooting

- If the game stops with an error, it says so and writes `logs\simsbustinout.log` and a dump in `CrashDumps`. The log of the run before is kept as `logs\simsbustinout.previous.log`, so starting the game again does not lose it. Send both files with a report.
- Your saves are in `hdd\E\UDATA\45410043`; earlier copies are in `save-backups`.

## System Requirements

### Minimum
- **OS**
  - Windows 10 64-bit
- **CPU** with SSE4.2 (Intel from 2008, AMD from 2011)
- **GPU** with support for Direct3D 12

## Building

### 1. Clone the Repository and the SDK
Clone this repository using [Git](https://git-scm.com/):

```
git clone https://github.com/JoniNykanen/SimsBustinOut-Recomp
```

The game is built with XDK 5558, which the upstream SDK does not know yet: clone the SDK branch that does beside this repository, so that `..\XboxXBE-Recomp\cmake\XbeRecomp.cmake` exists:

```
git clone -b sims-5558 https://github.com/JoniNykanen/XboxXBERecomp ..\XboxXBE-Recomp
```

### 2. Add the Required Game Files
Place your extracted game data files in the `assets` folder. `default.xbe` must be there, with the game's folders beside it.

### 3. Install Dependencies
Install Visual Studio 2022 with **Desktop development with C++**, **C++ Clang tools for Windows** and **C++ CMake tools for Windows** (`build.bat` offers to add the last two).

### 4. Build the Project
Run `build.bat`. The first build recompiles the game and takes a while. The exe is written to `out\build\release`. `kaanna-uudelleen.bat` rebuilds after a change (run it from a command prompt, not from PowerShell). `release.bat` makes `SimsBustinOut.zip` without any game files (needs [7-Zip](https://www.7-zip.org)).

`python nimea-kaatumisosoitteet.py [log]` names the guest addresses in a crash log: the XBE section, the recompiled function, the nearest `config\game.toml` symbol and whether the host replaces that function.

## License

MIT, see [LICENSE](LICENSE). Most of `src` (the game hooks, cheats, keyboard-and-mouse layer, overlay and scripted test pad) is carried over from [abilliontokens/bustinout-recomp](https://github.com/abilliontokens/bustinout-recomp) with Europe addresses; that work remains theirs, and permission to license it has been asked for in that repository.

## Credits

- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBERecomp), the recompiler and runtime this is built on.
- [abilliontokens/bustinout-recomp](https://github.com/abilliontokens/bustinout-recomp) and the `bustinout-port` branch of [abilliontokens/XboxXBERecomp](https://github.com/abilliontokens/XboxXBERecomp): the USA port whose analysis of the game (frame step, cheats, picture rect, SuperSampleScale), keyboard-and-mouse layer, overlay and XDK 5558 layout this port follows.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) for the overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis) for decoding x86 instructions, [toml++](https://github.com/marzer/tomlplusplus) for the configs, [Dear ImGui](https://github.com/ocornut/imgui) for the settings overlay and [SDL](https://github.com/libsdl-org/SDL) for the window, input and controllers.
