#pragma once
#include <format>
#include <system_error>

namespace asyncrt::core {

inline constexpr std::string_view ERROR_CATEGORY_NAME{"asyncrt.core.TaskError"};

enum class TaskError {
    unknown = 1,
    invalid_handle,
    exception,
    detached,
    cancelled
};

struct TaskException : std::runtime_error {
    TaskError code;

    TaskException();
    explicit TaskException(TaskError code);
    TaskException(TaskError code, const std::string& message);

    template<class... Args>
    TaskException(const TaskError code, std::format_string<Args...> fmt, Args&&... args)
        : TaskException(code, std::format(fmt, std::forward<Args>(args)...)) {
    }

    static TaskException unknown(auto&& ...args) {
        return TaskException{TaskError::unknown, std::forward<decltype(args)>(args)...};
    }

    static TaskException invalid_handle(auto&& ...args) {
        return TaskException{TaskError::invalid_handle, std::forward<decltype(args)>(args)...};
    }

    static TaskException exception(auto&& ...args) {
        return TaskException{TaskError::exception, std::forward<decltype(args)>(args)...};
    }

    static TaskException detached(auto&& ...args) {
        return TaskException{TaskError::detached, std::forward<decltype(args)>(args)...};
    }

    static TaskException cancelled(auto&& ...args) {
        return TaskException{TaskError::cancelled, std::forward<decltype(args)>(args)...};
    }
};

auto make_error_code(TaskError) -> std::error_code;

}

template<>
struct std::is_error_code_enum<asyncrt::core::TaskError> : std::true_type {};
