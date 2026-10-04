## Why

Directional and contact shadow preview modes are interpreted through numeric comparisons, offsets and independent display lists. A mode change requires manually synchronizing validation, inspection and rendering consumers.

## What Changes

- Define typed preview identities and explicit stable numeric/label mappings in the CPU-only RasterOptions module, consumed by Scene and Config.
- Interpret directional cascade selection, cascade coloring, normal lighting and contact/HZB preview through the shared contract.
- Project explicit value choices into Scene inspection using the companion `bind-gui-enum-options` change.
- Keep existing fields, numeric values, schema shapes and rendered behavior.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `scene-light-shadow-properties`: Preview modes have stable typed identities independent of display order.
- `cascaded-shadow-maps`: Cascade preview selection and lighting policy use explicit mode interpretation.
- `contact-shadows`: Contact preview selection uses explicit validated identities.

## Impact

RasterOptions, Scene shadow settings, Config legacy contact settings, Renderer shadow consumers and Editor feature availability checks. Scene gains a direct dependency only on the existing standard-library-only RasterOptions module. No rendering dependency enters Scene or Config. No operation IDs, revisions, persistence, shader ABI or feature lifecycle changes.
