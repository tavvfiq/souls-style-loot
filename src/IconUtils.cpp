// Skyrim doesn't have a dedicated inventory icon per item like Elden Ring. Records use NIF (mesh) and
// textures; the optional ICON field (TESIcon/TESTexture) is what the game uses in the inventory UI
// when present — so when we return a path here, it is the same image shown in the inventory menu.
// Many items leave ICON empty, so we often return empty and the loot UI falls back to type-based
// placeholder images. To "capture" the 3D preview (the rotating model in the menu) instead would
// require Inventory3DManager::UpdateItem3D + Render to an offscreen target and readback (complex).

#include "pch.h"
#include "IconUtils.h"
#include "RE/T/TESBipedModelForm.h"
#include "RE/T/TESIcon.h"
#include "RE/T/TESTexture.h"

namespace SoulsLoot::IconUtils
{
	/// Build path from default path + texture name when GetAsNormalFile returns empty (some runtimes).
	static void TryTexturePath(RE::TESTexture* tex, RE::BSString& path)
	{
		if (!tex || tex->textureName.size() == 0) return;
		tex->GetAsNormalFile(path);
		if (path.empty()) {
			const char* prefix = tex->GetDefaultPath();
			std::string built(prefix ? prefix : "Textures");
			if (!built.empty() && built.back() != '/' && built.back() != '\\')
				built += '/';
			built += tex->textureName.c_str();
			path = built.c_str();
		}
	}

	std::string GetInventoryIconPath(RE::TESBoundObject* a_item)
	{
		if (!a_item) {
			return {};
		}

		RE::BSString path;

		// 1) Forms with TESIcon/TESTexture (weapons, ammo, misc, books, potions, ingredients, etc.)
		if (auto* tex = a_item->As<RE::TESTexture>(); tex) {
			TryTexturePath(tex, path);
		}

		// 2) Armor: use inventory icon from biped model (ARMO doesn't have TESIcon)
		if (path.empty() && a_item->IsArmor()) {
			if (auto* bip = a_item->As<RE::TESBipedModelForm>()) {
				for (std::size_t i = 0; i < std::size(bip->inventoryIcons) && path.empty(); ++i) {
					TryTexturePath(&bip->inventoryIcons[i], path);
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

