#include "Events.h"
#include "Config.h"
#include "PrismaUI.h"
#include "SoulsLog.h"
#include "RE/B/BGSKeywordForm.h"
#include "RE/C/ContainerMenu.h"
#include "RE/T/TESDataHandler.h"
#include "RE/U/UIMessageQueue.h"
#include <random>
#include <unordered_set>
#include <Windows.h>
#ifdef PlaySound
#undef PlaySound
#endif

namespace SoulsLoot
{
	namespace
	{
		// Papyrus mod spell applied to corpses that have loot (shader/art effect). Looked up from DarkSoulsPickUp.esp.
		RE::SpellItem* GetLootEffectSpell()
		{
			static RE::SpellItem* cached = nullptr;
			if (cached) return cached;
			auto* dh = RE::TESDataHandler::GetSingleton();
			if (!dh) return nullptr;
			// Form ID 0x00000D6F = DarkSoulsPickUpEnemySpell in the plugin
			cached = dh->LookupForm<RE::SpellItem>(0x00000D6F, "DarkSoulsPickUp.esp");
			if (cached) {
				SoulsLog::Line("Loot effect spell (DarkSoulsPickUpEnemySpell) found - will apply to corpses with loot");
			}
			return cached;
		}

		// Soul Sediment (EldenSkyrim_RimSkills.esp) - auto-loot on death like gold. Look up by editor ID.
		static constexpr const char* SOUL_SEDIMENT_EDITOR_IDS[] = {
			"EldenTalentSoulGemShardAlch_1", "EldenTalentSoulGemShardAlch_2", "EldenTalentSoulGemShardAlch_3",
			"EldenTalentSoulGemShardAlch_4", "EldenTalentSoulGemShardAlch_5", "EldenTalentSoulGemShardAlch_6",
			"EldenTalentSoulGemShardAlch_7", "EldenTalentSoulGemShardAlch_8", "EldenTalentSoulGemShardAlch_9",
			"EldenTalentSoulGemShardAlch_10"
		};
		bool IsSoulGemFragment(RE::TESBoundObject* a_item)
		{
			if (!a_item) return false;
			static RE::TESBoundObject* s_fragments[10] = { nullptr };
			static bool s_lookedUp = false;
			if (!s_lookedUp) {
				s_lookedUp = true;
				for (int i = 0; i < 10; i++) {
					RE::TESForm* f = RE::TESForm::LookupByEditorID(SOUL_SEDIMENT_EDITOR_IDS[i]);
					s_fragments[i] = f ? f->As<RE::TESBoundObject>() : nullptr;
				}
			}
			for (RE::TESBoundObject* ref : s_fragments) {
				if (ref && a_item == ref) return true;
			}
			return false;
		}

		// Item tier for drop chance: 0=common, 1=uncommon, 2=rare, 3=legendary. Based on material keywords (weapon/armor) or gold value (misc/book/ammo).
		int GetItemTier(RE::TESBoundObject* a_item)
		{
			if (!a_item) return 0;
			auto* kf = a_item->As<RE::BGSKeywordForm>();
			auto hasKw = [kf](const char* id) -> bool {
				return kf && kf->HasKeywordString(id);
			};
			// Legendary: Daedric, Dragon
			if (hasKw("WeapMaterialDaedric") || hasKw("WeapMaterialDragonbone") || hasKw("WeapMaterialDragon")
				|| hasKw("ArmorMaterialDaedric") || hasKw("ArmorMaterialDragonplate") || hasKw("ArmorMaterialDragonscale"))
				return 3;
			// Rare: Ebony, Glass, Stalhrim
			if (hasKw("WeapMaterialEbony") || hasKw("WeapMaterialGlass") || hasKw("WeapMaterialStalhrim")
				|| hasKw("ArmorMaterialEbony") || hasKw("ArmorMaterialGlass") || hasKw("ArmorMaterialStalhrim"))
				return 2;
			// Uncommon: Orcish, Dwarven, Elven, Nordic
			if (hasKw("WeapMaterialOrcish") || hasKw("WeapMaterialDwarven") || hasKw("WeapMaterialElven") || hasKw("WeapMaterialNordic")
				|| hasKw("ArmorMaterialOrcish") || hasKw("ArmorMaterialDwarven") || hasKw("ArmorMaterialElven") || hasKw("ArmorMaterialNordic")
				|| hasKw("ArmorMaterialScaled") || hasKw("WeapMaterialSilver"))
				return 1;
			// Misc/Book/Ammo: use value. Common <100, Uncommon <500, Rare <2000, else Legendary
			if (a_item->Is(RE::FormType::Misc) || a_item->Is(RE::FormType::Book) || a_item->IsAmmo()) {
				int val = static_cast<int>(a_item->GetGoldValue());
				if (val >= 2000) return 3;
				if (val >= 500) return 2;
				if (val >= 100) return 1;
			}
			return 0;  // common (iron, steel, leather, low-value, etc.)
		}

