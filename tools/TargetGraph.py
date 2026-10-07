"""Collect direct target contracts from a successful expanded CMake configure."""

import argparse
import hashlib
import json
import pathlib
import re
import subprocess


ROOT = pathlib.Path(__file__).resolve().parents[1]
SUFFIXES = {'.cpp', '.h', '.hpp', '.inl'}


def graph_inputs(root):
    paths = {root / 'CMakeLists.txt', root / 'CMakePresets.json'}
    paths.update((root / 'Source').rglob('CMakeLists.txt'))
    paths.update((root / 'cmake').rglob('*.cmake'))
    paths.update((root / 'Source').rglob('*.cmake'))
    paths.add(root / 'tools/TargetGraph.py')
    return {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(paths) if path.is_file()}


def source_inventory(root):
    return sorted(path.relative_to(root).as_posix() for path in (root / 'Source').rglob('*')
                  if path.suffix in SUFFIXES)


def location(root, entry):
    return f'{pathlib.Path(entry["file"]).relative_to(root).as_posix()}:{entry["line"]}'


def expanded_items(args):
    return [item for arg in args for item in arg.split(';') if item]


def references_owned(args, targets):
    return any(token in targets or token.startswith('hyperion_')
               for arg in args for token in re.findall(r'[A-Za-z_][A-Za-z0-9_-]*', arg))


def collect(root, trace):
    root = root.resolve()
    allowed = {str((root / path).resolve()).casefold() for path in graph_inputs(root)
               if path.endswith(('.cmake', 'CMakeLists.txt'))}
    entries = [entry for line in trace.read_text(encoding='utf-8').splitlines()
               if 'cmd' in (entry := json.loads(line)) and
               str(pathlib.Path(entry['file']).resolve()).casefold() in allowed]
    targets = {}
    callers = {}
    for entry in entries:
        command = entry['cmd'].lower()
        args = expanded_items(entry['args'])
        depth = entry.get('global_frame', 1)
        caller = callers.get(depth - 1)
        callers = {level: value for level, value in callers.items() if level < depth}
        callers[depth] = entry
        entry['known_test_scope'] = (pathlib.Path(entry['file']).resolve() == root / 'Source/Tests/CMakeLists.txt'
                                     and caller and caller['cmd'] == 'hyp_test'
                                     and pathlib.Path(caller['file']).resolve() == root / 'Source/Tests/CMakeLists.txt')
        if command not in ('add_library', 'add_executable') or not args:
            continue
        directory = pathlib.Path(entry['file']).resolve().parent
        if not directory.is_relative_to(root / 'Source'):
            # Only Dependencies.cmake declares third-party wrapper/imported targets.
            if pathlib.Path(entry['file']).resolve() != root / 'cmake/Dependencies.cmake' or args[0].startswith('hyperion_'):
                raise ValueError(f'{location(root, entry)}: unsupported target declaration outside Source: {args[0]}')
            continue
        if pathlib.Path(entry['file']).name != 'CMakeLists.txt' or (entry.get('frame', 1) != 1 and not entry['known_test_scope']):
            raise ValueError(f'{location(root, entry)}: unsupported helper target declaration; current source directory is ambiguous')
        if 'ALIAS' in args or 'IMPORTED' in args:
            raise ValueError(f'{location(root, entry)}: unsupported owned alias/imported target')
        test = directory.is_relative_to(root / 'Source/Tests') or (
            command == 'add_executable' and not directory.is_relative_to(root / 'Source/Applications'))
        targets[args[0]] = {'directory': directory.relative_to(root).as_posix(),
                            'test': test, 'location': location(root, entry),
                            'sources': [], 'links': []}
    for entry in entries:
        command = entry['cmd'].lower()
        args = expanded_items(entry['args'])
        if not args:
            continue
        if command == 'link_libraries':
            raise ValueError(f'{location(root, entry)}: unsupported inherited directory links')
        if command in ('set_property', 'set_target_properties'):
            graph_properties = any(item in ('SOURCES', 'INTERFACE_SOURCES') or
                                   ('LINK' in item and 'LIBRARIES' in item) for item in args)
            if graph_properties and (references_owned(args, targets) or 'DIRECTORY' in args):
                raise ValueError(f'{location(root, entry)}: unsupported graph property mutation')
        if command == 'target_link_libraries':
            if args[0] == 'hyperion_image_data' and any(
                    item not in ('PUBLIC', 'PRIVATE', 'INTERFACE') for item in args[1:]):
                raise ValueError(f'{location(root, entry)}: ImageData must be independent; links {args[1:]}')
            if args[0] not in targets and references_owned(args[1:], targets):
                raise ValueError(f'{location(root, entry)}: non-owned target {args[0]} links owned targets: {args[1:]}')
        if args[0] not in targets:
            continue
        target = targets[args[0]]
        if command in ('add_library', 'add_executable', 'target_sources'):
            if command == 'target_sources' and any(item in ('PUBLIC', 'INTERFACE') for item in args[1:]):
                raise ValueError(f'{location(root, entry)}: unsupported PUBLIC/INTERFACE source propagation')
            add_sources(root, target, entry, args[1:])
        elif command == 'target_link_libraries':
            add_links(root, targets, target, entry, args[1:])
    return targets


