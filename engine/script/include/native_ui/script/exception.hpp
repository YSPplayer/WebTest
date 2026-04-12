#pragma once

#include <string>

#include <quickjs.h>

namespace native_ui::script {

[[nodiscard]] std::string format_exception(JSContext* context);
[[nodiscard]] std::string format_exception(JSContext* context, JSValueConst exception);

}  // namespace native_ui::script
