"""Compare both outline policies in the actual Editor viewport and GUI composition."""
import json
import pathlib
import struct
import subprocess
import sys
import tempfile


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(sys.argv[2]).resolve()
    parent = root / 'out' / 'editor-tests'
    parent.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(prefix='outlines-', dir=parent))
    report = output / 'Report.json'
    result = subprocess.run(
        [str(executable), '--hidden', '--exercise-outlines', str(output),
         '--layout', str(output / 'Layout.ini'), '--report', str(report)],
        cwd=root, capture_output=True, text=True, timeout=100)
    (output / 'Editor.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    assert result.returncode == 0, (result.stdout, result.stderr)
    data = json.loads(report.read_text())
    assert data['outlines_verified'] and not data['document_dirty'], data
    assert not data['scene_error'] and data['validation_errors'] == 0, data
    for name in ('Union', 'PerObject', 'UnionAgain', 'Occluded', 'Smooth', 'Cleared'):
        image = (output / f'{name}.png').read_bytes()
        assert image[:8] == b'\x89PNG\r\n\x1a\n', name
        assert struct.unpack('>II', image[16:24]) == (1600, 960), name
        assert len(image) > 20_000, name
    assert (output / 'Union.png').read_bytes() != (output / 'PerObject.png').read_bytes()
    unavailable = subprocess.run(
        [str(executable), '--disable-plugin', 'editor', '--exercise-outlines', str(output)],
        cwd=root, capture_output=True, text=True, timeout=30)
    assert unavailable.returncode != 0
    assert 'editor plugin did not start' in unavailable.stdout + unavailable.stderr
    print(f'PASS: Editor outlines, mode switching, occlusion, quality, clearing and shutdown: {output}')


if __name__ == '__main__':
    main()
