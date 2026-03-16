#include "pch.h"
#include "IconGenerator.h"
#include "Config.h"
#include "IconUtils.h"
#include "SoulsLog.h"

#include "RE/T/TESDataHandler.h"

#include <fstream>
#include <string>
#include <vector>

namespace SoulsLoot
{
	namespace
	{
		struct ItemIconRecord
		{
			std::string pluginName;
			RE::FormID  formID;
			std::string ddsPath;
			std::string pngPath;  // relative path used by PrismaUI (e.g. SoulsStyleLoot/assets/generated/...)
		};

		template <class T>
		void CollectItems(std::vector<ItemIconRecord>& a_out, const char* a_categoryName)
		{
			auto* dh = RE::TESDataHandler::GetSingleton();
			if (!dh) {
				return;
			}
			const std::size_t startSize = a_out.size();

			const auto& arr = dh->GetFormArray<T>();
			for (auto* form : arr) {
				if (!form || !form->GetPlayable()) {
					continue;
				}

				auto* base = static_cast<RE::TESBoundObject*>(form);
				std::string dds = IconUtils::GetInventoryIconPath(base);
				if (dds.empty()) {
					continue;
				}

				const RE::TESFile* file = form->GetFile(0);
				std::string pluginName = file ? std::string(file->GetFilename()) : std::string();

				ItemIconRecord rec;
				rec.pluginName = pluginName;
				rec.formID = form->GetFormID();
				rec.ddsPath = dds;
				a_out.push_back(std::move(rec));
			}

			SoulsLog::LineF("IconGenerator: collected %zu %s with icons (total %zu)", a_out.size() - startSize, a_categoryName, a_out.size());
		}

		std::string MakePngRelativePath(const std::string& a_ddsPath)
		{
			// Map "textures/foo/bar.dds" -> "SoulsStyleLoot/assets/generated/textures/foo/bar.png"
			std::string rel = "SoulsStyleLoot/assets/generated/";
			rel += a_ddsPath;
			auto dot = rel.find_last_of('.');
			if (dot != std::string::npos) {
				rel.replace(dot, std::string::npos, ".png");
			} else {
				rel += ".png";
			}
			return rel;
		}

		void WriteManifest(const std::vector<ItemIconRecord>& a_items, const std::string& a_manifestPath)
		{
			std::ofstream out(a_manifestPath);
			if (!out) {
				SoulsLog::LineF("IconGenerator: failed to open manifest for write: %s", a_manifestPath.c_str());
				return;
			}

			out << "{\n  \"version\": 1,\n  \"icons\": [\n";
			bool first = true;
			for (const auto& rec : a_items) {
				if (!first) {
					out << ",\n";
				}
				first = false;

				char formBuf[32];
				std::snprintf(formBuf, sizeof(formBuf), "0x%08X", rec.formID);

				out << "    {\"formID\": \"" << formBuf << "\", "
					<< "\"plugin\": \"" << rec.pluginName << "\", "
					<< "\"ddsPath\": \"" << rec.ddsPath << "\", "
					<< "\"pngPath\": \"" << rec.pngPath << "\"}";
			}
			out << "\n  ]\n}\n";
		}
	}

	void GenerateItemIconsIfNeeded()
	{
		int mode = Config::GetGenerateItemIconsMode();
		if (mode != 1) {
			return;
		}

		std::string manifestPathStr;
		{
			const char* cfg = Config::GetIconManifestPath();
			if (cfg && *cfg) {
				manifestPathStr = cfg;
			} else {
				// Default: write manifest next to SKSE logs (e.g. Documents\My Games\Skyrim Special Edition\SKSE\)
				auto logDir = SKSE::log::log_directory();
				if (!logDir) {
					SoulsLog::Line("IconGenerator: no log directory; cannot write default manifest");
					return;
				}
				manifestPathStr = (*logDir / "SoulsStyleLoot_ItemIcons.json").string();
				SoulsLog::LineF("IconGenerator: IconManifestPath not set; using default: %s", manifestPathStr.c_str());
			}
		}

		SoulsLog::Line("IconGenerator: starting item icon scan (generation does not run texconv here)");

		std::vector<ItemIconRecord> items;
		items.reserve(1024);

		CollectItems<RE::TESObjectWEAP>(items, "weapon");
		CollectItems<RE::TESObjectARMO>(items, "armor");
		CollectItems<RE::TESAmmo>(items, "ammo");
		CollectItems<RE::TESObjectBOOK>(items, "book");
		CollectItems<RE::AlchemyItem>(items, "alchemy");
		CollectItems<RE::IngredientItem>(items, "ingredient");
		CollectItems<RE::TESObjectMISC>(items, "misc");

		for (auto& rec : items) {
			rec.pngPath = MakePngRelativePath(rec.ddsPath);
		}

		WriteManifest(items, manifestPathStr);

		SoulsLog::LineF("IconGenerator: wrote manifest with %zu entries", items.size());
	}
}

