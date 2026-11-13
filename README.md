# TBOI: Launcher

A Windows launcher for **The Binding of Isaac** that lets you switch between game versions, manage Steam Workshop mods, and edit game settings from one window.

This project was developed for personal use.

---

## Included patches

| Game version | Downgrade patch | Experimental 60 FPS animation interpolation |
| --- | --- | --- |
| Rebirth | Included | — |
| Afterbirth | Included | — |
| Afterbirth+ | Included | — |
| Repentance | Included | Included for v1.7.9b.J835 |
| Repentance+ v1.9.7.15 | Included | Included |
| Repentance+ v1.9.7.17 | Steam base installation | Included |

The bundled downgrades require a Steam installation of **Repentance+ v1.9.7.17.J460**. Select a version and click **Prepare Version** to create a separate copy under the game's `versions/` folder. Select **Steam Vanilla** to launch the installed Steam version directly.

## Launcher features

- **Game Options** edits the selected version's `options.ini`. Repentance+ v1.9.7.17 includes controls for **Epic Overlay** and **EOS Crossplay**.
- **Mod Manager** manages Steam Workshop mods. Prepared versions reuse the Steam installation's `mods/` and `data/` folders. In Afterbirth+ and Repentance, the launcher locks **Enable Mods** off and skips Workshop updates at launch. The setting is editable in Repentance+.
- **60 FPS Patch (Experimental)** installs, toggles, or removes the animation interpolation patch for the selected supported version.
- **Steam Launch Options** lets you copy the launch command for normal or background execution (*Stealth Mode*). **Check Logs** opens the game's log from the launcher.

You can place an `options.ini` beside the launcher to supply defaults for missing settings. An optional `data/` folder beside the launcher supplies initial mod data without overwriting existing files.

## Running the launcher

Extract the release ZIP and keep `TBOI_Launcher.exe`, `data.bin`, and `steam_appid.txt` together. Start Steam and open `TBOI_Launcher.exe`. The launcher extracts its files and bundled patches from `data.bin` into `launcher-data/` and checks the package's SHA-256 on startup to detect updates.

---

## Credits

This project is strongly based on and inspired by the **REPENTOGON Launcher**, from which it draws:
- The interface design and layout.
- The system for reading and saving game options.
- The method for enabling and disabling mods.
- Steam support and background execution (*Stealth Mode*).

The downgrade method follows the approach used by the REPENTOGON developers.

---

## 🛠️ How to Build

1. Open a terminal and clone the repository with its submodules:
   ```bash
   git clone --recursive https://github.com/Johnsondavisv4/TBOI_Launcher.git
   cd TBOI_Launcher
   ```
2. Build the project using CMake (Visual Studio 2022 in 32-bit):
   ```bash
   cmake -B build32 -A Win32
   cmake --build build32 --config Release
   ```
3. The executable, `data.bin`, and `steam_appid.txt` are in `build32/Release/`.
