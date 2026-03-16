#pragma once

#include "RE/Skyrim.h"
#include <string>

namespace SoulsLoot::IconUtils
{
	/// Resolve the inventory icon texture path for an item as a normalized relative path.
	/// Returns empty string if no icon texture is available.
	std::string GetInventoryIconPath(RE::TESBoundObject* a_item);
}

