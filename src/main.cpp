#include "pch.h"
#include "Config.h"
#include "Events.h"
#include "OpenContainerHook.h"
#include "PrismaUI.h"
#include "SoulsLog.h"

void OnMessages(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		SoulsLog::Line("SoulsStyleLooting: kDataLoaded - loading config, hook, events and PrismaUI");
		SoulsLoot::Config::Load();
		SoulsLoot::InstallOpenContainerHook();
		SoulsLoot::DeathEventHandler::Register();
		SoulsLoot::ActivateEventHandler::Register();
		SoulsLoot::PrismaUI::Init();
		break;
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SoulsLog::Init();
	SoulsLog::Line("SoulsStyleLooting: plugin loaded");
	SKSE::log::info("Souls Style Looting loaded");

	auto messaging = SKSE::GetMessagingInterface();
	if (messaging) {
		messaging->RegisterListener(OnMessages);
	}

	return true;
}
