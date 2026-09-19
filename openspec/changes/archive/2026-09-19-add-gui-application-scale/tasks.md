## 1. Shared GUI scaling

- [x] 1.1 Add validated frame-boundary scaling, baseline styles and density-aware font snapshots.
- [x] 1.2 Convert explicit GUI widget metrics and application panel sizing to scale-aware dimensions.
- [x] 1.3 Update GUI rendering to replace and retain snapshot-specific font resources.

## 2. Application preferences

- [x] 2.1 Add GUI-service preference loading/saving and Editor startup overrides.
- [x] 2.2 Add Editor runtime scale controls and document usage and scope.

## 3. Verification

- [x] 3.1 Cover scale round trips, invalid values, input, density and queued atlas replacement with targeted tests.
- [x] 3.2 Build affected targets and run GUI, Editor and plugin regression checks, style, naming, boundaries and OpenSpec validation; record results without committing.

## 4. Runtime scale stability repair

- [x] 4.1 Reproduce repeated-frame instability during menu scale interaction and establish its cause.
- [x] 4.2 Fix the demonstrated cause and cover stationary input, settled frames and scale transitions.
- [x] 4.3 Rebuild affected applications, run targeted regressions and record validation without committing.
