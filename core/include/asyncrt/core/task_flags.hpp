#pragma once
#include <cstdint>

namespace asyncrt::core {

enum TaskFlags : std::uint8_t { // increase size whenever necessary
    TASK_DETACHED = 1 << 0,
    TASK_RESUME_ON_CAPTURED_CONTEXT = 1 << 1, // @TODO move to trait? unless configurable at runtime
    TASK_CANCELLED = 1 << 2,
    // more options
};

// default flags; will resume on captured context by default
inline constexpr TaskFlags DEFAULT_TASK_FLAGS {
    TASK_RESUME_ON_CAPTURED_CONTEXT
};

}