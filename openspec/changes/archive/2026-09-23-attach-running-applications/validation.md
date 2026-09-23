# Validation

Validated on Windows on 2026-09-23 against the uncommitted working tree based on
`d42e13016ecc27e7772a6dd184abd96f0a8ba903`.

## Build and binary scope

Debug builds passed for `hyperion_editor`, `hyperion_viewer`, `hyperion_automation_cli`,
`transport_tests`, `automation_connection_tests`, `automation_scene_tests`,
`automation_asset_tests`, `automation_tests` and affected Editor test targets.
Build evidence: `out/AttachDeliveryBuild.log` and earlier affected-test builds.
The delivered executables are under `out/build/debug/bin`.

At the initial Debug delivery, existing Release MCP processes were not stopped or
replaced. Recompilation does not hot-update a running application or frontend.
After the user stopped the MCP process, Release was rebuilt and validated as
recorded in the Release delivery section below.

## Final automation validation

```powershell
ctest --test-dir out/build/debug --output-on-failure -j 2 -R '^(automation_.*|transport_contracts)$'
```

All seven selected tests passed. Evidence: `out/AttachDeliveryTests.log`.

- Native Windows and memory transport: fragmented duplex bytes, bounded buffers,
  EOF, duplicate close, pending read/write cancellation and unsupported schemes.
- Protocol: framing limits, same-user admission policy, handshake and stale target
  rejection, isolated sessions, no implicit fallback, and admitted asynchronous work
  completing exactly once after client disconnect and server admission shutdown.
- Shared scene: revision/handle validation, atomic rejection, busy interaction gates,
  GUI/agent history equivalence, captured save points, cross-session job rejection,
  stale document identities and no-history host behavior.
- Real Editor/Viewer processes: JSONL, MCP and one-shot `--attach`, target discovery,
  target-owned schema/root query, transforms, Undo/Redo, explicit save and reopen,
  target isolation, reconnect with a fresh session, listener disablement and startup
  failure isolation. Fresh Viewer resource preparation leaves the document clean.

The integration tests create their own application processes and save destinations;
they do not mutate the input scene assets or attach to user-owned application processes.

## GUI regression validation

Passed `editor_state`, `editor_asset_documents`, `editor_asset_workspace`,
`editor_multiselect`, `editor_placement`, `editor_content_transition`,
`scene_viewer_controls` and `plugin_applications`, including fixture setup.
Evidence: `out/AttachRegressions.log` and `out/AttachFinalRegressions.log`.

The first Editor acceptance run exposed missing-scene publication access, followed by
a reference to an unavailable viewport texture. These failure-path issues were repaired;
the complete `editor_acceptance` then passed, including scene recovery, shared history,
component editing, gizmo, selection, saved documents and view behavior.
Final evidence: `out/AttachEditorFinalTest.log`.

## Static checks and documentation

- `python tools/CheckStyle.py`: all owned filenames/include casing and formatting passed.
- Semantic naming/declaration checks passed for 55 changed translation units; the final
  connection-manager adjustment was checked again separately.
- `python tools/CheckBoundaries.py`: dependency boundaries passed.
- `openspec validate attach-running-applications --strict`: passed.
- `git diff --check`: passed.

Usage, integration contracts and deferred adapters are documented in
`docs/AutomationConnections.md`, `docs/Automation.md`, `docs/PluginSystem.md`,
`docs/SourceLayout.md`, `docs/Editor.md` and `docs/Verification.md`.

No macOS/Linux provider, remote device transport, hot reload or new Viewer history
capability is claimed. Live asset-workspace and root-mutation adapters remain deferred.
The initial implementation and audit were delivered uncommitted with the OpenSpec
change active; subsequent Release validation and archival were explicitly authorized.

## Independent quality audit and repairs

The audit covered all 110 initially uncommitted files (tracked and new), with no
exclusions and no staged changes. `out/quality-audit-attach/Before.json` records
the baseline and file hashes; the independent reviewer verified those hashes
were unchanged during review. The primary agent independently checked all five
findings against the source and reproduction evidence before editing.

