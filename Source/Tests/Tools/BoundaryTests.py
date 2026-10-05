"""Adversarial fixtures for configured target contracts and source isolation."""

import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import CheckBoundaries
import TargetGraph


class BoundaryTests(unittest.TestCase):
    def setUp(self):
        output = ROOT / 'out/boundary-tests'
        output.mkdir(parents=True, exist_ok=True)
        self.workspace = tempfile.TemporaryDirectory(prefix='case-', dir=output)
        self.root = Path(self.workspace.name).resolve()
        self.assertTrue(self.root.is_relative_to(output.resolve()))
        self.addCleanup(self.workspace.cleanup)
        self.write('CMakeLists.txt', 'project(Fixture NONE)\n')
        self.write('CMakePresets.json', '{}\n')
        self.targets = {}
        self.trace = []

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding='utf-8')
        return path

    def target(self, name, directory, sources=(), test=False):
        self.write(directory + '/CMakeLists.txt', '# fixture\n')
        self.targets[name] = {'directory': directory, 'test': test, 'location': directory + '/CMakeLists.txt:1',
                              'sources': list(sources), 'links': []}
        return self.targets[name]

    def link(self, source, target, visibility='PRIVATE'):
        self.targets[source]['links'].append({'target': target, 'visibility': visibility,
                                              'location': self.targets[source]['location']})

    def command(self, directory, command, *args):
        self.trace.append({'file': str(self.root / directory / 'CMakeLists.txt'), 'line': len(self.trace) + 1,
                           'cmd': command, 'args': list(args)})

    def collect(self):
        trace = self.write('Trace.jsonl', '\n'.join(json.dumps(entry) for entry in self.trace))
        return TargetGraph.collect(self.root, trace)

    def test_multiple_targets_keep_test_links_separate(self):
        directory = 'Source/Runtime/Low'
        self.target('hyperion_low', directory)
        self.target('hyperion_high', 'Source/Runtime/High')
        self.command(directory, 'add_library', 'hyperion_low', 'STATIC', 'Private/Low.cpp')
        self.command('Source/Runtime/High', 'add_library', 'hyperion_high', 'STATIC', 'Private/High.cpp')
        self.command(directory, 'add_executable', 'low_tests', 'Private/LowTests.cpp')
        self.command(directory, 'target_link_libraries', 'low_tests', 'PRIVATE', 'hyperion_high')
        self.command('Source/Runtime/High', 'target_link_libraries', 'hyperion_high', 'PUBLIC', 'hyperion_low')
        targets = self.collect()
        self.assertEqual(targets['hyperion_low']['links'], [])
        self.assertTrue(targets['low_tests']['test'])
        self.assertEqual(targets['low_tests']['sources'], [directory + '/Private/LowTests.cpp'])
        self.assertEqual(CheckBoundaries.graph_errors(self.root, targets), [])

    def test_real_cycle_has_visibility_and_locations(self):
        self.target('hyperion_a', 'Source/Runtime/A')
        self.target('hyperion_b', 'Source/Runtime/B')
        self.link('hyperion_a', 'hyperion_b', 'PUBLIC')
        self.link('hyperion_b', 'hyperion_a')
        errors = CheckBoundaries.graph_errors(self.root, self.targets)
        self.assertTrue(any('cycle' in error and 'PUBLIC' in error and 'CMakeLists.txt:1' in error for error in errors))

    def include_fixture(self, public=False):
        directory = 'Source/Runtime/A'
        source = directory + ('/Public/Hyperion/A/A.h' if public else '/Private/A.cpp')
        self.write(source, '#include "Hyperion/B/B.h"\n')
        self.write('Source/Runtime/B/Public/Hyperion/B/B.h', '#pragma once\n')
        self.target('hyperion_a', directory, [source])
        self.target('hyperion_b', 'Source/Runtime/B')

    def test_missing_direct_link_despite_transitive_export(self):
        self.include_fixture()
        self.target('hyperion_c', 'Source/Runtime/C')
        self.link('hyperion_a', 'hyperion_c', 'PUBLIC')
        self.link('hyperion_c', 'hyperion_b', 'PUBLIC')
        self.assertTrue(any('missing direct' in error for error in CheckBoundaries.check(self.root, self.targets)[0]))

    def test_public_private_and_interface_visibility(self):
        self.include_fixture(public=True)
        self.link('hyperion_a', 'hyperion_b')
        self.assertTrue(any('PUBLIC/INTERFACE' in error for error in CheckBoundaries.check(self.root, self.targets)[0]))
        for visibility in ('PUBLIC', 'INTERFACE'):
            self.targets['hyperion_a']['links'][0]['visibility'] = visibility
            self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])

    def test_interface_does_not_satisfy_implementation(self):
        self.include_fixture()
        self.link('hyperion_a', 'hyperion_b', 'INTERFACE')
        self.assertTrue(any('PUBLIC/PRIVATE' in error for error in CheckBoundaries.check(self.root, self.targets)[0]))

    def test_environment_transitively_rejects_renderer(self):
        self.target('hyperion_environment', 'Source/Runtime/Environment')
        self.target('hyperion_middle', 'Source/Runtime/Middle')
        self.target('hyperion_render', 'Source/Runtime/Renderer')
        self.link('hyperion_environment', 'hyperion_middle')
        self.link('hyperion_middle', 'hyperion_render')
        self.assertTrue(any('CPU domain reaches rendering' in error for error in CheckBoundaries.graph_errors(self.root, self.targets)))

    def test_backend_private_sdk_and_cross_module_private(self):
        directory = 'Source/Backends/Native'
        source = directory + '/Private/Native.cpp'
        self.write(source, '#include <d3d12.h>\n')
        self.target('hyperion_native', directory, [source])
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])
        self.write(source, '#include "../../Other/Private/Secret.h"\n')
        self.write('Source/Backends/Other/Private/Secret.h', '#pragma once\n')
        self.target('hyperion_other', 'Source/Backends/Other')
        self.assertTrue(any('private header crosses' in error for error in CheckBoundaries.check(self.root, self.targets)[0]))

    def test_shared_source_retains_production_rules(self):
        self.include_fixture()
        self.target('a_tests', 'Source/Tests', ['Source/Runtime/A/Private/A.cpp'], test=True)
        self.assertTrue(any('missing direct' in error for error in CheckBoundaries.check(self.root, self.targets)[0]))

    def test_test_sdk_still_requires_adapter(self):
        source = 'Source/Tests/Bad.cpp'
        self.write(source, '#include <Windows.h>\n')
        self.target('bad_tests', 'Source/Tests', [source], test=True)
        self.assertTrue(any('native/vendor' in error for error in CheckBoundaries.check(self.root, self.targets)[0]))

    def test_disabled_include_guards_preserve_source_isolation(self):
        self.include_fixture()
        source = 'Source/Runtime/A/Private/A.cpp'
        self.targets.pop('hyperion_b')
        self.write(source, '#if HYP_ENABLE_RENDERDOC\n#include "Hyperion/B/B.h"\n#endif\n')
        self.assertEqual(CheckBoundaries.check(self.root, self.targets, {'HYP_ENABLE_RENDERDOC': 'OFF'})[0], [])
        for options in ({}, {'HYP_ENABLE_RENDERDOC': 'ON'}):
            self.assertTrue(any('configured production target' in error
                                for error in CheckBoundaries.check(self.root, self.targets, options)[0]))
        self.write(source, '#if HYP_ENABLE_RENDERDOC\n#include <Windows.h>\n#endif\n')
        self.assertTrue(any('native/vendor' in error for error in
                            CheckBoundaries.check(self.root, self.targets, {'HYP_ENABLE_RENDERDOC': 'OFF'})[0]))
        self.write(source, '#if HYP_ENABLE_RENDERDOC\n#else\n#include "Hyperion/B/B.h"\n#endif\n')
        self.assertTrue(CheckBoundaries.check(self.root, self.targets, {'HYP_ENABLE_RENDERDOC': 'OFF'})[0])

    def test_uncompiled_test_support_does_not_relax_production_or_private_checks(self):
        source = 'Source/Runtime/A/Private/Checks.cpp'
        self.target('hyperion_a', 'Source/Runtime/A')
        self.write('Source/Tests/Support/TestSupport.h', '#pragma once\n')
        self.write(source, '#include "Support/TestSupport.h"\n')
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])
        self.targets['hyperion_a']['sources'].append(source)
        self.assertTrue(any('private header crosses' in error for error in CheckBoundaries.check(self.root, self.targets)[0]))
        self.targets['hyperion_a']['sources'].clear()
        self.write('Source/Runtime/B/Private/Secret.h', '#pragma once\n')
        self.write(source, '#include "../../B/Private/Secret.h"\n')
        self.assertTrue(any('private header crosses' in error for error in CheckBoundaries.check(self.root, self.targets)[0]))

    def test_comment_and_string_directives_cannot_hide_active_includes(self):
        self.include_fixture()
        source = 'Source/Runtime/A/Private/A.cpp'
        for prefix, suffix in (('/*\n#if 0\n*/\n', '/*\n#endif\n*/\n'),
                               ('const char* Text = R"guard(\n#if 0\n)guard";\n', ''),
                               ('const auto Amount = 100\'000;\n', 'const char Tag = \'x\';\n'),
                               ('// continued comment \\\n#if 0\n', ''),
                               ('const char* Text = "// /*";\n', '')):
            self.write(source, prefix + '#include "Hyperion/B/B.h"\n' + suffix)
            self.assertTrue(any('missing direct' in error
                                for error in CheckBoundaries.check(self.root, self.targets)[0]))
        self.write(source, '/*\n#include <Windows.h>\n*/\n#if 0 /* actual guard */\n'
                           '#include "Hyperion/B/B.h"\n#endif\n')
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])
        self.write(source, '#undef HYP_ENABLE_RENDERDOC\n#define HYP_ENABLE_RENDERDOC 1\n'
                           '#if HYP_ENABLE_RENDERDOC\n#include "Hyperion/B/B.h"\n#endif\n')
        self.assertTrue(any('missing direct' in error for error in
                            CheckBoundaries.check(self.root, self.targets, {'HYP_ENABLE_RENDERDOC': 'OFF'})[0]))

    def test_expanded_lists_and_all_link_visibilities(self):
        directory = 'Source/Runtime/A'
        self.target('hyperion_a', directory)
        for name in ('b', 'c', 'd'):
            self.target('hyperion_' + name, 'Source/Runtime/' + name.upper())
            self.command('Source/Runtime/' + name.upper(), 'add_library', 'hyperion_' + name, 'INTERFACE')
        self.command(directory, 'add_library', 'hyperion_a', 'STATIC', 'Private/A.cpp;Private/B.cpp')
        self.command(directory, 'target_link_libraries', 'hyperion_a', 'PUBLIC', 'hyperion_b',
                     'PRIVATE', 'hyperion_c', 'INTERFACE', 'hyperion_d')
        target = self.collect()['hyperion_a']
        self.assertEqual(len(target['sources']), 2)
        self.assertEqual([edge['visibility'] for edge in target['links']], ['PUBLIC', 'PRIVATE', 'INTERFACE'])

    def test_unsupported_expressions_properties_and_ambiguity_fail(self):
        directory = 'Source/Runtime/A'
        self.target('hyperion_a', directory)
        self.command(directory, 'add_library', 'hyperion_a', 'INTERFACE')
        for command, args in [('target_link_libraries', ('hyperion_a', 'PRIVATE', '$<$<CONFIG:Debug>:hyperion_a>')),
                              ('target_sources', ('hyperion_a', 'PRIVATE', '$<TARGET_OBJECTS:hyperion_a>')),
                              ('set_property', ('TARGET', 'hyperion_a', 'APPEND', 'PROPERTY', 'LINK_LIBRARIES', 'hyperion_a'))]:
            self.command(directory, command, *args)
            with self.assertRaisesRegex(ValueError, 'unsupported'):
                self.collect()
            self.trace.pop()

    def test_freshness_input_inventory_and_cache(self):
        build = self.root / 'out/build'
        cache = self.write('out/build/CMakeCache.txt', 'BUILD_TESTING:BOOL=ON\n')
        trace = self.write('Trace.jsonl', '')
        TargetGraph.export_graph(self.root, build, trace)
        self.assertEqual(TargetGraph.load_graph(self.root, build)['options']['BUILD_TESTING'], 'ON')
        self.write('Source/New.cpp', '// new\n')
        with self.assertRaisesRegex(ValueError, 'stale'):
            TargetGraph.load_graph(self.root, build)
        TargetGraph.export_graph(self.root, build, trace)
        cache.write_text('BUILD_TESTING:BOOL=OFF\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'stale'):
            TargetGraph.load_graph(self.root, build)
        TargetGraph.export_graph(self.root, build, trace)
        self.write('CMakeLists.txt', '# changed\n')
        with self.assertRaisesRegex(ValueError, 'stale'):
            TargetGraph.load_graph(self.root, build)

    def test_directory_links_and_direct_injection_fail(self):
        directory = 'Source/Runtime/Environment'
        self.target('hyperion_environment', directory)
        self.command(directory, 'add_library', 'hyperion_environment', 'INTERFACE')
        for command, args in [('link_libraries', ('hyperion_rhi',)),
                              ('set_property', ('TARGET', 'hyperion_environment', 'PROPERTY',
                                                'INTERFACE_LINK_LIBRARIES_DIRECT', 'hyperion_rhi')),
                              ('set_property', ('DIRECTORY', 'PROPERTY', 'LINK_LIBRARIES', 'hyperion_rhi'))]:
            self.command(directory, command, *args)
            with self.assertRaisesRegex(ValueError, 'unsupported'):
                self.collect()
            self.trace.pop()

    def test_helper_target_declaration_fails_with_location(self):
        self.write('cmake/Owned.cmake', '# fixture\n')
        self.trace.append({'file': str(self.root / 'cmake/Owned.cmake'), 'line': 2,
                           'cmd': 'add_library', 'args': ['hyperion_environment', 'INTERFACE']})
        with self.assertRaisesRegex(ValueError, 'cmake/Owned.cmake:2: unsupported target declaration'):
            self.collect()
        self.write('Source/Runtime/Renderer/Owned.cmake', '# fixture\n')
        self.trace = [{'file': str(self.root / 'Source/Runtime/Renderer/Owned.cmake'), 'line': 3,
                       'cmd': 'add_library', 'args': ['hyperion_environment', 'INTERFACE'], 'frame': 2}]
        with self.assertRaisesRegex(ValueError, 'unsupported helper target declaration'):
            self.collect()
        self.target('hyperion_a', 'Source/Runtime/A')
        self.trace = [{'file': str(self.root / 'Source/Runtime/A/CMakeLists.txt'), 'line': 3,
                       'cmd': 'add_library', 'args': ['hyperion_environment', 'INTERFACE'], 'frame': 2}]
        with self.assertRaisesRegex(ValueError, 'unsupported helper target declaration'):
            self.collect()

    def test_public_and_interface_source_propagation_fail(self):
        directory = 'Source/Runtime/A'
        self.target('hyperion_a', directory)
        self.command(directory, 'add_library', 'hyperion_a', 'STATIC', 'Private/Own.cpp')
        for visibility in ('PUBLIC', 'INTERFACE'):
            self.command(directory, 'target_sources', 'hyperion_a', visibility, 'Private/Shared.cpp')
            with self.assertRaisesRegex(ValueError, 'unsupported PUBLIC/INTERFACE source propagation'):
                self.collect()
            self.trace.pop()

    def test_unknown_helper_relative_sources_fail(self):
        directory = 'Source/Runtime/A'
        self.target('hyperion_a', directory)
        self.command(directory, 'add_library', 'hyperion_a', 'STATIC', 'Private/Own.cpp')
        self.command(directory, 'target_sources', 'hyperion_a', 'PRIVATE', 'Private/Shared.cpp')
        self.trace[-1]['frame'] = 2
        with self.assertRaisesRegex(ValueError, 'unsupported helper relative source'):
            self.collect()
        absolute = str(self.write('Source/Runtime/B/Private/Shared.cpp', '// fixture\n'))
        self.trace[-1]['args'][-1] = absolute
        self.assertIn('Source/Runtime/B/Private/Shared.cpp', self.collect()['hyperion_a']['sources'])


if __name__ == '__main__':
    unittest.main()
