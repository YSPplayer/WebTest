# Step 14 - UI Runtime Listeners

## Goal

This step adds the first listener and binding layer without polluting `Node` or `ui_input`.

- Add `engine/ui_runtime`
- Add `ListenerRegistry`
- Add `UiRuntime`
- Keep `ui_input` focused on routing, focus, propagation, and default actions
- Keep listeners external and registered by `NodeId`

## Boundaries

- `ui_core` still owns only node data and semantics
- `ui_input` still owns event generation and dispatch traversal
- `ui_runtime` owns listener registration and event binding orchestration
- callbacks are stored in runtime, not on nodes

## First Version Scope

Implemented:

- register/remove/prune listeners by `NodeId`
- bind listeners to event type and phase
- multiple listeners on the same node and phase
- `stop_propagation`
- `stop_immediate_propagation`
- `prevent_default`
- unified `UiRuntime::process_platform_event(...)`

## Verification

Windows verification covers:

- previous eight demos still pass smoke tests
- new `ui_listener_demo` passes smoke test
- same-node listener order is preserved
- stop propagation keeps later same-node listeners but blocks ancestor phases
- stop immediate propagation blocks later same-node listeners
- prevent default suppresses semantic click generation
