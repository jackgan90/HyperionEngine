"""Exercise multi-selection through real editor events and its batch history."""
import pathlib
import sys
import tempfile

from EditorAcceptance import run_editor


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(sys.argv[2]).resolve()
    parent = root / 'out' / 'editor-tests'
    parent.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(prefix='multiselect-', dir=parent))
    report, capture = run_editor(executable, root, output, 'multiselect',
                                 '--exercise-multiselect', '--scene', '/Game/Scenes/Sponza.hasset')
    assert report['multiselect_verified'], report
    assert report['validation_errors'] == 0 and not report['scene_error'], report
    assert capture.stat().st_size > 10_000
    print(f'PASS: Ctrl selection, common Details, absolute fields, group PRS and deletion history: {output}')


if __name__ == '__main__':
    main()
