#pragma once
#include <coroutine>

// implement forced suspension for an aw
#define FORCE_SUSPEND [[nodiscard]] bool await_ready() const noexcept { return false; }