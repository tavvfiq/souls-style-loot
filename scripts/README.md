# Copy mod layout

Copies the built plugin and PrismaUI view into the correct mod folder structure.

## Usage

1. Build the plugin: `xmake build`
2. Run from the **souls-style-loot** directory:

   **Option A – default output (project folder)**  
   ```bat
   scripts\copy_mod_layout.bat
   ```
   Creates: `mod_output/Souls Style Looting/SKSE/Plugins/` (DLL + PDB) and `.../Data/PrismaUI/views/SoulsStyleLoot/` (so PrismaUI finds the view at game Data merge).

   **Option B – your mod manager folder**  
   Set `SOULS_MOD_OUTPUT` to the parent of the mod folder (e.g. your MO2 `mods` path), then run:
   ```bat
   set SOULS_MOD_OUTPUT=D:\Modding\MO2\mods
   scripts\copy_mod_layout.bat
   ```
   Creates: `D:\Modding\MO2\mods\Souls Style Looting\SKSE\Plugins\` and `...\Data\PrismaUI\views\SoulsStyleLoot\`

## PowerShell only

```powershell
cd "D:\Modding\Souls Style Looting\Data\skse_code\souls-style-loot"
.\scripts\copy_mod_layout.ps1
```

With custom output:
```powershell
$env:SOULS_MOD_OUTPUT = "D:\Modding\MO2\mods"
.\scripts\copy_mod_layout.ps1
```
