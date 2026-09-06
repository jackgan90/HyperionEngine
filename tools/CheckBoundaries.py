"""Third-party headers belong only to private adapter translation units."""
import pathlib
import re
ROOT = pathlib.Path(__file__).resolve().parents[1]
VENDORS = re.compile(r'(?:spdlog/|mimalloc|tracy/|oneapi/|tbb/|SDL3/|nlohmann/|glm/|cgltf|stb_|tinyexr|spirv_|spirv_cross|dxcapi|D3D12MemAlloc|imgui|implot|d3d12\.h|dxgi\d*\.h|wrl/)')
bad = []
count = 0
for folder in ('include', 'src', 'apps', 'plugins', 'tests'):
    for path in (ROOT / folder).rglob('*'):
        if path.suffix not in ('.h', '.hpp', '.cpp'):
            continue
        count += 1
        if path.is_relative_to(ROOT / 'src/adapters'):
            continue
        for line, text in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            if re.match(r'\s*#\s*include', text) and VENDORS.search(text):
                bad.append(f'{path.relative_to(ROOT)}:{line}: {text}')
if bad:
    raise SystemExit('\n'.join(bad))
print(f'PASS: {count} source files respect dependency boundaries')
