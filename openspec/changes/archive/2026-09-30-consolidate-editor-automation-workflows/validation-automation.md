# Automation adapters and active documentation

## Implementation scope

- Task 7.1 uses the private `SceneOperationRegistration.h` adapter for typed request/result registration, reflected example encoding and `FSceneEditError` conversion. Scene, authoring, clipboard and component families retain their existing owner, effects, completion, keywords, availability and operation IDs. Explicit component request descriptors remain unchanged; asynchronous save admission reuses only error conversion.
- `SceneRegistrationTests.cpp` checks discovery and describe schemas/examples/metadata across the four families, absent document providers, document-independent component type discovery, missing clipboard provider, successful typed component get and stale-document/stale-handle error codes. Existing scene and clipboard tests continue to cover edits, batches, history, persistence and clipboard invocation.
- Task 7.3 corrects `scene.settings.get` and render-settings API text plus active Editor, component, lighting/shadow and capability documentation. Clipboard/light behavior and archived OpenSpec artifacts are unchanged. Import preview fields are additive reflected `dimension` and `pixelBytes`, with existing typed dimensions and display Details retained.

## Verification

- `clang-format --dry-run --Werror` passed for the helper, four migrated adapter files, new registration tests and render-settings metadata file.
- `python tools/CheckStyle.py --paths-only`: passed, 956 owned source files at the checked working-tree snapshot.
- `python tools/CheckBoundaries.py`: passed, 919 source files and 38 modules at the checked working-tree snapshot.
- Targeted `git diff --check`: no whitespace errors.
- `.\tools\Build.ps1 -Preset debug -Target automation_scene_tests`: passed, including migrated adapters and the new focused registration tests.
- `ctest --test-dir out/build/debug -R '^automation_scene$' --output-on-failure`: passed 1/1 in 3.45 seconds. The suite exercises discovery/describe, typed successful invocation, domain-error conversion, missing providers, selection/structural/component edits, clipboard, history and captured save points.
- Coordinated Release build of `automation_scene_tests` passed; `automation_scene` passed in the final eight-test Release run (1.57 seconds).

Task 7.3 ownership text was checked against `FEditorViewport` (camera/target/resize/fit/navigation and view requests), `FEditorDocumentTransition` (pending open/root/close and discard decisions), the BUILD_TESTING-only `FEditorAcceptanceDriver` and production report/unavailable adapters. The primary implementation agent corrected the asset adapter's unsupported-type message to direct scene documents to attached `scene.open`; the supported non-scene document set remains unchanged. Documentation preserves current priority/clipboard semantics and makes no promise that copying lights leaves the derived source unchanged.

## Shared asset workflow adapter regression coverage

`AssetWorkflowTests.cpp` is linked into `automation_asset_tests` and exercises the real `FAssetAutomation` adapter against an `IAssetWorkspace` fake host holding shared documents. The direct workflow consumer models the shared CPU path used by GUI; these checks do not claim to drive actual `FAssetWorkspace` controls. Actual Editor workspace/UI acceptance remains in the primary implementation's validation.

- Encoding and material-slot reference success compare direct shared-workflow, attached-provider and standalone-provider results, generation, dirty state and one-step undo/redo. On-disk bytes remain unchanged until explicit save.
- Invalid field preparation and a referenced material with missing texture dependencies leave snapshot/generation/history unchanged, clear busy ownership and permit a subsequent successful edit. The missing-dependency path is also called through the registered `model.material_slots.set` operation and a real `FAutomationSession`, checking its failed terminal envelope.
- Worker encoding failure clears busy without a history entry. `Drain` joins pending preparation and leaves the document unchanged; attached close is busy during preparation and succeeds after drain.
- Replacing an attached host document causes stale-document completion without committing into either old or replacement state. `StopAdmission` finishes an admitted edit; real `FApplicationHost::Stop` with automation-assets/session drains a pending attached encoding edit before provider destruction, preserving history and leaving disk bytes unchanged.
- The existing workflow completion fixture now invalidates the asset cache before loading after the preceding external-writer conflict test. Its first run otherwise reused that conflict fixture's failed/stale cache entry; this isolation change does not alter production behavior.

`.\tools\Build.ps1 -Preset debug -Target automation_asset_tests` passed and `ctest --test-dir out/build/debug -R '^automation_assets$' --output-on-failure` passed 1/1 in 1.82 seconds. The coordinated Release build and final eight-test run passed all tests in 21.98 seconds, including `automation_assets` (1.20 seconds) with the final registered-session failure assertions and `automation_scene` (1.57 seconds). The final Debug integration checkpoint remains with the primary implementation agent. Formatter output is byte-identical to both workflow test files; LLVM 22.1.1 dry-run reports redundant definition-separator replacements for these files despite no net formatted difference, so this uses the same output-comparison method as CheckStyle.
