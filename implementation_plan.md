# Implementation Plan: Souls Style Looting (SKSE Conversion)

This document outlines the plan for converting the "Souls Style Looting" Skyrim mod from its current Papyrus implementation into a native SKSE plugin using the provided `commonlibsse` template.

## Goal Description
The objective is to translate the core mod logic—intercepting NPC death, rolling an item drop, and overriding the vanilla inventory interaction upon player activation—into C++ for better performance and reliability. The visual UI (iWant Widgets) will remain in Papyrus for now, but will be driven by new native SKSE Papyrus functions exposed by our C++ plugin.

## Proposed Changes

### [Plugin Core]
We need to set up the SKSE listening interfaces to track game events.

#### [MODIFY] [main.cpp](file:///d:/Modding/Souls%20Style%20Looting/Data/skse_code/souls-style-loot/src/main.cpp)
- **Initialize Event Sinks:** Register listeners for `RE::TESDeathEvent` (when an NPC dies to roll for loot) and `RE::TESActivateEvent` (when the player activates the corpse to trigger the pickup sequence).
- **Register Papyrus Functions:** Bind our custom native C++ functions to the scripting engine so the remaining UI Papyrus scripts can communicate with the SKSE plugin.

### [Event Handlers]
Create a new module to handle the intercepted engine events.

#### [NEW] `src/Events.h` and `src/Events.cpp`
- **Class `DeathEventHandler`:** Inherits from `RE::BSTEventSink<RE::TESDeathEvent>`.
  - On death, scans the actor's inventory (`actor->GetInventory()`).
  - Instantly transfers gold to the player.
  - Filters valid Weapons, Armor, and Misc Items into a temporary `std::vector<RE::TESBoundObject*>`.
  - Randomly selects 1-3 items based on a global variable chance (`DarkSoulsPickUpChances`).
  - Stores this generated "loot list" in an internal map/dictionary associating the dead actor's FormID to their generated drop list.
- **Class `ActivateEventHandler`:** Closes over `RE::BSTEventSink<RE::TESActivateEvent>`.
  - When an activation occurs, checks if the `activator` is the Player and the `target` is a dead Actor.
  - Looks up the target's FormID in our internal loot map.
  - If a drop exists, it transfers the items to the player, triggers the UI widget update (via a native Papyrus call), marks the corpse as looted, and importantly: **Returns `RE::BSEventNotifyControl::kStop`** to prevent the standard inventory menu from opening.

### [Papyrus Bindings]
Create a bridge between our C++ logic and the existing iWant Widgets UI script.

#### [NEW] `src/Papyrus.h` and `src/Papyrus.cpp`
- Expose a native function, e.g., `UpdateLootWidgets(RE::StaticFunctionTag*, std::vector<RE::TESBoundObject*> items, std::vector<int> counts)`.
- This function will be called internally from C++ when the player activates a valid corpse. The C++ will trigger an event/call to the [DarkSoulsPickUpWidgetsScript.psc](file:///d:/Modding/Souls%20Style%20Looting/Data/Source/Scripts/DarkSoulsPickUpWidgetsScript.psc) telling it what brackets and text to render on screen.

### [Papyrus Simplification]
The old Papyrus logic will be gutted, leaving only the UI handling.

#### [MODIFY] [DarkSoulsPickUpMainMESCript.psc](file:///d:/Modding/Souls%20Style%20Looting/Data/Source/Scripts/DarkSoulsPickUpMainMESCript.psc) (To Be Deleted/Deprecated)
- The entire `OnDying`, `OnActivate`, `RollForItems`, and `PickItems` logic will be outright deleted because it is now handled in C++.
- The Magic Effects, Spells, and dummy Dagger items are no longer needed.

#### [MODIFY] [DarkSoulsPickUpWidgetsScript.psc](file:///d:/Modding/Souls%20Style%20Looting/Data/Source/Scripts/DarkSoulsPickUpWidgetsScript.psc)
- Refactor the script to no longer depend on keyboard inputs (`OnKeyDown`).
- Expose a new function `ShowLoot(Form[] droppedItems, Int[] amounts)` that the SKSE plugin will call directly upon a successful native activation.

---

## Verification Plan

### Automated Tests
*Note: SKSE plugin development lacks traditional automated testing suites outside the game engine.*
- **Build Verification:** Run `xmake` locally to ensure the C++ code compiles successfully against CommonLibSSE and generates the `.dll`.

### Manual Verification
1. **Compilation & Installation:** Build the `.dll` and place it in the Skyrim `Data/SKSE/Plugins` directory along with the modified [.psc](file:///d:/Modding/Souls%20Style%20Looting/Data/Source/Scripts/DarkSoulsPickUpMCM.psc) compiled scripts.
2. **In-Game: Core Mechanic Check:**
   - Kill an NPC (e.g., a Bandit).
   - Verify the SKSE log confirms the `TESDeathEvent` was caught and a loot list was generated instantly.
   - Activate the dead bandit.
   - **Crucial:** Verify the vanilla inventory menu *does not* appear.
   - Verify the generated items and gold are added to the player's inventory.
3. **In-Game: UI Integration Check:**
   - Upon activation in the previous step, verify that the `iWant Widgets` (brackets and text) correctly appear on the screen displaying the exact items received.
4. **In-Game: Stress Test:**
   - Kill multiple enemies at once (e.g., using a wide AoE spell) and verify no script lag occurs and all bodies correctly generate and store their drop lists.
