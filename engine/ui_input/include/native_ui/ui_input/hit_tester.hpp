#pragma once

#include "native_ui/platform/types.hpp"
#include "native_ui/ui_core/scene.hpp"
#include "native_ui/ui_input/types.hpp"

namespace native_ui::ui_input {

class HitTester {
public:
    [[nodiscard]] HitTestResult hit_test(
        const native_ui::ui_core::Scene& scene,
        native_ui::platform::PointI window_position
    ) const;
};

}  // namespace native_ui::ui_input
