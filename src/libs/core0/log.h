#pragma once
#ifndef INCLUDE_GUARD_LOG_H
#define INCLUDE_GUARD_LOG_H
#include <functional>

namespace core0 {
	namespace log {
		enum log_levels {
			debug = 0,
			info,
			warn,
			error
		};

		// Provides a generic way to for callback logs.
		using cb_on_log = std::function<void(log_levels, const char*)>;
	}
}
#endif
