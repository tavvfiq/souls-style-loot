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

		/// Display loot notification (Souls-style). Items/counts must match by index. Safe to call if PrismaUI not available.
		void ShowLoot(const std::vector<RE::TESBoundObject*>& items, const std::vector<int>& counts);

		/// Whether the PrismaUI view is available.
		bool IsAvailable();
	}
}
