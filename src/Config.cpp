#include "pch.h"
#include "Config.h"
#include "SoulsLog.h"
#include <fstream>
#include <sstream>
#include <string>

namespace SoulsLoot
{
	namespace Config
	{
		namespace
		{
			int s_safetyKeyCode = 16;   // VK_SHIFT
			double s_lootDisplaySeconds = 5.0;
			double s_lootCycleDelaySeconds = 2.0;
			int s_lootCloseKeyCode = 0x45;  // VK 'E' (activate)
			double s_dropChancePercent = 100.0;  // chance that corpse drops random loot (0-100%)
			double s_tierDropChancePct[4] = { 100.0, 50.0, 25.0, 12.5 };  // common, uncommon, rare, legendary (%)
			double s_typeDropChancePct[5] = { 100.0, 100.0, 100.0, 100.0, 100.0 };  // weapon, armor, ammo, misc, book (%)

			int s_generateItemIconsMode = 0;  // 0=disabled, 1=run, 2=done
			std::string s_texconvPath;
			std::string s_iconOutputDir;
			std::string s_iconManifestPath;
			bool s_loaded = false;

			void ReadIni()
			{
				auto logDir = SKSE::log::log_directory();
				if (!logDir) return;
				std::ifstream f(*logDir / "SoulsStyleLooting.ini");
				if (!f) return;
				std::string line;
				while (std::getline(f, line)) {
					auto start = line.find_first_not_of(" \t\r\n");
					if (start == std::string::npos || line[start] == '[' || line[start] == ';' || line[start] == '#')
						continue;
					auto eq = line.find('=', start);
					if (eq == std::string::npos) continue;
					std::string key = line.substr(start, eq - start);
					auto endKey = key.find_last_not_of(" \t");
					if (endKey != std::string::npos) key.resize(endKey + 1);
					std::string val = line.substr(eq + 1);
					auto startVal = val.find_first_not_of(" \t");
					if (startVal != std::string::npos) val = val.substr(startVal);
					if (key == "SafetyKey") {
						int v = 0;
						if (std::istringstream(val) >> v) s_safetyKeyCode = v;
					} else if (key == "GenerateItemIcons") {
						int v = 0;
						if (std::istringstream(val) >> v) s_generateItemIconsMode = v;
					} else if (key == "TexconvPath") {
						s_texconvPath = val;
					} else if (key == "IconOutputDir") {
						s_iconOutputDir = val;
					} else if (key == "IconManifestPath") {
						s_iconManifestPath = val;
					} else if (key == "LootDisplaySeconds") {
						double v = 0;
						if (std::istringstream(val) >> v && v > 0) s_lootDisplaySeconds = v;
					} else if (key == "LootCycleDelaySeconds") {
						double v = 0;
						if (std::istringstream(val) >> v && v > 0) s_lootCycleDelaySeconds = v;
					} else if (key == "LootCloseKeyCode") {
						int v = 0;
						if (std::istringstream(val) >> v) s_lootCloseKeyCode = v;
					} else if (key == "DropChancePercent") {
						double v = 0;
						if (std::istringstream(val) >> v) { v = (v < 0) ? 0 : (v > 100 ? 100 : v); s_dropChancePercent = v; }
					} else if (key == "DropChanceDenom") {
						// Backward compat: 1 in N -> percent = 100/N
						int n = 0;
						if (std::istringstream(val) >> n && n >= 1) s_dropChancePercent = 100.0 / static_cast<double>(n);
					} else if (key == "Tier0DropChance" || key == "TierCommonChance") {
						double v = 0;
						if (std::istringstream(val) >> v) { v = (v < 0) ? 0 : (v > 100 ? 100 : v); s_tierDropChancePct[0] = v; }
					} else if (key == "Tier1DropChance" || key == "TierUncommonChance") {
						double v = 0;
						if (std::istringstream(val) >> v) { v = (v < 0) ? 0 : (v > 100 ? 100 : v); s_tierDropChancePct[1] = v; }
					} else if (key == "Tier2DropChance" || key == "TierRareChance") {
						double v = 0;
						if (std::istringstream(val) >> v) { v = (v < 0) ? 0 : (v > 100 ? 100 : v); s_tierDropChancePct[2] = v; }
					} else if (key == "Tier3DropChance" || key == "TierLegendaryChance") {
						double v = 0;
						if (std::istringstream(val) >> v) { v = (v < 0) ? 0 : (v > 100 ? 100 : v); s_tierDropChancePct[3] = v; }
					} else if (key == "WeaponDropChance") {
						double v = 0;
						if (std::istringstream(val) >> v) { v = (v < 0) ? 0 : (v > 100 ? 100 : v); s_typeDropChancePct[0] = v; }
					} else if (key == "ArmorDropChance") {
						double v = 0;
						if (std::istringstream(val) >> v) { v = (v < 0) ? 0 : (v > 100 ? 100 : v); s_typeDropChancePct[1] = v; }
					} else if (key == "AmmoDropChance") {
						double v = 0;
						if (std::istringstream(val) >> v) { v = (v < 0) ? 0 : (v > 100 ? 100 : v); s_typeDropChancePct[2] = v; }
					} else if (key == "MiscDropChance") {
						double v = 0;
						if (std::istringstream(val) >> v) { v = (v < 0) ? 0 : (v > 100 ? 100 : v); s_typeDropChancePct[3] = v; }
					} else if (key == "BookDropChance") {
						double v = 0;
						if (std::istringstream(val) >> v) { v = (v < 0) ? 0 : (v > 100 ? 100 : v); s_typeDropChancePct[4] = v; }
					}
				}
			}

