#pragma once

#include "RE/Skyrim.h"
#include <string>

namespace SoulsLoot::IconUtils
{
	/// Resolve the inventory icon texture path for an item (ICON field) when set.
	/// Skyrim uses NIF + textures; not every item has an ICON, so this often returns empty
	/// and the UI uses type-based placeholders.
	std::string GetInventoryIconPath(RE::TESBoundObject* a_item);
}

