# Repository development rules

Read [docs/CodingStyle.md](docs/CodingStyle.md) before editing owned code. Unreal Engine-inspired C++ conventions are the repository default, including the lowercase `b` prefix for boolean variables.

Before changing application composition, plugin behavior or feature integration, read and follow [docs/PluginSystem.md](docs/PluginSystem.md). Its development constraints are mandatory architecture rules.

- Use `Hyperion` namespaces and PascalCase functions, variables, members and parameters. Use `F` for concrete/value types, `E` for enums, `I` for abstract interfaces and `T` for class templates. Do not add UObject/Actor/Slate prefixes to unrelated types.
- Boolean variables use `b` followed by PascalCase (for example `bIsReady`), including atomic boolean flags. Boolean parameters use `bIn` / `bOut`; query functions remain PascalCase (`IsReady()`). Prefer descriptive names; no member `_` suffix or `m_` prefix. Other input parameters use `In`, output parameters use `Out`.
- Owned source and tool filenames use PascalCase, without type prefixes: `Core.h`, `TaskSystem.cpp`, `GenerateSolution.ps1`. Keep framework-required filenames and OpenSpec artifact names intact.
- Use the root `.clang-format`: Allman braces, tabs with four-column display, explicit braces even for a single control-flow statement. Declare one variable per statement. Follow `.clang-tidy` naming rules.
- A single function should normally not exceed 100 lines. Split longer functions at logical boundaries unless the logic is so tightly coupled that meaningful decomposition is difficult. Comments may clarify non-obvious coupling when helpful; no dedicated comment is required solely because a function exceeds 100 lines. Follow the counting and review guidance in `docs/CodingStyle.md`.
- Preserve mandated external spellings (`main`, STL/SDK overrides, vendor APIs, shader semantics/swizzles). Keep serialized keys, plugin IDs and CLI flags stable unless a behavior change explicitly requires migration. Do not reformat or rename dependency code under `out/deps`.
- Organize owned code as `Source/Runtime/<Module>`, `Source/Backends/<API>`, `Source/Plugins/<Plugin>` and `Source/Applications/<App>`. Each module has its own CMake target, `Public/Hyperion/<Module>` interface and `Private` implementation; declare direct dependencies. Follow `docs/SourceLayout.md`.
- Third-party APIs belong in each runtime module's `Private/Adapters` or a native backend's `Private` implementation. Public engine APIs, modules and plugins access them through engine-owned wrappers. Runtime must not depend on experiment plugins or native backends.
- Scene and Environment are independent CPU data/processing runtime modules; do not make them depend on Renderer or RHI. Future Animation code follows the same boundary. Scene-to-render bridging and reusable scene-camera navigation belong to Renderer. Keep shared utilities in focused Core subdirectories and module-specific helpers private.
- Keep CMake build targets and executable names stable. Python helper internals follow PEP 8; PowerShell uses PascalCase identifiers. Fixed tool/config filenames are documented exceptions.
- Validate changes with the appropriate style checks and builds described in `docs/CodingStyle.md` and `docs/VisualStudio.md`. Complete authorized OpenSpec work without requesting redundant permission for reversible implementation steps.
- Keep README focused on the project, supported features and portable build/run instructions. Keep documentation consistent with code and shipped configuration; do not present historical test counts, local machine state or completed task instructions as current requirements. See `docs/README.md` for the documentation index.

## Plugin architecture

- Keep `Runtime/Application` limited to Tasks, Main pumping, time and exit control. Applications select catalogs and native providers; feature orchestration and resource ownership belong to plugins. Reusable algorithms and data libraries remain Runtime modules, without depending on concrete plugins.
- Select statically linked plugins at startup only. Explicit disablement wins over dependencies; missing optional plugins must leave unrelated branches usable. Report unavailable required capabilities as controlled failures, never unchecked dereferences or false success.
- Use `Start/Update/Quiesce/Stop`, declared dependencies and scoped cleanup. Join plugin work before destroying captured state; preserve provider lifetimes and GPU fence retirement. Do not add a separate application-owned lifecycle for each feature.
- Communicate through engine-owned typed services, domain interfaces and scoped events, not concrete-plugin casts or global service locators. Keep lifecycle and event dispatch on Main; transfer owned snapshots across execution domains. Use the documented render/GUI extension points.
- Validate affected absence, disablement, startup-failure and shutdown paths; verify compilation/linking when changing build selection. Changes to these principles must first be explicit in the OpenSpec design and update the contract and regression coverage in the same change.
