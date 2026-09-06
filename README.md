# TBOI: Launcher

An easy-to-use launcher for **The Binding of Isaac: Repentance+** that allows you to switch between game versions, manage your mods, and adjust graphical settings from a single window.

---

## What does this Launcher do?

- **Play older versions (Downgrades)**: Easily switch to older game versions with a single click (currently v1.9.7.15).
- **Your mods and save files always available**: All versions automatically share your mods and save data without taking up extra disk space.
- **Mod Manager**: Easily enable, disable, and update your Steam Workshop mods.
- **Options Editor**: Modify graphical and gameplay settings (`options.ini`) directly from the interface.
- **60 FPS Patch (Experimental)**: Option to enable 60 FPS animation rendering (which is also a REPENTOGON feature).

---

## Credits

This project is strongly based on and inspired by the **REPENTOGON Launcher**, from which it draws:
- The interface design and layout.
- The system for reading and saving game options.
- The method for enabling and disabling mods.
- Steam support and background execution (*Stealth Mode*).

---

## 🛠️ How to Build

1. Open a terminal and clone the repository with its submodules:
   ```bash
   git clone --recursive https://github.com/your-username/TBOI_Launcher.git
   cd TBOI_Launcher
   ```
2. Build the project using CMake (Visual Studio 2022 in 32-bit):
   ```bash
   cmake -B build32 -A Win32
   cmake --build build32 --config Release
   ```
3. The ready-to-play executable will be located in the `build32/Release/` folder.
