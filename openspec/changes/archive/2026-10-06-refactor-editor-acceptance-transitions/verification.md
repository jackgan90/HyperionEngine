# Verification

Workspace: `F:/HyperionEngine`, base HEAD `04fcf6a`, 2026-10-06. This is an active, unarchived change; no commit or push is authorized.

## Scope and equivalence review

- Execution progress in all Editor acceptance scenarios and nested preview/raster/viewport exercises uses scenario-specific enums and explicit `TransitionTo` destinations. The old `ExerciseStep`, scenario-specific `*Step` fields and ordinal execution arithmetic/ranges are absent from the implementation. Actual case/asset indices and frame/sample/wait counters remain counters.
- Click and asset-tab-close helpers return completion; their callers own the transition. Pointer events, press/release order and existing wait resets remain at the original frame boundaries. `TransitionTo` only changes the typed state.
- Asset stages and input-window policy share one authoritative state declaration. Historical state numbers and their description wrapper are removed. State-name generation returns `string_view`; `interaction_verified` and exit control share the existing enabled-scenario, terminal-state and ready-frame predicate.
- Reviewed shared-state splits, helper early returns, loops, same-frame preparation, readiness gates, capture points and completion against the preserved original sources in ignored `out/AcceptanceTransitions/Baseline`. Assertion-call inventories match for every acceptance translation unit, and all original assertion/fixture literals remain; only obsolete numeric-state diagnostics changed.
- Public services, Automation operations, plugin/CMake selection and test registrations are unchanged. The private production report serializer replaces only `exercise_step` with boolean `interaction_verified`; the existing integration script replaces only its one completion assertion. `.clang-tidy` adds this one `T`-prefixed class template to the repository's existing exact template-name exception.
- Scenario/context switches remain cohesive test-only workflow descriptions. They do not introduce a generic next-state registry or infer transitions from enum order. No feature or test case was added.

## Initial typed-state iteration checks and builds (before report retirement)

| Command | Result |
| --- | --- |
| `python tools/CheckStyle.py` | PASS: paths, advisory size scan, formatting |
| `python out/AcceptanceTransitions/Naming.py` | PASS: existing naming/declaration checker on all 35 acceptance translation units using the Debug compilation database |
| `python tools/CheckBoundaries.py` | PASS: configured Debug graph and source isolation |
| `git -c safe.directory=F:/HyperionEngine diff --check` | PASS |
| `openspec validate refactor-editor-acceptance-transitions --strict` | PASS |
| `./tools/Build.ps1 -Preset debug -Target hyperion_editor` | PASS |
| `./tools/Build.ps1 -Preset release -Target hyperion_editor` | PASS |
| `cmake --build out/build/acceptance-production-debug --target hyperion_editor --parallel 8` | PASS: Debug, BUILD_TESTING=OFF |
| `cmake --build out/build/acceptance-production --target hyperion_editor --parallel 8` | PASS: Release, BUILD_TESTING=OFF |
| `python Source/Tests/Integration/EditorAcceptanceBoundary.py out/build/acceptance-production-debug out/build/acceptance-production-debug/Source/Plugins/Editor/EditorSources-Debug.txt` | PASS: source selection and compile database exclude acceptance sources |
| `python Source/Tests/Integration/EditorAcceptanceBoundary.py out/build/acceptance-production out/build/acceptance-production/Source/Plugins/Editor/EditorSources-Release.txt` | PASS: source selection and compile database exclude acceptance sources |

The two existing test-disabled configurations were built after loading the Visual Studio x64 developer environment. Both executables returned exit code 1 for the existing `--exercise --hidden` request with `Editor acceptance is unavailable: configure BUILD_TESTING=ON`, before plugin startup.

Raw build, check and regression logs are ignored under `out/AcceptanceTransitions`. GPU/desktop acceptance uses the existing D3D12 validation path and isolated test output/discovery directories.

## Initial typed-state iteration regression results (before report retirement)

