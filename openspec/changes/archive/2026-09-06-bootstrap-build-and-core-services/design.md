## Context

The repository has no application code. MSVC 14.50, Windows SDKs, and Visual Studio's bundled CMake/Ninja are available. The global PATH also contains older tools, so builds must select tools explicitly.

## Goals / Non-Goals

**Goals:** A C++20 build, private dependency adapters, tagged aligned allocation, logs and CPU profiling with verifiable behavior.

**Non-Goals:** Graphics, task scheduling, global interception of all CRT allocations, or installing tools into system directories.

## Decisions

- Use a checked-in JSON dependency lock with immutable GitHub commits and SHA256 archives. A Python bootstrap downloads sources to ignored `out/deps`; CMake consumes local sources without network side effects.
- Build engine modules as separate static targets with PUBLIC engine headers and PRIVATE third-party dependencies. The oneTBB runtime will be shared when introduced because that is its supported integration model.
- Provide narrow engine-owned APIs in `include/hyperion`; all vendor calls live under `src/adapters`. A source-boundary check is run through CTest.
- Use mimalloc's explicit aligned API with per-allocation size/tag metadata and thread-safe counters. Engine-owned PMR resources route selected allocations through this path; allocations internal to unrelated runtimes are not claimed to be intercepted.
- Expose log and profiling wrappers without third-party types. Retain application-visible timing independently of an attached Tracy client.

## Risks / Trade-offs

- Network outages → cache archives locally, verify their hashes, and use fixed lock data for subsequent runs.
- Global tool conflicts → detect Visual Studio and invoke its CMake/Ninja in a developer environment.
- Wrapper overhead → use static interfaces and batch coarse operations; do not mirror unused vendor APIs.

## Migration Plan

Introduce the foundation in an empty repository and validate a clean Debug and Release build. Rollback consists of reverting project files; system configuration is untouched.
