"""Check owned source filenames, include casing, formatting, and optional C++ naming."""
import argparse
import concurrent.futures
import json
import os
import pathlib
import re
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE_DIRS = ('Source', 'shaders')
SOURCE_SUFFIXES = ('.h', '.cpp', '.hlsl', '.hlsli')


def owned_files():
    return sorted(path for folder in SOURCE_DIRS for path in (ROOT / folder).rglob('*')
                  if path.is_file() and path.suffix in SOURCE_SUFFIXES)


def check_paths(sources):
    failures = []
    files = sources + [path for folder in ('tools', 'cmake', 'docs', 'experiments')
                       for path in (ROOT / folder).glob('*') if path.is_file()]
    # Discover legacy extensions too, so a newly added lowercase .hpp is not skipped.
    files += [path for folder in SOURCE_DIRS for path in (ROOT / folder).rglob('*.hpp')]
    for path in files:
        if not re.fullmatch(r'[A-Z][A-Za-z0-9]*', path.stem) or path.suffix == '.hpp':
            failures.append(f'{path.relative_to(ROOT)}: expected PascalCase filename (.h for headers)')
    headers = {path.relative_to(public).as_posix(): path
               for public in (ROOT / 'Source').rglob('Public')
               for path in public.rglob('*.h')}
    lookup = {name.lower(): name for name in headers}
    for path in sources:
        for line, include in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            match = re.match(r'\s*#\s*include\s*[<"]([^>"]+)[>"]', include)
            if not match:
                continue
            name = match[1]
            if name.lower().startswith('hyperion/'):
                if name not in headers:
                    failures.append(f'{path.relative_to(ROOT)}:{line}: invalid include {name}; '
                                    f'expected {lookup.get(name.lower(), "an existing engine header")}')
            elif '"' in include and '/' not in name:
                neighbors = {item.name for item in path.parent.iterdir() if item.is_file()}
                if name not in neighbors:
                    failures.append(f'{path.relative_to(ROOT)}:{line}: missing or incorrectly cased include {name}')
    if failures:
        raise RuntimeError('\n'.join(failures))
    print(f'PASS: owned filenames and include casing ({len(sources)} source files)', flush=True)


def llvm_tool(name):
    found = shutil.which(name)
    candidate = pathlib.Path(os.environ.get('ProgramFiles', r'C:\Program Files')) / 'LLVM/bin' / (name + '.exe')
    if found:
        return found
    if candidate.is_file():
        return str(candidate)
    raise RuntimeError(f'{name} is required; install LLVM 22.1.1 and add its bin directory to PATH.')


def check_naming(build_dir, sources):
    database = build_dir / 'compile_commands.json'
    if not database.is_file():
        raise RuntimeError(f'{database} is missing. Run tools/Build.ps1 to configure a Ninja build first.')
    entries = {pathlib.Path(item['file']).resolve() for item in json.loads(database.read_text(encoding='utf-8'))}
    units = [path for path in sources if path.suffix == '.cpp']
    missing = [str(path.relative_to(ROOT)) for path in units if path.resolve() not in entries]
    if missing:
        raise RuntimeError('Compile database is incomplete or stale: ' + ', '.join(missing))
    tool = llvm_tool('clang-tidy')

    def check(path):
        result = subprocess.run([tool, str(path), '-p', str(build_dir)], cwd=ROOT,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, encoding='utf-8', errors='replace')
        if result.returncode:
            return f'{path.relative_to(ROOT)}:\n{result.stdout}'
        return None

    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        failures = [result for result in pool.map(check, units) if result]
    if failures:
        raise RuntimeError('\n'.join(failures))
    print(f'PASS: semantic C++ naming and local declarations ({len(units)} translation units)', flush=True)


def check_formatting(sources, apply):
    tool = llvm_tool('clang-format')

    def check(path):
        original = path.read_bytes()
        formatted = subprocess.check_output([tool, '--style=file', str(path)], cwd=ROOT)
        # LLVM 22 can report no-op SeparateDefinitionBlocks replacements with
        # --dry-run. Compare the actual formatted bytes to avoid false positives.
        if formatted != original:
            if apply:
                path.write_bytes(formatted)
            else:
                return f'{path.relative_to(ROOT)}: formatting differs; run python tools/CheckStyle.py --format'
        return None

    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        failures = [result for result in pool.map(check, sources) if result]
    if failures:
        raise RuntimeError('\n'.join(failures))
    print('Formatted owned C++ and HLSL sources.' if apply else 'PASS: source formatting', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--format', action='store_true', help='Apply clang-format to owned C++ and HLSL files')
    mode.add_argument('--paths-only', action='store_true', help='Check filenames/includes without LLVM')
    mode.add_argument('--naming', action='store_true', help='Also run clang-tidy with a Ninja compile database')
    parser.add_argument('--build-dir', type=pathlib.Path, default=ROOT / 'out/build/debug')
    args = parser.parse_args()
    sources = owned_files()
    check_paths(sources)
    if args.paths_only:
        return
    check_formatting(sources, args.format)
    if args.naming:
        check_naming(args.build_dir.resolve(), sources)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error)) from error
