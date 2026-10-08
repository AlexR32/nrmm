# NRMM

**NRMM** (Night-Runners Mod Menu) is a mod menu for the game *NIGHT-RUNNERS PRIVATE ALPHA*.

## Features

- **Settings**
  - Block keyboard input when menu is open
  - Unload the DLL cleanly at runtime
  - Show or hide the debug console
  - Change menu keybinds
- **Player**
  - Set the player race crew type
  - End the current night
  - Unlock all parts
  - Meetspot owner editor
  - Disable crew restrictions
  - Money, Meetspot rep, night rep, debt, bettng money editor
  - Force save game
- **Current Car**
  - Set drivetrain, induction type and fuel type
  - Freeze engine health, fuel, water temperature, oil temperature, brake temperature and NOS
  - Change the oil
  - Clean car from dirt
  - Save the edited car stats
  - Number plate editor
- **World**
  - Toggle traffic
  - Trigger meetspot attack
  - Fast travel to trucks, garages, meetspots, etc
  - Enter garage, meetspot, car storage from any part of the map
  - Buy, add, remove garages
- **Garage**
  - Spawn a car from the model list and save it to a car storage
  - Spawn overrides: engine, fuel, mileage and dirt, gearbox, paint colour, and stock-only
- **Auction**
  - Refresh the auction listing
  - Unlock every available car for sale
- **Fixes**
  - Separate tab to show what game fixes this mod menu has

The **bootstrapper**:

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

Both keybinds are rebindable from the **Settings** tab: click the keybind button, then press the key you want (`ESC` cancels)

### Bootstrapper and save files

The game stores its save under `%USERPROFILE%\AppData\LocalLow\PLANET JEM SOFTWARE\NIGHT-RUNNERS PRIVATE ALPHA`:

- **Decrypt Game Save** reads `SaveFile.es3` from the game's save folder and writes the decoded JSON to `SaveFile.json` **next to the bootstrapper**
- **Encrypt Game Save** reads `SaveFile.json` from next to the bootstrapper and writes the encrypted `SaveFile.es3` back into the game's save folder so the game can load it

Close the game before re-encrypting, otherwise the file is locked.  
The save format is PBKDF2-HMAC-SHA1 + AES-128-CBC

## Building

### Requirements

- Visual Studio 2026 (or newer) with the **Desktop development with C++** workload and the **v145** platform toolset
- Windows 10/11 SDK (`WindowsTargetPlatformVersion` 10.0)
- x64 build target

### From Visual Studio

1. Open `nrmm.slnx`
2. Select the `Release` configuration and the `x64` platform
3. Build the solution (`Ctrl+Shift+B`)
4. The outputs are written to `bin` folder

A `Debug` build is also available and produces files with `-debug` prefix

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

`nrmm.dll` carries the release version resource (`nrmm\nrmm.rc`), which is what the build and release workflows read to name artifacts and tags. The bootstrapper has its own independent version resource (`bootstrapper\bootstrapper.rc`)

## Project layout

```
nrmm.slnx              Solution (mod menu + bootstrapper)
nrmm/                  Mod menu (native x64 DLL)
  nrmm.vcxproj         Visual Studio project (x64, Debug/Release)
  nrmm.rc              Version info
  src/
    dllmain.cpp        DllMain and the main worker thread
    globals.h          Shared globals
    pch.h/.cpp         Precompiled header
    backends/          Direct3D 11 Present hook + ImGui backend
    core/              Menu, overlay drawing, logger, input block
    il2cpp/            IL2CPP runtime wrapper, exports and bindings
    nr/                Game features per tab
  libs/
    imgui/             Dear ImGui
    minhook/           MinHook
bootstrapper/          Launcher, injector and save tool
  bootstrapper.vcxproj
  bootstrapper.rc      Icon and version info
  res/                 Icons
  src/
    main.cpp           Entry point
    app.*              Window, owner-drawn controls and theming
    injector.*         Process/module lookup and DLL injection
    save_crypto.*      Save encrypt/decrypt (PBKDF2 + AES + gzip)
    inflate.*          Minimal gzip/DEFLATE decoder
    theme.*            Light/dark palettes and Windows theme detection
.github/workflows/     CI (build and release)
```

## Versioning and releases

The release version lives in the `nrmm.dll` version resource (`nrmm\nrmm.rc`). Bump the `VERSION`/`VERSION_STRING` defines at the top of that file to change the release. On a push to `main`, the release workflow builds `nrmm.dll`, reads its version, compares the resulting `v<version>` tag against the existing tags and, when it is new, builds the bootstrapper, packages `nrmm-<version>.zip` (containing `nrmm.dll` and `bootstrapper.exe`), and publishes a GitHub release with an auto-generated changelog

## License

Released under the [MIT License](LICENSE). Vendored third-party dependencies keep their own licenses: Dear ImGui is MIT and MinHook is BSD-2-Clause
