"""Check configured direct contracts and source/private/vendor isolation."""

import argparse
import pathlib
import re

from TargetGraph import ROOT, SUFFIXES, load_graph


VENDORS = re.compile(r'(?:renderdoc_app|spdlog/|mimalloc|tracy/|oneapi/|tbb/|SDL3/|nlohmann/|glm/|cgltf|stb_|tinyexr|spirv_|spirv_cross|dxcapi|D3D12MemAlloc|imgui|implot|d3d12(?:sdklayers|shader)?\.h|dxgi\d*\.h|wrl/|Windows\.h)', re.I)
CPU_DOMAINS = {'Core', 'Math', 'Reflection', 'Tasks', 'Plugins', 'Config', 'RasterOptions',
               'Platform', 'IO', 'Serialization', 'AssetTypes', 'Assets', 'AssetEditing',
               'Automation', 'Transport', 'Content', 'Materials', 'Textures', 'Scene',
               'SceneEditing', 'Environment', 'Animation', 'AssetImport'}


def edge_text(source, edge):
    return f'{source} --{edge["visibility"]}--> {edge["target"]} ({edge["location"]})'


def cycle_errors(targets):
    errors = []
    visited = set()
    active = []
    edges = []

    def visit(name):
        if name in active:
            errors.append('Production dependency cycle: ' + ' / '.join(edges[active.index(name):]))
            return
        if name in visited or name not in targets:
            return
        active.append(name)
        for edge in targets[name]['links']:
            edges.append(edge_text(name, edge))
            visit(edge['target'])
            edges.pop()
        active.pop()
        visited.add(name)

    for name in targets:
        visit(name)
    return errors


def graph_errors(root, targets):
    errors = []
    production = {name: target for name, target in targets.items() if not target['test']}
    for name, target in production.items():
        directory = root / target['directory']
        for edge in target['links']:
            other = targets[edge['target']]
            other_directory = root / other['directory']
            if other['test']:
                errors.append(f'{edge_text(name, edge)}: production links a test target')
            if directory.is_relative_to(root / 'Source/Runtime') and other_directory.is_relative_to(root / 'Source/Plugins'):
                errors.append(f'{edge_text(name, edge)}: Runtime depends on a concrete plugin')
            if (other_directory.is_relative_to(root / 'Source/Backends') and directory != other_directory
                    and not directory.is_relative_to(root / 'Source/Applications')):
                errors.append(f'{edge_text(name, edge)}: backend providers are selected by applications')
        if directory.is_relative_to(root / 'Source/Runtime') and directory.name in CPU_DOMAINS:
            pending = [(name, [])]
            visited = set()
            while pending:
                current, chain = pending.pop()
                if current in visited or current not in production:
                    continue
                visited.add(current)
                other = root / production[current]['directory']
                if other.is_relative_to(root / 'Source/Backends') or other.name in ('Renderer', 'RHI'):
                    errors.append(f'{name}: CPU domain reaches rendering: ' + ' / '.join(chain))
                    continue
                pending.extend((edge['target'], [*chain, edge_text(current, edge)])
                               for edge in production[current]['links'])
    errors.extend(cycle_errors(production))
    if 'hyperion_materials' in production:
        allowed = {'hyperion_core', 'hyperion_math', 'hyperion_reflection', 'hyperion_asset_types', 'hyperion_textures'}
        errors.extend(f'{edge_text("hyperion_materials", edge)}: unexpected Materials data dependency'
                      for edge in production['hyperion_materials']['links'] if edge['target'] not in allowed)
    if 'hyperion_raster_options' in production:
        errors.extend(f'{edge_text("hyperion_raster_options", edge)}: RasterOptions must be independent'
                      for edge in production['hyperion_raster_options']['links'])
    return errors


def direct_error(target_name, target, dependencies, prefix, public):
    if target_name in dependencies:
        return None
    links = [edge for edge in target['links'] if edge['target'] in dependencies]
    if not links:
        return f'{prefix}: {target_name} missing direct dependency on {sorted(dependencies)}'
    if public and not any(edge['visibility'] in ('PUBLIC', 'INTERFACE') for edge in links):
        return f'{prefix}: public include requires PUBLIC/INTERFACE: ' + ' / '.join(edge_text(target_name, edge) for edge in links)
    if not public and all(edge['visibility'] == 'INTERFACE' for edge in links):
        return f'{prefix}: implementation requires PUBLIC/PRIVATE: ' + ' / '.join(edge_text(target_name, edge) for edge in links)
    return None


def condition_value(expression, options):
    expression = expression.strip()
    if expression in ('0', '1'):
        return expression == '1'
    match = re.fullmatch(r'(!\s*)?(HYP_ENABLE_[A-Z0-9_]+)', expression)
    if match and options.get(match[2]) in ('ON', 'OFF'):
        value = options[match[2]] == 'ON'
        return not value if match[1] else value
    return None


