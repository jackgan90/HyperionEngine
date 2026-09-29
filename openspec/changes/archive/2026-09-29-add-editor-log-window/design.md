## Context

Editor initializes logging before parsing options and starting plugins. Core currently forwards Info/Warning/Error to spdlog file and console sinks. Editor owns menu visibility; Gui owns ImGui and persisted docking. Integration tests capture Editor stdout/stderr and depend on diagnostics. Automation adapters use reflected request/result types and scoped catalog registration.

## Goals / Non-Goals

**Goals:** Complete current-run engine/stdout/stderr history, Debug visibility, a bottom-docked draggable panel, GUI-free typed reads, no automatically allocated Editor console, and preserved redirected diagnostics.

**Non-Goals:** Cross-process or OS debug-output collection, arbitrary GPU message queue subscriptions, interactive console commands, log filtering/search/export, or changing other application subsystems. Native static initialization before the entrypoint and abrupt process termination cannot be replayed by an entrypoint-owned collector.

## Decisions

1. Core owns a thread-safe `FLogHistory` with a temporary per-run disk journal and an in-memory offset index. Entries own text and contain sequence, time, level, thread and source. Reads are bounded by count and bytes. Text is paged rather than retained without limit; the index scales with the number of rows. Large messages are split into lossless bounded display fragments and multiline messages into rows. The existing readable log remains the persistent file; the journal is removed at orderly destruction. A storage failure is explicit, not silent eviction.
2. Logging initializes the journal before plugins and enables Debug explicitly for Editor. Existing Info/Warning/Error numeric values and other applications' log initialization remain compatible. GUI and automation receive the same history instance through declared services; no plugin or GUI calls occur on logging threads. Core does not depend on Reflection, Gui or plugins.
3. A scoped Platform adapter redirects CRT descriptors and Windows standard handles to pipes, drains them on owned threads, preserves original handles for external redirection, and joins on shutdown. Raw stdout uses Info, stderr Error with explicit source metadata. Engine logging writes directly to history/file and preserved output, bypassing capture pipes to avoid duplication. Stream fragments and final tails are retained. The adapter is entrypoint-owned process infrastructure and does not add feature orchestration to Runtime/Application.
4. Editor adds a private Log panel, initially hidden, whose closure only changes presentation. Gui adds reusable raw colored virtual-list drawing and optional bottom tabs. Fresh/reset layouts dock Log with Content Browser. Existing layouts receive a one-time placement only when Log has no saved settings; user-moved or floating windows remain where saved. Placement and sizing stay presentation concerns.
5. Only `hyperion_editor` uses the Windows GUI subsystem. Keep the existing `main` argument contract through the MSVC CRT main startup entry. Platform handles last-resort native failure presentation. Redirected and hidden/bounded runs never block on an error dialog; failures preserve nonzero exit status and stderr.
6. A scoped `automation-log` adapter registers reflected `application.log.read` before automation-session sealing, with optional history service availability. Requests specify the last seen sequence and a bounded limit; results include entries, next cursor and total. The adapter delegates to Core and adds no transport branches. Editor publishes the entrypoint-owned history through its declared services; absent Editor/history returns unavailable. Window toggles/docking are documented as presentation-only.

## Risks / Trade-offs

- Pipe lifetime or recursive forwarding can deadlock → separate preserved output from captured writers, join readers before logging shutdown, cover pipe/CRT/native/tail cases with child-process tests.
- Disk-backed history can fail or cost IO → bounded reads, visible storage errors, no per-frame full-history scan, retain ordinary file diagnostics.
- Offset metadata grows with history → eight bytes per row plus container overhead; full text stays on disk. No implicit history truncation.
- GUI subsystem can break scripts → inspect PE subsystem and run real redirected process tests, including absence and startup failures.
- Existing layouts lack Log → supplement only missing settings; test old layout, saved floating placement and reset.
- Native diagnostics before entrypoint or after forced termination are outside collector lifetime → document this boundary; establish capture before service startup and drain at orderly exit.

## Migration Plan

No serialized assets or existing automation IDs change. Existing layout files remain readable. Reverting the change restores the original Editor subsystem and removes the additive operation/panel. Keep this change active after completion; do not archive or commit.

## Open Questions

None blocking. Default panel visibility is hidden as accepted in the first development plan.
