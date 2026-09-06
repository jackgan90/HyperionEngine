# Repository development rules

Read [docs/CodingStyle.md](docs/CodingStyle.md) before editing owned code. Unreal Engine-inspired C++ conventions are the repository default, with the owner's explicit uppercase-first naming rule taking precedence.

- Use `Hyperion` namespaces and PascalCase functions, variables, members and parameters. Use `F` for concrete/value types, `E` for enums, `I` for abstract interfaces and `T` for class templates. Do not add UObject/Actor/Slate prefixes to unrelated types.
- Boolean names also start uppercase (for example `IsReady`); do not introduce a lowercase `b` prefix. Prefer descriptive names; no member `_` suffix or `m_` prefix. Input parameters use `In`, output parameters use `Out`.
- Owned source and tool filenames use PascalCase, without type prefixes: `Core.h`, `TaskSystem.cpp`, `GenerateSolution.ps1`. Keep framework-required filenames and OpenSpec artifact names intact.
- Use the root `.clang-format`: Allman braces, tabs with four-column display, explicit braces even for a single control-flow statement. Declare one variable per statement. Follow `.clang-tidy` naming rules.
- Preserve mandated external spellings (`main`, STL/SDK overrides, vendor APIs, shader semantics/swizzles). Keep serialized keys, plugin IDs and CLI flags stable unless a behavior change explicitly requires migration. Do not reformat or rename dependency code under `out/deps`.
- Third-party APIs belong in private `src/adapters` implementations. Public engine APIs, modules and plugins access them through engine-owned wrappers.
- Keep CMake build targets and executable names stable. Python helper internals follow PEP 8; PowerShell uses PascalCase identifiers. Fixed tool/config filenames are documented exceptions.
- Validate changes with the appropriate style checks and builds described in `docs/CodingStyle.md` and `docs/VisualStudio.md`. Complete authorized OpenSpec work without requesting redundant permission for reversible implementation steps.
