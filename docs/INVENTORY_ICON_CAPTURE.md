# Capturing the Item Image Shown in the Inventory Menu

Skyrim uses **NIF (mesh)** and **textures**; there is no separate “inventory icon” system like in Elden Ring.

## What the inventory actually shows

1. **2D icon in the list**  
   The small icon next to each item in the container/inventory list comes from the form’s **ICON** field (TESIcon/TESTexture). The game loads that texture path and draws it in the Scaleform (Flash) UI.  
   **We already use this:** `IconUtils::GetInventoryIconPath()` returns that same path when the form has an ICON. So when we have a path, the image we show (after DDS→PNG in your pipeline) **is** the same image as in the inventory menu. No extra “capture” is needed for that.

2. **3D preview (rotating model)**  
   The larger 3D view in the menu is rendered by **Inventory3DManager**: it loads the item’s NIF, renders it with the game’s renderer, and displays it. That is **not** a texture path; it’s a live render of the mesh.

## Options to “capture” an image

| Goal | Approach | Difficulty |
|------|----------|------------|
| Use the same 2D icon as the list | Use ICON path (current behavior). When present, it *is* the inventory icon. | Done |
| Get an image when ICON is missing | Use type-based placeholders (current fallback), or derive a path from the model (fragile). | Easy / heuristic |
| Capture the 3D preview as an image | Use `Inventory3DManager::UpdateItem3D(InventoryEntryData*)` then `Render()` to an **offscreen render target**, then read back pixels and encode to PNG. Requires creating `InventoryEntryData` for the form, setting up an RTT, and calling the renderer in a safe context. | Hard, version-sensitive |
| Snapshot the Scaleform/Flash icon | Use SKSE’s Scaleform API to find the icon movie clip for the current menu and read its bitmap. Tied to menu layout and Flash structure. | Hard, fragile |

## Practical recommendation

- **When a form has an ICON:** We already use it; that *is* the inventory list icon. Your manifest + texconv pipeline turns it into PNG for the loot UI.
- **When a form has no ICON:** Keep using type-based placeholders (weapon/armor/misc etc.). Optionally try heuristics (e.g. guess a texture path from the model path); that’s unreliable across mods.
- **3D preview capture:** Only worth it if you specifically want the rotating 3D view as the loot icon. It requires render-to-texture and readback using the game’s Inventory3DManager and renderer, which is a larger project and sensitive to game/SKSE versions.
