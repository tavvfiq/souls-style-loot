#include "pch.h"
#include "SoulsLog.h"
#include <fstream>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <vector>
#include <spdlog/sinks/basic_file_sink.h>

namespace SoulsLog
{
	namespace
	{
		std::filesystem::path s_logPath;
		std::mutex s_mutex;

		void WriteLine(const char* msg)
		{
			if (s_logPath.empty()) return;
			std::lock_guard<std::mutex> lock(s_mutex);
			std::ofstream f(s_logPath, std::ios::app);
			if (f) {
				auto now = std::chrono::system_clock::now();
				auto t = std::chrono::system_clock::to_time_t(now);
				std::tm tm;
#ifdef _WIN32
				localtime_s(&tm, &t);
#else
				localtime_r(&t, &tm);
#endif
				char buf[32];
				std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
				f << buf << " " << msg << "\n";
			}
		}
	}

	void Init()
	{
		if (auto logDir = SKSE::log::log_directory()) {
			s_logPath = *logDir / "SoulsStyleLooting.log";
			// Replace log on every run (truncate); subsequent Line() calls append
			{ std::ofstream clear(s_logPath, std::ios::out | std::ios::trunc); }
			Line("SoulsStyleLooting: log init");
			try {
				auto logger = spdlog::basic_logger_mt("SoulsStyleLooting", s_logPath.string());
				spdlog::set_default_logger(logger);
				spdlog::set_level(spdlog::level::info);
				spdlog::flush_every(std::chrono::seconds(1));
				Line("SoulsStyleLooting: spdlog default logger set");
			} catch (const std::exception& e) {
				LineF("SoulsStyleLooting: spdlog failed: %s", e.what());
			}
		}
	}

	void Line(const char* msg)
	{
		WriteLine(msg);
	}

	void LineF(const char* fmt, ...)
	{
		std::va_list args;
		va_start(args, fmt);
		std::vector<char> buf(512);
		int n = std::vsnprintf(buf.data(), buf.size(), fmt, args);
		va_end(args);
		if (n >= 0 && static_cast<size_t>(n) >= buf.size()) {
			buf.resize(static_cast<size_t>(n) + 1);
			va_start(args, fmt);
			std::vsnprintf(buf.data(), buf.size(), fmt, args);
			va_end(args);
		}
		WriteLine(buf.data());
	}
}
