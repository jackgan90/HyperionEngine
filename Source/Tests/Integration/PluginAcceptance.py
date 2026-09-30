"""Editor plugin disablement, graphics-free startup and controlled missing-output failure."""
import pathlib
import subprocess
import sys


def main():
    editor, root = (pathlib.Path(value).resolve() for value in sys.argv[1:])
    work = root / "out" / "plugin-acceptance"
    work.mkdir(parents=True, exist_ok=True)

    def run(name, *arguments, failure=False):
        result = subprocess.run(
            [str(editor), "--frames", "8", "--hidden", "--layout", str(work / "Layout.ini"),
             "--editor-preferences", str(work / "Preferences.ini"),
             "--ui-preferences", str(work / "Scale.ini"), *map(str, arguments)],
            cwd=work, capture_output=True, text=True, timeout=45)
        output = result.stdout + result.stderr
        (work / f"{name}.log").write_text(output, encoding="utf-8")
        assert (result.returncode != 0) == failure, output
        return output

    for name, arguments in (("kernel", ("--kernel-only",)),
                            ("graphics", ("--disable-plugin", "graphics")),
                            ("gui", ("--disable-plugin", "gui")),
                            ("editor", ("--disable-plugin", "editor"))):
        output = run(name, *arguments)
        assert "Editor ready:" not in output, output
        if name != "kernel":
            assert f"Plugin {name}: Plugin explicitly disabled" in output, output
        path = work / f"{name}-unavailable.png"
        path.unlink(missing_ok=True)
        output = run(name + "-output", *arguments, "--capture", path, failure=True)
        assert "output is unavailable" in output and not path.exists(), output
        output = run(name + "-render-controls", *arguments, "--exercise-render-controls", work, failure=True)
        assert "output is unavailable" in output, output
        output = run(name + "-clipboard", *arguments, "--exercise-clipboard", failure=True)
        assert "output is unavailable" in output, output
        output = run(name + "-framing", *arguments, "--exercise-framing", failure=True)
        assert "output is unavailable" in output, output
        output = run(name + "-import", *arguments, "--exercise-import", work / "Color.png", failure=True)
        assert "output is unavailable" in output, output
    output = run("render-controls-incomplete", "--exercise-render-controls", work, "--frames", "1", failure=True)
    assert "render controls acceptance did not complete" in output, output
    output = run("clipboard-incomplete", "--exercise-clipboard", "--frames", "1", failure=True)
    assert "clipboard acceptance did not complete" in output, output
    output = run("no-contact", "--disable-plugin", "contact-shadows")
    assert "Plugin contact-shadows: Plugin explicitly disabled" in output, output
    assert "validation errors: 0" in output, output
    print("Editor plugin disablement, kernel startup and missing output failures passed")


if __name__ == "__main__":
    main()
