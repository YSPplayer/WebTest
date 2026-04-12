#include "native_ui/script/host_callback.hpp"

namespace native_ui::script {

Result<CallbackHandle> HostCallbackRegistry::retain(JSContext* context, JSValueConst function) {
    if (!JS_IsFunction(context, function)) {
        return Result<CallbackHandle>{
            {},
            Status{
                StatusCode::invalid_argument,
                "Callback registry expected a JavaScript function."
            }
        };
    }

    const CallbackHandle handle = next_handle_++;
    entries_.push_back(Entry{
        handle,
        JS_DupValue(context, function)
    });

    return Result<CallbackHandle>{handle, Status::Ok()};
}

Status HostCallbackRegistry::release(JSContext* context, const CallbackHandle handle) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->handle != handle) {
            continue;
        }

        JS_FreeValue(context, it->function);
        entries_.erase(it);
        return Status::Ok();
    }

    return Status{
        StatusCode::callback_not_found,
        "Callback handle was not found."
    };
}

void HostCallbackRegistry::clear(JSContext* context) {
    for (auto& entry : entries_) {
        JS_FreeValue(context, entry.function);
    }
    entries_.clear();
}

JSValueConst HostCallbackRegistry::lookup(const CallbackHandle handle) const noexcept {
    for (const auto& entry : entries_) {
        if (entry.handle == handle) {
            return entry.function;
        }
    }

    return JS_UNDEFINED;
}

}  // namespace native_ui::script
