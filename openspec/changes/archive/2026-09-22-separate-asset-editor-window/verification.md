## Scope

Validated on Windows with the D3D12 backend. The Editor now owns one secondary native asset window, with its own GUI context, renderer, swapchain and docking layout. The main scene viewport and scene Details remain independent. Existing uncommitted native asset work is preserved; no third-party sources were changed.

## Build and static checks

- Full Debug and Release builds succeeded, followed by incremental Editor builds in both profiles for the final acceptance timing change.
- Release retains the existing third-party TinyEXR C4702 warning at `out/deps/tinyexr/tinyexr.h:6375`; it was neither edited nor suppressed.
- `python tools/CheckStyle.py`: owned filenames, include casing and formatting passed for 742 files.
- `python tools/CheckBoundaries.py`: 706 C++ sources across 35 modules passed.
- Semantic naming checks passed for 19 affected translation units, followed by checks of four later changed units (including EditorHost) and the three final acceptance-related units.
- Both `separate-asset-editor-window` and `add-native-asset-editors` passed strict OpenSpec validation. `git diff --check` passed.

Build/check logs: `out/AssetWindowFinalDebugBuild.log`, `out/AssetWindowFinalEditorDebugBuild.log`, `out/AssetWindowFinalReleaseBuild.log`, `out/AssetWindowFinalReleaseEditorBuild.log`, `out/AssetWindowFinalStyle.log`, `out/AssetWindowNaming.log`, `out/AssetWindowFinalNaming.log`, `out/AssetWindowAcceptanceNaming.log`.

## Automated acceptance

| Profile / run | Result | Log |
| --- | --- | --- |
| Debug asset documents/editors, GUI and native event routing | 6/6 passed | `out/AssetWindowFinalDebugTests.log` |
| Debug Editor/content/plugin/lifecycle regression, including fixtures | 17/17 passed | `out/AssetWindowDebugRegression.log` |
| Debug final asset acceptance, with regenerated fixture | 2/2 passed | `out/AssetWindowFinalDebugAssets.log` |
| Release corresponding combined suite | 20 passed; asset input timing failure investigated below | `out/AssetWindowReleaseRegression.log` |
| Release final asset acceptance, three separate runs with regenerated fixtures | 2/2 passed in each run | `out/AssetWindowFinalReleaseAssets1.log` through `out/AssetWindowFinalReleaseAssets3.log` |

Together these cover 21 distinct tests in each profile. Native/GPU acceptance reported zero validation errors. The selected suite includes `editor_acceptance`, `editor_multiselect`, `editor_placement`, `editor_capture_ui`, `gui_scale_acceptance`, `editor_content_transition`, `editor_content_startup`, `plugin_runtime`, `plugin_applications`, `lifecycle_recovery`, `d3d12_device_ownership` and `window_lifecycle`. Plugin acceptance includes controlled failure when asset output is requested with `gui` or `editor` explicitly disabled.

The extended `editor_asset_editors` test uses actual native windows, GUI draw data, GPU rendering and window-specific Platform input events. It covers:

- Existing Texture, Model, Material and Sky previews, properties, save/undo/redo, reference replacement and refresh while preserving scene edits.
- Simultaneous scene and asset rendering with distinct native handles, scene Undo/Redo while an asset remains selected, and asset Undo without changing scene history or camera.
- Separate GUI context initialization and destruction, including failure when the required font cannot load.
- Resize, shared application scale and minimization (updated to native owner-group behavior in the follow-up below).
- Closing only the requested asset tab, plus the earlier held-button Place Object panel drag regression with multiple asset tabs open.
- Native asset-window close cancellation, discard without persistence, recreation, save failure with retained drafts, and successful retry followed by close.

The save-failure test temporarily damages a file in the generated test fixture and restores the original bytes before retrying. It does not modify project content.

`gui_docking` tests independent GUI contexts and framebuffer densities. `window_event_routing` checks native event routing. Existing Editor, content transition, plugin-disablement, GPU ownership and lifecycle tests are retained.

The acceptance exposed an asset close-dialog bug: finishing editing again inside the Save button handler dismissed the popup before an asynchronous save failure could be retried. The redundant call was removed; finishing the current edit occurs when opening the dialog. Dismissing a per-tab prompt also releases its input-blocking state.

An initial Release run timed out while the synthetic Content Browser double-click sequence was opening Sky. The existing frame-count-based input used wall-clock GUI time, allowing rapid hidden-window frames and scrolled tiles to join separate click sequences. Asset acceptance now uses a fixed 60 Hz GUI timestep in both windows; normal interactive timing is unchanged. Temporary click logging was removed, while failure diagnostics retain the selected content path and both window captures. Each final rerun invokes CTest separately so its document fixture is regenerated; CTest's per-test `--repeat` does not reset a fixture between repetitions of the dependent editor test.

## Visual inspection

Inspected `out/build/debug/AssetEditor-Material.png` and `out/build/debug/AssetWindow-Scene.png`, captured during the same asset acceptance run. The asset window displays the material preview and its dedicated properties panel; the main window retains the scene viewport, Outliner, Details and the unsaved scene edit.

## Boundaries

This change supports one scene window plus one multi-tab asset window. Arbitrary tab tear-off and cross-window docking are outside scope. Mixed framebuffer density is covered by GUI tests; moving visible windows between physical monitors with different DPI settings was not manually exercised. All changes remain uncommitted.

## Desktop ownership follow-up

User acceptance exposed a gap in the initial validation: the two native windows had no owner relationship. Activating the main window could cover the asset window even though both windows still existed and rendered. Hidden-window GUI/GPU tests did not verify desktop stacking.