| Existing command | Result |
| --- | --- |
| `ctest --test-dir out/build/debug --output-on-failure -R '^editor_'` | PASS: 33/33, 536.51 seconds |
| `ctest --test-dir out/build/release --output-on-failure -R '^editor_'` (first final run) | 32/33, 169.60 seconds; `editor_placement` failed because the system clipboard was busy/inaccessible |
| Same Release command, unchanged sources and tests | PASS: 33/33, 160.01 seconds; placement passed in 4.19 seconds |

The successful Debug combination includes the existing interaction report assertion (`exercise_step == 21`), document/transform, views, gizmo, picking and framing; both configurations' first final runs also passed capture, asset/content/import, multiselection, shortcuts, clipboard, reparent, model placement, outline and render regressions. Raw final logs are `DebugEditorTestsFinal.log`, `ReleaseEditorTestsFinal.log` and `ReleaseEditorTestsRerun.log` under `out/AcceptanceTransitions`.

Initial runs exposed and fixed four migration mistakes without altering assertions: the actual capture click targeted the preference subscenario, range-selection input returned before its named continuation, placement cancellation consulted the finished drag subscenario's viewport-entry state, and framing initialization lost its outer continuation. The initial Debug group passed 29/33; the targeted run passed capture, selection-shortcut and placement (3/4) while its old framing binary's combined `editor_acceptance` hit the existing 180-second CTest budget. The final binaries include all four corrections; final Debug combined acceptance passed in 167.85 seconds and Release combined acceptance passed in 25.54 seconds. The initial/focused logs remain in `DebugEditorTests.log` and `DebugRetest.log`.

The first final Release group's placement failure reported `Ctrl+V after placement did not create a separate selected object: System clipboard is busy or inaccessible; try again`, at 29 GPU frames with zero validation errors. No clipboard retry, timing, input or assertion change was made for that external failure; the full unchanged Release group then passed 33/33, including placement. This is a separately recorded rerun, not a claim that the first Release group passed.

These initial nine tasks were completed before the user-authorized report retirement below. The active OpenSpec change and source/configuration edits remain unarchived, unstaged and uncommitted; no push was performed. This verification covers the existing Editor subset and its test-disabled boundary, not every unrelated engine CTest target.

## Semantic report follow-up

The user authorized removing the historical numeric report after the initial iteration. Repository search identified one consumer, the existing `EditorAcceptance.py` completion assertion. `exercise_step` is retired rather than assigned a replacement numeric value. Existing flags and other report fields retain their contracts; there is no new engine feature or test case.

- Comparison with ignored `out/AcceptanceTransitions/BeforeReportRetirement` confirms all 710 ordinary state rows and 104 asset rows differ only by removing their historical-number argument: state names/order and asset stage/window metadata are identical.
- The original `IsComplete()` interaction conjunction was extracted unchanged into private `IsInteractionComplete()`; exit control and report generation call the same predicate. Event generation, transitions, waits, other completion branches and scenario assertions are unchanged.
- The existing Python script differs in exactly one line: its numeric completion assertion now reads `interaction_verified`. Remaining assertions and test registrations are unchanged. Test-disabled reports use the existing empty report value and therefore emit false.
- At the report-retirement milestone, current implementation source and tools contained no `LegacyReportStep`, `FAcceptanceStateDescription`, old state-description calls or `exercise_step`. Documentation references describe their retirement or the earlier verification baseline; they are not current implementation contracts.

