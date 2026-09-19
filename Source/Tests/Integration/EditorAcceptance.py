"""Exercise the actual editor menu, viewport input, scene reopening and GPU composition."""
import json
import pathlib
import struct
import subprocess
import sys
import tempfile


def run_editor(executable, root, output, name, *arguments):
    report = output / f'{name}.json'
    capture = output / f'{name}.png'
    command = [str(executable), '--hidden', '--layout', str(output / f'{name}.ini'),
               '--ui-preferences', str(output / f'{name}-scale.ini'),
               '--report', str(report), '--capture', str(capture), *arguments]
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=100)
    (output / f'{name}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{name}: {result.stdout}\n{result.stderr}')
    return json.loads(report.read_text()), capture


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(sys.argv[2]).resolve()
    parent = root / 'out' / 'editor-tests'
    parent.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(prefix='acceptance-', dir=parent))
    report, capture = run_editor(executable, root, output, 'sponza', '--exercise',
                                 '--scene', '/Game/Scenes/DoesNotExist.hasset')
    assert report['scene'] == '/Game/Scenes/Sponza.hasset', report
    assert report['open_count'] >= 3 and report['ready_frames'] > 0, report
    assert report['nodes'] > 0 and report['draws'] > 0, report
    assert report['validation_errors'] == 0 and report['failed_models'] == 0, report
    assert report['load_error_observed'] and not report['scene_error'], report
    assert all(report[name] for name in ('movement', 'movement_gate', 'right_release', 'look',
                                         'dolly', 'wheel_speed', 'input_isolation')), report
    assert report['movement_speed'] > 0, report
    assert report['exercise_step'] == 21, report
    with capture.open('rb') as stream:
        header = stream.read(24)
    assert header[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', header[16:24]) == (1440, 900)
    assert capture.stat().st_size > 100_000, 'Missing scene image'
    run_editor(executable, root, output, 'close-during-load', '--frames', '2',
               '--scene', '/Game/Scenes/Sponza.hasset')

    document, _ = run_editor(executable, root, output, 'document',
                            '--scene', '/Game/Scenes/Sponza.hasset',
                            '--exercise-document', str(output / 'Edited.hasset'))
    assert document['document_verified'] and not document['document_dirty'], document
    assert document['save_ms'] > 0 and document['validation_errors'] == 0, document
    views, _ = run_editor(executable, root, output, 'views',
                           '--scene', '/Game/Scenes/Sponza.hasset',
                           '--exercise-views', str(output / 'Views.hasset'))
    assert views['views_verified'] and not views['document_dirty'], views
    assert views['validation_errors'] == 0 and not views['scene_error'], views
    gizmo, _ = run_editor(executable, root, output, 'gizmo', '--exercise-gizmo',
                          '--scene', '/Game/Scenes/Sponza.hasset')
    assert gizmo['gizmo_verified'] and gizmo['validation_errors'] == 0, gizmo
    report, _ = run_editor(executable, root, output, 'missing-scene', '--frames', '20',
                            '--scene', '/Game/Scenes/DoesNotExist.hasset')
    assert report['scene_error'] and report['validation_errors'] == 0, report
    print(f'PASS: menu scene open, RMB-gated movement, fixed-eye look, wheel speed/dolly, modal isolation, resize/reopen and error recovery: {output}')


if __name__ == '__main__':
    main()
