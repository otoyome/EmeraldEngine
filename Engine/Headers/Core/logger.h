#pragma once

#include <windows.h>
#include <string>
#include <string_view>
#include <mutex>
#include <list>

#include "System/singleton.h"

namespace core
{
	class logger
	{
		SINGLETON(logger)

	public:
		enum class categorys
		{
			common,
			system,
			shader,
			model,
			material,
			animation,

			max,
		};

		enum class levels
		{
			normal,
			warning,
			error,

			max,
		};

		struct data
		{
			categorys category;
			levels level;
			std::string text;
			data(categorys category, levels level, const char* text)
			: category(category), level(level), text(text)
			{
			}
		};

	public:
		UINT get_log_max() const { return log_max; }
		void set_log_max(UINT max) { log_max = max; }
		const std::list<data>& get_logs() const { return logs; }

		void print(categorys category, const char* format, ...);
		void warning(categorys category, const char* format, ...);
		void error(categorys category, const char* format, ...);

		void clear();

		std::vector<const char*> get_category_texts();
		std::vector<const char*> get_levels_texts();

	private:
		logger();
		~logger();
		void register_log(categorys category, levels level, const char* text);

	private:
		std::mutex critical_section;
		std::list<data> logs;
		UINT log_max;
	};
}	//	namespace core

#if defined(USE_IMGUI)
#define	DEBUG_LOG(...) if (core::logger::is_valid()){ core::logger::instance()->print(core::logger::categorys::common, __VA_ARGS__); }
#define	DEBUG_WARNING(...) if (core::logger::is_valid()){ core::logger::instance()->warning(core::logger::categorys::common, __VA_ARGS__); }
#define	DEBUG_ERROR(...) if (core::logger::is_valid()){ core::logger::instance()->error(core::logger::categorys::common, __VA_ARGS__); }

#define	DEBUG_LOG_CATEGORY(CATEGORY, ...) if (core::logger::is_valid()){ core::logger::instance()->print(CATEGORY, __VA_ARGS__); }
#define	DEBUG_WARNING_CATEGORY(CATEGORY, ...) if (core::logger::is_valid()){ core::logger::instance()->warning(CATEGORY, __VA_ARGS__); }
#define	DEBUG_ERROR_CATEGORY(CATEGORY, ...) if (core::logger::is_valid()){ core::logger::instance()->error(CATEGORY, __VA_ARGS__); }

#define	DEBUG_CHECK_WARNING(FLAG, ...) if (core::logger::is_valid()){ if (FLAG) {core::logger::instance()->warning(core::logger::categorys::common, __VA_ARGS__);} }
#define	DEBUG_CHECK_ERROR(FLAG, ...) if (core::logger::is_valid()){ if (FLAG) {core::logger::instance()->error(core::logger::categorys::common, __VA_ARGS__);} }
#define	DEBUG_CHECK_WARNING_CATEGORY(FLAG, CATEGORY, ...) if (core::logger::is_valid()){ if (FLAG) {core::logger::instance()->warning(CATEGORY, __VA_ARGS__);} }
#define	DEBUG_CHECK_ERROR_CATEGORY(FLAG, CATEGORY, ...) if (core::logger::is_valid()){ if (FLAG) {core::logger::instance()->error(CATEGORY, __VA_ARGS__);} }
#else // defined(USE_IMGUI)
#define	DEBUG_LOG(...) {}
#define	DEBUG_WARNING(...) {}
#define	DEBUG_ERROR(...) {}

#define	DEBUG_LOG_CATEGORY(CATEGORY, ...) {}
#define	DEBUG_WARNING_CATEGORY(CATEGORY, ...) {}
#define	DEBUG_ERROR_CATEGORY(CATEGORY, ...) {}

#define	DEBUG_CHECK_WARNING(FLAG, ...) {}
#define	DEBUG_CHECK_ERROR(FLAG, ...) {}
#define	DEBUG_CHECK_WARNING_CATEGORY(FLAG, CATEGORY, ...) {}
#define	DEBUG_CHECK_ERROR_CATEGORY(FLAG, CATEGORY, ...) {}
#endif // defined(USE_IMGUI)
