"""Download locked upstream archives into this workspace; no system installs."""
import argparse
import hashlib
import json
import pathlib
import shutil
import urllib.request
import urllib.error
import zipfile
import time

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

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--only', nargs='*', choices=[*REPOS, 'renderdoc'])
    parser.add_argument('--renderdoc', action='store_true', help='Also fetch the optional RenderDoc API header')
    args = parser.parse_args()
    lockfile = ROOT / 'dependencies.lock.json'
    lock = json.loads(lockfile.read_text()) if lockfile.exists() else {}
    cache = ROOT / 'out/downloads'
    cache.mkdir(parents=True, exist_ok=True)
    for name in args.only or [*REPOS, *(['renderdoc'] if args.renderdoc else [])]:
        entry = lock.get(name) or resolve(name)
        suffix = '.h' if entry.get('file') else '.zip'
        archive = cache / (name + '-' + entry.get('commit', entry['version']) + suffix)
        if not archive.exists():
            print(f'Downloading {name} {entry["version"]}', flush=True)
            archive.write_bytes(request(entry['url']))
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if entry.get('sha256') and digest != entry['sha256']:
            raise RuntimeError(f'Checksum mismatch: {archive}')
        entry['sha256'] = digest
        lock[name] = entry
        lockfile.write_text(json.dumps(lock, indent=2) + '\n')
        destination = (ROOT / 'out/deps' / name).resolve()
        marker = destination / '.hyperion-sha256'
        if marker.exists() and marker.read_text() == digest:
            print(f'Cached {name}', flush=True)
            continue
        if destination.exists() and any(destination.iterdir()):
            raise RuntimeError(f'Refusing to overwrite a different dependency tree: {destination}')
        destination.mkdir(parents=True, exist_ok=True)
        if entry.get('file'):
            target = (destination / entry['file']).resolve()
            if not target.is_relative_to(destination):
                raise RuntimeError('Unsafe dependency file path')
            shutil.copyfile(archive, target)
            marker.write_text(digest)
            print(f'Ready {name} {digest[:12]}', flush=True)
            continue
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                parts = pathlib.PurePosixPath(member.filename).parts
                parts = parts if entry.get('flat') else parts[1:]
                if not parts:
                    continue
                target = destination.joinpath(*parts).resolve()
                if not target.is_relative_to(destination):
                    raise RuntimeError('Unsafe archive path')
                if member.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with source.open(member) as src, target.open('wb') as dst:
                        shutil.copyfileobj(src, dst)
        marker.write_text(digest)
        print(f'Ready {name} {digest[:12]}', flush=True)

if __name__ == '__main__':
    main()
