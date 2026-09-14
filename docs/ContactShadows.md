# Hierarchical depth and contact shadows

Deferred contact shadows add short-range screen-space directional visibility to CSM. Enable **Contact shadows** at the top of the diagnostics panel. The switch and parameters are Main-owned settings frozen into each frame. CSM and contact enable switches are independent; the selected scene light's **casts shadows** flag applies to both.

```text
CSM views -> Deferred BasePass (GBuffer + depth)
         -> requested HZB compute mip chain
         -> fullscreen contact visibility mask
         -> Deferred directional/local/indirect lighting
         -> compatibility / sky / transparency -> tonemap -> previews / GUI
```

Only the directional direct-light term uses `min(ContactVisibility, CsmVisibility)`, with 1 meaning lit and 0 occluded. Local lights, indirect/environment light and emissive keep their existing behavior. This can fill missing CSM occlusion; it cannot brighten CSM acne or recover geometry hidden from the camera. Forward has no pre-Z pass and does not produce HZB/contact. Later compatibility and transparent surfaces do not sample the mask of an earlier receiver.

## HZB producer contract

`FHierarchicalDepthProducer` accepts requests containing a sampled D32 source, scope, view and `EDepthReduction`. `BeginFrame(Graph)` resets demand; `Request` schedules production on first use and returns `FHierarchicalDepthProduct`; `EndFrame` retires unused generations. There is no contact-effect setting or persistent consumer counter in the producer. Compatible requests share a product within the graph. Source identity, view identity/camera/revision, dimensions, convention and reduction prevent stale or incompatible reuse.

The product contains a real R32F texture with its complete mip chain, exact mip sizes, reduction/depth metadata, source/graph/view identities, camera, viewport, byte count and lifetime. Consumers call `Validate(Graph, View)` to reject mismatched views or retained products from an earlier graph; the contact pass and HZB preview do this before declaration. Mip zero normalizes the viewport's raw depth range back to clip depth in [0,1] and initializes uncovered pixels to far depth. Each next mip conservatively covers the normalized source footprint, including odd-size edges and overlapping footprints. Sampling uses point loads, never filtered depth.

| Product | Standard Z | Reversed Z |
| --- | --- | --- |
| Nearest depth, used by contact | min | max |
| Farthest depth, available to other consumers | max | min |
| Uncovered far depth | 1 | 0 |

HZB generation issues one dispatch per mip with restricted input/output views. Each write becomes visible to the next mip, and the complete chain becomes readable by the graphics mask pass. An HZB debug preview registers its own request; with contact disabled and no preview there are no HZB dispatches or active products. Submitted older frames can still retain their resources until the normal fence retirement.

## Tracing and settings

The full-resolution R8 visibility pass reconstructs world positions from normalized HZB depth and the matching inverse view projection. Rays point toward the same main directional-light vector used by shading. Geometric-normal and light-direction bias offset the start. Homogeneous clipping limits the ray to the camera frustum; a bounded hierarchical cell traversal skips empty depth and descends toward candidate intersections. Mip-zero depth, coverage and world-space thickness confirm hits. Coplanar receiver rejection, screen-edge fade and end-of-ray fade reduce self-shadowing and discontinuities. Tracing is deterministic and needs no temporal history or denoiser.

| Setting key | Default | Meaning |
| --- | --- | --- |
| `contact_shadows` | false | Request contact visibility in Deferred |
| `contact_shadow_length` | 0.35 | Maximum world-space ray length |
| `contact_shadow_thickness` | 0.05 | Accepted depth thickness in world units |
| `contact_shadow_bias` | 0.003 | Geometric start bias in world units |
| `contact_shadow_steps` | 96 | Maximum hierarchy traversal iterations |
| `contact_shadow_debug` | 0 | 0 lighting, 1 mask, 2 HZB mip preview |
| `hierarchical_depth_mip` | 4 | Preview level, clamped to the actual chain |

The diagnostics panel reports consumers, dispatches and active HZB bytes. HZB preview displays near depth bright in either convention. The existing benchmark CSV includes `contact_active`, `hzb_consumers`, `hzb_dispatches`, `hzb_bytes`, `hzb_gpu_ms` and `contact_gpu_ms`; timings come from the ordinary fenced GPU timestamp capture. Use a ready scene, sufficient warmup and a capture with `--verify-model` when comparing Sponza runs. `--exercise-contact-shadows` drives the actual diagnostics checkbox through off/on/off/on after the scene becomes ready, and rejects incomplete activation/deactivation observations. Combining it with `--exercise-window` delays resize/minimize/restore until contact is active, then checks the resized HZB allocation.

## Pipeline references and trade-offs

[Unreal's contact-shadow documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/contact-shadows-in-unreal-engine) describes per-light screen-space depth tracing as a supplement to ordinary shadows; its [Forward renderer documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/forward-shading-renderer-in-unreal-engine) lists contact shadows among unsupported screen-space techniques. This supports the current Forward limitation here; it does not imply that every engine uses an HZB contact implementation.

Unity HDRP establishes depth before contact processing and shares screen-space shadow results with opaque lighting. Its [Forward/Deferred comparison](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/Forward-And-Deferred-Rendering.html) and [render graph source](https://github.com/Unity-Technologies/Graphics/blob/master/Packages/com.unity.render-pipelines.high-definition/Runtime/RenderPipeline/HDRenderPipeline.RenderGraph.cs) provide the alternative: a Forward depth prepass makes early receiver depth available at additional geometry cost. Hyperion can adopt that ordering later. A mask remains valid only for receivers represented by its input depth.

The analytic tests compare Standard/Reversed masks exactly, reject self-shadowing on a plane, locate a known small occluder's shadow, and keep uncovered pixels lit. HZB tests read every float mip and compare against a CPU conservative-footprint reference, including 1D and odd-size inputs. Deferred tests cover CSM/contact combinations, clustered/ordinary lighting, sub-viewports, resizing, neutral-mask baseline identity, independent preview demand and Forward bypass.
