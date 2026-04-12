#include "native_ui/ui_runtime/runtime.hpp"

namespace native_ui::ui_runtime {

void UiRuntime::prune_invalid_bindings(const native_ui::ui_core::Scene& scene) {
    listener_registry_.prune_invalid_listeners(scene);
}

void UiRuntime::process_platform_event(
    const native_ui::ui_core::Scene& scene,
    const native_ui::platform::Event& event
) {
    prune_invalid_bindings(scene);

    const auto pointer_ui_events = input_router_.route(scene, event);
    for (const auto& ui_event : pointer_ui_events) {
        const auto dispatch_result = dispatch_ui_event(scene, ui_event);

        const auto focus_events = focus_manager_.handle_pointer_event(scene, ui_event);
        for (const auto& focus_event : focus_events) {
            (void)dispatch_ui_event(scene, focus_event);
        }

        const auto semantic_events = default_action_engine_.handle_post_dispatch(scene, ui_event, dispatch_result);
        for (const auto& semantic_event : semantic_events) {
            (void)dispatch_ui_event(scene, semantic_event);
        }
    }

    const auto key_ui_events = key_router_.route(scene, event, focus_manager_);
    for (const auto& ui_event : key_ui_events) {
        const auto dispatch_result = dispatch_ui_event(scene, ui_event);
        const auto semantic_events = default_action_engine_.handle_post_dispatch(scene, ui_event, dispatch_result);
        for (const auto& semantic_event : semantic_events) {
            (void)dispatch_ui_event(scene, semantic_event);
        }
    }
}

native_ui::ui_input::DispatchResult UiRuntime::dispatch_ui_event(
    const native_ui::ui_core::Scene& scene,
    const native_ui::ui_input::UiEvent& ui_event
) {
    return dispatcher_.dispatch(
        scene,
        ui_event,
        [&](const native_ui::ui_input::UiDispatchEvent& dispatch_event) {
            return listener_registry_.notify(dispatch_event);
        }
    );
}

}  // namespace native_ui::ui_runtime
