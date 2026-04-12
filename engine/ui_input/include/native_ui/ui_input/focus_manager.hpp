#pragma once

#include <vector>

#include "native_ui/ui_core/scene.hpp"
#include "native_ui/ui_input/types.hpp"

namespace native_ui::ui_input {

class FocusManager {
public:
    [[nodiscard]] native_ui::ui_core::NodeId focused_node_id() const noexcept {
        return focused_node_id_;
    }

    [[nodiscard]] std::vector<UiEvent> request_focus(
        const native_ui::ui_core::Scene& scene,
        native_ui::ui_core::NodeId node_id
    );

    [[nodiscard]] std::vector<UiEvent> clear_focus(const native_ui::ui_core::Scene& scene);
    [[nodiscard]] std::vector<UiEvent> move_focus_next(const native_ui::ui_core::Scene& scene);
    [[nodiscard]] std::vector<UiEvent> move_focus_previous(const native_ui::ui_core::Scene& scene);
    [[nodiscard]] std::vector<UiEvent> repair_focus(const native_ui::ui_core::Scene& scene);
    [[nodiscard]] std::vector<UiEvent> handle_pointer_event(
        const native_ui::ui_core::Scene& scene,
        const UiEvent& ui_event
    );

private:
    [[nodiscard]] std::vector<UiEvent> set_focus(
        const native_ui::ui_core::Scene& scene,
        native_ui::ui_core::NodeId node_id
    );

    native_ui::ui_core::NodeId focused_node_id_{native_ui::ui_core::kInvalidNodeId};
};

}  // namespace native_ui::ui_input
