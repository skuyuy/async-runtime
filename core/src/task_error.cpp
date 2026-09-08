#include <asyncrt/core/task_error.hpp>

namespace asyncrt::core {



struct TaskErrorCategory final : std::error_category {
    [[nodiscard]]
    auto name() const noexcept -> const char* override { return ERROR_CATEGORY_NAME.data(); }

    [[nodiscard]]
    auto message(const int code) const -> std::string override {
        switch (static_cast<TaskError>(code)) {
            case TaskError::invalid_handle:
                return "Invalid task handle";
            case TaskError::exception:
                return "Exception thrown in task";
            case TaskError::detached:
                return "Task is detached";
            case TaskError::cancelled:
                return "Task was cancelled";
            default:
                return "Unknown error";
        }
    }
};
static constexpr TaskErrorCategory error_category;

TaskException::TaskException()
    : TaskException(TaskError::unknown) {
}

TaskException::TaskException(const TaskError code)
    : TaskException(code, error_category.message(std::to_underlying(code))) {
}

TaskException::TaskException(const TaskError code, const std::string& message)
    : std::runtime_error(message)
    , code(code) {
}

auto make_error_code(const TaskError error) -> std::error_code {
    return std::error_code{std::to_underlying(error), error_category};
}

}