def add_sources(root, target, entry, args):
    keywords = {'STATIC', 'SHARED', 'MODULE', 'OBJECT', 'INTERFACE', 'WIN32',
                'MACOSX_BUNDLE', 'EXCLUDE_FROM_ALL', 'PUBLIC', 'PRIVATE'}
    for item in args:
        if item in keywords:
            continue
        if '$<' in item or item in ('FILE_SET', 'TYPE', 'BASE_DIRS', 'FILES'):
            raise ValueError(f'{location(root, entry)}: unsupported source expression: {item}')
        source = pathlib.Path(item)
        if not source.is_absolute():
            if entry.get('frame', 1) != 1 and not entry.get('known_test_scope'):
                raise ValueError(f'{location(root, entry)}: unsupported helper relative source: {item}')
            origin = pathlib.Path(entry['file']).resolve().parent
            owner = root / target['directory']
            if origin != owner:
                raise ValueError(f'{location(root, entry)}: ambiguous relative source: {item}')
            source = owner / source
        source = source.resolve()
        if source.is_relative_to(root) and source.suffix in SUFFIXES:
            name = source.relative_to(root).as_posix()
            if name not in target['sources']:
                target['sources'].append(name)


def add_links(root, targets, target, entry, args):
    visibility = None
    for item in args:
        if item in ('PUBLIC', 'PRIVATE', 'INTERFACE'):
            visibility = item
            continue
        if '$<' in item:
            raise ValueError(f'{location(root, entry)}: unsupported direct link expression: {item}')
        if item in ('debug', 'optimized', 'general', 'LINK_PUBLIC', 'LINK_PRIVATE'):
            raise ValueError(f'{location(root, entry)}: unsupported link selection: {item}')
        if visibility is None:
            raise ValueError(f'{location(root, entry)}: explicit link visibility required')
        if item.startswith('hyperion_') and item not in targets:
            raise ValueError(f'{location(root, entry)}: unknown owned link target {item}')
        if item in targets:
            target['links'].append({'target': item, 'visibility': visibility,
                                    'location': location(root, entry)})


def cache_options(build):
    result = {}
    for line in (build / 'CMakeCache.txt').read_text(encoding='utf-8').splitlines():
        if '=' in line and not line.startswith(('#', '//')):
            key, value = line.split('=', 1)
            name = key.split(':', 1)[0]
            if name.startswith('HYP_ENABLE_') or name in ('BUILD_TESTING', 'CMAKE_BUILD_TYPE',
                                                        'CMAKE_CONFIGURATION_TYPES', 'CMAKE_GENERATOR'):
                result[name] = value
    return result


def export_graph(root, build, trace):
    graph = {'version': 1, 'root': str(root.resolve()), 'build': str(build.resolve()),
             'options': cache_options(build), 'inputs': graph_inputs(root),
             'inventory': source_inventory(root), 'targets': collect(root, trace),
             'cache_hash': hashlib.sha256((build / 'CMakeCache.txt').read_bytes()).hexdigest()}
    (build / 'TargetGraph.json').write_text(json.dumps(graph, indent=2) + '\n', encoding='utf-8')


def load_graph(root, build):
    path = build / 'TargetGraph.json'
    if not path.is_file():
        raise ValueError(f'{path}: configured graph missing; configure with Build.ps1 or GenerateSolution.ps1')
    graph = json.loads(path.read_text(encoding='utf-8'))
    if (graph.get('version') != 1 or pathlib.Path(graph['root']).resolve() != root.resolve() or
            pathlib.Path(graph['build']).resolve() != build.resolve() or
            graph['inputs'] != graph_inputs(root) or graph['inventory'] != source_inventory(root) or
            graph['cache_hash'] != hashlib.sha256((build / 'CMakeCache.txt').read_bytes()).hexdigest()):
        raise ValueError(f'{path}: stale or incompatible graph; rerun the supported configure entry point')
    return graph


def configure(cmake, root, build, arguments):
    build.mkdir(parents=True, exist_ok=True)
    # A failed configure/export must never leave a previous graph usable.
    (build / 'TargetGraph.json').unlink(missing_ok=True)
    trace = build / 'TargetTrace.jsonl'
    trace_args = ['--trace-expand', '--trace-format=json-v1', f'--trace-redirect={trace}']
    # CMake accepts a semicolon list; one option avoids a banner for every file.
    trace_sources = [str(root / name) for name in graph_inputs(root)
                     if name.endswith(('.cmake', 'CMakeLists.txt'))]
    trace_args.append('--trace-source=' + ';'.join(trace_sources))
    subprocess.run([str(cmake), *trace_args, *arguments], cwd=root, check=True)
    export_graph(root, build, trace)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cmake', required=True)
    parser.add_argument('--build-dir', type=pathlib.Path, required=True)
    parser.add_argument('arguments', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    arguments = args.arguments[1:] if args.arguments[:1] == ['--'] else args.arguments
    configure(args.cmake, ROOT, args.build_dir.resolve(), arguments)


if __name__ == '__main__':
    main()
