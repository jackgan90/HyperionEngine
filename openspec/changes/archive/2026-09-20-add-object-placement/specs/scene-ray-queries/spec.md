## ADDED Requirements

### Requirement: Geometric hit normals
An accepted scene ray hit SHALL include a finite normalized world-space geometric triangle normal consistent with the transformed triangle winding. Nonuniform scale, reflection, shear and supported singular transforms SHALL use transformed geometry rather than an invalid inverse-normal calculation.

#### Scenario: Transformed surface placement
- **WHEN** a ray hits a mirrored or nonuniformly scaled triangle
- **THEN** the returned world hit position and normal describe that triangle and support placement without penetrating the surface
