## Context
Eight changes established private dependency adapters, reflection/plugins, CPU domains, assets/shaders and a validated hardware triangle path.
## Goals / Non-Goals
Goals: useful interactive diagnostics and a fully verified Windows launch. Non-goals: a full editor, docking/multi-viewport, dynamic font atlas streaming, custom GUI texture widgets or runtime plugin reload.
## Decisions
- Gui exposes owned input, widgets, reflection editing, plotting and copied draw data. ImGui and ImPlot calls stay in a private adapter; plugin UI code uses Gui only.
- Gui and platform input live on Main. A static font atlas is built using ImGui's supported legacy renderer path; GPU font upload and frame buffers use the RHI wrapper. Draw data is deep-copied before handing it to RHI/Render.
- The debug plugin owns its pipeline/font and appends a Load pass. Alpha blending, scissor rectangles, vertex offsets and 32-bit copied indices are handled by the generic RHI.
- Current frame time measures CPU application frame intervals, including present waits; it is explicitly not GPU timing. GPU memory totals cover D3D12MA allocations; CPU tagged totals cover hooked allocations only.
- Reflected visual parameters update immediately; plugin selection takes effect after restart. Save and capture are explicit UI actions; automated tests write only under out.
- Validation covers widget input and owned data, config reload, Debug/Release CTest, GPU screenshot pixels, GUI presence and thread activity. Final visible launch uses the same application executable.
## Risks / Trade-offs
- GUI adapter supports a static atlas only → fail explicitly on unsupported texture IDs/callbacks; add dynamic textures as a future capability.
- Input injection verifies the internal input path → combine it with actual SDL window lifecycle and visible rendering evidence.
- Baseline uses one graphics queue and synchronized CPU frame stages → record limits honestly in the README.
