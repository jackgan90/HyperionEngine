## ADDED Requirements

### Requirement: Mounted text shader sources
Shaders SHALL remain plain text assets and compile through engine-owned filesystem access for main sources and includes. Engine and Game shader paths SHALL be supported, including Game includes of Engine shared files. Content relocation SHALL not change cache identity when logical source contents and compilation inputs are unchanged.

#### Scenario: External material shader
- **WHEN** a Game text shader includes an Engine shader header
- **THEN** supported targets compile and reflect through the same pipeline without local path references in the material

#### Scenario: Relocated warm cache
- **WHEN** mount physical locations change with identical logical contents
- **THEN** the same shader cache entry remains valid and changed includes still invalidate it