| Follow-up command | Result |
| --- | --- |
| `python tools/CheckStyle.py` | PASS: 1239 owned source paths, advisory size scan of 1200 files with 23 reviewed exclusions and no advisory findings, formatting |
| `python out/AcceptanceTransitions/NamingReport.py` | PASS: existing naming/declaration checker on 35 acceptance translation units plus the private report serializer (36 total) |
| `python tools/CheckBoundaries.py` | PASS: 1200 sources, 41 production and 112 test targets |
| `git -c safe.directory=F:/HyperionEngine diff --check` | PASS |
| `openspec validate refactor-editor-acceptance-transitions --strict` | PASS |
| `./tools/Build.ps1 -Preset debug -Target hyperion_editor` | PASS: test-enabled Debug |
| `./tools/Build.ps1 -Preset release -Target hyperion_editor` | PASS: test-enabled Release |
| `cmake --build out/build/acceptance-production-debug --target hyperion_editor --parallel 8` | PASS: Debug, BUILD_TESTING=OFF, after loading the Visual Studio x64 developer environment |
| `cmake --build out/build/acceptance-production --target hyperion_editor --parallel 8` | PASS: Release, BUILD_TESTING=OFF, using the same developer environment |
| `python Source/Tests/Integration/EditorAcceptanceBoundary.py out/build/acceptance-production-debug out/build/acceptance-production-debug/Source/Plugins/Editor/EditorSources-Debug.txt` | PASS: source selection and compile database exclude acceptance sources |
| `python Source/Tests/Integration/EditorAcceptanceBoundary.py out/build/acceptance-production out/build/acceptance-production/Source/Plugins/Editor/EditorSources-Release.txt` | PASS: source selection and compile database exclude acceptance sources |

Both test-disabled executables were also run with `--hidden --frames 2 --report <output> --layout <output> --ui-preferences <output> --editor-preferences <output>` under the existing `python Source/Tests/Automation/DiscoveryEnvironment.py python -` wrapper, using isolated output paths under `out/AcceptanceTransitions/ReportBoundary`. Each subprocess returned 0; parsing each JSON report confirmed `interaction_verified` is boolean false and `exercise_step` is absent. Debug/Release JSON and runtime logs are retained in that directory. This checks the existing finite-frame workflow without adding a test case or registration.

| Existing follow-up regression command | Result |
| --- | --- |
| `ctest --test-dir out/build/debug --output-on-failure -R '^editor_'` | PASS: 33/33, 538.47 seconds; combined interaction acceptance 161.63 seconds |
| `ctest --test-dir out/build/release --output-on-failure -R '^editor_'` | PASS: 33/33, 183.64 seconds; combined interaction acceptance 24.55 seconds |

Both groups ran serially against the final report-retirement binaries and passed without a rerun. Their existing interaction assertion checks semantic completion; all other assertions, input waits and timeouts remain unchanged. Reading each group's successful `sponza.json` also confirms `interaction_verified` is boolean true and `exercise_step` is absent: Debug output `out/editor-tests/acceptance-uso9d51o`, Release output `out/editor-tests/acceptance-8z3e9zws`.

Follow-up logs use the `Report` prefix under ignored `out/AcceptanceTransitions`, including `ReportDebugBuild.log`, `ReportReleaseBuild.log`, `ReportProductionBuild.log`, `ReportStyle.log`, `ReportNaming.log`, `ReportBoundaries.log`, `ReportDebugEditorTests.log` and `ReportReleaseEditorTests.log`. The initial results and failures above remain separate evidence.

At the end of the semantic report follow-up, all 12 tasks then defined were complete. Its final residual and diff checks passed; HEAD remained `04fcf6a` and the index was empty. The change remained unarchived and all edits uncommitted; no push was performed. Validation covered the existing Editor subset and its test-disabled boundary, not unrelated engine CTest targets.

## Scenario-owned input and timing follow-up

The user subsequently authorized removing generic shared `Wait` storage and numeric input-phase/cadence logic. This remains an equivalent maintainability change with no new feature, test case, registration, CLI flag or report field. The previous verification results above belong to their earlier binaries.

