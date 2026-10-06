# Changelog

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
