#include "pch.h"
#include "PrismaUI.h"
#include "Config.h"
#include "IconUtils.h"
#include "SoulsLog.h"
#include "RE/B/BSString.h"
#include "RE/B/BGSBipedObjectForm.h"
#include "RE/B/BGSKeywordForm.h"
#include "RE/C/ControlMap.h"
#include "RE/P/PCGamepadType.h"
#include "RE/T/TESBipedModelForm.h"
#include "RE/T/TESObjectARMO.h"
#include "RE/T/TESObjectWEAP.h"
#include "RE/T/TESTexture.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <fstream>
#include <unordered_map>
#include <cctype>

namespace SoulsLoot
{
	namespace PrismaUI
	{
		namespace
		{
			PRISMA_UI_API::IVPrismaUI1* g_api = nullptr;
			::PrismaView g_view = 0;
			constexpr const char* VIEW_PATH = "SoulsStyleLoot/index.html";
			std::atomic<bool> g_domReady{ false };
			std::atomic<bool> g_waitingForClose{ false };
			std::string g_pendingJson;
			std::mutex g_pendingMutex;

			// Map of formID -> PNG path (relative, e.g. SoulsStyleLoot/assets/generated/...)
			std::unordered_map<RE::FormID, std::string> g_iconPngByForm;
			std::once_flag g_manifestOnce;

			// Keyword EditorID -> icon path (loaded from SoulsStyleLoot_TypeIcons.json). First matching keyword on item wins.
			std::unordered_map<std::string, std::string> g_keywordIconPath;
			std::once_flag g_typeIconsOnce;
			static constexpr const char* DEFAULT_TYPE_ICON = "assets/misc/misc1.png";
			// Fallback when no keyword matches (form-type -> path). Used when JSON is missing or item has no mapped keyword.
			static const std::unordered_map<int, std::string> FALLBACK_TYPE_ICONS = {
				{0, "assets/weapons/sword.png"}, {1, "assets/weapons/dagger.png"}, {2, "assets/weapons/waraxe.png"},
				{3, "assets/weapons/mace.png"}, {4, "assets/weapons/greatsword.png"}, {5, "assets/weapons/battleaxe.png"},
				{6, "assets/weapons/bow.png"}, {7, "assets/weapons/staff.png"}, {8, "assets/weapons/crossbow.png"},
				{10, "assets/armors/headgear_light.png"}, {11, "assets/armors/cuirass_light.png"}, {12, "assets/armors/gaunlets_light.png"},
				{13, "assets/armors/boots_light.png"}, {14, "assets/armors/shield_light.png"}, {15, "assets/jewelry/ring.png"},
				{16, "assets/jewelry/amulet.png"}, {17, "assets/weapons/arrows.png"}, {18, "assets/misc/book1.png"},
				{19, "assets/misc/misc1.png"}, {20, "assets/ingestable/potion.png"}, {21, "assets/alchemy/ingredient1.png"},
				{22, "assets/weapons/katana_1h.png"}, {23, "assets/weapons/katana_2h.png"}, {24, "assets/weapons/spear.png"},
				{25, "assets/weapons/halberd.png"}, {26, "assets/weapons/quarterstaff.png"}, {27, "assets/weapons/rapier.png"},
				{28, "assets/weapons/scimitar.png"}, {29, "assets/weapons/warhammer.png"}, {40, "assets/armors/headgear_heavy.png"},
				{41, "assets/armors/cuirass_heavy.png"}, {42, "assets/armors/gaunlets_heavy.png"}, {43, "assets/armors/boots_heavy.png"},
				{44, "assets/armors/shield_heavy.png"}, {45, "assets/jewelry/ring.png"}, {46, "assets/jewelry/amulet.png"},
				{50, "assets/clothing/headwear.png"}, {51, "assets/clothing/clothing.png"}, {52, "assets/clothing/gloves.png"},
				{53, "assets/clothing/footwear.png"}, {54, "assets/clothing/robe.png"}, {55, "assets/misc/key.png"}
			};

			bool poll_gamepad_activate()
			{
				using XInputGetState_t = unsigned long(__stdcall*)(unsigned long, void*);
				static XInputGetState_t fn = nullptr;
				static bool tried = false;
				if (!tried) {
					tried = true;
					HMODULE h = LoadLibraryW(L"xinput1_4.dll");
					if (!h) h = LoadLibraryW(L"xinput9_1_0.dll");
					if (h) fn = (XInputGetState_t)GetProcAddress(h, "XInputGetState");
				}
				if (!fn) return false;
				struct { unsigned long dwPacketNumber; unsigned short wButtons; unsigned char bLeftTrigger, bRightTrigger; short sThumbLX, sThumbLY, sThumbRX, sThumbRY; } state = {};
				if (fn(0, &state) != 0) return false;
				const unsigned short A_BUTTON = 0x1000u;
				return (state.wButtons & A_BUTTON) != 0;
			}

