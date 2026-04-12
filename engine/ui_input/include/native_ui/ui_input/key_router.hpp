#pragma once

#include <vector>

#include "native_ui/platform/event.hpp"
#include "native_ui/ui_input/focus_manager.hpp"

namespace native_ui::ui_input {

class KeyRouter {
public:
    [[nodiscard]] std::vector<UiEvent> route(
        const native_ui::ui_core::Scene& scene,
        const native_ui::platform::Event& event,
        FocusManager& focus_manager
    ) const;
};

}  // namespace native_ui::ui_input
