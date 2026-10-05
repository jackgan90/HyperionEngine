## Context

FComputePass is a separate caller declaration but AddCompute converts it into a graphics record with bCompute and compute-only fields. Topology, validators and preparation repeatedly interpret that loose record. Compile already performs declaration/content validation before resolution, physical checks before Prepare, and execution validates callback-produced commands before BeginFrame.

## Goals / Non-Goals

Make accepted graphics/compute payloads explicit and keep one color attachment list while preserving existing scheduling, resource state and execution contracts. Keep public Add/AddCompute and legacy Color input. Do not add render features, resource aliasing, multiple GPU queues or change pass order/fence retirement.

## Decisions

### Private accepted representation

Forward-declare accepted pass storage from RenderGraph and define it only in a private header. Common facts own Name, Reads, After and Timing. A variant owns either graphics attachments/viewport/batches/Prepare/buffer reads or compute writes/buffer accesses/dispatches/Prepare. Graphics input no longer exposes compute-only fields. Derived views and IsCompute inspect the payload rather than storing a second kind flag.

Use an out-of-line RenderGraph destructor to retain incomplete private vector storage. Copy and consume methods keep their existing ownership and graph identity rules. Typed preparation reads only the selected payload; native command kind is derived when emitting FPassCommands.

### Normalize admission once

Add rejects simultaneous Color and Colors before adding a pass. A legacy Color becomes the accepted vector; there is no accepted optional Color alongside it. A small explicit command-layout enum retains whether legacy single-color or vector RHI command encoding was requested, preserving existing FPassCommands shapes without retaining a second attachment value. Public GetColors remains a declaration convenience; graph internals read the normalized vector.

### Preserve staged validation

Admission checks only duplicate color storage and mutability; topology/declarations/content still validate for the entire graph before any resolver or deferred Prepare. Resolved physical descriptions/aliasing validate before callbacks. Callback result checks retain their ordering before frame begin/submission. Typed storage removes impossible mixed declarations rather than replacing resource/access validators. Compile works on a copy. CompileAndConsume preserves the existing limited failure semantics: validation failures before work consumption retain declarations, but failure after an earlier immediate pass has moved its packets does not restore those packets. No strong exception guarantee is added; consuming success invalidates declarations as before.

## Risks / Trade-offs

- Moving common fields may omit dependency or timing facts → compare existing plans and timing/copy tests.
- Normalization could alter load/store, sRGB DrawBatch or MRT behavior → preserve explicit command layout and real graphics regressions.
- Refactoring may run callbacks before a later failure → verify exact resolver/Prepare/BeginFrame/submit counts for each stage.
- Incomplete storage may break consumers → build full Debug/Release applications and public consumers.

## Migration Plan

Introduce private accepted payloads, migrate graph helpers, add admission/stage regressions and verify actual compute/graphics/HZB/buffer work, then independently audit. Existing persistent data and authored graph semantics require no migration.
