"""Static plugin activation, explicit disablement and graphics-free host profiles."""
import json
import pathlib
import subprocess
import sys


def main():
    viewer, editor, root = map(lambda value: pathlib.Path(value).resolve(), sys.argv[1:4])
    work = pathlib.Path.cwd() / "plugin-acceptance"
    work.mkdir(exist_ok=True)
    config = json.loads((root / "experiments/Triangle.json").read_text(encoding="utf-8"))
    config["properties"].update(plugins=[], scene_source="/Missing/StaleScene.hasset")
    path = work / "stale-source.json"
    path.write_text(json.dumps(config), encoding="utf-8")

    def run(name, executable, *arguments):
        result = subprocess.run([str(executable), "--frames", "8", "--hidden", *map(str, arguments)],
                                cwd=work, capture_output=True, text=True, timeout=45)
        output = result.stdout + result.stderr
        (work / f"{name}.log").write_text(output, encoding="utf-8")
        assert result.returncode == 0, output
        return output

    for name, executable in (("viewer", viewer), ("editor", editor)):
        output = run(name + "-kernel", executable, "--kernel-only")
        assert "GPU frames:" not in output and "rendering application started" not in output, output
        output = run(name + "-disabled-graphics", executable, "--disable-plugin", "graphics")
        assert "Plugin graphics: Plugin explicitly disabled" in output, output
        assert "GPU frames:" not in output, output
        result = subprocess.run([str(executable), "--frames", "8", "--hidden", "--disable-plugin", "graphics",
                                 "--capture", str(work / (name + "-unavailable.png"))],
                                cwd=work, capture_output=True, text=True, timeout=45)
        assert result.returncode != 0 and "output is unavailable" in result.stderr, result.stdout + result.stderr
    for disabled in ("gui", "editor"):
        result = subprocess.run([str(editor), "--frames", "8", "--hidden", "--disable-plugin", disabled,
                                 "--exercise-assets", str(work / "unavailable-assets")],
                                cwd=work, capture_output=True, text=True, timeout=45)
        assert result.returncode != 0 and "output is unavailable" in result.stderr, result.stdout + result.stderr
    output = run("stale", viewer, "--config", path, "--capture", work / "stale.png", "--verify-clear")
    assert "Scene:" not in output and "Plugin scene-viewer" not in output, output
    config["properties"].update(plugins=["missing-extension", "scene-viewer"], disabled_plugins=["scene-viewer"])
    path.write_text(json.dumps(config), encoding="utf-8")
    output = run("disabled", viewer, "--config", path)
    assert "Missing plugin: missing-extension" in output, output
    assert "Plugin scene-viewer: Plugin explicitly disabled" in output, output
    assert "Scene:" not in output and "validation errors: 0" in output, output
    output = run("no-contact", viewer, "--config", path, "--disable-plugin", "contact-shadows")
    assert "Contact shadows: active=0 HZB consumers=0 products=0 dispatches=0 bytes=0" in output, output
    result = subprocess.run([str(viewer), "--config", str(path), "--frames", "8", "--hidden",
                             "--capture-rdc", "4", "--renderdoc-library", str(work / "missing.dll")],
                            cwd=work, capture_output=True, text=True, timeout=45)
    assert result.returncode != 0 and "Requested RDC capture was not produced" in result.stderr, result.stdout + result.stderr
    print("Plugin selection, source isolation, missing branches and kernel profiles passed")


if __name__ == "__main__":
    main()
