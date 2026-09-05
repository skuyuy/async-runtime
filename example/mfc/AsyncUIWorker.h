#pragma once
#include <future>
#include <asyncrt/windows/dispatcher.hpp>


// @TODO MFC Module

#define DECLARE_ASYNC_HANDLER(Name, ...) \
    afx_msg void Name(); \
    asyncrt::windows::Task<void> Name##_Async();

#define IMPLEMENT_ASYNC_HANDLER(ClassName, Name, ...) \
void ClassName::Name() { Name##_Async(); } \
asyncrt::windows::Task<void> ClassName::Name##_Async()
