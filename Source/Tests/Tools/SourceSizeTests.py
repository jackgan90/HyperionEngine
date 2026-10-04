"""Exercise the file-size policy with isolated source trees and real CLI calls."""

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
        self.policy = {'version': 1, 'legacy_files': {}, 'exclusions': {}}

    def source(self, name='Source/Example.cpp', lines=501, newline=b'\n', terminated=True):
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
        for lines in (0, 1, 500, 501):
            for newline in (b'\n', b'\r\n'):
                for terminated in (False, True):
                    with self.subTest(lines=lines, newline=newline, terminated=terminated):
                        path = self.source(lines=lines, newline=newline, terminated=terminated)
                        self.assertEqual(physical_line_count(path), lines)
                        if lines > 500:
                            with self.assertRaisesRegex(RuntimeError, '501 lines exceed the 500-line limit'):
                                self.check()
                        else:
                            self.assertIn('PASS', self.check())

    def test_control_characters_do_not_add_lines(self):
        path = self.source(lines=0)
        path.write_bytes(b'// vertical\vtab and form\ffeed\n' * 500)
        self.assertEqual(physical_line_count(path), 500)
        self.check()

    def test_all_owned_cpp_suffixes_are_checked(self):
        for suffix in ('.cpp', '.h', '.inl'):
            with self.subTest(suffix=suffix):
                path = self.source('Source/Plugins/Disabled/Private/Example' + suffix)
                with self.assertRaisesRegex(RuntimeError, '501 lines exceed'):
                    self.check()
                path.unlink()

    def test_non_cpp_and_external_build_outputs_are_outside_scope(self):
        for name in ('Source/Shader.hlsl', 'Source/Include.hlsli', 'out/deps/Vendor.cpp',
                     'out/build/Generated.h', 'Content/Shaders/Shader.hlsl'):
            self.source(name, lines=700)
        self.assertIn('0 files', self.check())

    def test_names_and_test_directories_do_not_grant_exclusions(self):
        for name in ('Source/Tests/Production.cpp', 'Source/Runtime/ExampleTests.cpp',
                     'Source/Plugins/Editor/Private/EditorSomethingAcceptance.cpp'):
            with self.subTest(name=name):
                path = self.source(name)
                with self.assertRaisesRegex(RuntimeError, 'split by responsibility'):
                    self.check()
                path.unlink()

    def test_reviewed_exclusions_are_exact_and_do_not_exempt_other_files(self):
        for kind in ('test', 'generated', 'third_party'):
            name = f'Source/{kind}/Example.cpp'
            self.source(name, lines=900)
            self.policy['exclusions'][name] = {
                'kind': kind, 'reason': 'Reviewed fixture purpose.',
                'evidence': 'Fixture build owner, generator or vendor declaration.'}
        self.assertIn('3 reviewed exclusions', self.check())
        self.source('Source/test/ProductionUsedByTests.cpp')
        with self.assertRaisesRegex(RuntimeError, 'ProductionUsedByTests.cpp: 501'):
            self.check()

    def test_legacy_debt_growth_reduction_and_retirement(self):
        name = 'Source/Legacy.cpp'
        entry = {'lines': 602, 'split': 'Separate preparation from publication.'}
        self.policy['legacy_files'][name] = entry
        self.source(name, 602)
        self.assertIn('1 bounded legacy files', self.check())
        self.source(name, 603)
        with self.assertRaisesRegex(RuntimeError, 'exceed legacy ceiling 602'):
            self.check()
        self.source(name, 601)
        with self.assertRaisesRegex(RuntimeError, 'lower the legacy count from 602'):
            self.check()
        entry['lines'] = 601
        self.check()
        self.source(name, 500)
        with self.assertRaisesRegex(RuntimeError, 'remove the legacy entry'):
            self.check()
        self.policy['legacy_files'].clear()
        self.check()

    def test_deleted_and_renamed_policy_entries_require_review(self):
        old = 'Source/Old.cpp'
        new = 'Source/New.cpp'
        self.policy['legacy_files'][old] = {'lines': 600, 'split': 'Separate lifecycle.'}
        self.source(old, 600).rename(self.root / new)
        with self.assertRaisesRegex(RuntimeError, 'remove or update stale entries'):
            self.check()
        self.policy['legacy_files'][new] = self.policy['legacy_files'].pop(old)
        self.check()
        (self.root / new).unlink()
        with self.assertRaisesRegex(RuntimeError, r'must match an existing Source C\+\+ file exactly'):
            self.check()

    def test_policy_paths_are_canonical_and_existing(self):
        self.source(lines=400)
        for name in ('Source/./Example.cpp', 'Source\\Example.cpp', 'Source/example.cpp',
                     'Source/Missing.cpp', '../Source/Example.cpp', 'Source/*.cpp'):
            with self.subTest(name=name):
                self.policy['legacy_files'] = {name: {'lines': 600, 'split': 'Separate lifecycle.'}}
                with self.assertRaisesRegex(RuntimeError, 'must match an existing Source'):
                    self.check()

    def test_invalid_policy_records_fail(self):
        name = 'Source/Example.cpp'
        self.source(name)
        for entry in ({'lines': True, 'split': 'x'}, {'lines': 500, 'split': 'x'},
                      {'lines': 600.0, 'split': 'x'}, {'lines': 600, 'split': ''},
                      {'lines': 600, 'split': 'x', 'extra': 1}, None):
            with self.subTest(legacy=entry):
                self.policy['legacy_files'] = {name: entry}
                with self.assertRaises(RuntimeError):
                    self.check()
        self.policy['legacy_files'].clear()
        for entry in ({'kind': 'production', 'reason': 'x', 'evidence': 'x'},
                      {'kind': 'test', 'reason': ' ', 'evidence': 'x'},
                      {'kind': 'test', 'reason': 'x', 'evidence': ''},
                      {'kind': 'test', 'reason': 'x'}, None):
            with self.subTest(exclusion=entry):
                self.policy['exclusions'] = {name: entry}
                with self.assertRaises(RuntimeError):
                    self.check()

    def test_overlapping_policy_records_fail(self):
        name = 'Source/Example.cpp'
        self.source(name, 600)
        self.policy['legacy_files'][name] = {'lines': 600, 'split': 'Separate lifecycle.'}
        self.policy['exclusions'][name] = {'kind': 'test', 'reason': 'Fixture.', 'evidence': 'Test target.'}
        with self.assertRaisesRegex(RuntimeError, 'both legacy debt and exclusions'):
            self.check()

    def test_malformed_and_duplicate_policy_fail(self):
        for document in ('{', '[]', '{}', '{"version":1,"version":1}',
                         '{"version":true,"legacy_files":{},"exclusions":{}}',
                         '{"version":2,"legacy_files":{},"exclusions":{}}',
                         '{"version":1,"legacy_files":[],"exclusions":{}}',
                         '{"version":1,"legacy_files":{},"exclusions":{},"extra":0}'):
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

    def test_cli_size_failure_default_flow_and_paths_only_compatibility(self):
        self.source()
        self.prepare_cli()
        for arguments in (('--sizes-only',), ()):
            result = self.run_cli(*arguments)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn('501 lines exceed', result.stderr)
            self.assertNotIn('clang-format is required', result.stderr)
        result = self.run_cli('--paths-only')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn('C++ file sizes', result.stdout)

    def test_cli_read_only_success_without_llvm(self):
        source = self.source(lines=500)
        self.prepare_cli()
        policy = self.root / 'tools/SourceSizePolicy.json'
        before = (source.read_bytes(), policy.read_bytes())
        result = self.run_cli('--sizes-only')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('PASS: C++ file sizes', result.stdout)
        self.assertNotIn('filenames', result.stdout)
        self.assertNotIn('formatting', result.stdout)
        self.assertEqual(before, (source.read_bytes(), policy.read_bytes()))
        result = self.run_cli()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('clang-format is required', result.stderr)

    def test_format_validates_the_written_file_size(self):
        for before, after in ((1, 500), (1, 501), (501, 500)):
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
                      contextlib.redirect_stdout(io.StringIO())):
                    if after > 500:
                        with self.assertRaisesRegex(RuntimeError, '501 lines exceed'):
                            CheckStyle.main()
                    else:
                        CheckStyle.main()
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
