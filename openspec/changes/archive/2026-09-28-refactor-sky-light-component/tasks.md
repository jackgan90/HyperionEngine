## 1. Reflection and data model

- [x] 1.1 Add `FPropertyCondition` / `VisibleWhen` presentation metadata with descriptor validation and a shared matching helper.
- [x] 1.2 Migrate the environment record to v3 (non-optional sky reference, Tint, degree yaw, SkyAsset default) with v1/v2 migrations and validation.
- [x] 1.3 Update scene defaults, previews and all construction sites.

## 2. Scene editing and automation

- [x] 2.1 Assign an environment light on create/add only when none is active, in the same history entry.
- [x] 2.2 Remove `UseDefaultSceneSky` and the `scene.sky.use_default` automation operation.

## 3. GUI and Editor

- [x] 3.1 Apply conditional visibility in single and multi-selection reflected inspection.
- [x] 3.2 Add the typed asset-reference picker, provider injection and Content Browser drag source.
- [x] 3.3 Add the SkyLight placement entry, generated icon and marker; remove the toolbar Use Default Sky button.
- [x] 3.4 Show active/not-effective sky light status with an undoable Set as active sky light action.

## 4. Rendering

- [x] 4.1 Apply Tint to diffuse SH, specular IBL and sky background, and yaw in degrees, without reloading resources.

## 5. Documentation and verification

- [x] 5.1 Update Editor, automation capability, content and sky lighting documentation.
- [x] 5.2 Add scene activation, migration, reflection visibility, placement and tint rendering regressions; remove obsolete default-sky tests.
- [x] 5.3 Build affected targets, run targeted tests, style/boundary checks and strict OpenSpec validation; record evidence without archiving or committing.
