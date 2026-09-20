"""Exercise editor preference widgets, restart persistence, optional capture and exact replay."""
import pathlib
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

from RdcValidation import verify_capture


def main():
    editor = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(sys.argv[2]).resolve()
    compiled = sys.argv[3] == '1'
    parent = root / 'out' / 'editor-capture-tests'
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(dir=parent))
    preferences = work / 'Preferences.ini'

    def run(name, mode, *arguments):
        result = subprocess.run(
            [str(editor), '--hidden',
             '--editor-preferences', str(preferences), '--layout', str(work / 'Layout.ini'),
             '--capture', str(work / f'{name}.png'),
             '--ui-preferences', str(work / 'Scale.ini'), '--exercise-capture', mode, *arguments],
            cwd=root, capture_output=True, encoding='utf-8', errors='replace', timeout=100)
        text = result.stdout + result.stderr
        (work / f'{name}.log').write_text(text, encoding='utf-8')
        assert result.returncode == 0, text
        assert f'Editor capture acceptance passed: {mode}' in text, text
        assert 'validation errors: 0' in text, text
        return text

    first = run('enable', 'toggle')
    assert 'renderdoc_capture=true' in preferences.read_text()
    assert 'Restart the editor' in first if compiled else 'not compiled' in first
    disabled = run('explicit-disable', 'unavailable', '--disable-plugin', 'renderdoc')
    assert 'explicitly disabled' in disabled if compiled else 'not compiled' in disabled
    assert 'RenderDoc capture saved:' not in disabled
    runtime = pathlib.Path(r'C:\Program Files\RenderDoc')
    command = runtime / 'renderdoccmd.exe'
    if compiled and command.is_file() and (runtime / 'renderdoc.dll').is_file():
        text = run('capture', 'capture', '--scene', '/Game/Scenes/Sponza.hasset')
        captures = re.findall(r'RenderDoc capture saved: (.+)', text)
        launches = re.findall(r'RenderDoc replay launched: (\d+); (.+)', text)
        assert len(captures) == 1 and len(launches) == 1, text
        capture = pathlib.Path(captures[0].strip())
        assert capture == pathlib.Path(launches[0][1].strip()), text
        assert capture.is_file() and capture.stat().st_size > 1024
        try:
            replay = subprocess.run([str(command), 'replay', '--loops', '2', str(capture)],
                                    capture_output=True, text=True, timeout=60)
            assert replay.returncode == 0, replay.stdout + replay.stderr
            xml = work / 'Editor.xml'
            convert = subprocess.run([str(command), 'convert', '--filename', str(capture),
                                      '--output', str(xml), '--convert-format', 'xml'],
                                     capture_output=True, text=True, timeout=60)
            assert convert.returncode == 0, convert.stdout + convert.stderr
            draws = verify_capture(ET.parse(xml), 'Editor')
            print(f'Editor capture submitted draws: {dict(draws)}')
        finally:
            # Only close the replay process this test launched.
            subprocess.run(['powershell', '-NoProfile', '-Command',
                            f'Stop-Process -Id {int(launches[0][0])} -ErrorAction SilentlyContinue'],
                           capture_output=True, timeout=10)
    else:
        unavailable = run('unavailable', 'unavailable')
        assert 'RenderDoc capture saved:' not in unavailable
    run('disable', 'toggle')
    assert 'renderdoc_capture=false' in preferences.read_text()
    run('restore-disabled', 'toggle')
    assert 'renderdoc_capture=true' in preferences.read_text()
    preferences.write_text('version=1\nrenderdoc_capture=broken\n', encoding='utf-8')
    malformed = run('malformed', 'toggle')
    assert 'Could not load editor preferences' in malformed
    assert 'renderdoc_capture=true' in preferences.read_text()
    print(f'PASS: editor capture/preferences acceptance: {work}')


if __name__ == '__main__':
    main()
