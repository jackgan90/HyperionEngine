"""Check physical C++ file sizes against reviewed exclusions and legacy debt."""

import json
from pathlib import Path


FILE_LINE_LIMIT = 500
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
        require_fields(policy, ('version', 'legacy_files', 'exclusions'), POLICY_PATH)
        if type(policy['version']) is not int or policy['version'] != 1:
            raise ValueError('unsupported policy version')
        for group in ('legacy_files', 'exclusions'):
            entries = policy[group]
            if not isinstance(entries, dict):
                raise ValueError(f'{group}: expected an exact-path object')
            for name, entry in entries.items():
                if name not in sources:
                    raise ValueError(f'{name}: policy path must match an existing Source C++ file exactly; '
                                     'remove or update stale entries after deletion/rename')
                if group == 'legacy_files':
                    require_fields(entry, ('lines', 'split'), name)
                    if type(entry['lines']) is not int or entry['lines'] <= FILE_LINE_LIMIT:
                        raise ValueError(f'{name}: legacy lines must be an integer greater than {FILE_LINE_LIMIT}')
                    require_text(entry['split'], f'{name}: split')
                else:
                    require_fields(entry, ('kind', 'reason', 'evidence'), name)
                    if entry['kind'] not in ('test', 'generated', 'third_party'):
                        raise ValueError(f'{name}: unsupported exclusion kind')
                    require_text(entry['reason'], f'{name}: reason')
                    require_text(entry['evidence'], f'{name}: evidence')
        overlap = policy['legacy_files'].keys() & policy['exclusions'].keys()
        if overlap:
            raise ValueError(f'files cannot have both legacy debt and exclusions: {", ".join(sorted(overlap))}')
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
    failures = []
    for name, path in sources.items():
        if name in policy['exclusions']:
            continue
        lines = physical_line_count(path)
        legacy = policy['legacy_files'].get(name)
        if legacy:
            ceiling = legacy['lines']
            if lines <= FILE_LINE_LIMIT:
                failures.append(f'{name}: {lines} lines now meet the limit; remove the legacy entry')
            elif lines < ceiling:
                failures.append(f'{name}: reduced to {lines} lines; lower the legacy count from {ceiling}')
            elif lines > ceiling:
                failures.append(f'{name}: {lines} lines exceed legacy ceiling {ceiling}; '
                                f'do not raise the ceiling. Follow-up: {legacy["split"]}')
        elif lines > FILE_LINE_LIMIT:
            failures.append(f'{name}: {lines} lines exceed the {FILE_LINE_LIMIT}-line limit; '
                            'split by responsibility (test exclusions require purpose/build review)')
    if failures:
        raise RuntimeError('\n'.join(failures))
    print(f'PASS: C++ file sizes ({len(sources)} files, '
          f'{len(policy["exclusions"])} reviewed exclusions, '
          f'{len(policy["legacy_files"])} bounded legacy files)', flush=True)
