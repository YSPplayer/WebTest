#pragma once

#include <cstdint>
#include <string>

#include <quickjs.h>

namespace native_ui::script {

[[nodiscard]] std::string to_std_string(JSContext* context, JSValueConst value);
[[nodiscard]] bool to_int32(JSContext* context, JSValueConst value, std::int32_t& out_value);
[[nodiscard]] bool to_uint8(JSContext* context, JSValueConst value, std::uint8_t& out_value);
[[nodiscard]] bool to_float32(JSContext* context, JSValueConst value, float& out_value);
[[nodiscard]] bool to_bool(JSContext* context, JSValueConst value, bool& out_value);
[[nodiscard]] bool get_bool_property(
    JSContext* context,
    JSValueConst object,
    const char* property_name,
    bool& out_value
);
[[nodiscard]] bool get_float_property(
    JSContext* context,
    JSValueConst object,
    const char* property_name,
    float& out_value
);
[[nodiscard]] bool get_string_property(
    JSContext* context,
    JSValueConst object,
    const char* property_name,
    std::string& out_value
);

}  // namespace native_ui::script
