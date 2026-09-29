## 1. Shared placement implementation

- [x] 1.1 Introduce owned candidates and shared UI-independent resource preparation and commit logic; preserve preset behavior.
- [x] 1.2 Route model asset payloads through the common viewport gesture, feedback, preview and cancellation path.
- [x] 1.3 Integrate document/content cleanup and prepared resource invalidation.

## 2. Automation and documentation

- [x] 2.1 Add reflected model placement requests and register the asynchronous operation with stale/busy/unavailable handling.
- [x] 2.2 Document GUI behavior, loading/cache limits and automation capability coverage.

## 3. Verification

- [x] 3.1 Add meaningful model placement coverage for multipart resources, shared references, history/persistence, invalid requests and actual GUI dragging/cancellation.
- [x] 3.2 Build affected targets and run style, naming, boundaries, OpenSpec validation and targeted regression/desktop tests.
- [x] 3.3 Inspect the final diff and record validation evidence, leaving the change active and all code uncommitted.

## 4. Source material preview

- [x] 4.1 Prefer ready source materials per native-model section, with temporary shaded fallback, and render transient source items through the normal scene view passes without persistent registration.
- [x] 4.2 Verify material readiness transitions, rendered source colors/coverage/transparency, cancellation and GUI placement; update documentation and validation evidence.

## 5. Quality audit

- [x] 5.1 Independently audit the full change, verify findings and repair the preview-to-node handoff.
- [x] 5.2 Run targeted regression and independent re-review; record findings and leave the change unarchived and uncommitted.
