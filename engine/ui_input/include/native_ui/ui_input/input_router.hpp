#pragma once

#include <vector>

#include "native_ui/platform/event.hpp"
#include "native_ui/ui_input/hit_tester.hpp"

namespace native_ui::ui_input {

class InputRouter {
public:
    [[nodiscard]] std::vector<UiEvent> route(
        const native_ui::ui_core::Scene& scene,
        const native_ui::platform::Event& event
    );

    [[nodiscard]] native_ui::ui_core::NodeId hovered_node_id() const noexcept {
        return hovered_node_id_;
    }

    [[nodiscard]] native_ui::ui_core::NodeId pressed_node_id() const noexcept {
        return pressed_node_id_;
    }

private:
    HitTester hit_tester_{};
    native_ui::ui_core::NodeId hovered_node_id_{native_ui::ui_core::kInvalidNodeId};
    native_ui::ui_core::NodeId pressed_node_id_{native_ui::ui_core::kInvalidNodeId};
};

}  // namespace native_ui::ui_input
