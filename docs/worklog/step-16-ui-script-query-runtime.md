# Step 16 - UI Script Query Runtime

## Goal

Make the script bridge usable for real UI logic, not just creation and click wiring.

- add node query APIs
- add state getter APIs
- add focus control APIs
- add tag lookup APIs
- enrich script event objects
- validate everything in a dedicated `ui_script_query_demo`

## Boundaries

- `script` still owns only QuickJS hosting
- `bridge` owns all new `@native/ui` query and runtime helpers
- `ui_runtime` only exposes a minimal public dispatch entry for externally generated focus events
- `ui_core` only gains a small `tag` string on nodes

## First Version Scope

Implemented:

- `Node::tag()`
- `UiRuntime::dispatch_ui_event(...)`
- `@native/ui` query helpers:
  - `getNodeById`
  - `getChildren`
  - `getParent`
  - `getChildAt`
  - `getChildCount`
- `@native/ui` getters:
  - `getStyle`
  - `getLayout`
  - `getLayoutRect`
  - `getSemantics`
- `@native/ui` focus helpers:
  - `isFocused`
  - `requestFocus`
  - `clearFocus`
  - `getFocusedNode`
- `@native/ui` tag helpers:
  - `setTag`
  - `getTag`
  - `findByTag`
- richer script event objects:
  - `button`
  - `buttonName`
  - `modifiers`
  - `propagationStopped`
  - `immediatePropagationStopped`
  - `isDefaultPrevented()`
  - `isPropagationStopped()`
  - `isImmediatePropagationStopped()`
- `ui_script_query_demo`

## Verification

Windows verification should cover:

- previous demos still pass
- `ui_script_query_demo` passes smoke-test
- JS can inspect scene structure and layout data
- JS can request and read focus state
- JS can find nodes by tag
- JS listeners can read richer event object fields
