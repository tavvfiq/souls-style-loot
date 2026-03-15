# Copy Souls Style Looting plugin and PrismaUI view into mod layout.
# Run from: souls-style-loot (or repo root). Uses script directory to find paths.
#
# Optional env: SOULS_MOD_OUTPUT = full path to mod output folder (e.g. MO2 mods folder).
# If not set, copies to: <script_dir>/../mod_output/Souls Style Looting

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

# Prefer releasedbg (has .pdb); fallback to release
$ReleaseDbgDir = Join-Path $ProjectRoot "build\windows\x64\releasedbg"
$ReleaseDir   = Join-Path $ProjectRoot "build\windows\x64\release"
$BuildDir = if (Test-Path (Join-Path $ReleaseDbgDir "SoulsStyleLooting.dll")) { $ReleaseDbgDir } else { $ReleaseDir }
$DllSource = Join-Path $BuildDir "SoulsStyleLooting.dll"
$PdbSource = Join-Path $BuildDir "SoulsStyleLooting.pdb"
$ViewSource = Join-Path $ProjectRoot "view\SoulsStyleLoot\index.html"

$ModName = "Souls Style Looting"
if ($env:SOULS_MOD_OUTPUT) {
    $ModRoot = Join-Path $env:SOULS_MOD_OUTPUT $ModName
} else {
    $ModRoot = Join-Path $ProjectRoot "mod_output\$ModName"
}

# PrismaUI loads from Data/PrismaUI/views/ (game path). So our mod must ship Data/PrismaUI/views/SoulsStyleLoot/
# so it merges into the game Data (MO2/Vortex merge, or manual copy into game Data).
$SksePlugins = Join-Path $ModRoot "SKSE\Plugins"
$PrismaView  = Join-Path $ModRoot "Data\PrismaUI\views\SoulsStyleLoot"

# Create layout
New-Item -ItemType Directory -Force -Path $SksePlugins | Out-Null
New-Item -ItemType Directory -Force -Path $PrismaView  | Out-Null

# Copy plugin and PDB
if (-not (Test-Path $DllSource)) {
    Write-Error "Plugin not found. Build first: xmake build`n  $DllSource"
}
Copy-Item -Path $DllSource -Destination (Join-Path $SksePlugins "SoulsStyleLooting.dll") -Force
Write-Host "Copied: SKSE/Plugins/SoulsStyleLooting.dll"
if (Test-Path $PdbSource) {
    Copy-Item -Path $PdbSource -Destination (Join-Path $SksePlugins "SoulsStyleLooting.pdb") -Force
    Write-Host "Copied: SKSE/Plugins/SoulsStyleLooting.pdb"
}

# Copy view
if (-not (Test-Path $ViewSource)) {
    Write-Error "View not found: $ViewSource"
}
Copy-Item -Path $ViewSource -Destination (Join-Path $PrismaView "index.html") -Force
Write-Host "Copied: Data/PrismaUI/views/SoulsStyleLoot/index.html"

Write-Host "Mod layout updated at: $ModRoot"
Write-Host ""
Write-Host "PrismaUI loads views from: <Game>/Data/PrismaUI/views/"
Write-Host "Ensure this mod's Data folder is merged (MO2/Vortex or copy into game Data)."