		// Item type for type-based drop chance: 0=weapon, 1=armor, 2=ammo, 3=misc, 4=book
		int GetItemType(RE::TESBoundObject* a_item)
		{
			if (!a_item) return 3;
			if (a_item->IsWeapon()) return 0;
			if (a_item->IsArmor()) return 1;
			if (a_item->IsAmmo()) return 2;
			if (a_item->Is(RE::FormType::Misc)) return 3;
			if (a_item->Is(RE::FormType::Book)) return 4;
			return 3;
		}
	}
	// --- Loot Manager ---
	void LootManager::StoreLoot(RE::FormID a_actorID, LootDrop a_loot)
	{
		std::lock_guard<std::mutex> lock(_lock);
		_lootMap[a_actorID] = a_loot;
	}

	bool LootManager::GetLoot(RE::FormID a_actorID, LootDrop& a_outLoot)
	{
		std::lock_guard<std::mutex> lock(_lock);
		auto it = _lootMap.find(a_actorID);
		if (it != _lootMap.end()) {
			a_outLoot = it->second;
			return true;
		}
		return false;
	}

	void LootManager::RemoveLoot(RE::FormID a_actorID)
	{
		std::lock_guard<std::mutex> lock(_lock);
		_lootMap.erase(a_actorID);
	}

	// --- Death Event Handler ---
	DeathEventHandler* DeathEventHandler::GetSingleton()
	{
		static DeathEventHandler singleton;
		return &singleton;
	}

	void DeathEventHandler::Register()
	{
		auto scripts = RE::ScriptEventSourceHolder::GetSingleton();
		if (scripts) {
			scripts->AddEventSink(GetSingleton());
			SKSE::log::info("Registered DeathEventHandler");
		}
	}

	RE::BSEventNotifyControl DeathEventHandler::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		if (!a_event || !a_event->actorDying) return RE::BSEventNotifyControl::kContinue;

