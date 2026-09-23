"""Exercise object placement through actual Editor GUI events, including missing resources."""
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(sys.argv[2]).resolve()
    parent = root / 'out' / 'editor-tests'
    parent.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(prefix='placement-', dir=parent))

    def run(name, *arguments):
        report = output / f'{name}.json'
        result = subprocess.run(
            [str(executable), '--hidden', '--layout', str(output / f'{name}.ini'),
             '--ui-preferences', str(output / f'{name}-scale.ini'),
             '--report', str(report), *arguments], cwd=root, capture_output=True, text=True, timeout=100)
        (output / f'{name}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        assert result.returncode == 0, (name, result.stdout, result.stderr)
        return json.loads(report.read_text())

    report = run('placement',
                 '--exercise-placement', str(output / 'Placed.hasset'),
                 '--capture', str(output / 'Placed.png'))
    assert report['placement_verified'] and not report['document_dirty'], report
    assert report['nodes'] == 9 and report['draws'] == 5 and report['save_ms'] > 0, report
    assert not report['scene_error'] and report['failed_models'] == 0, report
    assert report['validation_errors'] == 0 and report['placement_unavailable'] == 0, report
    for name in ('Cube', 'Sphere', 'Cylinder', 'Cone', 'Plane', 'DirectionalLight', 'PointLight', 'SpotLight'):
        assert (output / f'{name}Preview.png').stat().st_size > 30_000, name

    engine = output / 'MissingResources'
    shutil.copytree(root / 'Content', engine,
                    ignore=shutil.ignore_patterns('Cube.hasset', 'PointLight.hasset'))
    game = output / 'Game'
    game.mkdir()
    failed = run('missing-resources', '--engine-content', str(engine), '--asset-root', str(game), '--frames', '120')
    assert failed['placement_unavailable'] == 2, failed
    assert failed['nodes'] == 0 and not failed['document_dirty'] and not failed['scene_error'], failed
    assert failed['validation_errors'] == 0, failed
    print(f'PASS: eight placeables, previews, cancellation, history, save/reload and isolated resource failure: {output}')


if __name__ == '__main__':
    main()
