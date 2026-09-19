"""Verify GUI preferences through actual Editor launches and isolated captures."""
import pathlib
import subprocess
import sys
import tempfile


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(sys.argv[2]).resolve()
    parent = root / 'out' / 'editor-tests'
    parent.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(prefix='gui-scale-', dir=parent))
    preference = output / 'UiScale.ini'

    def run(name, *arguments, success=True):
        result = subprocess.run(
            [str(executable), '--hidden', '--frames', '8', '--layout', str(output / 'Layout.ini'),
             '--ui-preferences', str(preference), *arguments], cwd=root,
            capture_output=True, text=True, timeout=60)
        (output / f'{name}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        assert (result.returncode == 0) == success, (name, result.stdout, result.stderr)

    run('default')
    assert float(preference.read_text()) == 1.25
    run('override', '--ui-scale', '1.5', '--capture', str(output / 'Scale150.png'))
    assert float(preference.read_text()) == 1.5
    run('restore')
    assert float(preference.read_text()) == 1.5
    for invalid in ('nan', 'inf', '0', '2.1', '1.5junk'):
        run(f'invalid-{invalid}', '--ui-scale', invalid, success=False)
        assert float(preference.read_text()) == 1.5
    for index, invalid in enumerate(('nan', '0', '2.1', '1.5 junk', '')):
        preference.write_text(invalid)
        run(f'corrupt-{index}')
        assert float(preference.read_text()) == 1.25
    for label, scale in (('100', '1'), ('200', '2')):
        run(label, '--ui-scale', scale, '--capture', str(output / f'Scale{label}.png'))
        assert float(preference.read_text()) == float(scale)
    saved = preference.read_bytes()
    run('gui-disabled', '--disable-plugin', 'gui')
    assert preference.read_bytes() == saved
    run('kernel', '--kernel-only')
    assert preference.read_bytes() == saved
    print(f'PASS: scale defaults, persistence, overrides, invalid input and GUI absence: {output}')


if __name__ == '__main__':
    main()
