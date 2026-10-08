# Changelog

## 1.0.6 — October 8, 2026

### Added
- **Loading video override** (Loading Video tab): replace the loading screen's intro video with your own file dropped into a `videos` folder next to `nrmm.dll`, with a toggle to pick a random clip each load just like the game does. The loading screen's clip and song text are hidden while your clip plays.
- **Disable meetspot restrictions** toggle on the Player tab: lets your car pass the owning crew's requirements (power, drivetrain, tires, origin, spec) when entering a meetspot.

### Changed
- **Menu tabs** now sit on the left side of the window.
- Release version now lives in the DLL version resource; the release workflow reads it from there.

---

## 1.0.5 — October 7, 2026

### Added
- **Custom music player** (Music tab): play your own `.wav`, `.mp3` and `.ogg` files from a `music` folder next to `nrmm.dll`, routed through the game's own music system so the in-game volume slider and speed effects still apply. Includes a track picker, Play / Stop / Pause / Resume, Prev / Next, and **Playlist** and **Shuffle** modes; the file name is mirrored into the game's now-playing UI and the in-game phone music keys control playback.

### Changed
- **Auction refresh** now waits for the game to finish rebuilding the listing: the button is disabled while cars are spawning and the status reports `Refreshing...` until it completes.
- **Unlock All Cars** only unlocks chassis that have a car origin, so the auction no longer stalls on chassis the game cannot spawn.
- **Garage spawns** pass only the chassis the selected model has an origin for, and the model list now only offers models with a loaded origin, avoiding spawn hangs.

---

## 1.0.4 — October 6, 2026

### Added
- **Bootstrapper** (`bootstrapper.exe`): a small launcher that injects the mod into *NIGHT-RUNNERS PRIVATE ALPHA* (preferring `nrmm-debug.dll` when present, and skipping the injection when the DLL is already loaded), decrypts and re-encrypts the game's `SaveFile.es3`, and follows the Windows light/dark theme automatically.
- **Number plate editor** on the Current Car tab: choose the plate style (black / green / glow) and set every glyph, with a **Random plate** button to roll a new plate.
- **Clean Car** button on the Current Car tab to wipe the car's built-up dirt.
- **Force Save Game** button on the Player tab to flush the save file to disk on demand.

### Changed
- **Save Car** now saves the full car (engine, parts and stats) and writes it to disk immediately instead of waiting for the next scene change or quit.
- Car spawns in the Garage tab now pick the car by **model type**.
- Releases now ship an `nrmm.zip` containing `nrmm.dll` and `bootstrapper.exe` (the DLL is also attached on its own).

---

## 1.0.3 — October 4, 2026

### Added
- **Rebindable keybinds**: set the open-menu and unload keys from the Settings tab.
- **Config file** (`nrmm.ini`) so your settings and keybinds are remembered between sessions.
- **Money and reputation editor** for adding money, reputation, meetspot reputation, debt and betting money.

### Changed
- Rewritten fast travel and garage management.

---

## 1.0.2 — October 2, 2026

### Changed
- More reliable game shutdown and clean unhooking.
- Improved keyboard input blocking.

---

## 1.0.1 — October 2, 2026

### Changed
- Rewritten logger.
- Rewritten input blocking.
- General project cleanup.