The fresh ignored baseline `out/AcceptanceTransitions/BeforeTimingOwnership` preserves 42 acceptance files, SHA-256 hashes and 242 input/timing references. A comparison after migration verifies every baseline hash, identical assertion-call inventories and preservation of all original string literals in all 35 acceptance translation units. This iteration changes 24 translation units and adds two private headers; scenario state declarations and asset routing metadata are byte-equivalent after newline normalization. The first regression binaries' acceptance source hashes (44 files) were frozen in `TimingFinalSourceHashes.json`; the scanner-reset correction below is a separate iteration, whose final hashes and repeated static comparison are in `TimingFinalSourceHashes2.json` and `TimingEquivalence2.log`.

- `TAcceptanceState` now owns only typed progress. The generic `FAcceptanceInput` and its shared `Wait`/mouse-down storage are removed. Concrete scenario contexts own click input, observations and settling budgets. Movement, Wheel, Transform, LightPriority, Depth, Raster and ViewportChoice no longer borrow their coordinator's input/timing data; movement and wheel also own their camera/sample baselines.
- `FAcceptanceClick` models delay, press and release explicitly. It preserves pending-release identity when scanning restarts a delay, and invalid control bounds still postpone events while the eligible input updates advance. Each caller retains its explicit scenario transition after completed release.
- Static review additionally preserved scanner interruption semantics: the active movement/wheel/isolation/hidden-viewport observation restarts through an explicit state switch. Completed wheel/isolation samples supply named click-delay policies to their coordinator; scanning invalidates that readiness and restores settling, retaining the original inherited immediate-click behavior without a shared elapsed counter.
- Text input, Enter confirmation, source text entry, window name entry, log observation and keyboard cadence use named phases. Independent release/settle/execute cycles replace modulo arithmetic. One-time initialization and before-toggle capture use named flags/actions, with the capture flag cleared at the beginning of each input collection.
- Frame waits keep numeric frame units without exposing elapsed values to scenario dispatch. `Advance()` completes on the budget's last eligible update; `ConsumeFrame()` keeps all budget updates idle before resuming. Construction, `Start()` and `Restart()` consume no update. Readiness guards retain their original relative position. Diagnostic observations remain independent of phase and timeout control.

Reviewed update boundaries against the fresh baseline:

| Operation | Preserved eligible-update behavior |
| --- | --- |
| Ordinary click | Idle updates 1–2, press on 3, release/completion on 4 |
| Inherited immediate interaction clicks and scene double-click | Explicit immediate delay policy retains the original press/release updates |
| Movement and wheel | Separate 6/8/6-frame movement observations and 6-frame wheel sample; unchanged expected displacement formula |
| Asset text input | Ctrl+A press on 1, release/text on 2, commit observation completes on 4; Enter press on 4 and release/completion on 5 where previously present |
| Import source/draft name and floating-window name | Source text on 1 then four observations; draft selection/text on 1/2 and completion on 6; window selection on 1, text on 3 and completion on 6 |
| Keyboard cadence | Release keys on first eligible update, settle on second, execute on third; each scenario owns its cycle |
| Tab close and model placement | 30 idle hover updates then tab press/release; 24 idle next-case updates before the existing model readiness checks |
| Panel drag | Baseline before the sixth held-frame move; eight further observations before verification/release; original drag deltas and release delay |
| Log, outline and capture | Log emission on observation 4 and verification on 13; outline preparation consumes its own update then three capture observations; toggle screenshot remains a one-update request before press |
| Asset diagnostics | Preview snapshot at observation 120; close-request snapshot retains its pre-increment boundary, close observation retains its post-increment boundary; no new timeout |

