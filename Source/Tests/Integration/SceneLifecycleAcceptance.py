"""Exercise real scene menus, decisions and Save As through the isolated GUI driver."""
import json
import pathlib
import subprocess
import sys


def run(editor, output):
    output.mkdir(parents=True, exist_ok=True)
    report = output / "Report.json"
    capture = output / "Closed.png"
    report.unlink(missing_ok=True)
    capture.unlink(missing_ok=True)
    log_path = output / "Editor.log"
    with log_path.open("w", encoding="utf-8") as log:
        result = subprocess.run(
            [str(editor), "--hidden", "--exercise-scene-lifecycle", str(output),
             "--report", str(report), "--capture", str(capture), "--frames", "20000"],
            stdout=log, stderr=subprocess.STDOUT, timeout=105, check=False)
    text = log_path.read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0, text
    assert json.loads(report.read_text(encoding="utf-8"))["scene_lifecycle_verified"], text
    assert capture.is_file() and (output / "Empty.hasset").is_file(), text
    assert "Final graphics validation errors: 0" in text, text
    print("PASS: Ctrl+S empty scene and new/close GUI save, discard, cancel, dismiss and Save As cancellation")


if __name__ == "__main__":
    run(*(pathlib.Path(argument).resolve() for argument in sys.argv[1:]))
