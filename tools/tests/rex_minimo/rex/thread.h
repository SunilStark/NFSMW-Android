// Stand-in for the SDK's thread helpers in tests built without the SDK.
#pragma once
#include <string_view>
namespace rex::thread {
inline void set_current_thread_name(std::string_view) {}
}
