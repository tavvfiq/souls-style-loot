#pragma once

namespace SoulsLoot
{
	/// Install hook on TESObjectREFR::OpenContainer so we can prevent the container menu from opening for corpses (no blink).
	void InstallOpenContainerHook();
}
