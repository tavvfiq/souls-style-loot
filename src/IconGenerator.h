#pragma once

namespace SoulsLoot
{
	/// Run one-time per-item icon generation pass if enabled by config.
	/// Safe to call on every kDataLoaded; it will no-op when disabled.
	void GenerateItemIconsIfNeeded();
}

