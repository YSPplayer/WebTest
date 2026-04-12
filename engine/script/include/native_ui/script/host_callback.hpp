#pragma once

#include <cstdint>
#include <vector>

#include <quickjs.h>

#include "native_ui/script/status.hpp"

namespace native_ui::script {

using CallbackHandle = std::uint64_t;

class HostCallbackRegistry {
public:
    [[nodiscard]] Result<CallbackHandle> retain(JSContext* context, JSValueConst function);
    [[nodiscard]] Status release(JSContext* context, CallbackHandle handle);
    void clear(JSContext* context);

    [[nodiscard]] JSValueConst lookup(CallbackHandle handle) const noexcept;

private:
    struct Entry {
        CallbackHandle handle{};
        JSValue function{JS_UNDEFINED};
    };

    CallbackHandle next_handle_{1};
    std::vector<Entry> entries_{};
};

}  // namespace native_ui::script
