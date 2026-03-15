# Souls Style Looting – PrismaUI Migration Plan

## Goal
Replace iWant Widgets with PrismaUI for the Souls-style item pickup notification. All UI is driven from the SKSE plugin via PrismaUI's C++ API; no Papyrus widget scripts.

## Current Flow (iWant Widgets)
1. **Events.cpp**: On activate corpse → transfer items → `Papyrus::UpdateWidgets(items, counts)`.
2. **Papyrus.cpp**: Builds VM arrays, dispatches `DarkSoulsPickUpWidgetsScript.ShowLoot(items, counts)`.
3. **Papyrus (iWant)**: Quest/script shows brackets and item names on screen.

## Target Flow (PrismaUI)
1. **Events.cpp**: On activate corpse → transfer items → `PrismaUI::ShowLoot(items, counts)`.
2. **PrismaUI module**: Resolve item display names, serialize to JSON, call `InteropCall(view, "showLoot", json)`.
3. **HTML/JS view**: Receives loot data, displays Souls-style lines (e.g. `[ Iron Sword x 1 ]`), auto-hide after delay.

## Implementation Steps

### 1. Add PrismaUI API and messaging
- Copy `PrismaUI_API.h` from example-skse-plugin into `src/` (or add include path).
- In `main.cpp`: register for `SKSE::MessagingInterface::kDataLoaded` (already done); in handler, request PrismaUI API via `RequestPluginAPI(V1)`, store interface pointer, create view.

### 2. PrismaUI view lifecycle
- **On kDataLoaded**: `CreateView("SoulsStyleLoot/index.html", onDomReady)`. Store `PrismaView` in a module singleton or namespace. Optionally call `Show`/`Hide` in onDomReady to start hidden.
- **ShowLoot**: Call `InteropCall(view, "showLoot", json)` with JSON array of `{ "name": "...", "count": N }`. View shows, then hides after timeout (JS or C++ timer).

### 3. Loot display module (C++)
- New files: `PrismaUI.h`, `PrismaUI.cpp`.
- `Init(IVPrismaUI1* api)` – store API, create view (path `SoulsStyleLoot/index.html`).
- `ShowLoot(const std::vector<RE::TESBoundObject*>& items, const std::vector<int>& counts)` – for each item get display name (`item->GetName()` or fallback), build JSON string, `InteropCall(view, "showLoot", json)`.
- Escape JSON strings (names) for safety. Use simple JSON array: `[{"name":"Iron Sword","count":1},...]`.

### 4. HTML/JS view
- Create `view/index.html` (or `views/SoulsStyleLoot/index.html`) in the Data side of the mod.
- Structure: minimal HTML, a container div for loot lines, CSS for Souls-like look (brackets, font, position).
- Expose `window.showLoot = function(jsonString) { ... }` – parse JSON, render lines, show container, set timeout to hide.
- Register with Prisma: `InteropCall` invokes `showLoot(argument)` from C++; argument is the JSON string.

### 5. Events.cpp changes
- Replace `#include "Papyrus.h"` with `#include "PrismaUI.h"` for widget display.
- Replace `Papyrus::UpdateWidgets(loot.items, loot.counts)` with `PrismaUI::ShowLoot(loot.items, loot.counts)`.
- Keep sounds and item transfer as-is.

### 6. Papyrus cleanup
- Remove or stub `Papyrus::UpdateWidgets` (no longer call it).
- Optionally keep `Register()` and `UpdateWidgets`/`DispatchWidgetUpdate` for backwards compatibility (no-op) or remove Papyrus registration of `UpdateWidgets` and the VM dispatch code. Prefer: remove widget-related Papyrus so the mod has no iWant dependency.

### 7. Build and data layout
- Add PrismaUI_API.h; no PrismaUI lib link (runtime GetProcAddress).
- Ensure view path matches: mod ships `Data/views/SoulsStyleLoot/index.html` (or document install to PrismaUI’s views folder). Example README: plugin folder = `PrismaUI/views/PrismaUI-Example-UI/`; we use `SoulsStyleLoot/index.html` so our mod folder should contain `PrismaUI/views/SoulsStyleLoot/index.html`.

### 8. Optional
- MCM or config for display duration, position, or toggling PrismaUI vs no UI (still give items, just no notification).

## File Checklist
- [x] `src/PrismaUI_API.h` – from example (standalone, uses GetModuleHandleW)
- [x] `src/PrismaUI.h` / `src/PrismaUI.cpp` – init, CreateView, ShowLoot, JSON build
- [x] `src/Events.cpp` – call PrismaUI::ShowLoot instead of Papyrus::UpdateWidgets
- [x] `src/main.cpp` – init PrismaUI on kDataLoaded (create view after API request)
- [x] `view/SoulsStyleLoot/index.html` – UI + showLoot(json)
- [x] Papyrus: removed; no iWant Widgets dependency

## Install (PrismaUI view)
PrismaUI loads views from **`<Game>/Data/PrismaUI/views/`** (base path = `Data/PrismaUI`). The plugin requests `SoulsStyleLoot/index.html`, so the file must exist at:
**`<Game>/Data/PrismaUI/views/SoulsStyleLoot/index.html`**

1. Install **PrismaUI** and **Souls Style Looting** (DLL in `Data/SKSE/Plugins/`).
2. Place the view so it ends up in the game's Data merge:
   - **MO2/Vortex:** Ship the mod with **`Data/PrismaUI/views/SoulsStyleLoot/index.html`** in your mod folder. The manager will merge it so the path above exists.
   - **Manual:** Copy `view/SoulsStyleLoot/index.html` to your game folder as **`Data/PrismaUI/views/SoulsStyleLoot/index.html`** (create `SoulsStyleLoot` if needed).
3. The copy script now outputs to `.../Data/PrismaUI/views/SoulsStyleLoot/` so the mod layout is correct for merging.

## JSON Format
```json
[{"name":"Iron Sword","count":1},{"name":"Gold","count":50}]
```
Names from `RE::TESForm::GetName()`; escape `"` and `\` in names.
