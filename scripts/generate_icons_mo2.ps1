param(
    # Replace these placeholders with your real paths/names before running.
    [string]$Mo2Base          = "[MO2_BASE_PATH]",      # e.g. C:\Modding\MO2-SkyrimSE
    [string]$ProfileName      = "[PROFILE_NAME]",       # e.g. "Default"
    [string]$ManifestPath     = "[ICON_MANIFEST_PATH]", # e.g. C:\...\SoulsStyleLoot\icon_manifest.json
    [string]$TexconvExe       = "[TEXCONV_EXE_PATH]",   # e.g. C:\Tools\texconv\texconv.exe
    [string]$OutputModName    = "[OUTPUT_MOD_NAME]"     # e.g. "SoulsStyleLoot - Generated Icons"
)

# rtk: basic validation
if (-not (Test-Path $ManifestPath)) {
    Write-Error "Manifest not found: $ManifestPath"
    exit 1
}
if (-not (Test-Path $TexconvExe)) {
    Write-Error "texconv.exe not found: $TexconvExe"
    exit 1
}

$modsDir     = Join-Path $Mo2Base "mods"
$profilesDir = Join-Path $Mo2Base "profiles"
$profileDir  = Join-Path $profilesDir $ProfileName
$modlistPath = Join-Path $profileDir "modlist.txt"

if (-not (Test-Path $modlistPath)) {
    Write-Error "modlist.txt not found: $modlistPath"
    exit 1
}

# rtk: load enabled mods in MO2 priority order (bottom = highest)
$modLines = Get-Content $modlistPath | Where-Object { $_ -and $_ -notmatch "^#" }
$enabledModsInFileOrder = $modLines |
    Where-Object { $_.StartsWith("+") } |
    ForEach-Object { $_.Substring(1).Trim() } |
    Where-Object { $_ -ne "" }

# Highest-priority first (bottom of modlist)
$enabledMods = [System.Collections.Generic.List[string]]::new()
for ($i = $enabledModsInFileOrder.Count - 1; $i -ge 0; $i--) {
    $enabledMods.Add($enabledModsInFileOrder[$i])
}

Write-Host "Enabled mods (high → low priority):"
$enabledMods | ForEach-Object { Write-Host "  $_" }

# rtk: ensure output mod directory exists
$outModDir   = Join-Path $modsDir $OutputModName
$outViewRoot = Join-Path $outModDir "PrismaUI\views"
New-Item -ItemType Directory -Path $outViewRoot -Force | Out-Null

# rtk: load manifest JSON
$manifestJson = Get-Content $ManifestPath -Raw
$manifest = $manifestJson | ConvertFrom-Json

if (-not $manifest.icons) {
    Write-Error "Manifest does not contain an 'icons' array."
    exit 1
}

$icons = $manifest.icons
Write-Host "Found $($icons.Count) icon entries in manifest."

function Find-DdsForIcon {
    param(
        [string]$RelativeDdsPath   # e.g. "textures\foo\bar.dds" or "textures/foo/bar.dds"
    )

    $rel = $RelativeDdsPath -replace "\\", "/"
    foreach ($mod in $enabledMods) {
        $candidate = Join-Path (Join-Path $modsDir $mod) $rel
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    return $null
}

# rtk: process each icon entry
foreach ($icon in $icons) {
    $ddsRel  = $icon.ddsPath
    $pngRel  = $icon.pngPath

    if (-not $ddsRel) { continue }
    if (-not $pngRel) { continue }

    $ddsFull = Find-DdsForIcon -RelativeDdsPath $ddsRel
    if (-not $ddsFull) {
        Write-Warning "DDS not found for $ddsRel (FormID=$($icon.formID), Plugin=$($icon.plugin))"
        continue
    }

    # Output path is Data-relative pngPath under PrismaUI/views
    $outPngFull = Join-Path $outViewRoot $pngRel
    $outDir     = Split-Path $outPngFull -Parent

    if (-not (Test-Path $outDir)) {
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    }

    Write-Host ""
    Write-Host "Converting:"
    Write-Host "  DDS:  $ddsFull"
    Write-Host "  PNG:  $outPngFull"

    # rtk: run texconv
    & $TexconvExe -ft png -y -o $outDir $ddsFull | Out-Null

    # Optional: verify
    $pngName = [System.IO.Path]::GetFileNameWithoutExtension($ddsFull) + ".png"
    $pngProduced = Join-Path $outDir $pngName
    if (-not (Test-Path $pngProduced)) {
        Write-Warning "texconv did not produce expected PNG at $pngProduced"
    }
}

Write-Host ""
Write-Host "Done. Enable '$OutputModName' in MO2 (or use Overwrite) and ensure PrismaUI views are active."

