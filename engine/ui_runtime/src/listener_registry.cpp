#include "native_ui/ui_runtime/listener_registry.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace native_ui::ui_runtime {
namespace {

Status make_status(const StatusCode code, std::string message) {
    return Status{code, std::move(message)};
}

template <typename T>
Result<T> make_error_result(const StatusCode code, std::string message) {
    return Result<T>{T{}, make_status(code, std::move(message))};
}

}  // namespace

Result<ListenerId> ListenerRegistry::add_listener(
    const native_ui::ui_core::NodeId node_id,
    const native_ui::ui_input::UiEventType event_type,
    const native_ui::ui_input::UiDispatchPhase phase,
    ListenerCallback callback
) {
    if (node_id == native_ui::ui_core::kInvalidNodeId) {
        return make_error_result<ListenerId>(StatusCode::invalid_argument, "Listener node id is invalid.");
    }
    if (event_type == native_ui::ui_input::UiEventType::none) {
        return make_error_result<ListenerId>(StatusCode::invalid_argument, "Listener event type is invalid.");
    }
    if (phase == native_ui::ui_input::UiDispatchPhase::none) {
        return make_error_result<ListenerId>(StatusCode::invalid_argument, "Listener phase is invalid.");
    }
    if (!callback) {
        return make_error_result<ListenerId>(StatusCode::invalid_argument, "Listener callback is empty.");
    }

    const ListenerId listener_id = next_listener_id_++;
    listeners_.push_back(ListenerEntry{
        listener_id,
        node_id,
        event_type,
        phase,
        std::move(callback)
    });
    return Result<ListenerId>{listener_id, Status::Ok()};
}

Status ListenerRegistry::remove_listener(const ListenerId listener_id) {
    const auto before = listeners_.size();
    std::erase_if(listeners_, [&](const ListenerEntry& entry) {
        return entry.id == listener_id;
    });

    if (listeners_.size() == before) {
        return make_status(StatusCode::listener_not_found, "Listener was not found.");
    }

    return Status::Ok();
}

void ListenerRegistry::clear_node_listeners(const native_ui::ui_core::NodeId node_id) {
    std::erase_if(listeners_, [&](const ListenerEntry& entry) {
        return entry.node_id == node_id;
    });
}

void ListenerRegistry::prune_invalid_listeners(const native_ui::ui_core::Scene& scene) {
    std::erase_if(listeners_, [&](const ListenerEntry& entry) {
        return scene.find_node(entry.node_id) == nullptr;
    });
}

native_ui::ui_input::DispatchControl ListenerRegistry::notify(
    const native_ui::ui_input::UiDispatchEvent& dispatch_event
) const {
    native_ui::ui_input::DispatchControl aggregate = native_ui::ui_input::DispatchControl::continue_dispatch;

    for (const ListenerEntry& entry : listeners_) {
        if (entry.node_id != dispatch_event.current_node_id ||
            entry.event_type != dispatch_event.type ||
            entry.phase != dispatch_event.phase) {
            continue;
        }

        const auto control = entry.callback(dispatch_event);
        if (native_ui::ui_input::has_dispatch_control(
                control,
                native_ui::ui_input::DispatchControl::prevent_default
            )) {
            aggregate |= native_ui::ui_input::DispatchControl::prevent_default;
        }
        if (native_ui::ui_input::has_dispatch_control(
                control,
                native_ui::ui_input::DispatchControl::stop_propagation
            )) {
            aggregate |= native_ui::ui_input::DispatchControl::stop_propagation;
        }
        if (native_ui::ui_input::has_dispatch_control(
                control,
                native_ui::ui_input::DispatchControl::stop_immediate_propagation
            )) {
            aggregate |= native_ui::ui_input::DispatchControl::stop_immediate_propagation;
            aggregate |= native_ui::ui_input::DispatchControl::stop_propagation;
            break;
        }
    }

    return aggregate;
}

}  // namespace native_ui::ui_runtime
