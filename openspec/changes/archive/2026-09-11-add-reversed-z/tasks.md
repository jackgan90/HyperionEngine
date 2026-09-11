## 1. Depth contracts

- [x] 1.1 Add engine depth convention helpers and finite perspective mapping with CPU tests.
- [x] 1.2 Add view-relative material depth resolution, preserve raw state, and cover PSO lookup/creation and instancing.

## 2. Renderer integration

- [x] 2.1 Propagate view convention through scene targets, legacy frame depth, sorting and preparation caches.
- [x] 2.2 Implement convention-correct CSM projection, sampler, neutral texture, bias and cache identity.

## 3. Application integration

- [x] 3.1 Add reflected startup-only configuration, active-mode diagnostics and explicit example settings.
- [x] 3.2 Update ModelViewer/SceneViewer projections and enable convention-correct Triangle depth.

## 4. Validation and delivery

- [x] 4.1 Add both-mode GPU tests for visibility, transparency, ordinary/instanced material state and shadows.
- [x] 4.2 Verify both modes in the three applications, startup persistence, Forward/Deferred and Debug/Release.
- [x] 4.3 Complete style, naming, module-boundary and OpenSpec checks; document API behavior and validation evidence.