| Follow-up command | Result |
| --- | --- |
| `python tools/CheckStyle.py` | PASS: 1241 owned source paths; advisory scan of 1202 C++ files, 23 reviewed exclusions, no advisory findings; formatting |
| `python out/AcceptanceTransitions/NamingReport.py` | PASS: existing semantic naming/declaration checker on 36 translation units, including the new private headers |
| `python tools/CheckBoundaries.py` | PASS: 1202 sources, 41 production and 112 test targets |
| `git -c safe.directory=F:/HyperionEngine diff --check` | PASS |
| `openspec validate refactor-editor-acceptance-transitions --strict` | PASS |
| `./tools/Build.ps1 -Preset debug -Target hyperion_editor` | PASS after diagnostic-interface correction and again after the scanner-reset correction |
| `./tools/Build.ps1 -Preset release -Target hyperion_editor` | PASS after diagnostic-interface correction and again after the scanner-reset correction |
| `./out/AcceptanceTransitions/BuildProduction.ps1` | PASS: existing BUILD_TESTING=OFF Debug and Release configurations, Visual Studio x64 developer environment, `cmake --build ... --target hyperion_editor --parallel 8` |
| `python Source/Tests/Integration/EditorAcceptanceBoundary.py out/build/acceptance-production-debug out/build/acceptance-production-debug/Source/Plugins/Editor/EditorSources-Debug.txt` | PASS |
| `python Source/Tests/Integration/EditorAcceptanceBoundary.py out/build/acceptance-production out/build/acceptance-production/Source/Plugins/Editor/EditorSources-Release.txt` | PASS |

The first test-enabled Debug/Release builds both failed because `ScenarioStatus()` still passed concrete contexts to a diagnostic helper expecting the typed progress interface. Updating those calls to pass `.Progress` fixed compilation; all other compiled units in those attempts had passed. The failed logs remain `TimingDebugBuild.log` and `TimingReleaseBuild.log`; successful rebuild logs are `TimingDebugBuild2.log` and `TimingReleaseBuild2.log`. This correction affects diagnostic access only.

The first Debug regression group passed 32/33 in 611.51 seconds; combined `editor_acceptance` reached its existing 180-second CTest limit (180.06 seconds). Its Sponza interaction, close-during-load, document, views, gizmo and picking outputs had completed; framing had not returned when the group timeout killed the combined test. Normalized source comparison confirms framing differs only in typed-state qualification and its equivalent release/settle/execute cadence. No assertion failure or framing-specific cause was established, and no timeout, readiness condition or input interval was changed to address this result. There was no leftover timed-out Editor subprocess. This failed run remains in `TimingDebugEditorTests.log`, with fixture output `out/editor-tests/acceptance-skmtaylk`.

Independent static review preserved the scanner reset path before the final rerun: restarting the original generic wait while blocked must restart the owning active observation, and invalidate inherited immediate-click readiness. The explicit scanner switch and named delay policies implement those original resets. Unused elapsed/start queries were also removed from the new primitive. This is a source-equivalence correction, not an established explanation of the group timeout. Final successful compilation logs are `TimingDebugBuild3.log` and `TimingReleaseBuild3.log`; repeated final style, naming, boundary, diff and strict validation all pass in the corresponding `Timing...2.log` files.

Both test-disabled executables also completed the existing `--hidden --frames 2` report workflow with exit code 0 under `python Source/Tests/Automation/DiscoveryEnvironment.py python -`, using isolated report/layout/preference paths. Their parsed JSON reports retain boolean false `interaction_verified` and no `exercise_step`; logs and reports are in `out/AcceptanceTransitions/TimingBoundary`, with summary `TimingBoundaryReports.log`. No acceptance implementation is selected in either production configuration.

| Final existing regression command | Result |
| --- | --- |
| `ctest --test-dir out/build/debug --output-on-failure -R '^editor_'` | PASS: 33/33, 446.91 seconds; combined interaction acceptance 130.11 seconds |
| `ctest --test-dir out/build/release --output-on-failure -R '^editor_'` | PASS: 33/33, 151.23 seconds; combined interaction acceptance 21.62 seconds |

The final groups ran serially after both final builds and naming checks completed. Debug uses `TimingDebugEditorTests2.log`; Release passed on its first run, recorded in `TimingReleaseEditorTests.log`. Assertions, test registrations, eligibility conditions and timeouts are unchanged. No new test case was added. The failed earlier Debug group above remains separate evidence; its timeout cause was not established.

