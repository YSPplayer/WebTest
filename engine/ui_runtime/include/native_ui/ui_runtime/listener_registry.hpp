#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "native_ui/ui_core/scene.hpp"
#include "native_ui/ui_input/event_dispatcher.hpp"
#include "native_ui/ui_runtime/status.hpp"

namespace native_ui::ui_runtime {

using ListenerId = std::uint64_t;
using ListenerCallback = std::function<native_ui::ui_input::DispatchControl(
    const native_ui::ui_input::UiDispatchEvent&
)>;

class ListenerRegistry {
public:
    [[nodiscard]] Result<ListenerId> add_listener(
        native_ui::ui_core::NodeId node_id,
        native_ui::ui_input::UiEventType event_type,
        native_ui::ui_input::UiDispatchPhase phase,
        ListenerCallback callback
    );

    [[nodiscard]] Status remove_listener(ListenerId listener_id);
    void clear_node_listeners(native_ui::ui_core::NodeId node_id);
    void prune_invalid_listeners(const native_ui::ui_core::Scene& scene);

    [[nodiscard]] native_ui::ui_input::DispatchControl notify(
        const native_ui::ui_input::UiDispatchEvent& dispatch_event
    ) const;

    [[nodiscard]] std::size_t listener_count() const noexcept {
        return listeners_.size();
    }

private:
    struct ListenerEntry {
        ListenerId id{};
        native_ui::ui_core::NodeId node_id{native_ui::ui_core::kInvalidNodeId};
        native_ui::ui_input::UiEventType event_type{native_ui::ui_input::UiEventType::none};
        native_ui::ui_input::UiDispatchPhase phase{native_ui::ui_input::UiDispatchPhase::none};
        ListenerCallback callback{};
    };

    ListenerId next_listener_id_{1};
    std::vector<ListenerEntry> listeners_{};
};

}  // namespace native_ui::ui_runtime
