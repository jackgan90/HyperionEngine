## Context

Editor persists capture startup selection and consumes optional FFrameCapture. RenderDoc currently retains its default overlay mask. GUI and automation already share IRenderCaptureControl on Main.

## Goals / Non-Goals

Goals: default-hidden Editor HUD; live toggling and persistence; shared domain control; explicit effective-state reporting; unchanged capture/replay.

Non-goals: per-window overlays, individual overlay labels, runtime plugin loading, changing capture keyboard bindings.

## Decisions

- Add an optional startup overlay setting and synchronized query/set methods to FFrameCapture; only the private adapter calls GetOverlayBits/MaskOverlayBits. Toggle only Enabled, preserving other mask bits. Query returns null when unavailable. No separate lifecycle or plugin dependencies.
- Editor passes its default-false preference before graphics initialization. Other callers that omit the optional startup setting retain existing RenderDoc behavior.
- Persist renderdoc_hud as an optional version-1 field; old files default false. Validate duplicates/boolean values. Save a candidate before publishing it to memory and applying it, retaining prior state on save failure.
- Add reflected HUD info and renderdoc.hud.get/set operations through IRenderCaptureControl; keep existing IDs and result schemas unchanged. Preference and effective enabled state are separate optional booleans. Absent capture runtime permits saving a preference but reports null effective state. No host means unavailable. Changes do not enter scene undo history.
- Keep Show RenderDoc HUD visible even when capture is disabled, allowing advance configuration and control of hooks retained from startup.

## Risks / Trade-offs

- Process-global overlay mask affects all RenderDoc windows: document scope.
- Old binaries reject new preference keys: remove renderdoc_hud when rolling back to an older binary.
- External RenderDoc controls can alter effective state: read the actual API mask instead of caching visibility.
- Optional provider cannot apply a saved setting: expose null effective state and preserve startup-only selection.

## Validation

Test old/default/roundtrip/malformed preferences, actual API mask preservation and unavailable/shutdown behavior, GUI toggle/restart and hidden-HUD capture/replay, automation discovery/schema/invocation/save failure, and build/style/module boundaries including a provider-disabled build.
