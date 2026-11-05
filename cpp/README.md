# Iraq Theater Command — Native C++20 Engine (macOS & Windows)

Standalone C++20 / CMake / Raylib cross-platform tactical engine for **Iraq Theater Command**, featuring:
- **251,503 OpenStreetMap Buildings & 327,983 Street Segments** loaded natively from binary streams (`cpp/assets/map/osm_buildings.bin` & `osm_streets.bin`).
- **Seamless 2D Theater Map, 3D Tactical & Base Builder, MQ-1 Predator FLIR Drone, and SOFLAM Laser Designator Views**.
- **16×16 Grid 3D Base Building System**: Snap-to-grid placement (`bldGridSnap`), collision detection, rotation (`[R]`), demolition (`[X]`), live 3D green/red ghost preview, and automatic `.glb` model override discovery.
- **Campaign Phase Control (`[C]`)**: First button on the left rail controlling progression through *Phase I: Build-Up in Kuwait*, *Phase II: 48-Hour Ultimatum*, *Phase III: Shock & Awe Air Blitz*, *Phase IV: Ground Invasion*, and *Phase V: Stability & COIN Ops*.
- **Compressed Mission-Type Joint Fires (`[F]`)**: Call in fire support by mission type (*CAS*, *Precision JDAM*, *Tomahawk Salvo*, *155mm Artillery*, *M270 MLRS*, *120mm Mortar*, *SEAD/HARM*, *MQ-1 Predator ISR*, *JSOC Raid*) without micromanaging individual firing batteries.
- **Customizable Keyboard Shortcuts (`WASD` / Arrow Keys pan, `Q/E` orbit, `B` 3D View, `C` Campaign, `F` Fires, `R` Rotate, `Space` Pause)** and synthesized + `.wav`/FMOD-ready military audio engine.

## Quick Build & Run

### macOS (Apple Silicon & Intel)
```bash
./cpp/build_mac.sh --run
```

### Windows (Visual Studio 2022 MSVC / MinGW)
```bat
cpp\build_win.bat --run
```
*(CMake automatically links a local Raylib installation if present, or fetches Raylib 5.5 from source via `FetchContent` on a clean Windows/Mac machine).*

## Building Native Desktop Installers (`.dmg`, `.pkg`, `Setup.exe`, `.zip`)

### Local Packaging
- **macOS (`.app` Bundle + `.dmg` Disk Image + `.pkg` Installer)**:
  ```bash
  ./cpp/packaging/package_mac.sh
  ```
  Outputs to `cpp/dist/`:
  - `Iraq Theater Command.app` (signed standalone bundle with `AppIcon.icns` & embedded `Resources/assets`)
  - `IraqTheaterCommand-macOS.dmg`
  - `IraqTheaterCommand-macOS-Installer.pkg`
- **Windows (NSIS `Setup.exe` Installer + Portable `.zip`)**:
  ```powershell
  pwsh -File cpp/packaging/package_win.ps1
  ```
  Outputs to `cpp\dist\`:
  - `IraqTheaterCommand-Windows-Setup.exe` (Installs to `Program Files\Iraq Theater Command`, creates Start Menu & Desktop shortcuts, and registers an Uninstaller)
  - `IraqTheaterCommand-Windows-Portable.zip`

### GitHub Actions CI/CD
The workflow [`.github/workflows/build-installers.yml`](../.github/workflows/build-installers.yml) runs automatically on pushes/PRs to `Full-rewrite` and `main`, version tags (`v*`), and manual `workflow_dispatch`, building both macOS and Windows installers and publishing them to GitHub Actions Artifacts and GitHub Releases (`latest-build`).

## Dropping Custom 3D `.glb` Assets into the Engine
Place any standard `.glb` (glTF 2.0 binary) file into `cpp/assets/models/` to override the built-in 3D models automatically on startup:
- **Units & Urban Models**: `tank.glb`, `ifv.glb`, `hmmwv.glb`, `bigres.glb`, `bigresL.glb`, `ruin.glb`, `house.glb`
- **Base Builder Structures (15 types)**: `toc.glb`, `sleep.glb`, `hesco.glb`, `tower.glb`, `heli.glb`, `motor.glb`, `ammo.glb`, `fuel.glb`, `med.glb`, `comms.glb`, `mort.glb`, `gen.glb`, `dfac.glb`, `det.glb`, `uav.glb`
- **Audio Overrides**: Place `click.wav`, `radio.wav`, `place.wav`, `explosion.wav`, `brrrt.wav`, or `missile.wav` in `cpp/assets/audio/` (or drop the FMOD Studio SDK into `cpp/third_party/fmod` and configure with `-DITC_ENABLE_FMOD=ON`).