| Finding | Verified cause and repair |
|---|---|
| R1 / P2 | Viewer native paths used ACP strings at a UTF-8 document boundary. Use PathToUtf8 for save and document metadata; keep the GUI destination native. The Viewer save/reload test now uses Chinese parent and file names and checks the reported document path. |
| R2 / P2 | Pending-material rejection escaped animation into a permanent scene error, preventing subsequent Scene.Tick. Animation now catches authored-edit errors separately, advances animation time only after success, and retries while loading continues. Other incomplete or failed resources do not globally block an editable node. |
| R3 / P2 | Cached GUI busy state survived skipped DrawGui calls. Consume GUI interaction state per scene tick; hidden/non-drawable hosts discard inactive input buffers. Synthetic GUI focus verifies busy admission, release after skipped GUI drawing, and preservation of external values when drawing resumes. |
| R4 / P2 | A close decision could be followed by local request admission in the same plugin update. The listener checks application exit before Poll, withdraws discovery and stops admission while retaining admitted-work draining. A real local connection regression queues a request before exit and verifies rejection before Quiesce. |
| R5 / P2 | Explicit empty --attach selected standalone state. Reject empty and repeated attach options during parsing. CLI tests cover one-shot, JSONL and MCP startup. |

Audit evidence is in `out/quality-audit-attach/ReviewerInitial.md`, `PathProbe.log`
and `PendingMoveProbe.log`. The material probe reproduces the underlying edit
rejection; the checkbox-to-animation error propagation was checked in source,
not reproduced through an interactive checkbox. The initial R3/R4 findings were
source-confirmed and now have targeted GUI/service and native-connection tests.

Debug builds of the affected applications and tests passed (`Build.log`,
`FinalBuild.log` in the audit directory). The final suite passed all 12 selected
tests including fixture setup (`FinalTests.log`): all automation tests, transport,
real Editor/Viewer attachment, Viewer controls and plugin applications. A first
test attempt exposed a duplicate Start in the new test fixture; the fixture now
publishes services through the normal single-start lifecycle, and the rerun passed.

The final animation adjustment was rebuilt and the affected Viewer test plus its
two fixtures passed again (`AnimationBuild.log`, `AnimationRegression.log`).
Full formatting/include checks (822 sources), module boundaries (786 sources / 42
modules), semantic naming of all eight audit-modified translation units (including
a separate final animation check), OpenSpec strict validation and diff whitespace
checks passed. Release MCP processes were left untouched.

The same independent reviewer completed targeted re-review: R1–R5 are resolved,
with no remaining actionable finding in the repaired paths and direct effects.
`After.json` records 114 uncommitted files, including the four directly related
files added to the initial audit scope; all hashes remained unchanged during
re-review. The reviewer independently reran the four invalid-attach cases,
native local lifecycle test and Viewer regression. All passed; the local test
required the normal permission to publish its isolated LOCALAPPDATA record.
Evidence: `out/quality-audit-attach/ReviewerFinal.md` and `ReviewerFinalRun/`.
The animation-checkbox and host Tab/minimize validation limits remain as stated
above. The final documentation update only records these results; no reviewed
source changed after the repaired snapshot was frozen.

## Release delivery and archival

After the user stopped the configured MCP process, the full release preset build
completed successfully with `tools/Build.ps1 -Preset release`. CLI, Editor, Viewer
and all configured test targets were built. The binaries are under
`out/build/release/bin`; evidence: `out/AttachReleaseBuild.log`.

The Release affected suite passed all 21 selected tests, including fixture setup:
all automation tests, transport contracts, Editor acceptance/state/asset documents/
workspace/multiselection/placement/content transition, Viewer controls, plugin
applications and source path checks. The real attachment test covers CLI/JSONL/MCP,
separate live Editor/Viewer targets, transforms, shared Undo/Redo, save/reopen,
reconnect, disablement and listener startup failure isolation.
Evidence: `out/AttachReleaseTests.log`.

A separate invocation of the shipped Release CLI with `--mcp` negotiated protocol
2025-11-25, listed all ten bootstrap tools, queried engine.info and searched the
standalone operation catalog, then exited with code 0. Evidence:
`out/AttachReleaseMcpSmoke.json`. This verifies the executable directly; it does
not claim that the user's disabled Codex connector was re-enabled.

The user authorized synchronization of the change's four capability deltas,
OpenSpec archive and Git commit after Release validation. The source files still
match the independently reviewed snapshot; only delivery records and synchronized
specifications change during this finalization step.