Final enabled reports in `out/editor-tests/acceptance-c7phy7k1` (Debug) and `out/editor-tests/acceptance-2_wp5qhr` (Release) contain boolean true `interaction_verified` with no `exercise_step`. Their framing reports verify success and zero graphics validation errors. Summaries are `TimingDebugEnabledReports.log` and `TimingReleaseEnabledReports.log`. All 44 acceptance source hashes match `TimingFinalSourceHashes2.json` after both groups, confirming these results cover the fixed final source.

At the end of the source/input-timing milestone, all 18 then-defined tasks were complete. Final strict OpenSpec validation, source residual and diff checks passed; HEAD remained `04fcf6a` and the index was empty. The change remained active and unarchived, with all edits unstaged and uncommitted; no push was performed. That validation covers the existing Editor subset and its production boundary, not unrelated engine CTest targets. The subsequent documentation extension is recorded separately below.

## Permanent maintenance documentation follow-up

Reviewed current `docs` and main OpenSpec contracts for execution-step, generic-wait and report descriptions. The Editor overview and `editor-application-consolidation` spec incorrectly promised compatibility for all report keys/types; both now explicitly describe boolean `interaction_verified` and retirement of `exercise_step`. Source-layout descriptions now identify typed state and owned input/observation contexts. Baseline wording in this change is historical, and earlier verification evidence and archived records retain their original context.

Added `docs/EditorAcceptance.md` with source entry points, local macro expansion, typed progress/options, single-purpose fields, scenario/operation input ownership, real numeric units, explicit reset/advance boundaries, click/text/cadence behavior, scanner recovery, capture pulses, completion/report ownership and scenario/build/test extension points. The guide is linked from CodingStyle, Editor, SourceLayout, Verification and the documentation index. CodingStyle adds the semantic-field rule; Verification and OpenSpec configuration no longer hard-code a count of maintainability rules. The active delta and current spec contain identical ownership and maintenance-guide requirements. This is contract synchronization, not archival.

| Documentation validation | Result |
| --- | --- |
| One-off Python standard-library check via `python -` | PASS: 12 Markdown files, 105 local links and 10 Markdown anchors; context example and named source entry points verified. Details: `out/AcceptanceTransitions/DocumentationChecks.json` |
| Compare acceptance SHA-256 values with `TimingFinalSourceHashes2.json` | PASS: all 44 acceptance source hashes unchanged |
| Compare relevant current-spec and active-delta requirements | PASS: both requirement blocks and their scenarios match |
| `openspec validate refactor-editor-acceptance-transitions --type change --strict --no-interactive` | PASS |
| `openspec validate editor-application-consolidation --type spec --strict --no-interactive` | PASS |
| `git diff --check` and manual documentation/source review | PASS |

The documentation follow-up changed documentation and OpenSpec metadata only. No implementation, test case or test registration was changed in that milestone, and no build or CTest suite was rerun; the successful source-validation results above remained evidence for the unchanged source. All 21 then-defined tasks, including the three documentation tasks, were complete. The active change remained unarchived, unstaged and uncommitted; HEAD remained `04fcf6a` and the index was empty. No push was performed. The subsequent user-requested independent audit and repair are recorded separately below.

## Independent quality audit and repair

The user requested `quality-audit` over all development in this session. Independent reviewers `review_transitions` and `review_contexts` were created with `fork_turns: none` and self-contained briefs. The baseline is `04fcf6ad2323c066fff1fc582172356a8c6b56f6`; the frozen working scope contains 59 modified/new files, including all source, style configuration, report consumer, documentation and active OpenSpec artifacts. Both reviewers verified 59/59 SHA-256 values against `out/AcceptanceTransitions/Audit/InitialSnapshot.json`. Their sandbox could not read `.git`; exact tracked baseline bytes were exported from that commit into `HeadBaseline`, cross-checked with the tracked diff. Source/doc files remained unchanged throughout initial review.

