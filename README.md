# NRMM

**NRMM** (Night-Runners Mod Menu) is an in-process mod menu for the Unity game *NIGHT-RUNNERS*. It is a native x64 DLL that hooks the game's IL2CPP runtime and its Direct3D 11 swap chain, then draws an ImGui overlay on top of the running game. Every game action is marshalled onto the game's own script thread, so the mod never touches managed state from the render thread

Injected at runtime, it exposes a tabbed menu for editing the player, the world, the current car, garages and the auction house, plus a set of utility fixes

## Features

The menu is opened with `INS` and has the following tabs

- **Settings**
  - Unhook the DLL cleanly at runtime
  - Show or hide the debug console
- **Player**
  - Set the player race crew type
  - Toggle traffic
  - Meetspot owner editor
  - End the current night
  - Unlock all parts
- **World**
  - Toggle traffic
  - Attack crew-owned meetspots
  - Fast travel to discovered trucks
  - Garage list: go to a garage, buy a garage, or mark one as owned
- **Current Car**
  - Set drivetrain, induction type and fuel type
  - Freeze engine health, fuel, water temperature, oil temperature, brake temperature and NOS
  - Change the oil
  - Save the edited car stats
- **Fixes**
  - Separate tab to show what game fixes this mod menu has
- **Garage**
  - Spawn a car from the full chassis list and save it to a house
  - Spawn overrides: engine, fuel, mileage and dirt, gearbox, paint colour, and stock-only
- **Auction**
  - Refresh the auction listing
  - Unlock every chassis for sale

A status line under each tab reports the result of the last action and is mirrored to the debug console

## Usage

1. [Build](#building) or [download](https://github.com/AlexR32/nrmm/releases/latest) `nrmm.dll`
2. Launch *NIGHT-RUNNERS* and get into the game
3. Inject `nrmm.dll` into the game process using your own DLL injector of choice.
4. Press `INS` to toggle the menu

Controls and behaviour:

| Input      | Effect            |
| ---------- | ----------------- |
| `INS`      | Toggle the menu   |
| `DELETE`   | Unload the DLL    |

## Building

### Requirements

- Visual Studio 2026 (or newer) with the **Desktop development with C++** workload and the **v145** platform toolset
- Windows 10/11 SDK (`WindowsTargetPlatformVersion` 10.0)
- x64 build target

### From Visual Studio

1. Open `nrmm.slnx`
2. Select the `Release` configuration and the `x64` platform
3. Build the solution (`Ctrl+Shift+B`)
4. The output is written to `bin\nrmm.dll` (and `bin\nrmm.pdb`)

A `Debug` build is also available and produces `bin\nrmm-debug.dll`

### From the command line

```bat
msbuild nrmm.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

The project is precompiled-header based and the third-party dependencies (ImGui and MinHook) are vendored under `libs/`, so no package restore is needed

## Project layout

```
nrmm.slnx            Solution
nrmm.vcxproj         Visual Studio project (x64, Debug/Release)
.version             Version tag for the release workflow
src/
  dllmain.cpp        DllMain and the main worker thread
  globals.h          Shared globals
  pch.h/.cpp         Precompiled header
  backends/          Direct3D 11 Present hook + ImGui backend
  core/              Menu, overlay drawing, logger, cursor
  il2cpp/            IL2CPP runtime wrapper, exports and bindings
  nr/                Game features per tab
libs/
  imgui/             Dear ImGui
  minhook/           MinHook
```

## Versioning and releases

The release version lives in the top-level `.version` file. Change it to bump the release. On a push to `main`, the release workflow reads the file, compares the resulting `v<version>` tag against the existing tags and, when it is new, builds the Release DLL, packages it and publishes a GitHub release with an auto-generated changelog. Pushing a version that already has a tag does not create a duplicate release

## Notes

- Only the 64-bit build is supported
- The mod relies on IL2CPP exports and game field/method names, so it is tied to a specific game build. Memory addresses and names may change between updates
- This project is intended for personal/offline use. Use it at your own risk
