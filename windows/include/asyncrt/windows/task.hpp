#pragma once
#include <asyncrt/core/task.hpp>
#include <asyncrt/windows/dispatcher.hpp>

namespace asyncrt::windows {

template<class T>
using Task = core::Task<T, Dispatcher>;

}