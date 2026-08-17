# Zeation

External Roblox health monitor with a reactive crosshair overlay.

Reads the local player's health from memory (read-only, no injection, no writes) and swaps the crosshair to a random style every time the player dies.

## Features

- Loads and validates offsets from `offsets.json` (RbxDumperV2 format, https://offsets.imtheo.lol)
- Auto-detects the installed Roblox version and warns when the offsets target a different build
- Resolves the full chain externally: DataModel -> Players -> LocalPlayer -> Character -> Humanoid
- Polls health at 10 Hz with 1 Hz console reports
- Edge-triggered death detection with respawn tracking and auto-reconnect on process exit
- Click-through, always-on-top overlay window (GDI, color-keyed transparency)
- 10 crosshair styles, randomized shape, size, thickness, and color on every death
- Diagnostic modes for layout probing and testing

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 (MSVC toolset) or any C++20 compiler
- CMake 3.16+

## Build

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
```

The build copies `offsets.json` next to the executable automatically.

## Usage

```bat
build\Release\ZetianExternal.exe
```

Launch while in-game (or before) - the monitor attaches to `RobloxPlayerBeta.exe` automatically. The overlay sits at screen center and is click-through, so gameplay is unaffected. Press `Ctrl+C` in the console to stop.

### Options

| Flag | Effect |
| --- | --- |
| `--debug` | Enables diagnostic output |
| `--probe` | Dumps the live pointer chain and layout strategies every 3 seconds |
| `--test-death` | Simulates a death after 3 seconds (verifies the crosshair swap) |
| `<path>` | Loads offsets from a specific file instead of searching for `offsets.json` |

### Example output

```
[status] Attached to Roblox process (pid 2432) at base 0x7ff705fc0000
[status] Monitoring local player health
[health] Player health: 100.0 / 100.0
[death] Player dead! (#1) Switching crosshair to: triangle (size 21, thickness 1, color black)
[respawn] Player respawned - monitoring again
[health] Player health: 100.0 / 100.0
```

## Crosshair styles

Cross, diagonal cross, circle, dot, ring + dot, triangle, brackets, square, cross + circle, cross + circle + dot.

## Layout

| Module | Responsibility |
| --- | --- |
| `src/json_parser` | Dependency-free JSON parser (DOM, errors with line/column) |
| `src/offset_loader` | Loads and validates required offsets from `offsets.json` |
| `src/process` | PID/module discovery, `ReadProcessMemory` wrappers |
| `src/roblox_state` | Pointer chain resolution, instance children traversal, class lookups |
| `src/death_detector` | Alive -> dead edge detection with per-life re-arming |
| `src/player_monitor` | Polling thread, process reconnect, status/health/death/respawn events |
| `src/crosshair_renderer` | Layered overlay window and GDI shape rendering |
| `src/main` | CLI, version check, wiring, console output |

## How the chain works

1. `VisualEngine.Pointer` / `FakeDataModel.Pointer` statics -> FakeDataModel -> `RealDataModel`
2. DataModel children (heap container: `ChildrenStart` -> container -> begin/end, 16-byte shared_ptr elements) -> find the `Players` service by class name
3. `Players` + `Player.LocalPlayer` -> `Player.ModelInstance` (character) -> child of class `Humanoid`
4. `Humanoid.Health` / `Humanoid.MaxHealth` -> health values

Instance names are unreliable in current builds, so all lookups use class names via `ClassDescriptor` + `ClassName`.

## Troubleshooting

| Symptom | Cause |
| --- | --- |
| `Version mismatch` warning | `offsets.json` was dumped from a different Roblox build. Re-dump from https://offsets.imtheo.lol and replace the file. |
| `Waiting for local player... (DataModel not resolved)` | Static pointers are stale (version mismatch) or the Roblox process was restarted. |
| `Waiting for local player... (Players service not found)` | Children traversal failed - run with `--probe` to inspect the live layout. |
| `Waiting for local player... (LocalPlayer pointer invalid)` | In a loading screen or menu without a player instance. Join a game. |
| Nothing happens | The overlay is click-through and topmost; it renders above the game. Check the console status lines. |

Run `ZetianExternal.exe --probe` to dump the resolved chain and the children/name layout strategies for debugging.
