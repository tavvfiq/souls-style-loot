#include "pch.h"
#include "Config.h"
#include "PrismaUI.h"
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
				if (item->Is(RE::FormType::Key)) return 55;
				if (item->Is(RE::FormType::Misc)) return 19;
				return 19;
			}

			std::string get_icon_path(RE::TESBoundObject* item)
			{
				if (!item) return {};
				RE::BSString path;
				RE::TESTexture* tex = item->As<RE::TESTexture>();
				if (tex && tex->textureName.size() > 0) {
					tex->GetAsNormalFile(path);
				} else if (item->IsArmor()) {
					auto* bip = item->As<RE::TESBipedModelForm>();
					if (bip && bip->inventoryIcons[0].textureName.size() > 0) {
						bip->inventoryIcons[0].GetAsNormalFile(path);
					}
				}
				if (path.empty()) return {};
				std::string s(path.c_str());
				for (char& c : s) if (c == '\\') c = '/';
				return s;
			}

			std::string build_loot_json(const std::vector<RE::TESBoundObject*>& items, const std::vector<int>& counts)
			{
				std::string escaped;
				std::ostringstream os;
				os << "{\"items\":[";
				const size_t n = (std::min)(items.size(), counts.size());
				for (size_t i = 0; i < n; ++i) {
					if (i) os << ',';
					RE::TESBoundObject* it = items[i];
					const char* name = it ? it->GetName() : nullptr;
					if (!name || !name[0]) name = "Item";
					escape_json_string(name, escaped);
					os << "{\"name\":\"" << escaped << "\",\"count\":" << counts[i] << ",\"type\":" << get_item_type(it);
					std::string iconPath = get_icon_path(it);
					if (!iconPath.empty()) {
						escape_json_string(iconPath.c_str(), escaped);
						os << ",\"iconPath\":\"" << escaped << '"';
					}
					os << '}';
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

		void ShowLoot(const std::vector<RE::TESBoundObject*>& items, const std::vector<int>& counts)
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
			std::string json = build_loot_json(items, counts);
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