			void on_loot_ready_for_close(const char*)
			{
				g_waitingForClose = true;
				int closeKey = Config::GetLootCloseKeyCode();
				std::thread([closeKey]() {
					while (g_waitingForClose.load() && g_api && g_view) {
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
						bool keyPressed = (closeKey != 0) && ((GetAsyncKeyState(closeKey) & 0x8000) != 0);
						bool gamepadPressed = poll_gamepad_activate();
						if (!keyPressed && !gamepadPressed) continue;
						g_waitingForClose = false;
						auto* task = SKSE::GetTaskInterface();
						if (task) {
							task->AddTask([]() {
								if (g_api && g_view && g_api->IsValid(g_view)) {
									g_api->InteropCall(g_view, "lootClosed", "");
									g_api->Hide(g_view);
								}
							});
						}
						break;
					}
				}).detach();
			}

			void escape_json_string(const char* in, std::string& out)
			{
				out.clear();
				out.reserve(64);
				for (const char* p = in; *p; ++p) {
					switch (*p) {
						case '"': out += "\\\""; break;
						case '\\': out += "\\\\"; break;
						case '\n': out += "\\n"; break;
						case '\r': out += "\\r"; break;
						case '\t': out += "\\t"; break;
						default: out += *p; break;
					}
				}
			}

			const char* get_prompt_type()
			{
				auto* cm = RE::ControlMap::GetSingleton();
				if (!cm) return "keyboard";
				if (!cm->ignoreKeyboardMouse) return "keyboard";
				return (cm->GetGamePadType() == RE::PC_GAMEPAD_TYPE::kOrbis) ? "controllerPs" : "controller";
			}

			void load_icon_manifest()
			{
				const char* manifestPath = Config::GetIconManifestPath();
				if (!manifestPath || !*manifestPath) {
					return;
				}

				std::ifstream in(manifestPath);
				if (!in) {
					SoulsLog::LineF("PrismaUI: failed to open icon manifest at %s", manifestPath);
					SKSE::log::warn("PrismaUI: failed to open icon manifest at {}", manifestPath);
					return;
				}

				std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
				if (data.empty()) {
					return;
				}

				g_iconPngByForm.clear();

				std::size_t pos = 0;
				for (;;) {
					pos = data.find("\"formID\"", pos);
					if (pos == std::string::npos) {
						break;
					}

					std::size_t colon = data.find(':', pos);
					if (colon == std::string::npos) {
						break;
					}

					std::size_t q1 = data.find('"', colon);
					if (q1 == std::string::npos) {
						break;
					}
					std::size_t q2 = data.find('"', q1 + 1);
					if (q2 == std::string::npos) {
						break;
					}
					std::string formStr = data.substr(q1 + 1, q2 - q1 - 1);
					RE::FormID formID = 0;
					if (!formStr.empty()) {
						formID = static_cast<RE::FormID>(std::strtoul(formStr.c_str(), nullptr, 0));
					}

					std::size_t pngPos = data.find("\"pngPath\"", q2);
					if (pngPos == std::string::npos) {
						pos = q2;
						continue;
					}
					std::size_t pngColon = data.find(':', pngPos);
					if (pngColon == std::string::npos) {
						pos = pngPos;
						continue;
					}
					std::size_t pq1 = data.find('"', pngColon);
					if (pq1 == std::string::npos) {
						pos = pngColon;
						continue;
					}
					std::size_t pq2 = data.find('"', pq1 + 1);
					if (pq2 == std::string::npos) {
						pos = pq1;
						continue;
					}
					std::string pngPath = data.substr(pq1 + 1, pq2 - pq1 - 1);

					if (formID != 0 && !pngPath.empty()) {
						g_iconPngByForm[formID] = pngPath;
					}

					pos = pq2;
				}

				SoulsLog::LineF("PrismaUI: loaded icon manifest (%zu entries)", g_iconPngByForm.size());
				SKSE::log::info("PrismaUI: loaded icon manifest ({} entries)", g_iconPngByForm.size());
			}