| Finding | Main-agent verification and disposition |
| --- | --- |
| CTX-01 / P2: Preview `Frames` still mixed control settling, drag sampling and history settling | Confirmed by reading the context and all three current/HEAD call sites. This was a retained migration omission, not a confirmed behavior regression. Replaced it with three independently owned named budgets and explicit resets, retaining 4-update control settling, 48 move updates then release, and at least 3 history updates before the ready check. |
| CTX-C01 / optional P3: Unreachable preview transition branches | Confirmed that the outer dispatch excludes all three listed non-drag states. Removed those branches and directly named `VerifyDragAndUndo`, retaining the actual destination and event order. |
| TR-O1 / optional P3: `PressSurface` names an idle held-input update | Confirmed that the press occurs in `PrepareSurfaceClick`; this state emits no event. Renamed it `HoldSurface`, retaining the same empty update and next destination. |

Initial reviews did not confirm an execution-state, helper, report or production-boundary behavior regression. Static transition review covered all modified Acceptance C++ and related consumers; input/context review also checked documentation/configuration and the production report path. The context reviewer compiled and ran a temporary out-only probe against the actual headers/report implementation: frame budgets 0–32, 59,049 ten-update valid/invalid-bounds/scanner click traces, immediate click, capture pulse, cadence and report serialization passed. Both existing test-disabled source/compile-database boundary checks passed. These probes are audit evidence, not added/registered repository test cases, and do not replace existing runtime acceptance.

Repair validation logs are under `out/AcceptanceTransitions/Audit`:

| Command/check | Result and original evidence |
| --- | --- |
| `python tools/CheckStyle.py` | PASS: paths, advisory sizes and formatting; `RepairStyle.log` |
| `python out/AcceptanceTransitions/Audit/RepairNaming.py` | PASS: semantic naming/local declarations in the two repaired translation units and their headers; `RepairNaming.log` |
| `python tools/CheckBoundaries.py` | PASS: 1202 sources, 41 production and 112 test targets; `RepairBoundaries.log` |
| `git -c safe.directory=F:/HyperionEngine diff --check` | PASS; `RepairDiff.log` |
| `./tools/Build.ps1 -Preset debug -Target hyperion_editor` | PASS: current Editor compiled/linked; `RepairDebugBuild.log` |
| `./tools/Build.ps1 -Preset release -Target hyperion_editor` | PASS: current Editor compiled/linked; `RepairReleaseBuild.log` |
| `ctest --test-dir out/build/debug --output-on-failure -R '^editor_(asset_editors|acceptance)$'` | PASS: 3/3, 166.04 seconds; `RepairDebugTests.log` |
| `ctest --test-dir out/build/release --output-on-failure -R '^editor_(asset_editors|acceptance)$'` | PASS: 3/3, 28.62 seconds; `RepairReleaseTests.log` |
| Documentation/source consistency and local links/anchors | PASS: 12 Markdown files, 105 links, 10 anchors; `RepairDocumentationChecks.json` |
| Strict validation of active change and current spec | PASS: `openspec validate refactor-editor-acceptance-transitions --type change --strict --no-interactive` and `openspec validate editor-application-consolidation --type spec --strict --no-interactive`; `RepairStrictChange.log`, `RepairStrictSpec.log` |

The first default-sandbox Debug build attempt could not replace the existing `out/build/debug/TargetGraph.json` (WinError 5); it did not reach compilation. That failed attempt is preserved in `RepairDebugBuildSandbox.log`. Both successful builds subsequently used the authorized build environment. Debug and Release acceptance groups ran serially; their existing fixture automatically included `editor_asset_documents`. The other two tests were `editor_asset_editors` (25.90/6.25 seconds) and combined `editor_acceptance` (139.77/22.10 seconds). No timeout, assertion, feature, test case or registration was added or altered by the repair.

