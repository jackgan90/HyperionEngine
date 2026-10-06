"""Verify scene-wide selection, range gestures and input ownership with real Editor events."""
import pathlib
import sys
import tempfile

from EditorAcceptance import run_editor


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[3] / "tools"))
from TestEnvironment import test_output_root


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(sys.argv[2]).resolve()
    parent = test_output_root() / 'editor-tests'
    parent.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(prefix='selection-shortcuts-', dir=parent))
    report, capture = run_editor(executable, root, output, 'selection-shortcuts',
                                 '--exercise-selection-shortcuts', '--scene', '/Game/Scenes/Sponza.hasset')
    assert report['selection_shortcuts_verified'] and not report['document_dirty'], report
    assert report['validation_errors'] == 0 and not report['scene_error'], report
    assert capture.stat().st_size > 10_000
    print(f'PASS: Ctrl+A, Outliner ranges, Shift picking, text/focus/drag ownership: {output}')


if __name__ == '__main__':
    main()
