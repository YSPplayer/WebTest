#pragma once

#include <vector>

#include "native_ui/ui_core/scene.hpp"
#include "native_ui/ui_input/hit_tester.hpp"
#include "native_ui/ui_input/types.hpp"

namespace native_ui::ui_input {

class DefaultActionEngine {
public:
    [[nodiscard]] std::vector<UiEvent> handle_post_dispatch(
        const native_ui::ui_core::Scene& scene,
        const UiEvent& ui_event,
        const DispatchResult& dispatch_result
    );

private:
    HitTester hit_tester_{};
    native_ui::ui_core::NodeId pending_pointer_click_node_id_{native_ui::ui_core::kInvalidNodeId};
};

}  // namespace native_ui::ui_input
