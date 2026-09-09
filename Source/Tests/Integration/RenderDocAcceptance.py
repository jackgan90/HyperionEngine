"""Real optional D3D12 captures, widget input, XML inspection and GPU replay."""
from collections import Counter
import os
import pathlib
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

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


def verify_capture(document, experiment):
    # Recorders interleave chunks: track marker scopes per command list, then count
    # draws only when that list is submitted. GUI draws cannot stand in for a scene.
    markers = {}
    recorded = {}
    submitted = Counter()
    chunks = []
    for node in document.iter('chunk'):
        name = node.get('name', '')
        chunks.append(name)
        command_list = node.findtext("ResourceId[@name='pCommandList']", '')
        if name == 'ID3D12GraphicsCommandList::Reset':
            markers[command_list] = []
            recorded[command_list] = Counter()
        elif name == 'ID3D12GraphicsCommandList::BeginEvent':
            markers.setdefault(command_list, []).append(node.findtext("string[@name='MarkerText']", ''))
        elif name == 'ID3D12GraphicsCommandList::EndEvent':
            if markers.get(command_list):
                markers[command_list].pop()
        elif name == 'ID3D12GraphicsCommandList::DrawIndexedInstanced':
            indices = int(node.findtext("uint[@name='IndexCountPerInstance']", '0'))
            instances = int(node.findtext("uint[@name='InstanceCount']", '0'))
            if indices > 0 and instances > 0:
                recorded.setdefault(command_list, Counter()).update(markers.get(command_list, []))
        elif name == 'ID3D12CommandQueue::ExecuteCommandLists':
            for submitted_list in node.findall("array[@name='ppCommandLists']/ResourceId"):
                submitted.update(recorded.get(submitted_list.text, {}))
    # Ordinary sessions retain their structured identity. The forward pipeline owns stage names.
    scene_draws = sum(count for marker, count in submitted.items()
                      if re.fullmatch(r'(Scene [1-9]\d*/[1-9]\d*/[1-9]\d*/[1-9]\d*/Forward|Forward)/\d+', marker))
    assert scene_draws > 0, f'{experiment}: missing submitted session scene draws'
    if experiment == 'Shadows':
        for cascade in range(4):
            assert submitted[f'Shadow cascade {cascade}/0'] > 0, f'missing cascade {cascade} draws'
    assert submitted['Debug UI'] > 0, f'{experiment}: missing submitted GUI draws'
    assert any('Present' in name for name in chunks), f'{experiment}: missing Present'
    assert any('CreateGraphicsPipeline' in name for name in chunks), f'{experiment}: missing pipeline'
    return submitted


def capture(experiment, frame_count, capture_frames):
    output = work / (experiment + ' RDC 空格')
    arguments = [viewer, '--config', root / ('experiments/' + experiment + '.json'),
                 '--frames', frame_count, '--hidden', '--renderdoc-library', library,
                 '--rdc-output', output, '--exercise-rdc-ui']
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
capture('Model', 100, [80])
capture('Shadows', 100, [80])

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
