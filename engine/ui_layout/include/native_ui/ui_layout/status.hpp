#pragma once

#include <string>

namespace native_ui::ui_layout {

enum class StatusCode {
    ok = 0,
    invalid_argument,
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

}  // namespace native_ui::ui_layout
