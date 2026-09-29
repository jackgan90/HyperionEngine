"""GUI-subsystem standard handles, redirected diagnostics, history, and final-tail draining."""
import pathlib
import struct
import subprocess
import sys
import tempfile


def subsystem(executable):
    data = executable.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    return struct.unpack_from('<H', data, pe + 24 + 68)[0]


def main():
    child, editor, root = (pathlib.Path(value).resolve() for value in sys.argv[1:])
    assert subsystem(child) == 2 and subsystem(editor) == 2
    parent = root / 'out' / 'log-tests'
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix='process-', dir=parent))
    result = subprocess.run([str(child), str(work)], capture_output=True, timeout=30)
    assert result.returncode == 0, (result.stdout[-1000:], result.stderr)
    result.stdout = result.stdout.replace(b'\r\n', b'\n')
    assert result.stdout.count(b'structured-debug') == 1
    assert b'stdout-joined\n' in result.stdout
    assert b'unterminated-tail\ncapture-passed' in result.stdout
    assert result.stderr.count(b'stderr-crt') == 1 and result.stderr.count(b'stderr-native') == 1
    assert (work / 'History.txt').read_text().count('structured-debug') == 1
    assert not (work / 'Capture.bin').exists()
    detached = subprocess.run([str(child), str(work)], creationflags=subprocess.DETACHED_PROCESS, timeout=30)
    assert detached.returncode == 0
    result = subprocess.run([str(editor), '--hidden', '--invalid-log-test-option'], capture_output=True, timeout=30)
    assert result.returncode != 0 and b'Hyperion Editor:' in result.stderr
    result = subprocess.run([str(editor), '--kernel-only', '--frames', '2'], capture_output=True, timeout=30)
    assert result.returncode == 0 and b'Editor log history initialized' in result.stdout, result.stderr
    print('GUI subsystem, raw/structured forwarding, replay, tails and startup failures passed')


if __name__ == '__main__':
    main()
