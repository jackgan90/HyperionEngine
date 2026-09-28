# Verification

Validated on Windows x64 with the Debug preset on 2026-09-28. The change is neither archived nor committed.

Follow-up independent review and repairs are recorded in [review.md](review.md). The repaired working tree passed a full Debug build, 25 selected regressions, formatting/naming/boundary checks and strict validation; the full-suite count below refers to the earlier implementation run.

## Build

- `tools/Build.ps1 -Preset debug` passed: all targets, including Editor, AssetTool, the Engine icon content and the test executables.

## Tests

The full CTest run passed 112/112 tests (`ctest -j6 --output-on-failure`, 569.78 seconds).

Targeted reruns after the final code fixes also passed 4/4: `automation_scene`, `editor_placement`, `native_asset_publication` and the required `model_fixtures`.

Coverage for the new behavior:

- **`scene_management` (component tests).** v1 and v2 environment records migrate. v1 keeps ConstantColor and v2 keeps SkyAsset. Radian yaw becomes equivalent degrees, a missing or null sky becomes the Engine default, and tint defaults to white. Values hidden by the selected source still round-trip.
- **Serialization inspection tests.** `VisibleWhen` descriptors are validated: missing, uninspected, self-referencing and empty conditions are rejected. The tests also cover condition matching and equality, and check that hidden members keep their values through archive round-trips and draft edits.
- **`automation_scene`.** `scene.node.create` with an environment component produces SkyAsset + Cloudy + white tint and becomes active only when nothing is active. Adding a component follows the same rule and shares one history entry with its activation. Changing the active light through settings supports undo and redo. Deleting the active light clears the setting. Invalid values forged on the wire are rejected as `invalid_arguments` with no document or history change: an empty sky reference, a non-sky type and a negative tint. Tint and degree yaw edits apply. `scene.sky.use_default` is no longer discoverable.
- **`editor_placement`.** Real GUI input drags all nine placeables, including Sky Light, from Place Object. The first Sky Light references the Engine default sky and becomes active. Copy/paste keeps the active light, and creation undo/redo restores it. A second Sky Light does not replace the active light, and Set as active supports undo and redo. The explicit active selection persists through save and reopen.
- **`sky_rendering`.** Tint filters background, diffuse (dielectric) and specular (metallic) sky radiance per channel without restarting the sky load. A black tint leaves point-light direct lighting identical to a zero-intensity sky. Degree yaw orients the sky.
- **`native_asset_publication`.** A model-to-scene import now records the model dependency and the Engine default sky dependency (resolved through an `/Engine` mount, as AssetTool and Editor do).
- **Other.** `object_placement`, `editor_clipboard`, `deferred_rendering` and the automation parity tests exercise the surrounding placement, history and shared rendering/domain operations. Direct conditional-inspection and typed-picker input regressions were added during the review recorded in `review.md`; the original tests did not directly cover those new controls.

## Static checks

- `python tools/CheckStyle.py`: filenames/include casing (912 owned source files) and formatting passed.
- `CheckStyle.check_naming` on the 32 changed or new C++ translation units: clang-tidy and boolean declaration checks passed.
- `python tools/CheckBoundaries.py`: 876 source files and 38 modules passed.
- `git diff --check`: passed.
- `openspec validate refactor-sky-light-component --strict`: passed.