			const std::string& get_manifest_png_for_item(RE::TESBoundObject* item)
			{
				static const std::string empty;
				if (!item) {
					return empty;
				}

				std::call_once(g_manifestOnce, load_icon_manifest);

				RE::TESForm* form = item;
				if (!form) {
					return empty;
				}

				auto it = g_iconPngByForm.find(form->GetFormID());
				if (it == g_iconPngByForm.end()) {
					return empty;
				}
				return it->second;
			}

			// Display type: 0-9 weapon, 22-29 OCF keyword weapons, 10-16 light armor, 17 ammo, 18 book, 19 misc, 20 potion, 21 ingredient,
			// 40-46 heavy armor, 50-54 clothing
			int get_item_type(RE::TESBoundObject* item)
			{
				if (!item) return 19;
				// Weapon: check OCF weapon-type keywords first, then WEAPON_TYPE 0-9
				if (item->IsWeapon()) {
					auto* kf = item->As<RE::BGSKeywordForm>();
					auto hasKw = [kf](const char* id) { return kf && kf->HasKeywordString(id); };
					if (hasKw("ocf_weaptypekatana1h"))   return 22;
					if (hasKw("ocf_weaptypekatana2h"))   return 23;
					if (hasKw("ocf_weaptypespear2h") || hasKw("ocf_weaptypepike2h")) return 24;
					if (hasKw("ocf_weaptypehalberd2h"))  return 25;
					if (hasKw("ocf_weaptypequarterstaff2h")) return 26;
					if (hasKw("ocf_weaptyperapier1h"))   return 27;
					if (hasKw("ocf_weaptypescimitar1h")) return 28;
					if (hasKw("ocf_weaptypewarhammer2h")) return 29;

					auto* weap = item->As<RE::TESObjectWEAP>();
					if (weap) {
						using WT = RE::WeaponTypes::WEAPON_TYPE;
						switch (weap->GetWeaponType()) {
							case WT::kOneHandSword:   return 0;
							case WT::kOneHandDagger:  return 1;
							case WT::kOneHandAxe:     return 2;
							case WT::kOneHandMace:    return 3;
							case WT::kTwoHandSword:   return 4;
							case WT::kTwoHandAxe:     return 5;
							case WT::kBow:            return 6;
							case WT::kStaff:         return 7;
							case WT::kCrossbow:      return 8;
							default:                  return 0;
						}
					}
					return 0;
				}
				// Armor: differentiate light (10-16), heavy (40-46), clothing (50-53)
				if (item->IsArmor()) {
					auto* armo = item->As<RE::TESObjectARMO>();
					if (armo) {
						auto* bip = armo->As<RE::BGSBipedObjectForm>();
						if (bip) {
							using Slot = RE::BIPED_MODEL::BipedObjectSlot;
							int base = 11; // body default
							if (bip->HasPartOf(Slot::kShield))  base = 14;
							else if (bip->HasPartOf(Slot::kHead))   base = 10;
							else if (bip->HasPartOf(Slot::kBody))   base = 11;
							else if (bip->HasPartOf(Slot::kHands))  base = 12;
							else if (bip->HasPartOf(Slot::kFeet))   base = 13;
							else if (bip->HasPartOf(Slot::kRing))   base = 15;
							else if (bip->HasPartOf(Slot::kAmulet)) base = 16;

							if (bip->IsClothing()) {
								// 50 headwear, 51 clothing (body), 52 gloves, 53 footwear, 54 robe (body)
								if (base == 10) return 50;
								if (base == 11) {
									const char* name = armo->GetName();
									if (name && name[0]) {
										std::string lower;
										for (const char* p = name; *p; ++p) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
										if (lower.find("robe") != std::string::npos) return 54;
									}
									return 51;
								}
								if (base == 12) return 52;
								if (base == 13) return 53;
								// shield/ring/amulet as clothing: use same as light
							} else if (bip->IsHeavyArmor()) {
								return base + 30; // 40-46
							}
							// Light armor (or jewelry): 10-16
							return base;
						}
					}
					return 11;
				}
				if (item->IsAmmo()) return 17;
				if (item->Is(RE::FormType::Book)) return 18;
				if (item->Is(RE::FormType::AlchemyItem)) return 20;
				if (item->Is(RE::FormType::Ingredient)) return 21;
				if (item->IsKey()) return 55;
				if (item->Is(RE::FormType::Misc)) return 19;
				return 19;
			}

			// Rarity string from tier (0=common, 1=uncommon, 2=rare, 3=legendary). Tier comes from loot roll in Events.
			static const char* tier_to_rarity(int tier)
			{
				switch (tier) {
					case 1: return "uncommon";
					case 2: return "rare";
					case 3: return "legendary";
					default: return "common";
				}
			}

