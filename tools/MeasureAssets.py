"""Compare source conversion and native CPU loading using the same C++ asset pipeline."""
import argparse
import hashlib
import json
import pathlib
import re
import statistics
import subprocess


def run(tool, arguments):
    completed = subprocess.run([str(tool), *map(str, arguments)], capture_output=True,
                               encoding="utf-8", errors="replace", timeout=120)
    log = completed.stdout + completed.stderr
    if completed.returncode:
        raise RuntimeError(log)
    return log


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", type=pathlib.Path, required=True)
    parser.add_argument("--source", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--samples", type=int, default=7)
    parser.add_argument("--warmup", type=int, default=2)
    args = parser.parse_args()
    if args.samples < 1 or args.warmup < 0:
        parser.error("Require positive samples and nonnegative warmup")
    tool = args.tool.resolve()
    source = args.source.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    native = output / "Measured.hasset"
    publication = run(tool, ("import", source, native))
    (output / "Import.log").write_text(publication, encoding="utf-8")
    model_check = run(tool, ("measure-source", source))
    counts = re.search(r"primitives=(\d+) instances=(\d+)", model_check)
    if not counts or min(map(int, counts.groups())) < 1:
        raise RuntimeError("Source model is not ready or is empty")
    summary = {
        "tool": str(tool), "tool_sha256": hashlib.sha256(tool.read_bytes()).hexdigest(),
        "source": str(source), "native": str(native),
        "primitives": int(counts[1]), "instances": int(counts[2]),
        "context": "CPU readiness only; fresh process per sample; OS file cache warmed, not flushed; no GPU work",
        "warmup_per_mode": args.warmup, "samples_per_mode": args.samples, "results": {},
    }
    modes = {"source": ("measure-source", source), "native": ("validate", native)}
    samples = {name: [] for name in modes}
    for index in range(args.warmup + args.samples):
        for name, command in modes.items():
            log = run(tool, command)
            if name == "native" and "Validated native graph: 1 assets" not in log:
                raise RuntimeError("Expected a validated native model without external native dependencies")
            values = {key: float(value) for key, value in re.findall(
                r"\b(elapsed_ms|reads|read_bytes|writes|written_bytes|peak_resident_bytes)=([0-9.]+)", log)}
            if len(values) != 6 or values["elapsed_ms"] <= 0 or values["read_bytes"] <= 0:
                raise RuntimeError("Missing or invalid AssetTool counters: " + log)
            if index >= args.warmup:
                samples[name].append(values)
                (output / f"{name}-{index - args.warmup}.log").write_text(log, encoding="utf-8")
    if "Up to date:" not in run(tool, ("import", source, native)):
        raise RuntimeError("Sources changed during measurement")
    for name, rows in samples.items():
        summary["results"][name] = {
            "median": {key: statistics.median(row[key] for row in rows) for key in rows[0]},
            "samples": rows,
        }
    (output / "Summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps({name: result["median"] for name, result in summary["results"].items()}, indent=2))


if __name__ == "__main__":
    main()
