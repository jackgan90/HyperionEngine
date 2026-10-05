"""Exercise advisory file sizes and reviewed exclusions with isolated CLI calls."""

import contextlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import CheckStyle
from SourceSize import check_source_sizes, physical_line_count


class SourceSizeTests(unittest.TestCase):
    def setUp(self):
        output = ROOT / 'out/source-size-tests'
        output.mkdir(parents=True, exist_ok=True)
        self.workspace = tempfile.TemporaryDirectory(prefix='case-', dir=output)
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        (self.root / 'Source').mkdir()
        (self.root / 'tools').mkdir()
        self.policy = {'version': 2, 'exclusions': {}}

    def source(self, name='Source/Example.cpp', lines=1001, newline=b'\n', terminated=True):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        data = (b'// fixture' + newline) * lines
        if data and not terminated:
            data = data[:-len(newline)]
        path.write_bytes(data)
        return path

    def write_policy(self):
        (self.root / 'tools/SourceSizePolicy.json').write_text(
            json.dumps(self.policy), encoding='utf-8')

    def check(self):
        self.write_policy()
        with contextlib.redirect_stdout(io.StringIO()) as output:
            check_source_sizes(self.root)
        return output.getvalue()

    def test_physical_line_boundaries(self):
        for lines in (0, 1, 501, 999, 1000, 1001):
            for newline in (b'\n', b'\r\n'):
                for terminated in (False, True):
                    with self.subTest(lines=lines, newline=newline, terminated=terminated):
                        path = self.source(lines=lines, newline=newline, terminated=terminated)
                        self.assertEqual(physical_line_count(path), lines)
                        output = self.check()
                        self.assertIn('PASS', output)
                        if lines > 1000:
                            self.assertIn('1001 lines exceed the recommended 1000-line size', output)
                            self.assertIn('1 advisory findings', output)
                        else:
                            self.assertNotIn('WARN', output)
                            self.assertIn('0 advisory findings', output)

    def test_control_characters_do_not_add_lines(self):
        path = self.source(lines=0)
        path.write_bytes(b'// vertical\vtab and form\ffeed\n' * 1000)
        self.assertEqual(physical_line_count(path), 1000)
        self.assertNotIn('WARN', self.check())

    def test_all_owned_cpp_suffixes_are_checked(self):
        for suffix in ('.cpp', '.h', '.inl'):
            with self.subTest(suffix=suffix):
                path = self.source('Source/Plugins/Disabled/Private/Example' + suffix)
                self.assertIn('Example' + suffix + ': 1001 lines exceed', self.check())
                path.unlink()

    def test_non_cpp_and_external_build_outputs_are_outside_scope(self):
        for name in ('Source/Shader.hlsl', 'Source/Include.hlsli', 'out/deps/Vendor.cpp',
                     'out/build/Generated.h', 'Content/Shaders/Shader.hlsl'):
            self.source(name, lines=1100)
        self.assertIn('0 files', self.check())

    def test_names_and_test_directories_do_not_grant_exclusions(self):
        for name in ('Source/Tests/Production.cpp', 'Source/Runtime/ExampleTests.cpp',
                     'Source/Plugins/Editor/Private/EditorSomethingAcceptance.cpp'):
            with self.subTest(name=name):
                path = self.source(name)
                self.assertIn('split at logical boundaries', self.check())
                path.unlink()

    def test_reviewed_exclusions_are_exact_and_do_not_exempt_other_files(self):
        for kind in ('test', 'generated', 'third_party'):
            name = f'Source/{kind}/Example.cpp'
            self.source(name, lines=1200)
            self.policy['exclusions'][name] = {
                'kind': kind, 'reason': 'Reviewed fixture purpose.',
                'evidence': 'Fixture build owner, generator or vendor declaration.'}
        output = self.check()
        self.assertIn('3 reviewed exclusions', output)
        self.assertNotIn('WARN', output)
        self.source('Source/test/ProductionUsedByTests.cpp')
        output = self.check()
        self.assertIn('ProductionUsedByTests.cpp: 1001 lines exceed', output)
        self.assertIn('1 advisory findings', output)

    def test_production_resizing_and_retirement_need_no_baseline(self):
        name = 'Source/Example.cpp'
        for lines in (1001, 1200, 1100, 1000):
            with self.subTest(lines=lines):
                self.source(name, lines)
                output = self.check()
                self.assertEqual('WARN' in output, lines > 1000)
        path = self.source(name)
        renamed = self.root / 'Source/Renamed.cpp'
        path.rename(renamed)
        self.assertIn('Renamed.cpp: 1001 lines exceed', self.check())
        renamed.unlink()
        self.assertIn('0 files', self.check())
        policy = json.loads((self.root / 'tools/SourceSizePolicy.json').read_text(encoding='utf-8'))
        self.assertEqual(policy, {'version': 2, 'exclusions': {}})

    def test_deleted_and_renamed_exclusions_require_review(self):
        old = 'Source/Old.cpp'
        new = 'Source/New.cpp'
        self.policy['exclusions'][old] = {
            'kind': 'test', 'reason': 'Fixture.', 'evidence': 'Test target.'}
        self.source(old).rename(self.root / new)
        with self.assertRaisesRegex(RuntimeError, 'remove or update stale entries'):
            self.check()
        self.policy['exclusions'][new] = self.policy['exclusions'].pop(old)
        self.assertNotIn('WARN', self.check())
        (self.root / new).unlink()
        with self.assertRaisesRegex(RuntimeError, r'must match an existing Source C\+\+ file exactly'):
            self.check()

    def test_policy_paths_are_canonical_and_existing(self):
        self.source(lines=400)
        for name in ('Source/./Example.cpp', 'Source\\Example.cpp', 'Source/example.cpp',
                     'Source/Missing.cpp', '../Source/Example.cpp', 'Source/*.cpp'):
            with self.subTest(name=name):
                self.policy['exclusions'] = {name: {
                    'kind': 'test', 'reason': 'Fixture.', 'evidence': 'Test target.'}}
                with self.assertRaisesRegex(RuntimeError, 'must match an existing Source'):
                    self.check()

    def test_invalid_policy_records_fail(self):
        name = 'Source/Example.cpp'
        self.source(name)
        for entry in ({'kind': 'production', 'reason': 'x', 'evidence': 'x'},
                      {'kind': 'test', 'reason': ' ', 'evidence': 'x'},
                      {'kind': 'test', 'reason': 'x', 'evidence': ''},
                      {'kind': 'test', 'reason': 'x'},
                      {'kind': 'test', 'reason': 'x', 'evidence': 'x', 'extra': 1}, None):
            with self.subTest(exclusion=entry):
                self.policy['exclusions'] = {name: entry}
                with self.assertRaises(RuntimeError):
                    self.check()

    def test_malformed_and_duplicate_policy_fail(self):
        self.source()
        entry = '{"kind":"test","reason":"Fixture.","evidence":"Test target."}'
        duplicate_paths = ('{"version":2,"exclusions":{"Source/Example.cpp":' + entry
                           + ',"Source/Example.cpp":' + entry + '}}')
        for document in ('{', '[]', '{}', '{"version":2,"version":2,"exclusions":{}}',
                         '{"version":true,"exclusions":{}}',
                         '{"version":1,"exclusions":{}}',
                         '{"version":3,"exclusions":{}}',
                         '{"version":2,"exclusions":[]}',
                         '{"version":2,"exclusions":{},"extra":0}',
                         '{"version":2,"exclusions":{},"legacy_files":{}}', duplicate_paths):
            with self.subTest(document=document):
                (self.root / 'tools/SourceSizePolicy.json').write_text(document, encoding='utf-8')
                with self.assertRaises(RuntimeError):
                    check_source_sizes(self.root)

    def prepare_cli(self):
        for name in ('CheckStyle.py', 'SourceSize.py'):
            shutil.copyfile(ROOT / 'tools' / name, self.root / 'tools' / name)
        self.write_policy()

    def run_cli(self, *arguments):
        # Isolate discovery in the child after environment initialization.
        launcher = '''
import os
from pathlib import Path
import runpy
import sys
script = Path(sys.argv[1])
os.environ['PATH'] = ''
os.environ['ProgramFiles'] = str(script.parent / 'NoLlvm')
sys.path.insert(0, str(script.parent))
sys.argv = sys.argv[1:]
runpy.run_path(str(script), run_name='__main__')
'''
        return subprocess.run(
            [sys.executable, '-B', '-c', launcher, str(self.root / 'tools/CheckStyle.py'), *arguments],
            cwd=self.root.parent, capture_output=True, text=True, encoding='utf-8')

    def test_cli_advisories_default_flow_and_paths_only_compatibility(self):
        self.source()
        self.prepare_cli()
        result = self.run_cli('--sizes-only')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('1001 lines exceed the recommended 1000-line size', result.stdout)
        self.assertEqual(result.stderr, '')
        result = self.run_cli()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('1001 lines exceed', result.stdout)
        self.assertIn('clang-format is required', result.stderr)
        result = self.run_cli('--paths-only')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn('C++ file size scan', result.stdout)
        self.assertNotIn('WARN', result.stdout)

    def test_default_flow_continues_after_advisories(self):
        source = self.source()
        self.write_policy()
        with (mock.patch.object(CheckStyle, 'ROOT', self.root),
              mock.patch.object(CheckStyle, 'check_formatting') as formatting,
              mock.patch.object(sys, 'argv', ['CheckStyle.py']),
              contextlib.redirect_stdout(io.StringIO()) as output):
            CheckStyle.main()
        formatting.assert_called_once_with([source], False)
        self.assertIn('1001 lines exceed', output.getvalue())

    def test_cli_read_only_success_without_llvm(self):
        source = self.source()
        self.prepare_cli()
        policy = self.root / 'tools/SourceSizePolicy.json'
        before = (source.read_bytes(), policy.read_bytes())
        result = self.run_cli('--sizes-only')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('PASS: C++ file size scan', result.stdout)
        self.assertIn('1001 lines exceed', result.stdout)
        self.assertNotIn('filenames', result.stdout)
        self.assertNotIn('formatting', result.stdout)
        self.assertEqual(before, (source.read_bytes(), policy.read_bytes()))
        result = self.run_cli()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('clang-format is required', result.stderr)

    def test_cli_invalid_policy_still_fails(self):
        self.source()
        self.policy['exclusions']['Source/Missing.cpp'] = {
            'kind': 'test', 'reason': 'Fixture.', 'evidence': 'Test target.'}
        self.prepare_cli()
        result = self.run_cli('--sizes-only')
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('Source/Missing.cpp', result.stderr)
        self.assertNotIn('WARN', result.stdout)

    def test_format_reports_the_written_file_size_without_failing(self):
        for before, after in ((1, 1000), (1, 1001), (1001, 1000), (1001, 1100)):
            with self.subTest(before=before, after=after):
                source = self.source(lines=before)
                self.write_policy()

                def format_source(sources, apply):
                    self.assertEqual(sources, [source])
                    self.assertTrue(apply)
                    self.source(lines=after)

                with (mock.patch.object(CheckStyle, 'ROOT', self.root),
                      mock.patch.object(CheckStyle, 'check_formatting', side_effect=format_source),
                      mock.patch.object(sys, 'argv', ['CheckStyle.py', '--format']),
                      contextlib.redirect_stdout(io.StringIO()) as output):
                    CheckStyle.main()
                self.assertEqual('WARN' in output.getvalue(), after > 1000)
                if after > 1000:
                    self.assertIn(f'{after} lines exceed', output.getvalue())
                self.assertEqual(physical_line_count(source), after)

    def test_cli_rejects_conflicting_modes(self):
        self.prepare_cli()
        for mode in ('--format', '--naming', '--paths-only'):
            with self.subTest(mode=mode):
                result = self.run_cli('--sizes-only', mode)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn('not allowed with argument', result.stderr)


if __name__ == '__main__':
    unittest.main()
