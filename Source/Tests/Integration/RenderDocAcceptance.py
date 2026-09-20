"""Real optional D3D12 captures, widget input, XML inspection and GPU replay."""
import os
import json
import pathlib
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from NativeContent import set_initial_view_from_camera
from RdcValidation import verify_capture

viewer = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(sys.argv[2]).resolve()
runtime = pathlib.Path(os.environ.get('HYP_RENDERDOC_RUNTIME', r'C:\Program Files\RenderDoc'))
library = runtime / 'renderdoc.dll'
command = runtime / 'renderdoccmd.exe'
if not library.is_file() or not command.is_file():
    print('SKIP: install RenderDoc or set HYP_RENDERDOC_RUNTIME to its directory')
    raise SystemExit(77)

work = pathlib.Path.cwd() / 'renderdoc-acceptance' / str(time.time_ns())
work.mkdir(parents=True)


def run(name, arguments, expect_success=True):
    result = subprocess.run(list(map(str, arguments)), cwd=root, capture_output=True,
                            encoding='utf-8', errors='replace', timeout=90)
    text = result.stdout + result.stderr
    (work / (name + '.log')).write_text(text, encoding='utf-8')
    assert (result.returncode == 0) == expect_success, text
    return text

def capture(experiment, frame_count, capture_frames):
    output = work / (experiment + ' RDC 空格')
    # Keep the actual capture widget inside the panel at the default 125% scale.
    # The 720px sample window clips it below the scroll region.
    settings = json.loads((root / ('experiments/' + experiment + '.json')).read_text())
    settings['properties']['height'] = 1080
    config = work / f'{experiment}.json'
    config.write_text(json.dumps(settings), encoding='utf-8')
    arguments = [viewer, '--config', config,
                 '--frames', frame_count, '--hidden', '--renderdoc-library', library,
                 '--rdc-output', output, '--exercise-rdc-ui']
    if experiment == 'Shadows':
        # Keep all four cascades covered by the authored fixture camera.
        scene = set_initial_view_from_camera(viewer, '/Game/Scenes/Shadows.hasset', work / 'Shadows.hasset',
                                             mounts=root / 'ContentMounts.json')
        arguments += ['--scene', scene]
    for frame in capture_frames:
        arguments += ['--capture-rdc', frame]
    text = run(experiment, arguments)
    assert 'validation errors: 0' in text, text
    captures = [pathlib.Path(path.strip()) for path in re.findall(r'RenderDoc capture saved: (.+)', text)]
    assert len(captures) == len(capture_frames) and len(set(captures)) == len(captures), text
    assert set(captures) == set(output.glob('*.rdc')), text
    for index, path in enumerate(captures):
        assert path.is_file() and path.stat().st_size > 1024
        run(f'{experiment}-{index}-replay', [command, 'replay', '--loops', '2', path])
        xml = work / f'{experiment}-{index}.xml'
        run(f'{experiment}-{index}-xml', [command, 'convert', '--filename', path,
                                        '--output', xml, '--convert-format', 'xml'])
        draws = verify_capture(ET.parse(xml), experiment)
        print(f'{experiment} capture {index}: replayed; submitted draws {dict(draws)}')


capture('Triangle', 24, [8, 16])
capture('Model', 900, [800])
capture('Shadows', 900, [800])

missing = work / 'missing' / 'renderdoc.dll'
text = run('missing-runtime', [viewer, '--frames', '8', '--hidden', '--renderdoc-library', missing])
assert 'RenderDoc unavailable:' in text and 'Rendering lifecycle completed successfully' in text, text
text = run('missing-runtime-capture', [viewer, '--frames', '8', '--hidden', '--renderdoc-library', missing,
                                       '--capture-rdc', '4'], expect_success=False)
assert 'Requested RDC capture was not produced' in text and 'RenderDoc capture saved:' not in text

blocker = work / 'OutputIsAFile'
blocker.write_text('preserve me', encoding='utf-8')
text = run('bad-output', [viewer, '--frames', '8', '--hidden', '--renderdoc-library', library,
                           '--capture-rdc', '4', '--rdc-output', blocker / 'child'], expect_success=False)
assert 'RenderDoc capture saved:' not in text and 'Requested RDC capture was not produced' in text
assert blocker.read_text(encoding='utf-8') == 'preserve me'

disabled = work / 'disabled'
text = run('disabled', [viewer, '--frames', '8', '--hidden', '--rdc-output', disabled])
assert 'RenderDoc: Ready' not in text and not disabled.exists(), text
print('RenderDoc acceptance passed. Evidence: ' + str(work))
