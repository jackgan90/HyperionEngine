"""Contracts for the pinned Showcase workloads, independent of demo defaults."""
import re


def config_path(root, scene="Scene"):
    return root / "experiments" / f"Benchmark{scene}.json"


def validate_workload(log, rows, scene="Scene"):
    """Keep readiness, nonempty coverage and per-item accounting mandatory."""
    if scene == "Scene":
        ready = re.findall(r"(\d+)/(\d+) models ready \| (\d+) failed", log)
        if not ready or tuple(map(int, ready[-1])) != (79, 79, 0):
            raise RuntimeError("Showcase benchmark requires all 79 models ready")
        # 52 four-section Showcase + 26 single-section Interleaved + one ground section.
        minimum = maximum = 235
    else:
        if "Model: Ready | 4 primitives | ready=1" not in log:
            raise RuntimeError("Model benchmark requires all 4 Showcase primitives ready")
        minimum = maximum = 4
    if not rows:
        raise RuntimeError("Benchmark produced no samples")
    for row in rows:
        visible = int(row["visible_items"])
        if not minimum <= visible <= maximum or int(row["scene_draws"]) <= 0:
            raise RuntimeError("Benchmark visible coverage differs from the pinned workload")
        if int(row["failed_items"]) or int(row["shadow_failed"]):
            raise RuntimeError("Benchmark contains failed items")
        if int(row["instanced_items"]) + int(row["single_draws"]) != visible:
            raise RuntimeError("Benchmark item accounting is incomplete")