			void ApplyPapyrusGlobals()
			{
				RE::TESForm* form = RE::TESForm::LookupByEditorID("DarkSoulsPickUpSafetyKey");
				RE::TESGlobal* global = form ? form->As<RE::TESGlobal>() : nullptr;
				if (global && global->value >= 0) {
					s_safetyKeyCode = static_cast<int>(global->value);
					SoulsLog::LineF("Config: SafetyKey from Papyrus global = %d", s_safetyKeyCode);
				}
				form = RE::TESForm::LookupByEditorID("DarkSoulsPickUpChances");
				global = form ? form->As<RE::TESGlobal>() : nullptr;
				if (global && global->value >= 0) {
					int denom = static_cast<int>(global->value) + 1;
					if (denom >= 1) { s_dropChancePercent = 100.0 / denom; SoulsLog::LineF("Config: DropChancePercent from Papyrus global = %.1f%% (1 in %d)", s_dropChancePercent, denom); }
				}
			}
		}

		void Load()
		{
			if (s_loaded) return;
			s_loaded = true;
			ReadIni();
			ApplyPapyrusGlobals();
			SoulsLog::LineF(
				"Config: SafetyKey=%d, LootDisplaySeconds=%.1f, LootCycleDelay=%.1f, LootCloseKey=0x%X, DropChancePercent=%.1f, GenerateItemIcons=%d",
				s_safetyKeyCode, s_lootDisplaySeconds, s_lootCycleDelaySeconds, s_lootCloseKeyCode, s_dropChancePercent, s_generateItemIconsMode);
			SKSE::log::info(
				"Config: SafetyKey={}, LootDisplaySeconds={}, LootCycleDelay={}, LootCloseKey=0x{:X}, DropChancePercent={}%, GenerateItemIcons={}",
				s_safetyKeyCode, s_lootDisplaySeconds, s_lootCycleDelaySeconds, s_lootCloseKeyCode, s_dropChancePercent, s_generateItemIconsMode);
		}

		int GetSafetyKeyCode()
		{
			return s_safetyKeyCode;
		}

		double GetLootDisplaySeconds()
		{
			return s_lootDisplaySeconds;
		}

		double GetLootCycleDelaySeconds()
		{
			return s_lootCycleDelaySeconds;
		}

		int GetLootCloseKeyCode()
		{
			return s_lootCloseKeyCode;
		}

		double GetDropChancePercent()
		{
			return s_dropChancePercent;
		}

		double GetTierDropChancePercent(int a_tier)
		{
			if (a_tier < 0 || a_tier > 3) return 100.0;
			return s_tierDropChancePct[a_tier];
		}

		double GetTypeDropChancePercent(int a_type)
		{
			if (a_type < 0 || a_type > 4) return 100.0;
			return s_typeDropChancePct[a_type];
		}

		int GetGenerateItemIconsMode()
		{
			return s_generateItemIconsMode;
		}

		const char* GetTexconvPath()
		{
			return s_texconvPath.empty() ? nullptr : s_texconvPath.c_str();
		}

		const char* GetIconOutputDir()
		{
			return s_iconOutputDir.empty() ? nullptr : s_iconOutputDir.c_str();
		}

		const char* GetIconManifestPath()
		{
			return s_iconManifestPath.empty() ? nullptr : s_iconManifestPath.c_str();
		}
	}
}
