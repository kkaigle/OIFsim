@echo off
setlocal
cd /d "%~dp0"

if not exist "assets\map\theater.bin" (
    echo [ITC] Extracting native assets and 3D .glb models...
    python tools\extract_assets.py
)

echo [ITC] Configuring and compiling C++20 Windows Release build...
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

echo [ITC] Build complete!
if "%1"=="--run" (
    if exist "build\Release\IraqTheaterCommand.exe" (
        "build\Release\IraqTheaterCommand.exe"
    ) else (
        "build\IraqTheaterCommand.exe"
    )
)

