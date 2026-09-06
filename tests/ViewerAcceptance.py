"""Real process checks: restart, configuration, plugin absence and RHI thread activity."""
import json
import pathlib
import re
import subprocess
import sys

viewer = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(sys.argv[2]).resolve()
work = pathlib.Path.cwd() / "viewer-acceptance"
work.mkdir(exist_ok=True)
base = json.loads((root / "experiments/Triangle.json").read_text())
base["properties"].update(triangle_scale=0.63, workers=2, rhi_threads=2)
initial, saved = work / "initial.json", work / "saved.json"
initial.write_text(json.dumps(base), encoding="utf-8")

def run(*args, fail=False):
    result = subprocess.run([str(viewer), "--frames", "16", "--hidden", *map(str, args)],
                            cwd=work, capture_output=True, text=True, timeout=45)
    (work / f"run-{run.count}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    run.count += 1
    if fail:
        assert result.returncode != 0 and "Missing plugin" in result.stderr, result.stdout + result.stderr
    else:
        assert result.returncode == 0, result.stdout + result.stderr
        assert "validation errors: 0" in result.stdout, result.stdout
    return result.stdout

run.count = 0
output = run("--config", initial, "--exercise-window", "--save-config", saved)
for domain in ("Render", "RHI 0", "RHI 1", "Workers (2)"):
    match = re.search(r"Domain " + re.escape(domain) + r": (\d+) tasks", output)
    assert match and int(match.group(1)) > 0, f"No work on {domain}"
ids = re.findall(r"Domain (?:Main|Render|RHI 0|RHI 1): \d+ tasks; thread ID (\d+)", output)
assert len(ids) == 4 and len(set(ids)) == 4, "Dedicated domains share a thread"
restored = json.loads(saved.read_text())["properties"]
assert restored["triangle_scale"] == 0.63 and restored["width"] == 960 and restored["height"] == 540
assert restored["plugins"] == ["triangle", "debug-ui"]
run("--config", saved, "--capture", work / "restored.png", "--verify-triangle", "--verify-ui")
base["properties"].update(plugins=[], rhi_threads=1)
initial.write_text(json.dumps(base), encoding="utf-8")
run("--config", initial, "--capture", work / "disabled.png", "--verify-clear")
base["properties"]["plugins"] = ["does-not-exist"]
initial.write_text(json.dumps(base), encoding="utf-8")
run("--config", initial, fail=True)
print("Viewer restart, plugin selection, dedicated domains and configuration checks passed")
