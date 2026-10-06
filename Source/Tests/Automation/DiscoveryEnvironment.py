"""Give one acceptance run and all its children an isolated discovery root."""
import pathlib
import subprocess
import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[3] / "tools"))
from TestEnvironment import ensure_isolated_environment


def ensure_isolated_discovery(*, new_run=False):
    return ensure_isolated_environment(new_run=new_run)


def main():
    if len(sys.argv) < 2:
        raise SystemExit("Usage: DiscoveryEnvironment.py <command> [args ...]")
    ensure_isolated_discovery(new_run=True)
    return subprocess.run(sys.argv[1:], check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
