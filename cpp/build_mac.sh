#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if [ ! -f "assets/map/theater.bin" ]; then
    echo "[ITC] Extracting native assets and 3D .glb models from docs/index.html..."
    python3 tools/extract_assets.py
fi

echo "[ITC] Configuring and compiling C++20 macOS Release build..."
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

echo "[ITC] Build complete: $(pwd)/build/IraqTheaterCommand"
if [ "${1:-}" = "--run" ]; then
    ./build/IraqTheaterCommand
fi
