# NRMM

**NRMM** (Night-Runners Mod Menu) is an in-process mod menu for the Unity game *NIGHT-RUNNERS*. It is a native x64 DLL that hooks the game's IL2CPP runtime and its Direct3D 11 swap chain, then draws an ImGui overlay on top of the running game. Every game action is marshalled onto the game's own script thread, so the mod never touches managed state from the render thread

Injected at runtime, it exposes a tabbed menu for editing the player, the world, the current car, garages and the auction house, plus a set of utility fixes

A companion **bootstrapper** (`bootstrapper.exe`) launches the mod: it injects the DLL into the running game and can decrypt or re-encrypt the game save

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
  - Spawn a car from the full model list and save it to a house
  - Spawn overrides: engine, fuel, mileage and dirt, gearbox, paint colour, and stock-only
- **Auction**
  - Refresh the auction listing
  - Unlock every chassis for sale

A status line under each tab reports the result of the last action and is mirrored to the debug console

The **bootstrapper** (`bootstrapper.exe`) is a separate launcher:

- **Load Mod Menu** — injects the mod DLL into *NIGHT-RUNNERS PRIVATE ALPHA*. It prefers `nrmm-debug.dll` when present next to the bootstrapper and otherwise uses `nrmm.dll`, and it skips the injection when the DLL is already loaded
- **Decrypt Game Save** — reads the game's `SaveFile.es3` and writes the decoded `SaveFile.json` next to the bootstrapper
- **Encrypt Game Save** — reads `SaveFile.json` next to the bootstrapper and writes the encrypted `SaveFile.es3` into the game's save folder
- It follows the Windows light/dark theme automatically and reports every action in a status line

## Usage

1. [Build](#building) or [download](https://github.com/AlexR32/nrmm/releases/latest) the latest release (`nrmm.zip` contains `nrmm.dll` and `bootstrapper.exe`)
2. Launch *NIGHT-RUNNERS* and get into the game
3. Run `bootstrapper.exe` and click **Load Mod Menu**, or inject `nrmm.dll` yourself with any DLL injector
4. Press `INS` to toggle the menu

Controls and behaviour:

| Input      | Effect            |
| ---------- | ----------------- |
| `INS`      | Toggle the menu   |
| `DELETE`   | Unload the DLL    |

The **Settings** tab is persisted to `nrmm.ini` next to the DLL: the block-keyboard and debug-console toggles plus both keybinds are restored on the next injection. Both keybinds are rebindable from the **Settings** tab: click the keybind button, then press the key you want (`ESC` cancels).

### Bootstrapper and save files

The game stores its save under `%USERPROFILE%\AppData\LocalLow\PLANET JEM SOFTWARE\NIGHT-RUNNERS PRIVATE ALPHA`:

- **Decrypt Game Save** reads `SaveFile.es3` from the game's save folder and writes the decoded JSON to `SaveFile.json` **next to the bootstrapper** (the folder with `bootstrapper.exe` and `nrmm.dll`)
- **Encrypt Game Save** reads `SaveFile.json` from next to the bootstrapper and writes the encrypted `SaveFile.es3` back into the game's save folder so the game can load it

Close the game before re-encrypting, otherwise the file is locked. The save format is PBKDF2-HMAC-SHA1 + AES-128-CBC

## Building

### Requirements

- Visual Studio 2026 (or newer) with the **Desktop development with C++** workload and the **v145** platform toolset
- Windows 10/11 SDK (`WindowsTargetPlatformVersion` 10.0)
- x64 build target

### From Visual Studio

1. Open `nrmm.slnx`
2. Select the `Release` configuration and the `x64` platform
3. Build the solution (`Ctrl+Shift+B`)
4. The outputs are written to `bin\nrmm.dll` (and `bin\nrmm.pdb`) and `bin\bootstrapper.exe` (and `bin\bootstrapper.pdb`)

A `Debug` build is also available and produces `bin\nrmm-debug.dll`

### From the command line

Build the whole solution:

```bat
msbuild nrmm.slnx /m /p:Configuration=Release /p:Platform=x64
```

Or build the projects individually:

```bat
msbuild nrmm\nrmm.vcxproj /m /p:Configuration=Release /p:Platform=x64
msbuild bootstrapper\bootstrapper.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

Both projects are precompiled-header based where relevant and the third-party dependencies (ImGui and MinHook) are vendored under `nrmm/libs/`, so no package restore is needed

The bootstrapper's version resource is kept in sync with the top-level `.version` file by `scripts/update_bootstrapper_version.ps1`, which runs automatically before the bootstrapper build (and can be run by hand: `pwsh ./scripts/update_bootstrapper_version.ps1`)

## Project layout

```
nrmm.slnx              Solution (mod menu + bootstrapper)
.version               Version tag for the release workflow
nrmm/                  Mod menu (native x64 DLL)
  nrmm.vcxproj         Visual Studio project (x64, Debug/Release)
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
bootstrapper/          Launcher, injector and save tool (native x64 EXE)
  bootstrapper.vcxproj
  bootstrapper.rc      Icon, version info and identity
  res/                 Icons
  src/
    main.cpp           Entry point
    app.*              Window, owner-drawn controls and theming
    injector.*         Process/module lookup and DLL injection
    save_crypto.*      Save encrypt/decrypt (PBKDF2 + AES + gzip)
    inflate.*          Minimal gzip/DEFLATE decoder
    theme.*            Light/dark palettes and Windows theme detection
scripts/               Build helpers (version sync)
.github/workflows/     CI (build and release)
```

## Versioning and releases

The release version lives in the top-level `.version` file. Change it to bump the release. On a push to `main`, the release workflow reads the file, compares the resulting `v<version>` tag against the existing tags and, when it is new, builds both the Release DLL and the bootstrapper, packages `nrmm-<version>.zip` (containing `nrmm.dll` and `bootstrapper.exe`), and publishes a GitHub release with an auto-generated changelog (the `nrmm.dll` is also attached on its own). Pushing a version that already has a tag does not create a duplicate release

## Notes

- Only the 64-bit build is supported
- The mod relies on IL2CPP exports and game field/method names, so it is tied to a specific game build. Memory addresses and names may change between updates
- This project is intended for personal/offline use. Use it at your own risk

## License

Released under the [MIT License](LICENSE). Vendored third-party dependencies keep their own licenses: Dear ImGui is MIT and MinHook is BSD-2-Clause
