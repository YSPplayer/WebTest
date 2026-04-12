# Step 15 - UI Script Bridge Bootstrap

## Goal

Add the first script layer without polluting `ui_core`, `ui_input`, or `ui_runtime`.

- add `quickjs-ng`
- add `engine/script`
- add `engine/bridge`
- expose a minimal `@native/ui` module
- keep script bindings external to nodes
- validate the chain in a dedicated `ui_script_demo`

## Boundaries

- `script` owns QuickJS runtime, exception formatting, value helpers, and retained JS callbacks
- `bridge` owns the `@native/ui` API surface and JS-to-UI binding
- `ui_runtime` still owns listener registration and event orchestration
- `ui_core` still owns scene and node state
- `render` still only consumes paint data

## First Version Scope

Implemented:

- `quickjs-ng` manifest dependency
- `ScriptRuntime`
- `HostCallbackRegistry`
- `UiScriptBridge`
- `@native/ui` exports:
  - `root`
  - `createBox`
  - `appendChild`
  - `setStyle`
  - `setLayout`
  - `setSemantics`
  - `addEventListener`
  - `removeEventListener`
  - `requestLayout`
  - `requestRender`
- minimal global `console.log` / `console.error`
- `ui_script_demo`

## Verification

Windows verification should cover:

- previous demos still build
- new `ui_script_demo` builds
- JS can create nodes and mutate scene state
- JS `click` listeners are triggered through `ui_runtime`
- script callbacks can update button colors
