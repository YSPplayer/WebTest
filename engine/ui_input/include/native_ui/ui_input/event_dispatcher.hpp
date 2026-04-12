#pragma once

#include <functional>

#include "native_ui/ui_core/scene.hpp"
#include "native_ui/ui_input/types.hpp"

namespace native_ui::ui_input {

using DispatchHandler = std::function<DispatchControl(const UiDispatchEvent&)>;

class EventDispatcher {
public:
    void dispatch(
        const native_ui::ui_core::Scene& scene,
        const UiEvent& ui_event,
        const DispatchHandler& handler
    ) const;
};

}  // namespace native_ui::ui_input
