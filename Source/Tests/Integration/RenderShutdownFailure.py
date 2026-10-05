"""Assert terminal destructor policies in isolated fault-injected processes."""

from pathlib import Path
import subprocess
import sys


executable = Path(sys.argv[1]).resolve()
for mode in ('session-idle', 'resource-idle', 'resource-direct-idle', 'resource-collection', 'session-collection',
             'session-unknown', 'session-log-failure', 'graphics-idle'):
    result = subprocess.run([executable, '--persistent-shutdown', mode],
                            capture_output=True, text=True, timeout=30)
    output = result.stdout + result.stderr
    assert result.returncode == 1, (mode, result.returncode, output)
    assert 'UNEXPECTED_TERMINATE' not in output, (mode, output)
    assert 'UNSAFE_ATEXIT_EXECUTION' not in output, (mode, output)
    assert 'UNSAFE_DEVICE_DESTRUCTION' not in output, (mode, output)
    assert 'UNSAFE_SWAPCHAIN_DESTRUCTION' not in output, (mode, output)
    assert 'UNSAFE_RETAINED_RESOURCE_DESTRUCTION' not in output, (mode, output)
    owner = 'RenderResourceService' if mode.startswith('resource') else 'RenderSession'
    assert f'owner={owner}; stage=destructor-close;' in output, (mode, output)
    assert 'outcome=immediate-process-exit; exit-code=1' in output, (mode, output)
    reason = 'Unknown exception' if mode.endswith('unknown') else (
        'persistent collection failure' if 'collection' in mode else 'persistent idle failure')
    assert f'reason={reason}' in output, (mode, output)
    expected_calls = 1 if 'direct' in mode else 2
    assert output.count('SHUTDOWN_FAULT') == expected_calls, (mode, output)
    assert 'call=1 domain=Rhi0' in output, (mode, output)
    if expected_calls == 2:
        assert 'call=2 domain=Rhi0' in output, (mode, output)
    if mode != 'graphics-idle' and 'direct' not in mode:
        assert 'EXPLICIT_CLOSE_FAILURE_OBSERVED' in output, (mode, output)
    print(f'{mode}: explicit failure, RHI 0 retry, terminal exit verified')
