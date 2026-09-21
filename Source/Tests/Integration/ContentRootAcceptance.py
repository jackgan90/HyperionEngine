"""Verify last-root restoration, explicit mount precedence and invalid-root startup."""
import json
import pathlib
import subprocess
import sys


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    engine = pathlib.Path(sys.argv[2]).resolve()
    output = pathlib.Path(sys.argv[3]).resolve()
    preferences = output / 'StartupPreferences.ini'
    preferences.write_text(
        f'version=1\nrenderdoc_capture=false\nasset_root={(output / "A").as_posix()}\n',
        encoding='utf-8')

    def run(name, *arguments):
        report = output / f'{name}.json'
        result = subprocess.run(
            [str(executable), '--hidden', '--frames', '24', '--editor-preferences', str(preferences),
             '--layout', str(output / 'StartupLayout.ini'), '--report', str(report), *arguments],
            cwd=engine, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=45)
        (output / f'{name}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        assert result.returncode == 0, result.stdout + result.stderr
        data = json.loads(report.read_text(encoding='utf-8'))
        assert not data['scene'] and data['validation_errors'] == 0, data
        return data

    restored = run('restored')
    assert pathlib.Path(restored['asset_root']) == output / 'A', restored
    assert not restored['root_restore_failed'], restored
    mounts = output / 'ExplicitMounts.json'
    mounts.write_text(json.dumps({'version': 1, 'mounts': [
        {'root': '/Engine', 'directory': str(engine / 'Content'), 'read_only': True},
        {'root': '/Game', 'directory': str(output / 'B'), 'read_only': False}]}), encoding='utf-8')
    explicit = run('explicit', '--mounts', str(mounts))
    assert pathlib.Path(explicit['asset_root']) == output / 'B', explicit
    preferences.write_text(
        f'version=1\nrenderdoc_capture=false\nasset_root={(output / "Missing").as_posix()}\n',
        encoding='utf-8')
    invalid = run('invalid')
    assert not invalid['asset_root'] and invalid['root_restore_failed'], invalid
    print('PASS: root restoration, explicit mount precedence and invalid-root recovery')


if __name__ == '__main__':
    main()
