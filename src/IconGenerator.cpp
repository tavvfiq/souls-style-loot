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
				std::string pluginName = file ? file->GetFilename() : "";

				ItemIconRecord rec;
				rec.pluginName = pluginName;
				rec.formID = form->GetFormID();
				rec.ddsPath = dds;
				a_out.push_back(std::move(rec));
			}

			SoulsLog::LineF("IconGenerator: collected %zu %s items with icons", a_out.size(), a_categoryName);
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

		const char* manifestPath = Config::GetIconManifestPath();
		if (!manifestPath || !*manifestPath) {
			SoulsLog::Line("IconGenerator: IconManifestPath not set; skipping generation");
			return;
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

		WriteManifest(items, manifestPath);

		SoulsLog::LineF("IconGenerator: wrote manifest with %zu entries", items.size());
	}
}

