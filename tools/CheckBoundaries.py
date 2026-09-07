"""Check wrapper isolation, module visibility and engine dependency direction."""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Source'
VENDORS = re.compile(r'(?:renderdoc_app|spdlog/|mimalloc|tracy/|oneapi/|tbb/|SDL3/|nlohmann/|glm/|cgltf|stb_|tinyexr|spirv_|spirv_cross|dxcapi|D3D12MemAlloc|imgui|implot|d3d12(?:sdklayers|shader)?\.h|dxgi\d*\.h|wrl/|Windows\.h)', re.I)
modules = {}
headers = {}
bad = []
for cmake in SOURCE.rglob('CMakeLists.txt'):
    if cmake.parent == SOURCE / 'Tests':
        continue
    body = cmake.read_text(encoding='utf-8')
    target = re.search(r'add_(?:library|executable)\((hyperion_\w+)', body)[1]
    dependencies = set(re.findall(r'\bhyperion_\w+', body)) - {target}
    modules[cmake.parent] = (target, dependencies)
    for path in (cmake.parent / 'Public').rglob('*.h'):
        name = path.relative_to(cmake.parent / 'Public').as_posix()
        if name in headers:
            bad.append(f'Duplicate public include: {name}')
        headers[name] = (cmake.parent, target)

count = 0
for path in SOURCE.rglob('*'):
    if path.suffix not in ('.h', '.hpp', '.cpp'):
        continue
    count += 1
    test = path.is_relative_to(SOURCE / 'Tests')
    owner = next((folder for folder in modules if path.is_relative_to(folder)), None)
    if not test and owner is None:
        bad.append(f'{path.relative_to(ROOT)}: source has no module CMakeLists.txt')
        continue
    adapter = owner and (path.is_relative_to(owner / 'Private/Adapters') or
                         (owner.is_relative_to(SOURCE / 'Backends') and path.is_relative_to(owner / 'Private')))
    for line, text in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        include = re.match(r'\s*#\s*include\s*[<"]([^>"]+)[>"]', text)
        if not include:
            continue
        name = include[1]
        prefix = f'{path.relative_to(ROOT)}:{line}'
        if VENDORS.search(name) and not adapter:
            bad.append(f'{prefix}: native/vendor include outside private wrapper: {name}')
        if name.startswith('Hyperion/'):
            if name not in headers:
                bad.append(f'{prefix}: unknown public header {name}')
                continue
            dependency, target = headers[name]
            if test:
                continue
            if dependency != owner and target not in modules[owner][1]:
                bad.append(f'{prefix}: missing direct CMake dependency on {target}')
            if dependency.is_relative_to(SOURCE / 'Backends') and not owner.is_relative_to(SOURCE / 'Applications') and dependency != owner:
                bad.append(f'{prefix}: backend providers may only be selected by applications')
            if owner.is_relative_to(SOURCE / 'Runtime') and dependency.is_relative_to(SOURCE / 'Plugins'):
                bad.append(f'{prefix}: Runtime must not depend on experiment plugins')
            if owner.name in ('Core', 'Math', 'Reflection', 'Tasks', 'Plugins', 'Config', 'Platform', 'Assets', 'Materials', 'Scene', 'Animation') and dependency.name in ('RHI', 'Renderer'):
                bad.append(f'{prefix}: data/foundation module must not depend on rendering')
        elif '"' in text:
            resolved = (path.parent / name).resolve()
            support = (SOURCE / 'Tests' / name).resolve()
            if test and support.is_file() and support.is_relative_to(SOURCE / 'Tests'):
                continue
            if not owner or not resolved.is_relative_to(owner / 'Private') or not resolved.is_file() or path.is_relative_to(owner / 'Public'):
                bad.append(f'{prefix}: private header crosses a module boundary: {name}')

targets = {target: (folder, dependencies) for folder, (target, dependencies) in modules.items()}
for target in ('hyperion_scene', 'hyperion_materials'):
    pending = [target]
    visited = set()
    while pending:
        dependency = pending.pop()
        if dependency in visited or dependency not in targets:
            continue
        visited.add(dependency)
        folder, dependencies = targets[dependency]
        if folder.name in ('RHI', 'Renderer') or folder.is_relative_to(SOURCE / 'Backends'):
            bad.append(f'{target}: transitive rendering dependency on {dependency}')
        pending.extend(dependencies)
if 'hyperion_materials' in targets:
    unexpected = targets['hyperion_materials'][1] - {'hyperion_core', 'hyperion_math'}
    if unexpected:
        bad.append(f'Materials may depend only on Core/Math: {sorted(unexpected)}')

if bad:
    raise SystemExit('\n'.join(bad))
print(f'PASS: {count} source files, {len(modules)} modules respect dependency boundaries')
