#include "native_ui/script/value_utils.hpp"

#include <algorithm>

namespace native_ui::script {

std::string to_std_string(JSContext* context, JSValueConst value) {
    const char* c_string = JS_ToCString(context, value);
    if (c_string == nullptr) {
        return {};
    }

    std::string result = c_string;
    JS_FreeCString(context, c_string);
    return result;
}

bool to_int32(JSContext* context, JSValueConst value, std::int32_t& out_value) {
    int converted{};
    if (JS_ToInt32(context, &converted, value) < 0) {
        return false;
    }

    out_value = static_cast<std::int32_t>(converted);
    return true;
}

bool to_uint8(JSContext* context, JSValueConst value, std::uint8_t& out_value) {
    std::int32_t converted{};
    if (!to_int32(context, value, converted)) {
        return false;
    }

    converted = std::clamp(converted, 0, 255);
    out_value = static_cast<std::uint8_t>(converted);
    return true;
}

bool to_float32(JSContext* context, JSValueConst value, float& out_value) {
    double converted{};
    if (JS_ToFloat64(context, &converted, value) < 0) {
        return false;
    }

    out_value = static_cast<float>(converted);
    return true;
}

bool to_bool(JSContext* context, JSValueConst value, bool& out_value) {
    const int converted = JS_ToBool(context, value);
    if (converted < 0) {
        return false;
    }

    out_value = converted != 0;
    return true;
}

bool get_bool_property(
    JSContext* context,
    JSValueConst object,
    const char* property_name,
    bool& out_value
) {
    JSValue property = JS_GetPropertyStr(context, object, property_name);
    if (JS_IsException(property) || JS_IsUndefined(property)) {
        JS_FreeValue(context, property);
        return false;
    }

    const bool ok = to_bool(context, property, out_value);
    JS_FreeValue(context, property);
    return ok;
}

bool get_float_property(
    JSContext* context,
    JSValueConst object,
    const char* property_name,
    float& out_value
) {
    JSValue property = JS_GetPropertyStr(context, object, property_name);
    if (JS_IsException(property) || JS_IsUndefined(property)) {
        JS_FreeValue(context, property);
        return false;
    }

    const bool ok = to_float32(context, property, out_value);
    JS_FreeValue(context, property);
    return ok;
}

bool get_string_property(
    JSContext* context,
    JSValueConst object,
    const char* property_name,
    std::string& out_value
) {
    JSValue property = JS_GetPropertyStr(context, object, property_name);
    if (JS_IsException(property) || JS_IsUndefined(property)) {
        JS_FreeValue(context, property);
        return false;
    }

    out_value = to_std_string(context, property);
    JS_FreeValue(context, property);
    return true;
}

}  // namespace native_ui::script
