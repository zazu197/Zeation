# Zeation

External Roblox health monitor with a reactive crosshair overlay.

The program watches the local player's health from outside the game (read-only memory reading - nothing is injected into Roblox and nothing inside the game is modified). Every time the player dies, the on-screen crosshair swaps to a random style. You can also drop in your own images and have it switch between those instead.

This guide assumes you have never built a C++ project before. It walks through everything step by step.

---

## Table of contents

1. [What you need](#what-you-need)
2. [Setting up the tools (one time)](#setting-up-the-tools-one-time)
3. [Building the program](#building-the-program)
4. [Getting offsets for your Roblox version](#getting-offsets-for-your-roblox-version)
5. [Running it](#running-it)
6. [Understanding the console output](#understanding-the-console-output)
7. [Custom crosshair images](#custom-crosshair-images)
8. [Command line options](#command-line-options)
9. [Troubleshooting](#troubleshooting)
10. [How it works under the hood](#how-it-works-under-the-hood)
11. [Project files](#project-files)

---

## What you need

- **Windows 10 or 11, 64-bit** (x64). Most modern PCs are.
- **Visual Studio 2022 Community** - the free edition is enough. It installs the C++ compiler and CMake.
- **The project files** (this repository).
- **An offsets file** (`offsets.json`) that matches your installed Roblox version. One is included in the repo - see [Getting offsets](#getting-offsets-for-your-roblox-version) for how to keep it fresh.

You do **not** need to install Roblox Studio, any Lua tools, or any "executor" software.

---

## Setting up the tools (one time)

### Step 1 - Install Visual Studio 2022

1. Go to https://visualstudio.microsoft.com/downloads/
2. Download **Visual Studio 2022 Community** (free).
3. Run the installer.
4. On the "Workloads" tab, tick **Desktop development with C++**.
5. Click Install and wait. It downloads several GB, so let it run.

### Step 2 - Find the Developer Command Prompt

After installation, open the Windows Start menu and search for:

```
Developer Command Prompt for VS 2022
```

Open it. This is just a normal command prompt, but it has `cmake` and the C++ compiler added to its PATH automatically. Use this window for all commands in this guide.

---

## Building the program

Open the Developer Command Prompt and navigate to the project folder (the one containing `CMakeLists.txt`). If the project is at `C:\path\to\Zeation`, type:

```bat
cd /d C:\path\to\Zeation
```

Then run these two commands:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
```

What these do:

- The first command configures the project and creates a `build` folder. It only needs to run once (or again if you add new source files).
- The second command compiles everything. After it finishes, you will see a line like:

```
ZetianExternal.vcxproj -> C:\path\to\Zeation\build\Release\ZetianExternal.exe
```

That is your program. The build also copies `offsets.json` and the `crosshairs` folder next to it automatically.

From now on, to rebuild after changing code, you only need the second command:

```bat
cmake --build build --config Release
```

---

## Getting offsets for your Roblox version

Roblox updates often, and each update changes where data lives inside the game's memory. The offsets file tells the program where to look. Using offsets from a different version than your installed Roblox makes everything fail, so this matters.

### The easy check

The program checks this for you. When it starts, it prints:

```
[info]   Offsets target Roblox version: version-ddf602d9cfe44005
[info]   Installed Roblox version:    version-ddf602d9cfe44005
```

If the two lines match, you are fine. If they differ, it prints a `[warn] Version mismatch` warning - replace the offsets file (see below).

### Getting a fresh offsets file

1. Go to https://offsets.imtheo.lol
2. Download the offsets JSON (the same format as the `offsets.json` in this repo).
3. Replace the `offsets.json` file - either:
   - The one in the project root, then rebuild (the build copies it next to the exe), or
   - Directly the one next to `ZetianExternal.exe` in `build\Release\` - no rebuild needed.

The program also accepts a custom path:

```bat
build\Release\ZetianExternal.exe C:\Downloads\my_offsets.json
```

---

## Running it

### From the terminal

```bat
build\Release\ZetianExternal.exe
```

### By double-clicking

You can also double-click `ZetianExternal.exe` in Explorer. It works the same way.

### What happens

1. A console window opens and prints status messages.
2. A transparent overlay window is created over the whole screen. It is click-through - your mouse and keyboard input go to the game, never to the overlay.
3. The program waits for `RobloxPlayerBeta.exe`. Start Roblox and join a game.
4. Once in-game, it reads your health about 10 times per second and prints it once per second.
5. When you die, the crosshair changes to a random style and it prints a death line.

### Stopping it

- Press `Ctrl+C` in the console, or close the console window.
- On exit it prints a summary and waits for a keypress so the window does not vanish before you can read it.

**Tip:** start the program *before* or *after* Roblox - either order works. It finds the process automatically and reconnects if you restart Roblox.

---

## Understanding the console output

Every line has a tag in brackets. Here is what they mean:

| Line | Meaning |
| --- | --- |
| `[info]` | Startup information: loaded offsets, version check, crosshair info. |
| `[status] Waiting for Roblox process...` | Roblox is not running yet. Start it and join a game. |
| `[status] Attached to Roblox process (pid 2432)...` | Found the game and opened a read handle. Working. |
| `[status] Waiting for local player...` | The game was found, but the player could not be resolved. Usually you are still in the menu or loading. The message says exactly which step failed (see the detailed messages below). |
| `[status] Monitoring local player health` | Everything resolved. Health is being read. |
| `[status] Player is dead - waiting for respawn` | Your character died or is respawning. Normal. |
| `[health] Player health: 100.0 / 100.0` | Live health read. The second number is max health. |
| `[death] Player dead! (#1) Switching crosshair to: ...` | A death was detected and the crosshair changed. |
| `[respawn] Player respawned - monitoring again` | You are back alive; the death detector is re-armed. |
| `[warn]` | Non-fatal problem, e.g. offsets version mismatch. |

### Detailed "Waiting for local player..." messages

The status line tells you which stage of the chain failed:

- `(DataModel not resolved - offsets likely stale)` - the static pointers point at garbage. Your offsets file does not match the running Roblox. Update it.
- `(Players service not found)` - the DataModel was found but its children could not be walked. Run with `--probe` to inspect the live layout.
- `(LocalPlayer pointer invalid)` - you are in a menu or loading screen without a player instance. Join a game.

---

## Custom crosshair images

If you want your own crosshairs instead of the built-in shapes, this is the easy part.

1. Find the `crosshairs` folder next to `ZetianExternal.exe` (`build\Release\crosshairs`). If it does not exist, create it.
2. Drop any number of images into it.
3. Supported formats: `.png`, `.jpg`, `.jpeg`, `.bmp`, `.gif`, `.tif`, `.tiff`, `.ico`.
4. Restart the program (or start it before playing).

Rules:

- **If the folder contains images, deaths switch between those images.** The built-in shapes are ignored.
- **If the folder is empty or missing, the built-in shapes are used.**
- **Transparency works.** PNG alpha is fully supported - everything outside the opaque parts of your image is invisible, so the game shows through around your crosshair. This is the recommended format.
- Images are drawn centered on your screen, scaled randomly between 32 and 96 pixels on the longest side per death, always keeping the aspect ratio.
- Unreadable or corrupt files are skipped and reported as a warning, they do not crash the program.

Startup output tells you which mode is active:

```
[info] Loaded 3 custom crosshair image(s) from 'crosshairs'
[info]   Deaths will switch between these images
```

or

```
[info] No custom images in 'crosshairs' - using built-in shapes
[info]   Drop .png/.jpg/.bmp/.gif files there to use them instead
```

---

## Command line options

All flags are optional. Order does not matter.

```bat
build\Release\ZetianExternal.exe [options] [path-to-offsets.json]
```

| Option | What it does |
| --- | --- |
| (no arguments) | Normal mode. Uses `offsets.json` searched in the exe folder, working directory, and parent folders. |
| `--debug` | Enables diagnostic output. |
| `--probe` | Prints the live pointer chain and layout details every 3 seconds. Use this when the player never resolves - the output shows exactly where the chain breaks. |
| `--test-death` | Simulates a death 3 seconds after startup so you can verify the crosshair swapping without dying. |
| `--snapshot` | Renders one frame of the overlay (exactly as it appears) into `overlay_snapshot.png` next to the exe, then exits. Useful to check the overlay without a game running. |
| `path\to\offsets.json` | Use a specific offsets file instead of the automatic search. |

Examples:

```bat
build\Release\ZetianExternal.exe
build\Release\ZetianExternal.exe --probe
build\Release\ZetianExternal.exe --test-death
build\Release\ZetianExternal.exe C:\Downloads\offsets.json
```

---

## Troubleshooting

### The console closes instantly

If the program hits a fatal error, it shows a message box with the exact problem and then waits for a keypress - so look for a popup titled `ZetianExternal - Fatal Error`.

- **`Failed to load offsets from 'offsets.json'`** - the file is missing or unreadable. Put a valid `offsets.json` next to the exe, or pass the path as an argument.
- **`Failed to initialize the crosshair overlay window`** - usually means another copy of the program is already running. Close it first.

### It says "Waiting for local player..." forever

| Sub-message | What to do |
| --- | --- |
| `DataModel not resolved` | Offsets are stale (Roblox updated). Get a fresh `offsets.json`. |
| `Players service not found` | Run with `--probe` and look at the output. If `class='DataModel'` is missing or the children list is empty, offsets are stale. |
| `LocalPlayer pointer invalid` | Join an actual game. In the menu there is no player yet. |

### It prints a version mismatch warning

Your `offsets.json` was dumped from a different Roblox build than the one installed. Static pointers from another version point at garbage, so the player will never resolve. Replace the offsets file (see [Getting offsets](#getting-offsets-for-your-roblox-version)).

### The crosshair does not appear on screen

- Make sure Roblox is running and you are in a game.
- The overlay is topmost but click-through; it should float above the game. If your game is in true fullscreen-exclusive mode (rare; Roblox uses borderless by default), try borderless/windowed.
- Run `ZetianExternal.exe --snapshot` - if `overlay_snapshot.png` shows the crosshair, rendering works and the issue is the game's display mode.

### Health stays at 100 / 0 / or the numbers look frozen

- If it stays at 100 you may simply not have taken damage yet.
- If it never changes while playing, the offsets probably went stale mid-session (Roblox updated). Restart with a fresh offsets file.

### The game still works normally with this running

Yes - that is the point. The program only reads memory and draws its own overlay window. It sends no input, writes no memory, and loads nothing into Roblox.

---

## How it works under the hood

### External memory reading

Roblox is a normal Windows program. While it runs, its data (players, characters, health values) lives in its process memory. `ZetianExternal` opens the process with read-only access (`ReadProcessMemory`) and follows a chain of pointers until it reaches your health value. It never uses `WriteProcessMemory`, never allocates memory in the game, and never injects anything.

### The pointer chain

Offsets describe how to walk from a known starting point to the value you want:

1. **Static pointers** - `VisualEngine.Pointer` and `FakeDataModel.Pointer` are fixed addresses inside `RobloxPlayerBeta.exe`. From them the program finds the FakeDataModel and, through `RealDataModel`, the real DataModel (the `game` object).
2. **Children traversal** - instances store their children in a heap container: `ChildrenStart` points to the container, the container holds begin/end pointers, and each element is a 16-byte entry whose first 8 bytes are the child pointer. The program walks the DataModel's children and finds the `Players` service **by class name** (read via `ClassDescriptor` + `ClassName` - more reliable than instance names, which are currently unreadable in Roblox builds).
3. **Player chain** - `Players + LocalPlayer` gives your Player object, `Player + ModelInstance` gives your character, and the character's child of class `Humanoid` is found the same way.
4. **Health** - `Humanoid + Health` and `Humanoid + MaxHealth` are two floats. Read 10 times per second.

### Death detection

The monitor feeds every health sample into a detector that watches for the *transition* alive -> dead (health 0 or missing character). It fires exactly once per life and re-arms when the player respawns, so one death = one crosshair change.

### The overlay

A topmost, click-through, tool window covers the screen. The background color is used as a color key (made fully transparent by the OS), and the crosshair is drawn onto it. Built-in shapes are drawn with GDI; custom images are loaded and alpha-blended with GDI+ (built into Windows), which is what makes transparent PNGs work.

---

## Project files

| File / folder | What it is |
| --- | --- |
| `CMakeLists.txt` | Build configuration. |
| `offsets.json` | Offsets for your Roblox version (RbxDumperV2 format). |
| `crosshairs/` | Your custom crosshair images. Empty by default (`.gitkeep` keeps it in git). |
| `src/main.cpp` | Entry point: CLI, version check, wiring everything together, console output. |
| `src/json_parser` | A small dependency-free JSON parser used to read the offsets file. |
| `src/offset_loader` | Loads and validates the required offsets; reports which one is missing. |
| `src/process` | Finding the Roblox process and module, and all `ReadProcessMemory` helpers. |
| `src/roblox_state` | The memory walk: DataModel -> Players -> LocalPlayer -> Character -> Humanoid -> health. |
| `src/death_detector` | Edge-triggered alive -> dead detection with re-arming on respawn. |
| `src/player_monitor` | The polling loop: attach, resolve, report, reconnect on process exit. |
| `src/crosshair_renderer` | The overlay window, GDI shapes, GDI+ image loading, random style picking. |
