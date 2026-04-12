# Step 13 - UI Click Default Action

## Goal

This step adds the first default semantic action layer without mixing it into nodes or render.

- Add minimal node semantics with `SemanticRole::button`.
- Extend dispatch with `prevent_default`.
- Add `DefaultActionEngine` inside `ui_input`.
- Generate semantic `click` events from mouse and keyboard defaults.
- Add an isolated `ui_click_demo`.

## Boundaries

- `ui_core` owns semantic role data only.
- `ui_input` owns default action synthesis.
- `render` remains unaware of semantic roles.
- existing mouse and keyboard raw event routing stays intact.

## First Version Scope

Implemented:

- mouse left-button click on the same button target
- `Enter` on a focused button
- `Space` on a focused button
- `prevent_default` suppressing semantic click generation
- click propagation through `capture -> target -> bubble`

Deferred:

- checkbox and toggle semantics
- double click
- disabled state
- shortcuts and default buttons
- form submission behavior

## Verification

Windows verification covers:

- previous seven demos still pass smoke tests
- new `ui_click_demo` passes smoke test
- pointer miss release does not produce click
- keyboard button activation produces click
- prevented default suppresses click emission
