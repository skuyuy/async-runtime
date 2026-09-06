#pragma once
#include <asyncrt/core/task.hpp>
#include "context.hpp"

namespace asyncrt::qt {

template<class T>
using Task = core::Task<T, Context>;

}