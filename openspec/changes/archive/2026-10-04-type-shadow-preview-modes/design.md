## Context

Baseline `05d572c` stores directional modes 0..5 and contact modes 0..2 as unsigned integers. Scene validators, inspector lists and Renderer consumers separately interpret these values. Config exposes the existing signed contact_shadow_debug setting. Their numeric wire/schema shapes must remain stable.

## Goals / Non-Goals

Goals: one CPU-only definition of supported preview identities, wire values and labels; explicit cascade selection and lighting policy; compatibility across GUI/automation/save/reload.

Non-goals: changing shadow algorithms, cascade counts, GPU ABI, adding preview modes or changing plugin lifecycle.

## Decisions

- Extend the existing standard-library-only RasterOptions module with ShadowPreviewOptions. Scene and Config both consume it directly. This avoids a Config-to-Scene dependency or a rendering dependency in CPU authoring modules.
- Preserve uint32 DebugMode fields and reflected descriptors as boundary representations. Parse them with strict typed codecs before behavior decisions; explicit tables own wire values, labels and optional cascade index. Do not register a changed enum schema for existing integer fields.
- Use typed identities in cascade-color, cascade-depth, normal-lighting and contact/HZB decisions. Both forward paths use the same cascade lookup, with no numeric subtraction.
- Project value-bearing inspector choices from the canonical tables using the companion GUI change. Legacy Config range/validation derives from the same contact options while retaining the existing numeric property.
- Keep all domain operations, validation entry points, dirty/history behavior, missing-feature checks and startup defaults in place.

## Risks / Trade-offs

- Schema or persistence drift -> freeze numeric fields and record versions; test archive/wire round trips and unknown mode rejection.
- Display order changes affecting rendering -> reordered table tests plus explicit mode-to-cascade assertions and existing GPU rendering tests.
- Local-light suppression changes -> migrate every numeric mode branch and compare normal/debug behavior with existing tests.

## Migration Plan

Implement after explicit inspector choices, validate Scene/Config/Renderer and real Editor/automation consumers in Debug and Release, and leave the change active without archive or commit.
