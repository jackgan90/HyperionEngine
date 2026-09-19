## Context

FGui wraps ImGui 1.92.9b privately. Fonts currently use the legacy static atlas path and FGuiRenderer uploads one font texture at startup. Main produces owned draw snapshots; deferred RHI preparation builds retained draw packets. Editor stores docking separately and uses fixed 15-pixel Roboto.

## Goals / Non-Goals

**Goals:** reusable application scaling, clear text, correct input and clipping, persistent Editor preference, unchanged docking topology and safe in-flight font lifetimes.

**Non-Goals:** dependency upgrades, native multi-window DPI policy, scene camera or render-resolution scaling, native OS title-bar scaling, runtime plugin selection changes.

## Decisions

- FGui owns requested and applied scales. A finite value in [1, 2] is applied before the next NewFrame. Runtime instances retain a 1.0 compatibility baseline; application GUI services default to 1.25. Every style change derives from an unscaled snapshot, including ImPlot metrics and explicit logical dimensions.
- Retain original font bytes and base size. Bake at base size times application scale times framebuffer density, compensating density in logical font size. Application scale is independent of framebuffer conversion; do not modify mouse coordinates or DisplaySize. Default GUI fonts use the embedded scalable font.
- Keep legacy atlas integration rather than adopting all ImGui texture protocols. Each draw snapshot carries a shared immutable FImage atlas. RHI preparation replaces its cached texture/binding when snapshot identity changes. Draw packets retain previous bindings through existing resource/fence ownership. No Main/RHI mutable context sharing or per-change GPU idle is introduced.
- GUI services own preference load/save; expose a path and optional startup scale override. Use a small version-independent text preference with validated float value, save on normal quiescence, and surface write failure diagnostically. Invalid saved values fall back to the default; invalid explicit overrides fail clearly. Isolated acceptance runs disable persistence.
- Editor Window > Application Scale contains 100/125/150/175/200 percent presets, a bounded relative numeric drag control with Ctrl-click text input, and reset. Editor and Viewer share FGui::ApplicationScaleControl. An absolute-position slider feeds its changing width back into the next frame's value; stationary input reproduced a permanent 1.69/1.70 two-frame cycle. Relative dragging retains live adjustment without this feedback. Dock split ratios and viewport sizing stay in actual window coordinates. Positive explicit widget dimensions are logical baseline units; fill sentinels stay unchanged. Pixel-space panel placement is scaled explicitly by consumers.

## Risks / Trade-offs

DockSpace consumes the current frame's BuildWorkInset after menu/tool/status bars have reserved their areas. ImGui's default WorkPos/WorkSize uses the preceding frame's reservations and otherwise leaves the dock host one frame behind a scale transition. This synchronization stays inside the existing private ImGui docking adapter.

- Atlas rebuild can briefly cost CPU time while dragging the scale control -> only rebuild for changed scale/density and never each unchanged frame; stationary-pointer regression requires both value and atlas identity to remain stable.
- Small screens have less space at 200 percent -> scroll regions, proportional dock splits and scale-aware minimum dimensions; preserve layout rather than resetting it.
- Existing GPU frames reference old atlases -> immutable snapshots and retained binding sets, covered by deferred rendering regression.
- Preference file may be unwritable -> retain runtime usability and report failure; do not let shutdown throw.

## Migration Plan

Missing preferences use 1.25. Provide --ui-scale and --ui-preferences for reproducible launch/tests. Existing scene/layout formats are unchanged. Reset returns to 1.25. No commit or archive is performed unless separately requested.

## Open Questions

None blocking implementation. Automatic per-monitor UI density policy remains separate from this user preference.
