#pragma once

#include <string>

namespace native_ui::platform {

enum class StatusCode {
    ok = 0,
    invalid_argument,
    init_failed,
    create_window_failed,
    unsupported,
    internal_error
};

struct Status {
    StatusCode code{StatusCode::ok};
    std::string message{};

    [[nodiscard]] bool ok() const noexcept {
        return code == StatusCode::ok;
    }

    static Status Ok() {
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

}  // namespace native_ui::platform

