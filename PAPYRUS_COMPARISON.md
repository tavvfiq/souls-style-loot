# Papyrus vs SKSE Plugin — Logic Comparison

Comparison of the original Papyrus scripts (DarkSoulsPickUp*) with the Souls Style Looting SKSE plugin.

## Implemented (matches or equivalent)

| Papyrus | Plugin |
|--------|--------|
| **OnDying** → roll items, store 1–3 (with chance), quest items, gold | **TESDeathEvent** → same: quest items + gold always; 1–3 random when drop chance succeeds |
| **OnActivate** → give stored items (copy), block menu unless safety key | **TESActivateEvent** → give items (copy), block container menu; safety key opens normal inventory |
| **DarkSoulsPickUpSafetyKey** (MCM) | **Config**: INI `SafetyKey` + Papyrus global `DarkSoulsPickUpSafetyKey` |
| **DarkSoulsPickUpChances** (MCM slider 1–25 = 1 in N) | **Config**: INI `DropChanceDenom` + Papyrus global `DarkSoulsPickUpChances` (value+1 = denominator) |
| Keys excluded from loot pool | Keys not in valid items (we only allow weapon/armor/ammo/misc/book) |
| Quest items always given | Quest items (VendorItemQuest keyword) always in stored loot |
| AddItem(..., true) so corpse stays dressed | We only AddObjectToContainer (no RemoveItem from corpse) |
| Gold removed on death, given on activate | Gold removed on death, stored, given on activate |

## Added in plugin

- **LootDisplaySeconds** (INI): how long the Prisma UI popup is shown (Papyrus used fixed widget timing).
- **Always block container menu** unless safety key is held (even when no loot or already looted).
- **Books** included in valid items (Papyrus “Else” branch added all non-Key; we add playable books).

## Not implemented (optional / edge cases)

1. **Crosshair + key to open inventory**  
   `DarkSoulsPickUpCrosshairQS`: when crosshair is on an actor and key 34 is pressed, open that actor’s inventory without activating. Would require crosshair-ref polling and key check in a frame hook; not done.

2. **Already-dead on load**  
   Papyrus `OnEffectStart`: if the spell is applied to an actor that is already dead, they still run `RollForItems()` so you can loot. We only get `TESDeathEvent` when someone dies after load, so corpses that were dead before load never get loot stored. Edge case; could be documented.

3. **Gold counter / iWant widgets**  
   Papyrus has a separate gold counter HUD and item widgets (iWant). We use PrismaUI for the loot popup only; no gold counter.

4. **“Busy” spin**  
   Papyrus does `While Busy == True` on activate. We don’t need that; we don’t have a separate async roll on activate.

## Config summary

- **SafetyKey**: INI or global `DarkSoulsPickUpSafetyKey` (MCM).
- **LootDisplaySeconds**: INI only.
- **DropChanceDenom**: INI or global `DarkSoulsPickUpChances` (MCM stores slider−1; we use value+1 as denominator, so slider “3” = 1 in 3).
