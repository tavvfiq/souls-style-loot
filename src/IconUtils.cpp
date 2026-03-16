#include "pch.h"
#include "IconUtils.h"

namespace SoulsLoot::IconUtils
{
	std::string GetInventoryIconPath(RE::TESBoundObject* a_item)
	{
		if (!a_item) {
			return {};
		}

		RE::BSString path;

		// Primary: object texture (works for many misc items, books, etc.)
		if (auto* tex = a_item->As<RE::TESTexture>(); tex && tex->textureName.size() > 0) {
			tex->GetAsNormalFile(path);
		} else if (a_item->IsArmor()) {
			// Fallback for armor: use first inventory icon from biped model form
			if (auto* bip = a_item->As<RE::TESBipedModelForm>()) {
				if (bip->inventoryIcons[0].textureName.size() > 0) {
					bip->inventoryIcons[0].GetAsNormalFile(path);
				}
			}
		}

		if (path.empty()) {
			return {};
		}

		std::string result(path.c_str());
		for (char& c : result) {
			if (c == '\\') {
				c = '/';
			}
		}
		return result;
	}
}