The Editor now associates its asset window with the main window through `FWindow::SetOwner`, implemented with the existing SDL adapter. The asset remains a non-modal top-level window, above its owner while scene controls receive input; it is not desktop-always-on-top. Activation restores only minimized windows, preserving a maximized asset window. The owner outlives its asset host, including partial initialization and root-switch teardown. Ownership cycles are rejected.

Native owner minimization now hides the asset window with the main window; both presentations pause while saves/loading/polling continue. Restoring the owner resumes both presentations. Minimizing only the asset window leaves the scene running. The asset acceptance assertions and the specification were updated to this desktop behavior.

Added `window_ownership`, a visible native-window test checking actual Win32 owner handles, top-level/non-topmost styles, repeated owner activation with focus retained, relative Z-order, an unrelated window covering both, owner/asset minimization and restoration, close routing, ownership-cycle rejection and recreation. It creates only test-owned windows and uses native activation instead of injecting input into another application.

References: [SDL_SetWindowParent](https://wiki.libsdl.org/SDL3/SDL_SetWindowParent), [Windows owned-window semantics](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#owned-windows). Dependency sources were inspected but not modified.

Follow-up validation completed:

- Full Debug and Release builds passed; the final Debug Editor target was rebuilt after the close-prompt restoration adjustment. Logs: `out/AssetWindowOwnershipDebugBuild.log`, `out/AssetWindowOwnershipDebugEditorBuild.log`, `out/AssetWindowOwnershipReleaseBuild.log`.
- Debug 13/13 and Release 13/13 targeted tests passed, including their fixture setup: `editor_asset_documents`, `editor_asset_editors`, `editor_content`, `editor_content_transition`, `editor_content_startup`, `window_event_routing`, `window_ownership`, `model_fixtures`, `plugin_applications`, `native_model_fixtures`, `lifecycle_recovery`, `d3d12_device_ownership`, `window_lifecycle`. GPU acceptance reported zero validation errors. Logs: `out/AssetWindowOwnershipDebugTests.log`, `out/AssetWindowOwnershipReleaseTests.log`.
- Owned-source style checks passed for 743 files; boundaries passed for 707 C++ sources across 35 modules; semantic naming passed for the six affected translation units. Logs: `out/AssetWindowOwnershipStyle.log`, `out/AssetWindowOwnershipNaming.log`.
- Strict OpenSpec validation and `git diff --check` passed. No commit, push, dependency edit or warning suppression was performed.

## Activation double-click follow-up

User acceptance found that the first double-click on an unselected asset only selected it. The user confirmed this also occurred with an empty scene, and that first clicking blank Content Browser space made the next double-click work. The owned Platform adapter had retained SDL's default suppression of clicks that activate a native window. Returning from the asset window therefore lost the first press/release of the double-click.

Configured `SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH` in the engine-owned adapter. GUI click timing, single-click selection, asset loading and tab activation continue through their existing paths; third-party sources remain unchanged.

Added `window_activation_input`, which creates two test-owned native windows, activates them with actual mouse input and verifies that each return to the main window delivers both presses/releases of its first double-click. Input targets are checked against the test windows, button release is handled on failure, and the original cursor/foreground are restored on teardown. Unlike synthetic GUI input, this exercises SDL's native activation filtering. The original code failed with `Activation double-click delivered 1 presses and 1 releases` (`out/ContentDoubleClickBefore.log`); the same regression passed after the adapter fix (`out/ContentDoubleClickAfter.log`).

Added GUI coverage for first-double-click opening of two different unselected tiles, single-click selection without opening and queued click events. These use the existing FileTile API within a docked workspace.

Validation completed:

- Full Debug and Release builds passed (`out/ContentDoubleClickDebugBuild.log`, `out/ContentDoubleClickReleaseBuild.log`).
- Debug 13/13 and Release 13/13 passed: asset document/editor acceptance, Content Browser and root transitions/startup, GUI input/docking, native activation input, window routing/ownership/lifecycle, complete scene-editor acceptance and plugin startup/disablement paths. Logs: `out/ContentDoubleClickDebugTests.log`, `out/ContentDoubleClickReleaseTests.log`.
- Style passed for 745 owned files, boundaries for 709 C++ sources across 35 modules, and semantic naming for all four changed translation units. Logs: `out/ContentDoubleClickStyle.log`, `out/ContentDoubleClickNaming.log`.
- Strict OpenSpec validation and `git diff --check` passed. No third-party changes, commits or pushes were made.

## Independent quality audit follow-up

The independent full-workspace audit found QA-01/P1: main-window Ctrl+S called SaveScene without the scene-readiness/modal guards or exception containment used by the menu, allowing a failed snapshot to escape the application loop. The shortcut now requires a ready scene, no modal/root transition or drag, a destination and no existing save; persistence failures remain Editor errors. Real Editor acceptance exercises loading and failed scenes, modal states and a ready scene whose source-less model makes Snapshot throw, verifying that scene and asset document states survive.

QA-03/P2 also affected this host: pending texture encoding work was not included in the workspace's dirty state. It now protects native-window/application/root closure, and save-close controls remain unavailable until the complete edit has been applied; explicit Discard can cancel it. The runtime refresh finding QA-02/P2 and full repair evidence are recorded in the native-asset-editor change's audit follow-up.

Full Debug/Release builds and six affected regressions per configuration passed, with zero Editor GPU validation errors. The original reviewer verified the frozen repair snapshot and independently reran three Debug regression programs including full asset acceptance, closing QA-01/02/03 without a new confirmed blocker. See `out/QualityAuditAssetEditors/IndependentReReview.md`, `FixesDebugEditorDetailed.log`, `FixesReleaseDetailed.log` and the `ReReview` logs. No source or third-party edits occurred during re-review; no commit or push was made. Pending-encoding tab-save and root-switch branches received static state-transition verification; the new automated coverage directly exercises workspace/native-window/application-close protection.