			void load_type_icons_json()
			{
				auto pluginDir = Config::GetPluginDirectory();
				if (!pluginDir || pluginDir->empty()) return;
				auto path = *pluginDir / "SoulsStyleLoot_TypeIcons.json";
				std::ifstream f(path);
				if (f) {
					std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
					f.close();
					// Parse "key": "value" pairs. Key and value are quoted strings (key = keyword EditorID, value = icon path).
					g_keywordIconPath.clear();
					const char* p = content.c_str();
					while (*p) {
						while (*p && *p != '"') ++p;
						if (!*p) break;
						++p; // skip opening "
						const char* keyStart = p;
						while (*p && *p != '"') {
							if (*p == '\\') ++p; // skip escaped char
							if (*p) ++p;
						}
						if (!*p) break;
						std::string key(keyStart, static_cast<size_t>(p - keyStart));
						++p; // skip closing "
						while (*p && *p != ':') ++p;
						if (!*p) break;
						++p;
						while (*p == ' ' || *p == '\t') ++p;
						if (*p != '"') continue;
						++p; // skip opening " of value
						const char* valStart = p;
						while (*p && *p != '"') {
							if (*p == '\\') ++p;
							if (*p) ++p;
						}
						if (!*p) break;
						std::string value(valStart, static_cast<size_t>(p - valStart));
						++p;
						if (!key.empty() && !value.empty())
							g_keywordIconPath[std::move(key)] = std::move(value);
					}
					SKSE::log::info("PrismaUI: loaded {} keyword icon paths from SoulsStyleLoot_TypeIcons.json", g_keywordIconPath.size());
					return;
				}
				SKSE::log::info("PrismaUI: TypeIcons JSON not found at {}; using fallback by form type", path.string());
			}

			// Fallback icon path when no keyword in g_keywordIconPath matches (by get_item_type).
			static const char* get_fallback_icon_path_for_type(int type)
			{
				auto it = FALLBACK_TYPE_ICONS.find(type);
				if (it != FALLBACK_TYPE_ICONS.end() && !it->second.empty())
					return it->second.c_str();
				return DEFAULT_TYPE_ICON;
			}

			// Resolve icon path: first matching keyword from item's keywords, else fallback by form type.
			static std::string get_icon_path_for_item(RE::TESBoundObject* item)
			{
				std::call_once(g_typeIconsOnce, load_type_icons_json);
				auto* kf = item ? item->As<RE::BGSKeywordForm>() : nullptr;
				if (kf) {
					for (RE::BGSKeyword* kw : kf->GetKeywords()) {
						if (!kw) continue;
						const char* editorID = kw->GetFormEditorID();
						if (!editorID || !*editorID) continue;
						auto it = g_keywordIconPath.find(editorID);
						if (it != g_keywordIconPath.end() && !it->second.empty())
							return it->second;
					}
				}
				return get_fallback_icon_path_for_type(get_item_type(item));
			}

			std::string build_loot_json(const std::vector<RE::TESBoundObject*>& items, const std::vector<int>& counts, const std::vector<int>& tiers)
			{
				std::string escaped;
				std::ostringstream os;
				os << "{\"items\":[";
				const size_t n = (std::min)(items.size(), counts.size());
				bool first = true;
				for (size_t i = 0; i < n; ++i) {
					RE::TESBoundObject* it = items[i];
					if (!it || counts[i] <= 0) continue;  // skip null or zero-count to avoid ghost "Item" row
					if (!first) os << ',';
					first = false;
					const char* name = it->GetName();
					if (!name || !name[0]) name = "Item";
					escape_json_string(name, escaped);
					int tier = (i < tiers.size()) ? tiers[i] : 0;
					const std::string& pngFromManifest = get_manifest_png_for_item(it);
					std::string iconPath = pngFromManifest.empty() ? IconUtils::GetInventoryIconPath(it) : pngFromManifest;
					if (iconPath.empty())
						iconPath = get_icon_path_for_item(it);
					escape_json_string(iconPath.c_str(), escaped);
					os << "{\"name\":\"" << escaped << "\",\"count\":" << counts[i] << ",\"rarity\":\"" << tier_to_rarity(tier) << "\",\"iconPath\":\"" << escaped << "\"}";
				}
				int displayMs = static_cast<int>(Config::GetLootDisplaySeconds() * 1000.0);
				if (displayMs < 2000) displayMs = 2000;
				if (displayMs > 15000) displayMs = 15000;
				int cycleDelayMs = static_cast<int>(Config::GetLootCycleDelaySeconds() * 1000.0);
				if (cycleDelayMs < 500) cycleDelayMs = 500;
				if (cycleDelayMs > 10000) cycleDelayMs = 10000;
				const char* promptType = get_prompt_type();
				escape_json_string(promptType, escaped);
				os << "],\"displayMs\":" << displayMs << ",\"cycleDelayMs\":" << cycleDelayMs << ",\"requireActivateToClose\":true,\"promptType\":\"" << escaped << "\"}";
				return os.str();
			}

