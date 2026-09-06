"""Reject default Viewer TCP/UDP endpoints without changing Windows firewall policy."""
import pathlib
import subprocess
import sys
import time


def main():
    viewer = pathlib.Path(sys.argv[1]).resolve()
    work = pathlib.Path.cwd() / "offline-startup"
    work.mkdir(exist_ok=True)
    log = work / "viewer.log"
    with log.open("w", encoding="utf-8") as output:
        process = subprocess.Popen(
            [str(viewer), "--hidden"], cwd=work, stdout=output,
            stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW,
        )
        try:
            deadline = time.monotonic() + 25
            ready_at = None
            samples = 0
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f"Viewer exited early ({process.returncode}); see {log}")
                result = subprocess.run(
                    ["netstat.exe", "-ano"], check=True, capture_output=True,
                    text=True, errors="replace", timeout=5,
                    creationflags=subprocess.CREATE_NO_WINDOW,
                )
                endpoints = []
                for line in result.stdout.splitlines():
                    fields = line.split()
                    if fields and fields[0] in ("TCP", "UDP") and fields[-1] == str(process.pid):
                        endpoints.append(line.strip())
                if endpoints:
                    raise RuntimeError("Default Viewer opened network endpoints: " + "; ".join(endpoints))
                samples += 1
                if ready_at is None and "Windows rendering application started" in log.read_text(
                        encoding="utf-8", errors="replace"):
                    ready_at = time.monotonic()
                if ready_at is not None and time.monotonic() - ready_at >= 3:
                    print(f"PASS: default Viewer has no TCP/UDP endpoints ({samples} samples)")
                    return
                time.sleep(0.2)
            raise RuntimeError(f"Viewer did not complete offline startup observation; see {log}")
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=5)


if __name__ == "__main__":
    main()
