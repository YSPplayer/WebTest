# Step 12 - UI Focus Keyboard

## Goal

This step adds the first keyboard focus loop without polluting the existing layers.

- Extend platform events with key modifiers.
- Add focus policy to `ui_core::Style`.
- Add `FocusManager` and `KeyRouter` inside `ui_input`.
- Keep `Node` free of callbacks and business handlers.
- Add an isolated `ui_focus_demo`.

## Boundaries

- `platform` owns raw key events and testing injection.
- `ui_core` owns only focus policy data on nodes.
- `ui_input` owns focus state, tab traversal, click-to-focus, and keyboard routing.
- `render` remains unaware of focus logic.

## First Version Scope

Implemented:

- `Tab` forward focus traversal
- `Shift+Tab` reverse focus traversal
- mouse-down focus acquisition for pointer-focusable nodes
- `focus_in` and `focus_out`
- `key_down` and `key_up` routed to the focused node
- keyboard event propagation through `capture -> target -> bubble`

Deferred:

- text input
- IME
- hotkeys
- default actions
- drag and capture
- focus groups

## Verification

Windows verification covers:

- existing six demos still build and pass smoke tests
- new `ui_focus_demo` passes smoke test
- focus sequence stays stable across tab, reverse tab, and click focus
- keyboard events propagate to the focused target node
