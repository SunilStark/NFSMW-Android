// Stand-in for the SDK's logging in tests built without the SDK: the lines go to stdout through fmt.
#pragma once
#include <fmt/format.h>
#define REXLOG_INFO(...) fmt::print("I {}\n", fmt::format(__VA_ARGS__))
#define REXLOG_WARN(...) fmt::print("W {}\n", fmt::format(__VA_ARGS__))
#define REXLOG_ERROR(...) fmt::print("E {}\n", fmt::format(__VA_ARGS__))
