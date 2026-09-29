"""Real Editor Log menu, rendered output and attached JSONL/MCP reads."""
import json
import pathlib
import subprocess
import sys
import tempfile

from AttachmentAcceptance import Application, AttachedSession
from AutomationAcceptance import completed


def main():
    editor, cli, root = (pathlib.Path(value).resolve() for value in sys.argv[1:])
    parent = root / 'out' / 'log-tests'
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix='editor-', dir=parent))
    capture = work / 'Log.png'
    result = subprocess.run(
        [str(editor), '--hidden', '--exercise-log', '--frames', '500', '--capture', str(capture),
         '--layout', str(work / 'Layout.ini'), '--ui-preferences', str(work / 'Scale.ini'),
         '--editor-preferences', str(work / 'Preferences.ini')], capture_output=True, timeout=90)
    (work / 'Editor.log').write_bytes(result.stdout + result.stderr)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert b'Editor Log menu, reopening, startup replay and colors acceptance passed' in result.stdout
    assert capture.is_file() and capture.stat().st_size > 10000
    assert '[Window][Log]' in (work / 'Layout.ini').read_text()
    app = Application(editor, work, 'attached', '', work, frames=10000)
    try:
        instance = app.target()
        for mcp in (False, True):
            session = AttachedSession(cli, instance, mcp)
            try:
                assert 'application.log.read' in json.dumps(session.request('api.search', {'query': 'log'}))
                schema = session.request('api.describe', {'operation': 'application.log.read'})
                assert 'debug' in json.dumps(schema) and not schema['unavailable']
                page = completed(session.call('application.log.read', after='0', limit=1))
                assert page['entries'][0]['message'] == 'Hyperion Editor starting', page
                assert page['next'] == '1'
                page = completed(session.call('application.log.read', after=page['next'], limit=100))
                assert any(entry['level'] == 3 for entry in page['entries']), page
                assert 'invalid_arguments' in json.dumps(session.call('application.log.read', limit=0))
                if mcp:
                    completed(session.call('application.close.request'))
            finally:
                session.close()
        app.finish()
    finally:
        app.close()
    print(f'Editor Log menu, PNG and attached JSONL/MCP passed: {capture}')


if __name__ == '__main__':
    main()
