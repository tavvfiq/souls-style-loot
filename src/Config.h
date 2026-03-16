#pragma once

namespace SoulsLoot
{
	namespace Config
	{
		/// Call once after kDataLoaded. Loads from INI and optional Papyrus globals (MCM).
		void Load();

		/// Virtual key code for "open normal inventory" when held during corpse activate. 0 = disabled. Default 16 (Shift).
		int GetSafetyKeyCode();

		/// Seconds to show the loot popup (fallback if not using cycle). Default 5.0.
		double GetLootDisplaySeconds();

		/// Seconds to show each item before cycling to the next. Default 2.0.
		double GetLootCycleDelaySeconds();

		/// Virtual key code to close the loot UI after last item (activate key). Default 0x45 (E).
		int GetLootCloseKeyCode();

		/// Chance that a corpse drops any random loot, as percentage (0-100). 100 = always. Quest items and gold always. Default 100.
		double GetDropChancePercent();

		/// Tier drop chance as percentage (0-100). Tier 0=common, 1=uncommon, 2=rare, 3=legendary. Defaults 100, 50, 25, 12.5.
		double GetTierDropChancePercent(int a_tier);

		/// Item-type drop chance as percentage (0-100). Type 0=weapon, 1=armor, 2=ammo, 3=misc, 4=book. Default 100.
		double GetTypeDropChancePercent(int a_type);
	}
}
