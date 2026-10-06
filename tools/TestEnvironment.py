"""Explicit, inherited per-run storage for engine acceptance and measurement tools."""
import os
import pathlib
import tempfile
from DevelopmentPaths import test_root, tool_cache_root


def ensure_isolated_environment(*, new_run=False):
    inherited = os.environ.get("HYP_TEST_RUN_ROOT")
    if inherited and not new_run:
        return pathlib.Path(inherited)
    # LOCALAPPDATA below is a compatibility shim for child runtimes, not a new tool-cache selection.
    os.environ.setdefault("HYP_TOOL_CACHE", str(tool_cache_root()))
    parent = test_root() / "Runs"
    parent.mkdir(parents=True, exist_ok=True)
    root = pathlib.Path(tempfile.mkdtemp(prefix="run-", dir=parent))
    temporary = root / "Temp"
    temporary.mkdir()
    tempfile.tempdir = str(temporary)
    os.environ.update({
        "HYP_TEST_RUN_ROOT": str(root),
        "HYP_TEST_DISCOVERY_ROOT": str(root),
        "HYP_STORAGE_SETTINGS": str(root / "Bootstrap.json"),
        "HYP_USER_DATA_ROOT": str(root / "UserData"),
        "HYP_CACHE_ROOT": str(root / "Cache"),
        "HYP_DISCOVERY_ROOT": str(root / "Discovery"),
        "HYP_ISOLATED_STORAGE": "1",
        "TMP": str(temporary),
        "TEMP": str(temporary),
        # Retained for older child tools; engine isolation uses the explicit variables above.
        "LOCALAPPDATA": str(root),
    })
    return root


def test_output_root():
    return ensure_isolated_environment() / "Artifacts"
