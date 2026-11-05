$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CppDir    = Split-Path -Parent $ScriptDir
$BuildDir  = Join-Path $CppDir "build"
$DistDir   = Join-Path $CppDir "dist"
$StageDir  = Join-Path $DistDir "win_stage"

Write-Host "[ITC Windows Packager] Configuring and building Release executable..."
if (-not (Test-Path (Join-Path $CppDir "assets\map\theater.bin"))) {
    python (Join-Path $CppDir "tools\extract_assets.py")
}

cmake -B $BuildDir -S $CppDir -DCMAKE_BUILD_TYPE=Release
cmake --build $BuildDir --config Release --parallel

if (Test-Path $DistDir) {
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

$ExePath = Join-Path $BuildDir "Release\IraqTheaterCommand.exe"
if (-not (Test-Path $ExePath)) {
    $ExePath = Join-Path $BuildDir "IraqTheaterCommand.exe"
}

Copy-Item $ExePath (Join-Path $StageDir "IraqTheaterCommand.exe")
Copy-Item -Recurse (Join-Path $CppDir "assets") (Join-Path $StageDir "assets")

Write-Host "[ITC Windows Packager] Creating Portable ZIP archive..."
$ZipPath = Join-Path $DistDir "IraqTheaterCommand-Windows-Portable.zip"
Compress-Archive -Path "$StageDir\*" -DestinationPath $ZipPath -Force

Write-Host "[ITC Windows Packager] Building NSIS Windows Setup Installer (IraqTheaterCommand-Windows-Setup.exe)..."
$NsiScript = Join-Path $ScriptDir "installer.nsi"
makensis $NsiScript

Write-Host "[ITC Windows Packager] Done! Generated installers in ${DistDir}:"
Get-ChildItem $DistDir -File | Format-Table Name, Length

