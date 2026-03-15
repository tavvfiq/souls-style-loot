#include "pch.h"
#include "Config.h"
#include "PrismaUI.h"
#include "SoulsLog.h"
#include <atomic>
#include <mutex>
#include <sstream>
#include <algorithm>

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
			std::string g_pendingJson;
			std::mutex g_pendingMutex;

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

			std::string build_loot_json(const std::vector<RE::TESBoundObject*>& items, const std::vector<int>& counts)
			{
				std::string escaped;
				std::ostringstream os;
				os << "{\"items\":[";
				const size_t n = (std::min)(items.size(), counts.size());
				for (size_t i = 0; i < n; ++i) {
					if (i) os << ',';
					const char* name = items[i] ? items[i]->GetName() : nullptr;
					if (!name || !name[0]) name = "Item";
					escape_json_string(name, escaped);
					os << "{\"name\":\"" << escaped << "\",\"count\":" << counts[i] << '}';
				}
				int displayMs = static_cast<int>(Config::GetLootDisplaySeconds() * 1000.0);
				if (displayMs < 2000) displayMs = 2000;
				if (displayMs > 15000) displayMs = 15000;
				os << "],\"displayMs\":" << displayMs << '}';
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
