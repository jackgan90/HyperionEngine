"""Report advisory C++ file sizes and validate reviewed scan exclusions."""

import json
from pathlib import Path


FILE_LINE_RECOMMENDATION = 1000
CPP_SUFFIXES = {'.cpp', '.h', '.inl'}
POLICY_PATH = Path('tools/SourceSizePolicy.json')


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'duplicate policy key: {key}')
        result[key] = value
    return result


def require_fields(value, fields, location):
    if not isinstance(value, dict) or set(value) != set(fields):
        raise ValueError(f'{location}: expected fields {", ".join(fields)}')


def require_text(value, location):
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f'{location}: expected nonempty text')


def load_policy(root, sources):
    try:
        policy = json.loads((root / POLICY_PATH).read_text(encoding='utf-8'),
                            object_pairs_hook=unique_object)
        require_fields(policy, ('version', 'exclusions'), POLICY_PATH)
        if type(policy['version']) is not int or policy['version'] != 2:
            raise ValueError('unsupported policy version')
        entries = policy['exclusions']
        if not isinstance(entries, dict):
            raise ValueError('exclusions: expected an exact-path object')
        for name, entry in entries.items():
            if name not in sources:
                raise ValueError(f'{name}: policy path must match an existing Source C++ file exactly; '
                                 'remove or update stale entries after deletion/rename')
            require_fields(entry, ('kind', 'reason', 'evidence'), name)
            if entry['kind'] not in ('test', 'generated', 'third_party'):
                raise ValueError(f'{name}: unsupported exclusion kind')
            require_text(entry['reason'], f'{name}: reason')
            require_text(entry['evidence'], f'{name}: evidence')
        return policy
    except ValueError as error:
        raise RuntimeError(f'{POLICY_PATH.as_posix()}: {error}') from error


def source_files(root):
    directory = root / 'Source'
    if not directory.is_dir():
        raise RuntimeError(f'{directory}: owned Source directory is missing')
    result = {}
    for path in sorted(directory.rglob('*')):
        if not path.is_file() or path.suffix not in CPP_SUFFIXES:
            continue
        if not path.resolve().is_relative_to(directory.resolve()):
            raise RuntimeError(f'{path}: source resolves outside the owned Source directory')
        result[path.relative_to(root).as_posix()] = path
    return result


def physical_line_count(path):
    data = path.read_bytes()
    return data.count(b'\n') + int(bool(data) and not data.endswith(b'\n'))


def check_source_sizes(root):
    root = Path(root).resolve()
    sources = source_files(root)
    policy = load_policy(root, sources)
    advisories = []
    for name, path in sources.items():
        if name in policy['exclusions']:
            continue
        lines = physical_line_count(path)
        if lines > FILE_LINE_RECOMMENDATION:
            advisories.append(f'WARN: {name}: {lines} lines exceed the recommended '
                              f'{FILE_LINE_RECOMMENDATION}-line size; split at logical boundaries '
                              'unless review confirms extremely cohesive logic is difficult to decompose')
    for advisory in advisories:
        print(advisory, flush=True)
    print(f'PASS: C++ file size scan ({len(sources)} files, '
          f'{len(policy["exclusions"])} reviewed exclusions, '
          f'{len(advisories)} advisory findings; file sizes do not cause failure)', flush=True)
