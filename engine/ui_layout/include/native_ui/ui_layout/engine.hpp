#pragma once

#include "native_ui/ui_core/scene.hpp"
#include "native_ui/ui_layout/status.hpp"

namespace native_ui::ui_layout {

struct LayoutConfig {
    bool use_web_defaults{true};
    float point_scale_factor{1.0f};
};

class LayoutEngine {
public:
    explicit LayoutEngine(LayoutConfig config = {})
        : config_(config) {}

    [[nodiscard]] Status compute(native_ui::ui_core::Scene& scene) const;

private:
    LayoutConfig config_{};
};

}  // namespace native_ui::ui_layout
