#pragma once

#include "RE/Skyrim.h"
#include "PrismaUI_API.h"
#include <string>
#include <vector>

namespace SoulsLoot
{
	namespace PrismaUI
	{
		/// Call after receiving SKSE kDataLoaded. Requests PrismaUI API and creates the loot view.
		void Init();

		/// Display loot notification (Souls-style). Items/counts/tiers must match by index. Tier: 0=common, 1=uncommon, 2=rare, 3=legendary (from loot roll). Safe to call if PrismaUI not available.
		void ShowLoot(const std::vector<RE::TESBoundObject*>& items, const std::vector<int>& counts, const std::vector<int>& tiers);

		/// Whether the PrismaUI view is available.
		bool IsAvailable();
	}
}
