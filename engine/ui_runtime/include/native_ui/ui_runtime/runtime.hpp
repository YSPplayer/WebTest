#pragma once

#include "native_ui/platform/event.hpp"
#include "native_ui/ui_input/default_action_engine.hpp"
#include "native_ui/ui_input/event_dispatcher.hpp"
#include "native_ui/ui_input/focus_manager.hpp"
#include "native_ui/ui_input/input_router.hpp"
#include "native_ui/ui_input/key_router.hpp"
#include "native_ui/ui_runtime/listener_registry.hpp"

namespace native_ui::ui_runtime {

class UiRuntime {
public:
    ListenerRegistry& listeners() noexcept {
        return listener_registry_;
    }

    [[nodiscard]] const ListenerRegistry& listeners() const noexcept {
        return listener_registry_;
    }

    native_ui::ui_input::FocusManager& focus_manager() noexcept {
        return focus_manager_;
    }

    [[nodiscard]] const native_ui::ui_input::FocusManager& focus_manager() const noexcept {
        return focus_manager_;
    }

    void prune_invalid_bindings(const native_ui::ui_core::Scene& scene);
    void process_platform_event(
        const native_ui::ui_core::Scene& scene,
        const native_ui::platform::Event& event
    );

private:
    [[nodiscard]] native_ui::ui_input::DispatchResult dispatch_ui_event(
        const native_ui::ui_core::Scene& scene,
        const native_ui::ui_input::UiEvent& ui_event
    );

    ListenerRegistry listener_registry_{};
    native_ui::ui_input::InputRouter input_router_{};
    native_ui::ui_input::FocusManager focus_manager_{};
    native_ui::ui_input::KeyRouter key_router_{};
    native_ui::ui_input::EventDispatcher dispatcher_{};
    native_ui::ui_input::DefaultActionEngine default_action_engine_{};
};

}  // namespace native_ui::ui_runtime
