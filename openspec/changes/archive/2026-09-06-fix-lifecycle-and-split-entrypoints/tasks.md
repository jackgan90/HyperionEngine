## 1. Reproduced lifecycle fixes

- [x] 1.1 Balance window subsystem ownership and add surviving-window regression coverage.
- [x] 1.2 Add frame cancellation, failure cleanup and real/mock backend recovery regressions.
- [x] 1.3 Retry failed asset entries and test repaired files, shared retries and preserved successful cache.

## 2. Readability and repository principle

- [x] 2.1 Document the 100-line function principle and tightly coupled exception in repository guidance.
- [x] 2.2 Split Viewer options, lifecycle, frame and output/capture responsibilities while preserving CLI behavior.
- [x] 2.3 Split glTF accessors, primitive conversion and scene orchestration while preserving import semantics.

## 3. Verification

- [x] 3.1 Run focused regressions, Debug/Release complete builds and tests, including RenderDoc-disabled compilation.
- [x] 3.2 Run format, naming, boundary, function-length review and OpenSpec strict checks.
- [x] 3.3 Record verified results and actual change size, with unrelated review findings deferred.

## 4. Independent audit follow-up

- [x] 4.1 Run an independent reviewer without inherited conversation context and independently confirm its findings.
- [x] 4.2 Keep Present failures cancellable until submitted GPU work drains; add a native failure regression.
- [x] 4.3 Validate the minimal fix, obtain reviewer re-review, and record the final audit result.
