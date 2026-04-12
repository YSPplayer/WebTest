#include "native_ui/script/exception.hpp"

#include "native_ui/script/value_utils.hpp"

namespace native_ui::script {

namespace {

std::string append_stack_if_present(JSContext* context, JSValueConst exception, const std::string& message) {
    JSValue stack_value = JS_GetPropertyStr(context, exception, "stack");
    if (JS_IsException(stack_value) || JS_IsUndefined(stack_value) || JS_IsNull(stack_value)) {
        JS_FreeValue(context, stack_value);
        return message;
    }

    const std::string stack = to_std_string(context, stack_value);
    JS_FreeValue(context, stack_value);

    if (stack.empty() || stack == message) {
        return message;
    }

    if (message.empty()) {
        return stack;
    }

    return message + "\n" + stack;
}

}  // namespace

std::string format_exception(JSContext* context) {
    JSValue exception = JS_GetException(context);
    const std::string formatted = format_exception(context, exception);
    JS_FreeValue(context, exception);
    return formatted;
}

std::string format_exception(JSContext* context, JSValueConst exception) {
    const std::string message = to_std_string(context, exception);
    return append_stack_if_present(context, exception, message);
}

}  // namespace native_ui::script
