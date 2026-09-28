## Context

`FSceneEnvironmentLight` (record `hyperion.sceneenvironmentlight` v2) stores Color, Intensity, Source, an optional Sky reference, YawRadians and bVisible. Reflected inspection renders optional shapes as an "Override" checkbox. Clearing the Sky asset checkbox then fails validation while Source is SkyAsset. The Editor's toolbar action and `scene.sky.use_default` call `UseDefaultSceneSky` in SceneEditing. Placement is registry-driven (`FPlacementRegistry`), with icons published from `Content/Editor/Icons/*.png` by `hyperion_asset_tool build-placement`. Scene settings hold at most one active environment handle. Renderer resolves the active environment into `EnvironmentV1` SH coefficients, prefiltered cube and BRDF LUT bindings, and the `SkyViewV1` block.

## Goals / Non-Goals

**Goals:**
- Sky Light is an ordinary placeable object and component.
- Every Details property is visible only when it has an effect, and it works.
- Tint provides UE-like sky-light color modulation.
- Conditional visibility and asset selection are reusable reflection/GUI features.
- GUI and automation use the same domain paths.

**Non-Goals:**
- A separate background component.
- Asset compatibility beyond migrating v1/v2 records.
- Multi-environment blending.
- Asset thumbnails in the picker.
- Archiving or committing.

## Decisions

1. **Record v3.**
   - `Sky` becomes a non-optional `FAssetRef`, defaulting to `DefaultSkyReference()`.
   - `YawRadians` becomes `YawDegrees`.
   - `Tint` (FVec3, default white) is added.
   - The default `Source` for new values is SkyAsset. `MakeSceneEnvironmentLightNode` (default content and imported scenes) explicitly keeps ConstantColor.
   - Migration 1→2 adds a missing `source` as ConstantColor. Migration 2→3 converts yaw to degrees and drops a null/monostate sky, so the default applies. Angles beyond one turn are reduced through their sine/cosine before conversion so large finite legacy values retain their direction without float overflow or phase loss.
   - Validation always requires a valid native sky reference and finite nonnegative `Tint * Intensity`.
   - Alternatives considered: keeping the optional reference plus a custom widget. This was rejected because it keeps the invalid state representable.

2. **Conditional visibility.**
   - `FPropertyPresentation::VisibleWhen` is an optional `FPropertyCondition{Field, Values}`. The member is shown only when the sibling field's current archive value equals one of the values.
   - Descriptor validation requires Field to name another inspected member of the same record.
   - Single-object drafts evaluate against draft values. Selection drafts hide conditional members when the controlling field is mixed or does not match.
   - Hidden fields keep their values and continue to be validated and persisted. Visibility is presentation-only, so automation schemas are unchanged.

3. **Asset-reference picker.**
   - When an `FAssetRef` member declares `Presentation.ReferenceType`, generic inspection draws `FGui::EditAssetReference` instead of nested record fields.
   - Candidates come from an application-injected provider `SetAssetReferenceProvider(TypeId -> vector<FAssetRef>)`. Gui therefore remains independent of asset services.
   - The combo shows file stems with path tooltips, and it accepts `Hyperion.ContentAsset.v1` path drops that resolve against the candidates. Identity controls highlighting; an explicit choice compares the complete reference so a moved or republished asset with the same ID can replace an old path or pinned revision. Disabled controls reject drops.
   - Editor supplies the asset index filtered by type. It adds the Engine default sky when the index lacks it, and clears revisions so requests pin nothing new.
   - Content Browser file tiles become drag sources with that payload.
   - Without a provider, the picker shows only the current value.

4. **Single active sky light.**
   - `CommitCreate(Node, bInAssignLights)` assigns the main directional light and the environment only when the corresponding selection is empty. This covers placement and automation create. Clipboard paste keeps its existing behavior of never changing light selections.
   - `CommitEdits` assigns a newly added environment component to an empty selection in the same history entry. This covers the "Add component" path in both GUI and automation.
   - Replacing an active sky light requires the explicit `Set as active sky light` action, which uses shared `CommitSettings`.
   - Details displays Active, or "Not effective: another sky light is active" / "no sky light is active", a disabled-node note and the sky asset load status.

5. **Tint rendering.**
   - The CPU multiplies SH coefficients by Tint.
   - The specular tint is packed into the unused `.w` lanes of `EnvironmentSh0..2`. This keeps `EnvironmentV1` layout and existing material assets stable.
   - `SkyViewV1` gains `SkyTint` after `SkyRotationIntensity`. The sky shader is compiled with the renderer, not stored in material assets.
   - Reload keys remain Source/Sky, so Tint/Intensity/Yaw edits update parameters only.
   - Background and IBL share a degree-to-radian conversion that reduces the authored angle before conversion, keeping all accepted finite yaw values safe.
   - ConstantColor ambient is unaffected by Tint. Tint is hidden in that mode.

6. **Placement entry.**
   - The `SkyLight` placeable belongs to Basic and Lights. Its factory creates a node with a default SkyAsset environment.
   - It has no preview geometry, and the marker uses the generated `SkyLight` icon.
   - Removing the toolbar action also removes `UseDefaultSceneSky`, its automation operation and its acceptance test.

## Risks / Trade-offs

- Packed specular tint in SH `.w` lanes is implicit. This is mitigated by the documented shader helper, which is the single reader, and by render regression tests for diffuse/specular/background tint.
- Existing v2 scenes with no sky reference now resolve to Cloudy if switched to SkyAsset. This is intended, and ConstantColor behavior is unchanged.
- Hidden conditional fields are still validated. A hidden invalid value can therefore block a commit. Migration and defaults keep hidden values valid.
