## Context

Hyperion is a C++20 Windows rendering lab with engine-owned wrappers, HLSL shaders, Python/PowerShell tooling, CMake and a Visual Studio solution workflow. The requested migration applies to code we own; SDK and dependency spellings are external contracts.

## Goals / Non-Goals

**Goals:** Consistent Unreal-inspired C++ and HLSL, PascalCase source/tool filenames, durable contributor rules, formatter and semantic naming checks, unchanged rendering and test behavior.

**Non-Goals:** Add Unreal dependencies, UObject/GC/macros, replace STL containers, reorganize the architecture, or change persisted property IDs, plugin IDs, CLI flags, dependency lock contents, build target IDs or historical OpenSpec evidence.

## Decisions

- Use `Hyperion` as the namespace; PascalCase functions, members, locals and parameters; `F` for value/concrete types and aliases, `E` for enums, `I` for abstract interfaces, `T` for class templates. Prefer descriptive names. Parameters use `In` to distinguish them from members; output parameters use `Out` where appropriate.
- Honor the owner's explicit uppercase-first rule for booleans rather than UE's lowercase `b` exception. SDK overrides, `main`, operators, STL customization points, compiler attributes, HLSL semantics/swizzles and third-party names retain required spellings.
- Use Allman braces, mandatory braces for control flow, four-column tabs, one declaration per line, `.h` headers and PascalCase filenames without type prefixes. Keep existing directory layout to avoid coupling a naming change to architectural restructuring. Framework-reserved filenames and OpenSpec artifact paths retain their prescribed spellings.
- Keep Python and CMake language conventions for internal identifiers, while their owned filenames use PascalCase. These are auxiliary tools rather than Unreal C++ APIs.
- Use clang-tidy semantic rename replacements across every owned translation unit, followed by clang-format. This avoids accidentally renaming SDK fields, STL methods or serialized keys. Check in both configurations, contributor documentation and reusable style tooling.
- Preserve executable/target names and CLI/configuration contracts so the established solution, test and experiment workflows remain recognizable.

## Risks / Trade-offs

- Renaming APIs can miss dependent templates or macro-generated references → collect semantic replacements across all owned translation units and build Debug/Release with MSVC.
- Formatting can reorder Windows/COM includes incorrectly → preserve intentional include groups and validate DXC compilation.
- Windows case-insensitive paths can hide incorrect include casing → check exact repository path spelling, including headers and build references.
- UE conventions cannot be applied to external APIs → document narrow interoperability exceptions and keep vendor sources outside checker scope.

## Migration Plan

Record conventions; migrate names and files; update tooling and live docs; run style checks and the full Visual Studio Debug/Release suite including DX12 image checks; synchronize the new specification and archive the change. Generated outputs stay under `out`.
