#pragma once

#include <cstdarg>

namespace SoulsLog
{
	// Call once from plugin load; sets log path and writes first line.
	void Init();
	// Append a line to SoulsStyleLooting.log (no format).
	void Line(const char* msg);
	// Append a formatted line (printf-style).
	void LineF(const char* fmt, ...);
}
