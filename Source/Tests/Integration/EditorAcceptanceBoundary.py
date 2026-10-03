"""Guard the private Editor production/acceptance source and build boundary."""

import json
from pathlib import Path
import re
import sys


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def check_sources(private):
    scenario_headers = {"EditorAcceptanceHarness.h", "EditorAcceptanceState.h"}
    production = [
        path for path in private.glob("Editor*.cpp")
        if "Acceptance" not in path.name and "Tests" not in path.name
    ]
    production.extend(private / name for name in (
        "EditorApplication.h", "EditorAcceptanceDriver.h", "EditorOptions.h",
        "EditorAcceptanceUnavailable.cpp", "EditorAcceptanceReport.cpp"))
    for path in production:
        source = path.read_text(encoding="utf-8")
        if path.name != "EditorOptions.cpp":
            case_option = r"\b(?:\w+\.)?(?:Options|InOptions|Result)\.(?:bExercise|Exercise)"
            require(not re.search(case_option, source),
                    f"Production case branch in {path.name}")
        snapshot = r"\b(?:InspectionBounds|MultiSelectionRows|OutlineExerciseObjects)\b"
        require(not re.search(snapshot, source),
                f"Scenario snapshot leaked into {path.name}")
        require("light-courtyard-3" not in source and "/Sponza.hasset" not in source,
                f"Scenario fixture leaked into {path.name}")
        pending = [path]
        visited = set()
        while pending:
            header = pending.pop()
            if header in visited:
                continue
            visited.add(header)
            require(header.name not in scenario_headers,
                    f"{path.name} includes scenario state through {header.name}")
            header_source = header.read_text(encoding="utf-8")
            for include in re.findall(r'^#include "([^"]+)"', header_source, re.M):
                candidate = header.parent / include
                if candidate.is_file():
                    pending.append(candidate)
    for path in private.glob("Editor*Acceptance*.cpp"):
        source = path.read_text(encoding="utf-8")
        require(not re.search(r"FEditorPlugin::(?:Exercise|Prepare|Check)", source),
                f"Scenario method still belongs to Editor in {path.name}")


def check_selection(private, selected, enabled, origin):
    common = {"EditorAcceptanceReport.cpp"}
    disabled = {"EditorAcceptanceUnavailable.cpp"}
    for path in private.glob("Editor*Acceptance*.cpp"):
        if path.name in common:
            expected = True
        elif path.name in disabled:
            expected = not enabled
        else:
            expected = enabled
        require((path.resolve() in selected) == expected,
                f"Wrong BUILD_TESTING selection for {path.name} in {origin}")


def check_build(private, build, source_list):
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    testing = re.search(r"^BUILD_TESTING:BOOL=(ON|OFF)$", cache, re.M)
    require(testing is not None, "BUILD_TESTING selection was not found")
    enabled = testing[1] == "ON"
    sources = source_list.read_text(encoding="utf-8").splitlines()
    selected = {(private.parent / path).resolve() for path in sources if path}
    check_selection(private, selected, enabled, source_list)
    database = build / "compile_commands.json"
    if database.is_file():
        commands = json.loads(database.read_text(encoding="utf-8"))
        compiled = {Path(entry["file"]).resolve() for entry in commands}
        check_selection(private, compiled, enabled, database)
    print(f"Editor acceptance boundary passed (BUILD_TESTING={'ON' if enabled else 'OFF'})")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[3]
    private = root / "Source/Plugins/Editor/Private"
    check_sources(private)
    check_build(private, Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
