## 1. Native content

- [x] 1.1 Migrate Cloudy and its baked textures, preserve IDs/provenance and update native references.
- [x] 1.2 Update source recipes, licensing and content documentation; validate both libraries.

## 2. Scene and preview integration

- [x] 2.1 Add canonical Engine sky reference and shared undoable default sky assignment.
- [x] 2.2 Add Editor UI and registered automation operation with existing reflected contracts.
- [x] 2.3 Enable default sky in transient Model and Material previews.

## 3. Verification

- [x] 3.1 Add regression coverage for create/replace, history, stale requests, persistence and catalog invocation.
- [x] 3.2 Build affected targets, run style/boundary and targeted regression checks, validate live rendering and failure/lifecycle behavior.
- [x] 3.3 Record validation evidence and leave the change active with both repositories uncommitted.

## 4. Audit repairs

- [x] 4.1 Prepare Engine and Game source fixtures together and verify a clean output directory (SKY-01).
- [x] 4.2 Preserve the canonical sky root ID when reconstructing missing/corrupt output, reject identity conflicts, and expose the shared option through AssetTool and automation (SKY-02).
- [x] 4.3 Run targeted regression checks and obtain independent re-review of both fixes.
