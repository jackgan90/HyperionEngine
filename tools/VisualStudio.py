"""Generate/build Hyperion solutions without relying on a developer shell or global CMake."""
import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
YEARS = {17: "2022", 18: "2026"}


def version_tuple(value):
    return tuple(int(part) for part in value.split("."))


def capture(arguments, environment):
    return subprocess.check_output(
        list(map(str, arguments)), env=environment, cwd=ROOT,
        encoding="utf-8", errors="replace", stderr=subprocess.STDOUT,
    )


def run(arguments, environment):
    # Argument lists avoid shell interpretation of paths, spaces and CMake semicolon lists.
    print("> " + subprocess.list2cmdline(list(map(str, arguments))), flush=True)
    subprocess.run(list(map(str, arguments)), cwd=ROOT, env=environment, check=True)


def installations(environment):
    program_files = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = pathlib.Path(program_files) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not vswhere.is_file():
        raise RuntimeError("Visual Studio Installer/vswhere was not found. Install Visual Studio with C++ tools.")
    output = capture([vswhere, "-all", "-products", "*", "-requires",
                      "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-format", "json", "-utf8"], environment)
    found = []
    for item in json.loads(output):
        major = version_tuple(item["installationVersion"])[0]
        if major not in YEARS or not item.get("isComplete"):
            continue
        directory = pathlib.Path(item["installationPath"])
        msbuild = directory / "MSBuild/Current/Bin/MSBuild.exe"
        if msbuild.is_file():
            found.append({**item, "year": YEARS[major], "major": major, "directory": directory,
                          "msbuild": msbuild, "ide": directory / "Common7/IDE/devenv.exe"})
    if not found:
        raise RuntimeError("Visual Studio 2022/2026 with x64 C++ build tools is required.")
    return found


def select_installation(found, requested):
    candidates = [item for item in found if requested == "auto" or item["year"] == requested]
    if not candidates:
        raise RuntimeError(f"Visual Studio {requested} with C++ tools was not found.")
    # Prefer the user's usable IDE over a newer Build Tools-only installation.
    return max(candidates, key=lambda item: (item["ide"].is_file(), version_tuple(item["installationVersion"])))


def select_cmake(found, generator, environment):
    candidates = [item["directory"] / "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
                  for item in sorted(found, key=lambda item: version_tuple(item["installationVersion"]), reverse=True)]
    system_cmake = shutil.which("cmake")
    if system_cmake:
        candidates.append(pathlib.Path(system_cmake))
    for candidate in dict.fromkeys(candidates):
        if not candidate.is_file():
            continue
        try:
            capabilities = json.loads(capture([candidate, "-E", "capabilities"], environment))
        except (subprocess.CalledProcessError, json.JSONDecodeError):
            continue
        version = capabilities["version"]
        if (version["major"], version["minor"]) >= (3, 28) and any(
                entry["name"] == generator for entry in capabilities["generators"]):
            return candidate
    raise RuntimeError(f"CMake 3.28+ supporting '{generator}' was not found. Install the VS CMake tools component.")


def prepare_dependencies(environment, renderdoc=False):
    lock = json.loads((ROOT / "dependencies.lock.json").read_text(encoding="utf-8"))
    pending = []
    for name, package in lock.items():
        if package.get("optional") and not renderdoc:
            continue
        marker = ROOT / "out/deps" / name / ".hyperion-sha256"
        if not marker.is_file() or marker.read_text(encoding="utf-8").strip() != package["sha256"]:
            pending.append(name)
    if pending:
        run([sys.executable, ROOT / "tools/Bootstrap.py", "--only", *pending], environment)
    else:
        print("Dependencies: all required locked packages are available.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vs-version", choices=("auto", "2022", "2026"), default="auto")
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Debug")
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--test", action="store_true")
    parser.add_argument("--open", action="store_true")
    parser.add_argument("--fresh", action="store_true")
    parser.add_argument("--renderdoc", action=argparse.BooleanOptionalAction, default=None,
                        help="Enable or disable the optional capture plugin; otherwise preserve the CMake cache")
    parser.add_argument("--tracy", action=argparse.BooleanOptionalAction, default=None,
                        help="Enable or disable Tracy network profiling; otherwise preserve the CMake cache (default OFF)")
    args = parser.parse_args()
    if os.name != "nt" or sys.version_info < (3, 10):
        raise RuntimeError("This script requires Windows and Python 3.10+.")

    # Some launchers supply PATH and Path simultaneously; .NET MSBuild rejects that
    # environment. Normalize keys only for child processes, never the system/user PATH.
    environment = {key.upper(): value for key, value in os.environ.items()}
    found = installations(environment)
    selected = select_installation(found, args.vs_version)
    generator = f"Visual Studio {selected['major']} {selected['year']}"
    cmake = select_cmake(found, generator, environment)
    directory = ROOT / "out/build" / f"vs{selected['year']}"
    solution = directory / "Hyperion.sln"
    print(f"Visual Studio: {selected['displayName']}\nCMake: {cmake}", flush=True)
    cache = directory / "CMakeCache.txt"
    renderdoc = args.renderdoc
    if renderdoc is None:
        renderdoc = cache.is_file() and bool(re.search(r"^HYP_ENABLE_RENDERDOC:BOOL=(ON|TRUE|YES|Y|1)$",
                                                      cache.read_text(encoding="utf-8"), re.MULTILINE | re.IGNORECASE))
    prepare_dependencies(environment, renderdoc)
    configure = [cmake, "-S", ROOT, "-B", directory, "-G", generator, "-A", "x64",
                 "-DCMAKE_GENERATOR_INSTANCE=" + str(selected["directory"]),
                 "-DCMAKE_MAKE_PROGRAM=" + str(selected["msbuild"]),
                 "-DCMAKE_CONFIGURATION_TYPES=Debug;Release", "-DBUILD_TESTING=ON",
                 "-DPython3_EXECUTABLE=" + sys.executable]
    configure.append("-DHYP_ENABLE_RENDERDOC=" + ("ON" if renderdoc else "OFF"))
    if args.tracy is not None:
        configure.append("-DHYP_ENABLE_TRACY=" + ("ON" if args.tracy else "OFF"))
    if args.fresh:
        configure.append("--fresh")
    run(configure, environment)
    if not solution.is_file():
        raise RuntimeError(f"CMake did not produce the expected solution: {solution}")
    if args.build or args.test:
        command = [cmake, "--build", directory, "--config", args.configuration, "--parallel", "8"]
        if args.test:
            command += ["--target", "hyperion_check"]
        run(command, environment)
    print(f"\nSolution: {solution}\nOpen in Visual Studio, select Debug/Release | x64, then press F5.\n"
          "To run all tests: right-click hyperion_check under Hyperion/Tests and choose Build.", flush=True)
    if args.open:
        if not selected["ide"].is_file():
            raise RuntimeError("The selected installation contains Build Tools only. Open the solution in a compatible IDE.")
        subprocess.Popen([str(selected["ide"]), str(solution)], cwd=ROOT, env=environment,
                         creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP,
                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         close_fds=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print(f"Visual Studio workflow: {error}", file=sys.stderr)
        raise SystemExit(1)