		auto actor = a_event->actorDying->As<RE::Actor>();
		if (!actor || actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;

		auto inventory = actor->GetInventory();
		static constexpr std::string_view QUEST_ITEM_KEYWORD = "VendorItemQuest";

		std::vector<RE::TESBoundObject*> validItems;
		std::vector<RE::TESBoundObject*> questItems;
		int totalGold = 0;
		std::vector<std::pair<RE::TESBoundObject*, int>> soulGemFragmentsToGive;
		auto goldRef = RE::TESForm::LookupByID<RE::TESBoundObject>(0xF); // Septim

		auto is_quest_item = [](RE::TESBoundObject* item) -> bool {
			auto* kf = item ? item->As<RE::BGSKeywordForm>() : nullptr;
			return kf && kf->HasKeywordString(QUEST_ITEM_KEYWORD);
		};

		for (const auto& [item, data] : inventory) {
			if (!item || item->IsDeleted()) continue;

			// Handle Gold instantly
			if (item == goldRef) {
				totalGold += data.first;
				actor->RemoveItem(item, data.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				continue;
			}

			// Auto-loot Soul Gem Fragments (SoulGemPiece001-005) on death (collect here, remove/give after loop)
			if (IsSoulGemFragment(item)) {
				soulGemFragmentsToGive.push_back({ item, data.first });
				continue;
			}

			// Quest items (by keyword): always include in loot but do not put in random pool
			if (is_quest_item(item)) {
				questItems.push_back(item);
				continue;
			}

			// Filter: weapons, armor, ammo, misc, books (playable). Keys excluded like Papyrus.
			if (item->IsWeapon() || item->IsArmor() || item->IsAmmo() || item->Is(RE::FormType::Misc) || item->Is(RE::FormType::Book)) {
				if (item->GetPlayable()) {
					validItems.push_back(item);
				}
			}
		}

		// Give gold to the player immediately on death (auto pick up)
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (player) {
			if (totalGold > 0 && goldRef) {
				player->AddObjectToContainer(goldRef, nullptr, totalGold, nullptr);
				RE::PlaySound("ITMCoinB");
			}
			// Auto-loot Soul Gem Fragments on death
			for (const auto& [form, count] : soulGemFragmentsToGive) {
				if (form && count > 0) {
					player->AddObjectToContainer(form, nullptr, count, nullptr);
					actor->RemoveItem(form, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				}
			}
		}

		// Store: all quest items + gold amount (for UI) + 1–3 random when drop chance succeeds (like Papyrus DarkSoulsPickUpChances)
		LootDrop finalLoot;
		finalLoot.goldAmount = totalGold;

		for (RE::TESBoundObject* qi : questItems) {
			finalLoot.items.push_back(qi);
			finalLoot.counts.push_back(inventory[qi].first);
		}

		std::unordered_set<RE::TESBoundObject*> alreadyAdded(finalLoot.items.begin(), finalLoot.items.end());
		double dropChancePct = Config::GetDropChancePercent();
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<double> realDist(0.0, 1.0);
		bool rollSuccess = (dropChancePct >= 100.0) || (realDist(gen) < (dropChancePct / 100.0));
		if (!validItems.empty() && rollSuccess) {
			std::vector<RE::TESBoundObject*> shuffled = validItems;
			std::shuffle(shuffled.begin(), shuffled.end(), gen);
			std::vector<RE::TESBoundObject*> candidates;
			for (RE::TESBoundObject* it : shuffled) {
				int tier = GetItemTier(it);
				double tierChancePct = Config::GetTierDropChancePercent(tier);
				bool passTier = (tierChancePct >= 100.0) || (realDist(gen) < (tierChancePct / 100.0));
				int type = GetItemType(it);
				double typeChancePct = Config::GetTypeDropChancePercent(type);
				bool passType = (typeChancePct >= 100.0) || (realDist(gen) < (typeChancePct / 100.0));
				if (passTier && passType) candidates.push_back(it);
			}
			// If no item passed tier roll, add one random so something can still drop
			if (candidates.empty()) candidates.push_back(shuffled[0]);
			const size_t maxDrops = candidates.size() < 3u ? candidates.size() : 3u;
			for (size_t i = 0; i < maxDrops; i++) {
				RE::TESBoundObject* rolledItem = candidates[i];
				if (alreadyAdded.insert(rolledItem).second) {
					finalLoot.items.push_back(rolledItem);
					finalLoot.counts.push_back(inventory[rolledItem].first);
				}
			}
		}

		// Store it in our global map and apply Papyrus-style loot effect (shader/art) if spell exists
		if (finalLoot.goldAmount > 0 || !finalLoot.items.empty()) {
			LootManager::GetSingleton()->StoreLoot(actor->GetFormID(), finalLoot);
			RE::SpellItem* effectSpell = GetLootEffectSpell();
			if (effectSpell) {
				actor->AddSpell(effectSpell);
			}
			SoulsLog::LineF("Death: stored loot for %08X (%u items, %d gold)", actor->GetFormID(), static_cast<unsigned>(finalLoot.items.size()), finalLoot.goldAmount);
			SKSE::log::info("Stored loot for actor {:08X} ({} items, {} gold)", actor->GetFormID(), finalLoot.items.size(), finalLoot.goldAmount);
		} else {
			SoulsLog::LineF("Death: actor %08X - no loot stored (validItems=%u, gold=%d)", actor->GetFormID(), static_cast<unsigned>(validItems.size()), totalGold);
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	// --- Activate Event Handler ---
	ActivateEventHandler* ActivateEventHandler::GetSingleton()
	{
		static ActivateEventHandler singleton;
		return &singleton;
	}

	void ActivateEventHandler::Register()
	{
		auto scripts = RE::ScriptEventSourceHolder::GetSingleton();
		if (scripts) {
			scripts->AddEventSink(GetSingleton());
			SKSE::log::info("Registered ActivateEventHandler");
		}
	}

	RE::BSEventNotifyControl ActivateEventHandler::ProcessEvent(const RE::TESActivateEvent* a_event, RE::BSTEventSource<RE::TESActivateEvent>*)
	{
		if (!a_event || !a_event->actionRef || !a_event->objectActivated) return RE::BSEventNotifyControl::kContinue;

		auto activator = a_event->actionRef->As<RE::Actor>();
		auto target = a_event->objectActivated->As<RE::Actor>();

		if (!activator || !activator->IsPlayerRef() || !target || !target->IsDead()) {
			return RE::BSEventNotifyControl::kContinue;
		}

		auto player = RE::PlayerCharacter::GetSingleton();

		// Safety key (set in MCM or SoulsStyleLooting.ini): hold to open normal container menu instead of Souls-style loot
		int safetyKey = Config::GetSafetyKeyCode();
		if (safetyKey != 0 && (GetAsyncKeyState(safetyKey) & 0x8000)) {
			SoulsLog::LineF("Activate: corpse %08X - safety key held, opening normal menu", target->GetFormID());
			return RE::BSEventNotifyControl::kContinue;
		}

		// Always block container menu unless safety key held (no loot, already looted, or we have loot)
		// Close container menu immediately to avoid blink (menu may already be open when we're called)
		if (auto* uiQueue = RE::UIMessageQueue::GetSingleton()) {
			uiQueue->AddMessage(RE::BSFixedString(RE::ContainerMenu::MENU_NAME), RE::UI_MESSAGE_TYPE::kForceHide, nullptr);
		}

		LootDrop loot;
		bool haveLoot = LootManager::GetSingleton()->GetLoot(target->GetFormID(), loot);
		if (haveLoot) {
			SoulsLog::LineF("Activate: corpse %08X - HAVE LOOT (%u items), blocking menu", target->GetFormID(), static_cast<unsigned>(loot.items.size()));
			SKSE::log::info("Activate corpse {:08X}: have loot ({} items), blocking menu and showing UI", target->GetFormID(), loot.items.size());

			// Transfer the items (gold was already given on death)
			for (size_t i = 0; i < loot.items.size(); i++) {
				player->AddObjectToContainer(loot.items[i], nullptr, loot.counts[i], nullptr);
			}

			std::vector<RE::TESBoundObject*> itemsCopy = loot.items;
			std::vector<int> countsCopy = loot.counts;
			if (auto* task = SKSE::GetTaskInterface()) {
				task->AddTask([itemsCopy, countsCopy]() {
					if (auto* uiQueue = RE::UIMessageQueue::GetSingleton()) {
						uiQueue->AddMessage(RE::BSFixedString(RE::ContainerMenu::MENU_NAME), RE::UI_MESSAGE_TYPE::kForceHide, nullptr);
					}
					SoulsLoot::PrismaUI::ShowLoot(itemsCopy, countsCopy);
				});
			} else {
				SoulsLoot::PrismaUI::ShowLoot(loot.items, loot.counts);
			}
			RE::PlaySound("UISkillsLevelUp");
			// Remove Papyrus-style loot effect spell from corpse (same as Papyrus RemoveSpell when looted)
			RE::SpellItem* effectSpell = GetLootEffectSpell();
			if (effectSpell) {
				target->RemoveSpell(effectSpell);
			}
			LootManager::GetSingleton()->RemoveLoot(target->GetFormID());
		} else {
			SoulsLog::LineF("Activate: corpse %08X - no loot / already looted, blocking menu", target->GetFormID());
			// Still close container menu if it opened (event order)
			if (auto* task = SKSE::GetTaskInterface()) {
				task->AddTask([]() {
					if (auto* uiQueue = RE::UIMessageQueue::GetSingleton()) {
						uiQueue->AddMessage(RE::BSFixedString(RE::ContainerMenu::MENU_NAME), RE::UI_MESSAGE_TYPE::kForceHide, nullptr);
					}
				});
			}
		}

		return RE::BSEventNotifyControl::kStop;
	}
}
