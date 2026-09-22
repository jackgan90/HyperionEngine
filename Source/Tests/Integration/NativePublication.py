"""Exercise native import identity, texture reuse and renamed dependency CLI paths."""
import hashlib
import json
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib


def png(path, rgba):
    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data)))

    path.write_bytes(b'\x89PNG\r\n\x1a\n'
                     + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(b'\0' + bytes(rgba)))
                     + chunk(b'IEND', b''))


class PublicationChecks:
    def __init__(self, tool, fixtures, root):
        self.tool = tool
        self.fixtures = fixtures
        self.root = root

    def run(self, *args, error=None):
        result = subprocess.run([str(self.tool), *map(str, args)], capture_output=True,
                                text=True, timeout=60)
        text = result.stdout + result.stderr
        print(text, end='', flush=True)
        if error is None:
            assert result.returncode == 0, text
        else:
            assert result.returncode != 0 and error in text, text
        return text

    def prepare(self, name, both=False, reverse=False):
        case = self.root / name
        case.mkdir()
        doc = json.loads((self.fixtures / 'Showcase.gltf').read_text())
        doc['images'] = [{'uri': 'A.png'}]
        doc['textures'] = [{'source': 0, 'sampler': 0}]
        if reverse:
            del doc['materials'][0]['pbrMetallicRoughness']['baseColorTexture']
            doc['materials'][1]['pbrMetallicRoughness']['baseColorTexture'] = {'index': 0}
        if both:
            self.add_texture(doc, 'B.png', 1)
            png(case / 'B.png', [255, 0, 0, 255])
        png(case / 'A.png', [255, 0, 0, 255])
        shutil.copyfile(self.fixtures / 'Showcase.bin', case / 'Showcase.bin')
        (case / 'Model.gltf').write_text(json.dumps(doc))
        self.publish(case)
        return case, doc

    @staticmethod
    def add_texture(doc, name, material):
        index = len(doc['textures'])
        doc['textures'].append({'source': len(doc['images']), 'sampler': 0})
        doc['images'].append({'uri': name})
        doc['materials'][material]['pbrMetallicRoughness']['baseColorTexture'] = {'index': index}

    def publish(self, case, error=None):
        return self.run('import', case / 'Model.gltf', case / 'native/Model.hasset', error=error)

    @staticmethod
    def snapshot(case):
        return {str(path.relative_to(case)): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in case.rglob('*.hasset')}

    def read(self, path):
        output = self.root / 'Envelope.json'
        self.run('export-envelope', path, output)
        return json.loads(output.read_text())['fields']

    def texture(self, case, slot):
        model = self.read(case / 'native/Model.hasset')
        material_path = case / 'native' / model['materialSlots'][slot]['fields']['path']
        material = self.read(material_path)
        value = next(value['fields']['value']['fields'] for value in material['values']
                     if value['fields']['name'] == 'BaseColorTexture')
        ref = value['texture']['fields']
        texture = self.read(material_path.parent / ref['path'])
        return ref['id'], texture['mips'][0]['fields']['bytes']['data']

    def check_divergence(self):
        case, _ = self.prepare('divergence', both=True)
        first = self.texture(case, 0)
        assert self.texture(case, 1) == first
        before = self.snapshot(case)
        png(case / 'A.png', [0, 0, 255, 255])
        self.publish(case, error='Conflicting products claim one native asset ID')
        assert self.snapshot(case) == before, 'Rejected publication changed native files'
        png(case / 'B.png', [0, 0, 255, 255])
        self.publish(case)
        assert self.texture(case, 0) == (first[0], [0, 0, 255, 255])
        assert self.texture(case, 1) == self.texture(case, 0)
        assert 'written_assets=0' in self.publish(case)

    def check_stale_candidate(self):
        case, doc = self.prepare('stale')
        self.add_texture(doc, 'C.png', 1)
        png(case / 'A.png', [0, 0, 255, 255])
        png(case / 'C.png', [255, 0, 0, 255])
        (case / 'Model.gltf').write_text(json.dumps(doc))
        self.publish(case)
        blue = self.texture(case, 0)
        red = self.texture(case, 1)
        assert blue[1] == [0, 0, 255, 255] and red[1] == [255, 0, 0, 255]
        assert blue[0] != red[0]
        assert 'written_assets=0' in self.publish(case)

    def check_reuse_before_update(self):
        case, doc = self.prepare('reuse-before-update', reverse=True)
        before = self.snapshot(case)
        self.add_texture(doc, 'C.png', 0)
        png(case / 'A.png', [0, 0, 255, 255])
        png(case / 'C.png', [255, 0, 0, 255])
        (case / 'Model.gltf').write_text(json.dumps(doc))
        self.publish(case, error='Conflicting products claim one native asset ID')
        assert self.snapshot(case) == before

    def check_rename(self):
        case, _ = self.prepare('rename')
        original = self.read(case / 'native/Model.hasset')
        material = case / 'native' / original['materialSlots'][0]['fields']['path']
        material.rename(material.with_name('ReadableMaterial.hasset'))
        before = self.snapshot(case)
        self.run('validate', case / 'native/Model.hasset')
        self.run('inspect', case / 'native/Model.hasset')
        assert 'written_assets=0' in self.publish(case)
        assert self.snapshot(case) == before
        exported = case / 'native/Model.json'
        self.run('export-json', case / 'native/Model.hasset', exported)
        refs = json.loads(exported.read_text())['fields']['materialSlots']
        assert refs[0]['fields']['path'] == 'Materials/ReadableMaterial.hasset'
        for ref in original['materialSlots']:
            ref['fields']['path'] = '/Game/' + ref['fields']['path']
        source = case / 'External.json'
        source.write_text(json.dumps({'type': 'hyperion.modelasset', 'version': 3, 'fields': original}))
        mounts = case / 'Mounts.json'
        mounts.write_text(json.dumps({'version': 1, 'mounts': [
            {'root': '/Game', 'directory': str(case / 'native'), 'read_only': False}]}))
        self.run('--mounts', mounts, 'validate', '/Game/Model.hasset')
        self.run('--mounts', mounts, 'import', source, '/Game/External.hasset', '--library', '/Game')
        self.run('--mounts', mounts, 'validate', '/Game/External.hasset')
        assert 'written_assets=0' in self.run('--mounts', mounts, 'import', source,
                                             '/Game/External.hasset', '--library', '/Game')
        renamed = material.with_name('ReadableMaterial.hasset')
        for name in ('RenamedAgain.hasset', 'RenamedTwice.hasset'):
            destination = renamed.with_name(name)
            renamed.rename(destination)
            renamed = destination
            before = self.snapshot(case)
            self.run('--mounts', mounts, 'validate', '/Game/External.hasset')
            assert 'written_assets=0' in self.run('--mounts', mounts, 'import', source,
                                                 '/Game/External.hasset', '--library', '/Game')
            assert self.snapshot(case) == before
        texture = next((case / 'native/Textures').glob('A_png-*.hasset'))
        texture_source = case / 'EditedTexture.json'
        self.run('export-json', texture, texture_source)
        edited = json.loads(texture_source.read_text())
        edited['fields']['mips'][0]['fields']['bytes']['data'] = [0, 255, 0, 255]
        texture_source.write_text(json.dumps(edited))
        self.run('--mounts', mounts, 'import', texture_source,
                 '/Game/Textures/' + texture.name, '--library', '/Game')
        assert 'written_assets=1' in self.run('--mounts', mounts, 'import', source,
                                             '/Game/External.hasset', '--library', '/Game')
        assert 'written_assets=0' in self.run('--mounts', mounts, 'import', source,
                                             '/Game/External.hasset', '--library', '/Game')


def main():
    tool = pathlib.Path(sys.argv[1]).resolve()
    fixtures = pathlib.Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix='native-publication-', dir=pathlib.Path.cwd()) as directory:
        checks = PublicationChecks(tool, fixtures, pathlib.Path(directory))
        checks.run('catalog', directory, pathlib.Path(directory) / 'Catalog.hasset', error='Unknown command')
        assert not (pathlib.Path(directory) / 'Catalog.hasset').exists()
        checks.check_divergence()
        checks.check_stale_candidate()
        checks.check_reuse_before_update()
        checks.check_rename()
    print('Native publication conflicts, staged texture reuse and renamed dependency tools passed')


if __name__ == '__main__':
    main()
