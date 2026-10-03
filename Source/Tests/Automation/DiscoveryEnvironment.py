"""Give one acceptance run and all its children an isolated discovery root."""
import os
import pathlib
import subprocess
import sys
import tempfile


def ensure_isolated_discovery(*, new_run=False):
    inherited = os.environ.get("HYP_TEST_DISCOVERY_ROOT")
    if inherited and not new_run:
        # Do not overwrite an explicit LOCALAPPDATA failure fixture in this run.
        return pathlib.Path(inherited)
    parent = pathlib.Path(__file__).resolve().parents[3] / "out" / "discovery-tests"
    parent.mkdir(parents=True, exist_ok=True)
    root = pathlib.Path(tempfile.mkdtemp(prefix="run-", dir=parent))
    os.environ["HYP_TEST_DISCOVERY_ROOT"] = str(root)
    os.environ["LOCALAPPDATA"] = str(root)
    return root


def main():
    if len(sys.argv) < 2:
        raise SystemExit("Usage: DiscoveryEnvironment.py <command> [args ...]")
    ensure_isolated_discovery(new_run=True)
    return subprocess.run(sys.argv[1:], check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