			void send_loot_to_view(const std::string& json)
			{
				if (!g_api || !g_view || !g_api->IsValid(g_view)) return;
				SoulsLog::LineF("PrismaUI: Show + InteropCall showLoot (%u chars)", static_cast<unsigned>(json.size()));
				SKSE::log::info("PrismaUI: Show + InteropCall showLoot ({} chars)", json.size());
				g_api->Show(g_view);
				// Do not call Focus() - keep as overlay so pause menu and other UI remain accessible
				g_api->InteropCall(g_view, "showLoot", json.c_str());
			}
		}

		void Init()
		{
			void* raw = PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1);
			g_api = static_cast<PRISMA_UI_API::IVPrismaUI1*>(raw);
			if (!g_api) {
				SoulsLog::Line("PrismaUI: API not available - loot UI disabled");
				SKSE::log::warn("PrismaUI not available; loot notifications will not be shown.");
				return;
			}
			SoulsLog::Line("PrismaUI: API obtained, creating view SoulsStyleLoot/index.html");
			SKSE::log::info("PrismaUI API obtained, creating view: {}", VIEW_PATH);
			g_view = g_api->CreateView(VIEW_PATH, [](::PrismaView view) {
				g_view = view;
				g_domReady = true;
				SKSE::log::info("Souls Style Loot PrismaUI view DOM ready");
				std::lock_guard<std::mutex> lock(g_pendingMutex);
				if (!g_pendingJson.empty()) {
					send_loot_to_view(g_pendingJson);
					g_pendingJson.clear();
				}
			});
			if (!g_view) {
				SoulsLog::Line("PrismaUI: CreateView failed (returned 0)");
				SKSE::log::warn("PrismaUI CreateView returned 0 for {}", VIEW_PATH);
				g_api = nullptr;
				return;
			}
			if (!g_api->IsValid(g_view)) {
				SoulsLog::Line("PrismaUI: view invalid after create");
				SKSE::log::warn("PrismaUI view invalid after create for {}", VIEW_PATH);
			} else {
				g_api->RegisterJSListener(g_view, "lootReadyForClose", on_loot_ready_for_close);
				SoulsLog::LineF("PrismaUI: view created OK (ID %u). Ensure Data/PrismaUI/views/SoulsStyleLoot/index.html exists.", static_cast<unsigned>(g_view));
			}
			SKSE::log::info("PrismaUI view created (ID {}). Ensure Data/PrismaUI/views/SoulsStyleLoot/index.html exists.", g_view);
		}

		void ShowLoot(const std::vector<RE::TESBoundObject*>& items, const std::vector<int>& counts, const std::vector<int>& tiers)
		{
			if (items.empty()) return;
			if (!g_api || !g_view) {
				SoulsLog::LineF("ShowLoot: PrismaUI not available (api=%d view=%u)", g_api != nullptr ? 1 : 0, static_cast<unsigned>(g_view));
				SKSE::log::warn("ShowLoot: PrismaUI not available (api={} view={})", g_api != nullptr, g_view != 0);
				return;
			}
			if (!g_api->IsValid(g_view)) {
				SoulsLog::Line("ShowLoot: view invalid");
				SKSE::log::warn("ShowLoot: view invalid");
				return;
			}
			std::string json = build_loot_json(items, counts, tiers);
			if (g_domReady) {
				send_loot_to_view(json);
			} else {
				SoulsLog::LineF("PrismaUI: DOM not ready, queueing %u items", static_cast<unsigned>(items.size()));
				SKSE::log::info("PrismaUI: DOM not ready, queueing loot ({} items)", items.size());
				std::lock_guard<std::mutex> lock(g_pendingMutex);
				g_pendingJson = json;
			}
		}

		bool IsAvailable()
		{
			return g_api != nullptr && g_view != 0 && g_api->IsValid(g_view);
		}
	}
}
