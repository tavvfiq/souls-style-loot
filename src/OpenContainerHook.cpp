#include "pch.h"
#include "Config.h"
#include "OpenContainerHook.h"
#include "SoulsLog.h"
#include "RE/T/TESObjectREFR.h"
#include "REL/Relocation.h"
#include "SKSE/Trampoline.h"
#include <Windows.h>

namespace SoulsLoot
{
	namespace
	{
		using OpenContainerFn = void (*)(RE::TESObjectREFR* self, std::int32_t openType);

		OpenContainerFn g_originalOpenContainer = nullptr;

		void OpenContainer_Hook(RE::TESObjectREFR* self, std::int32_t openType)
		{
			if (!self) {
				if (g_originalOpenContainer) g_originalOpenContainer(self, openType);
				return;
			}

			RE::Actor* actor = self->As<RE::Actor>();
			if (actor && actor->IsDead()) {
				int safetyKey = Config::GetSafetyKeyCode();
				if (safetyKey == 0 || (GetAsyncKeyState(safetyKey) & 0x8000) == 0) {
					return;
				}
			}

			if (g_originalOpenContainer) {
				g_originalOpenContainer(self, openType);
			}
		}
	}

	void InstallOpenContainerHook()
	{
		auto& trampoline = SKSE::GetTrampoline();
		if (trampoline.empty()) {
			trampoline.create(128);
		}

		REL::Relocation<std::uintptr_t> openContainerAddr{ RELOCATION_ID(50211, 51140) };
		std::uintptr_t addr = openContainerAddr.address();

		constexpr std::size_t hookSize = 5u;

		std::byte prologue[hookSize];
		std::memcpy(prologue, reinterpret_cast<void*>(addr), hookSize);

		std::uintptr_t trampBuf = reinterpret_cast<std::uintptr_t>(trampoline.allocate(64));
		if (!trampBuf) {
			SoulsLog::Line("OpenContainerHook: trampoline allocate failed");
			return;
		}

		REL::safe_write(trampBuf, prologue, hookSize);

		std::uintptr_t afterPrologue = addr + hookSize;
		std::uintptr_t jmpSrc = trampBuf + hookSize;
		std::int32_t rel = static_cast<std::int32_t>(afterPrologue - (jmpSrc + 5));
		std::byte jmpInsn[5] = { std::byte(0xE9), std::byte(rel & 0xFF), std::byte((rel >> 8) & 0xFF), std::byte((rel >> 16) & 0xFF), std::byte((rel >> 24) & 0xFF) };
		REL::safe_write(jmpSrc, jmpInsn, 5);

		g_originalOpenContainer = reinterpret_cast<OpenContainerFn>(trampBuf);

		openContainerAddr.write_branch<hookSize>(reinterpret_cast<std::uintptr_t>(&OpenContainer_Hook));

		SoulsLog::Line("OpenContainerHook: installed (container menu will not open for corpses unless safety key held)");
		SKSE::log::info("OpenContainer hook installed");
	}
}
