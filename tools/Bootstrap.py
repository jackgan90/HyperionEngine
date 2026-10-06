"""Restore locked dependency sources into an independent developer cache."""
import argparse
import hashlib
import json
import pathlib
import shutil
import urllib.request
import urllib.error
import zipfile
import time
import os
import tempfile
from contextlib import contextmanager
from DevelopmentPaths import dependency_root, tool_cache_root

ROOT = pathlib.Path(__file__).resolve().parents[1]
REPOS = {
    'spdlog': 'gabime/spdlog', 'mimalloc': 'microsoft/mimalloc',
    'tracy': 'wolfpld/tracy', 'onetbb': 'uxlfoundation/oneTBB',
    'sdl': 'libsdl-org/SDL', 'json': 'nlohmann/json',
    'glm': 'g-truc/glm', 'cgltf': 'jkuhlmann/cgltf', 'stb': 'nothings/stb',
    'tinyexr': 'syoyo/tinyexr', 'spirv-cross': 'KhronosGroup/SPIRV-Cross',
    'd3d12ma': 'GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator',
    'imgui': 'ocornut/imgui', 'implot': 'epezent/implot',
    'dxc': 'microsoft/DirectXShaderCompiler',
}

def request(url):
    last = None
    for attempt in range(3):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'Hyperion-bootstrap'}), timeout=120) as response:
                return response.read()
        except (urllib.error.URLError, TimeoutError) as error:
            last = error
            if isinstance(error, urllib.error.HTTPError) and error.code == 404:
                raise
            time.sleep(attempt + 1)
    raise last

def api(path):
    return json.loads(request('https://api.github.com/repos/' + path))

def resolve(name):
    repo = REPOS[name]
    if name in ('stb', 'spirv-cross'):
        version = 'HEAD'
    else:
        try:
            release = api(repo + '/releases/latest')
            version = release['tag_name']
        except urllib.error.HTTPError as error:
            if error.code != 404:
                raise
            version = 'HEAD'
    if name == 'dxc':
        asset = next(a for a in release['assets'] if a['name'].startswith('dxc_') and a['name'].endswith('.zip'))
        return {'repository': repo, 'version': version, 'url': asset['browser_download_url'], 'flat': True}
    commit = api(repo + '/commits/' + urllib.parse.quote(version, safe=''))['sha']
    return {'repository': repo, 'version': version, 'commit': commit, 'url': f'https://codeload.github.com/{repo}/zip/{commit}'}

@contextmanager
def publication_lock(path):
    import msvcrt
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as stream:
        stream.seek(0)
        if not stream.read(1):
            stream.write(b"0")
            stream.flush()
        stream.seek(0)
        try:
            msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError as error:
            raise RuntimeError(f"Dependency cache is busy: {path}") from error
        try:
            yield
        finally:
            stream.seek(0)
            msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)


def prepare_package(name, entry, cache, destination, legacy_root, offline):
    expected = entry["sha256"]
    marker = destination / ".hyperion-sha256"
    if marker.is_file() and marker.read_text().strip() == expected:
        print(f"Cached {name}", flush=True)
        return
    suffix = '.h' if entry.get('file') else '.zip'
    filename = name + '-' + entry.get('commit', entry['version']) + suffix
    archive = cache / filename
    if not archive.exists():
        previous = legacy_root / 'downloads' / filename if legacy_root else None
        if previous and previous.is_file():
            data = previous.read_bytes()
        elif offline:
            raise RuntimeError(f"Missing locked archive for offline restore: {archive}")
        else:
            print(f'Downloading {name} {entry["version"]}', flush=True)
            data = request(entry['url'])
        if hashlib.sha256(data).hexdigest() != expected:
            raise RuntimeError(f'Checksum mismatch: {previous or archive}')
        with tempfile.NamedTemporaryFile(dir=cache, delete=False) as stream:
            temporary = pathlib.Path(stream.name)
            stream.write(data)
        try:
            os.replace(temporary, archive)
        finally:
            temporary.unlink(missing_ok=True)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        raise RuntimeError(f'Checksum mismatch: {archive}')
    if destination.exists():
        raise RuntimeError(f'Refusing to overwrite a different dependency tree: {destination}')
    staging = pathlib.Path(tempfile.mkdtemp(prefix=name + '-prepare-', dir=destination.parent)).resolve()
    try:
        if entry.get('file'):
            target = (staging / entry['file']).resolve()
            if not target.is_relative_to(staging):
                raise RuntimeError('Unsafe dependency file path')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(archive, target)
        else:
            with zipfile.ZipFile(archive) as source:
                for member in source.infolist():
                    parts = pathlib.PurePosixPath(member.filename).parts
                    parts = parts if entry.get('flat') else parts[1:]
                    if not parts:
                        continue
                    target = staging.joinpath(*parts).resolve()
                    if not target.is_relative_to(staging):
                        raise RuntimeError('Unsafe archive path')
                    if member.is_dir():
                        target.mkdir(parents=True, exist_ok=True)
                    else:
                        target.parent.mkdir(parents=True, exist_ok=True)
                        with source.open(member) as src, target.open('wb') as dst:
                            shutil.copyfileobj(src, dst)
        (staging / '.hyperion-sha256').write_text(expected)
        staging.rename(destination)
    finally:
        # Only this invocation's fresh staging tree is disposable; never remove source/destination trees.
        if staging.exists() and staging.parent == destination.parent.resolve() and staging.name.startswith(name + '-prepare-'):
            shutil.rmtree(staging)
    print(f'Ready {name} {expected[:12]}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--only', nargs='*', choices=[*REPOS, 'renderdoc'])
    parser.add_argument('--renderdoc', action='store_true')
    parser.add_argument('--cache-root', type=pathlib.Path, help='Override HYP_TOOL_CACHE for this invocation')
    parser.add_argument('--legacy-root', type=pathlib.Path, help='Explicit old out directory; only verified archives are imported')
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--print-deps', action='store_true')
    args = parser.parse_args()
    if args.cache_root:
        os.environ['HYP_TOOL_CACHE'] = str(args.cache_root.resolve())
    lock = json.loads((ROOT / 'dependencies.lock.json').read_text(encoding='utf-8'))
    dependencies = dependency_root(lock)
    if args.print_deps:
        print(dependencies.as_posix())
        return
    cache = tool_cache_root() / 'Downloads'
    cache.mkdir(parents=True, exist_ok=True)
    dependencies.mkdir(parents=True, exist_ok=True)
    with publication_lock(dependencies / '.bootstrap-lock'):
        for name in args.only or [*REPOS, *(['renderdoc'] if args.renderdoc else [])]:
            entry = lock[name]
            if not entry.get('sha256'):
                raise RuntimeError(f'Missing locked checksum for {name}')
            prepare_package(name, entry, cache, dependencies / name,
                            args.legacy_root.resolve() if args.legacy_root else None, args.offline)


if __name__ == '__main__':
    main()