Both original independent reviewers completed targeted re-review against `RepairSnapshot.json`, with 59/59 hashes matching throughout review and both runtime groups. They closed CTX-01, CTX-C01 and TR-O1 and found no new confirmed or pending code issue. Reports are `ContextRereview.md` and `TransitionRereview.md`; initial reports remain `ContextReview.md` and `TransitionReview.md`. The context reviewer additionally compiled and ran `ContextPreviewTimingProbe.cpp`, using the actual preview budget declarations and current input primitive: control update 4, move updates 1–48/release 49, and 4096 twelve-update readiness traces with successful-history resets/query guards passed. The transition reviewer rechecked state/reference integrity, unchanged held-input events and the preview parent call chain. A mistaken initial report description of the old Python comparison was corrected to `== 21`; it did not change the code or conclusions.

All 44 acceptance source hashes still match `RepairSourceHashes.json` after both runtime groups. Parsed reports from Debug `out/editor-tests/acceptance-n3bs_tb0` and Release `acceptance-ti0f5vza` confirm boolean `interaction_verified` true for Sponza interaction and false for the unrelated picking/framing runs, no obsolete `exercise_step`, successful picking/framing and zero graphics validation errors. Summary: `RepairRuntimeReports.json`.

This audit covers the complete session diff, while post-repair runtime validation covers the directly affected existing subset. The earlier 33/33 Debug and Release groups remain separate pre-audit evidence; they were not repeated in this repair milestone. No new BUILD_TESTING=OFF binary was built because the repair touches only test-private code and guide/records; its source/compile-database boundary and unchanged production report/driver were independently checked. Reviewers did not independently run the GUI/GPU suites or visually inspect captures; they checked original results and source equivalence. The older combined Debug timeout remains unexplained and is not attributed to these findings.

All 25 tasks are complete. After reviewer completion, only `tasks.md` and this verification record were updated to record the actual results and closure; reviewed source and the guide remain unchanged. The final scope/hash record is `out/AcceptanceTransitions/Audit/FinalSnapshot.json`. HEAD remains `04fcf6ad2323c066fff1fc582172356a8c6b56f6`, the index is empty, and all changes remain unstaged and uncommitted. The change is active and unarchived; no push was performed. No finding requires an expanded-scope decision.

## Archive and local delivery

On 2026-10-06, the user authorized OpenSpec archival followed by a local Git commit. Earlier statements about leaving the change active, unarchived or uncommitted describe the implementation, documentation and audit milestones; this later authorization supersedes those delivery restrictions.

Before archival, all 25 tasks and all four workflow artifacts were complete. Both delta requirement bodies and their scenarios already matched the current main specification exactly. `openspec archive refactor-editor-acceptance-transitions --skip-specs --yes` preserved that completed synchronization and moved the change, including `.openspec.yaml`, to `openspec/changes/archive/2026-10-06-refactor-editor-acceptance-transitions`. The flag avoids reapplying an already-synchronized added requirement; no specification update remains pending. `openspec list --json` reports no active changes, and `openspec validate --all --strict --no-interactive` passes all 133 current specifications.

The permanent [Editor acceptance maintenance guide](../../../../docs/EditorAcceptance.md) and [current Editor ownership specification](../../../specs/editor-application-consolidation/spec.md) remain the development contracts. Post-archive checks verify 12 Markdown files, 107 local links and 10 Markdown anchors, the guide's current context example and named preview budgets, and exact delta/main requirement equivalence. No reference to the former active-change directory remains in these documents. Evidence is `out/AcceptanceTransitions/Audit/ArchiveDocumentationChecks.json`.

The delivery scope contains the same 59 session files, with the six OpenSpec artifact paths relocated to the archive. All other 58 file hashes match the final audit snapshot; this delivery record is the only additional content change. Implementation and test inputs are unchanged, so the recorded build and regression results are reused without another build or CTest run. Local staging and commit use the frozen path/content manifest and verify staged blobs, the resulting tree and the final worktree. No push is authorized.