def include_lines(path, options):
    # Only configured boolean guards can establish an inactive dependency.
    # Unknown expressions conservatively inspect both branches; isolation always scans every include.
    stack = []
    active = True
    guard_options = dict(options)
    original = path.read_text(encoding='utf-8')
    tokens = re.compile(r'R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=delimiter)"'
                        r'|"(?:\\[\s\S]|[^"\\])*"|(?<!\w)(?:u8|u|U|L)?\'(?:\\[\s\S]|[^\'\\])*\''
                        r'|//(?:\\\r?\n|[^\r\n])*|/\*[\s\S]*?\*/')

    def mask(match, keep_strings=False):
        token = match[0]
        if keep_strings and token.startswith(('"', "'")):
            return token
        return re.sub(r'[^\r\n]', ' ', token)

    lexical = tokens.sub(mask, original).splitlines()
    includes = tokens.sub(lambda match: mask(match, True), original).splitlines()
    for line, (code, text) in enumerate(zip(lexical, includes), 1):
        mutation = re.match(r'\s*#\s*(?:define|undef)\s+(HYP_ENABLE_[A-Z0-9_]+)\b', code)
        if mutation and active:
            guard_options.pop(mutation[1], None)
        directive = re.match(r'\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)', code)
        if directive:
            command, expression = directive.groups()
            if command in ('if', 'ifdef', 'ifndef'):
                value = condition_value(expression, guard_options) if command == 'if' else None
                stack.append((active, value))
                active = active and value is not False
            elif stack and command in ('elif', 'else'):
                parent, previous = stack[-1]
                value = condition_value(expression, guard_options) if command == 'elif' else True
                active = parent and previous is not True and value is not False
                combined = True if previous is True or value is True else (
                    False if previous is False and value is False else None)
                stack[-1] = (parent, combined)
            elif stack and command == 'endif':
                active, _ = stack.pop()
        include = re.match(r'\s*#\s*include\s*[<"]([^>"]+)[>"]', text)
        if include and re.match(r'\s*#\s*include\b', code):
            yield line, text, include[1], active


def check(root, targets, options=None):
    options = options or {}
    source = root / 'Source'
    modules = sorted(path.parent for path in source.rglob('CMakeLists.txt') if path.parent != source / 'Tests')
    headers = {}
    errors = graph_errors(root, targets)
    for module in modules:
        for path in (module / 'Public').rglob('*'):
            if path.suffix not in ('.h', '.inl', '.hpp'):
                continue
            name = path.relative_to(module / 'Public').as_posix()
            if name in headers:
                errors.append(f'{path.relative_to(root)}: duplicate public include {name}')
            headers[name] = module
    module_targets = {}
    compiled = {}
    for name, target in targets.items():
        if not target['test']:
            module_targets.setdefault(root / target['directory'], set()).add(name)
        for path in target['sources']:
            if pathlib.Path(path).suffix == '.cpp':
                compiled.setdefault(root / path, set()).add(name)
    count = 0
    uncompiled = []
    for path in source.rglob('*'):
        if path.suffix not in SUFFIXES:
            continue
        count += 1
        owner = next((module for module in reversed(modules) if path.is_relative_to(module)), None)
        assignments = compiled.get(path, set())
        test = path.is_relative_to(source / 'Tests') or (assignments and all(targets[name]['test'] for name in assignments))
        if not owner and not test:
            errors.append(f'{path.relative_to(root)}: source has no module owner')
            continue
        if path.suffix == '.cpp' and not assignments:
            uncompiled.append(path.relative_to(root).as_posix())
        consumers = {name for name in assignments if not targets[name]['test']}
        if path.suffix != '.cpp' and not test:
            consumers = module_targets.get(owner, set())
        adapter = (owner and (path.is_relative_to(owner / 'Private/Adapters') or
                               (owner.is_relative_to(source / 'Backends') and path.is_relative_to(owner / 'Private'))))
        adapter = adapter or (test and path.is_relative_to(source / 'Tests/Private/Adapters'))
        for line, text, name, active in include_lines(path, options):
            prefix = f'{path.relative_to(root).as_posix()}:{line}'
            if VENDORS.search(name) and not adapter:
                errors.append(f'{prefix}: native/vendor include outside private wrapper: {name}')
            if name.startswith('Hyperion/'):
                if name not in headers:
                    errors.append(f'{prefix}: unknown public header {name}')
                    continue
                dependency = headers[name]
                if active and consumers and dependency not in module_targets:
                    errors.append(f'{prefix}: included module has no configured production target: {name}')
                for consumer in consumers if active else ():
                    error = direct_error(consumer, targets[consumer], module_targets.get(dependency, set()),
                                         prefix, path.is_relative_to(owner / 'Public'))
                    if error:
                        errors.append(error)
            elif '"' in text:
                resolved = (path.parent / name).resolve()
                support = (source / 'Tests' / name).resolve()
                source_only = path.suffix == '.cpp' and not assignments
                if (test or source_only) and support.is_file() and support.is_relative_to(source / 'Tests'):
                    continue
                if (not owner or not resolved.is_relative_to(owner / 'Private') or not resolved.is_file()
                        or path.is_relative_to(owner / 'Public')):
                    errors.append(f'{prefix}: private header crosses a module boundary: {name}')
    omitted = [module.relative_to(root).as_posix() for module in modules if module not in module_targets]
    return errors, count, omitted, uncompiled


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=pathlib.Path, default=ROOT / 'out/build/debug')
    args = parser.parse_args()
    try:
        graph = load_graph(ROOT, args.build_dir.resolve())
        errors, count, omitted, uncompiled = check(ROOT, graph['targets'], graph['options'])
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit(str(error)) from error
    print('Configured graph coverage: ' + ', '.join(f'{key}={value}' for key, value in graph['options'].items()))
    print('Unselected modules (source isolation only): ' + (', '.join(omitted) or 'none'))
    print('Uncompiled sources (source isolation only): ' + (', '.join(uncompiled) or 'none'))
    if errors:
        raise SystemExit('\n'.join(errors))
    production = sum(not target['test'] for target in graph['targets'].values())
    print(f'PASS: {count} sources; {production} production and {len(graph["targets"]) - production} test targets')


if __name__ == '__main__':
    main()
