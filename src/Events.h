#pragma once

#include "RE/Skyrim.h"
#include <vector>
#include <map>
#include <mutex>

namespace SoulsLoot
{
	struct LootDrop
	{
		std::vector<RE::TESBoundObject*> items;
		std::vector<int> counts;
		int goldAmount = 0;
	};

	class LootManager
	{
	public:
		static LootManager* GetSingleton()
		{
			static LootManager singleton;
			return &singleton;
		}

		void StoreLoot(RE::FormID a_actorID, LootDrop a_loot);
		bool GetLoot(RE::FormID a_actorID, LootDrop& a_outLoot);
		void RemoveLoot(RE::FormID a_actorID);

	private:
		LootManager() = default;
		std::map<RE::FormID, LootDrop> _lootMap;
		std::mutex _lock;
	};

	class DeathEventHandler : public RE::BSTEventSink<RE::TESDeathEvent>
	{
	public:
		static DeathEventHandler* GetSingleton();
		static void Register();

		virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>* a_eventSource) override;
	};

	class ActivateEventHandler : public RE::BSTEventSink<RE::TESActivateEvent>
	{
	public:
		static ActivateEventHandler* GetSingleton();
		static void Register();

		virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* a_event, RE::BSTEventSource<RE::TESActivateEvent>* a_eventSource) override;
	};
}
