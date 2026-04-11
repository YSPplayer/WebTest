#pragma once

#include <string_view>

#include "native_ui/platform/status.hpp"
#include "native_ui/platform/types.hpp"

namespace native_ui::platform {

class Window {
public:
    virtual ~Window() = default;

    [[nodiscard]] virtual WindowId id() const noexcept = 0;
    [[nodiscard]] virtual SizeI size() const noexcept = 0;
    [[nodiscard]] virtual NativeWindowHandle native_handle() const noexcept = 0;

    virtual Status set_title(std::string_view title) = 0;
    virtual Status set_size(SizeI size) = 0;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void request_close() = 0;
};

}  // namespace native_ui::platform

