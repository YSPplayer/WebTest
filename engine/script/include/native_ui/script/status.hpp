#pragma once

#include <string>

namespace native_ui::script {

enum class StatusCode {
    ok = 0,
    invalid_argument,
    init_failed,
    eval_failed,
    exception_raised,
    module_not_found,
    callback_not_found,
    internal_error
};

struct Status {
    StatusCode code{StatusCode::ok};
    std::string message{};

    [[nodiscard]] bool ok() const noexcept {
        return code == StatusCode::ok;
    }

    [[nodiscard]] static Status Ok() noexcept {
        return {};
    }
};

template <typename T>
struct Result {
    T value{};
    Status status{};

    [[nodiscard]] bool ok() const noexcept {
        return status.ok();
    }
};

}  // namespace native_ui::script
