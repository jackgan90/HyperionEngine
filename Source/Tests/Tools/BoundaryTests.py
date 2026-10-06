"""Adversarial fixtures for configured target contracts and source isolation."""

import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from DevelopmentPaths import test_root
import CheckBoundaries
import TargetGraph


class BoundaryTests(unittest.TestCase):
    def setUp(self):
        output = test_root() / "boundary-tests"
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

    def test_owned_test_source_local_headers(self):
        directory = 'Source/Plugins/A'
        source = directory + '/Tests/Acceptance/Scenario.cpp'
        self.write(source, '#include "Harness.h"\n#include "../../Private/Owner.h"\n')
        self.write(directory + '/Tests/Acceptance/Harness.h', '#pragma once\n')
        self.write(directory + '/Private/Owner.h', '#pragma once\n')
        self.target('hyperion_a', directory, [source])
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])

    def test_production_and_public_cannot_include_test_headers(self):
        directory = 'Source/Plugins/A'
        source = directory + '/Private/Owner.cpp'
        self.write(source, '#include "../Tests/Harness.h"\n')
        self.write(directory + '/Tests/Harness.h', '#pragma once\n')
        self.write(directory + '/Public/Hyperion/A/A.h', '#include "../../../Tests/Harness.h"\n')
        self.target('hyperion_a', directory, [source])
        errors = CheckBoundaries.check(self.root, self.targets)[0]
        self.assertEqual(len(errors), 2)
        self.assertTrue(all('private header crosses' in error for error in errors))

    def test_foreign_module_test_header_is_private(self):
        directory = 'Source/Plugins/A'
        source = directory + '/Tests/Scenario.cpp'
        self.write(source, '#include "../../B/Tests/Harness.h"\n')
        self.write('Source/Plugins/B/Tests/Harness.h', '#pragma once\n')
        self.target('hyperion_a', directory, [source])
        self.target('hyperion_b', 'Source/Plugins/B')
        self.assertTrue(any('private header crosses' in error for error in
                            CheckBoundaries.check(self.root, self.targets)[0]))

    def test_global_test_support_cannot_hide_resolved_local_header(self):
        directory = 'Source/Plugins/A'
        source = directory + '/Private/Owner.cpp'
        self.write(source, '#include "../Tests/Harness.h"\n')
        self.write(directory + '/Tests/Harness.h', '#pragma once\n')
        self.write('Source/Tests/Harness.h', '#pragma once\n')
        self.target('a_tests', directory, [source], test=True)
        self.assertTrue(any('private header crosses' in error for error in
                            CheckBoundaries.check(self.root, self.targets)[0]))
        self.write(source, '#include "Harness.h"\n')
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])
        root_source = 'Source/Tests/Consumer.cpp'
        self.write(root_source, '#include "Harness.h"\n')
        self.target('consumer_tests', 'Source/Tests', [root_source], test=True)
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])

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

    def test_test_header_dependencies_belong_to_actual_consumer(self):
        directory = 'Source/Runtime/A'
        source = directory + '/Tests/Scenario.cpp'
        helper = directory + '/Tests/Support.h'
        self.write(source, '#include "Support.h"\n')
        self.write(helper, '#include "Hyperion/Renderer/Renderer.h"\n')
        self.write('Source/Runtime/Renderer/Public/Hyperion/Renderer/Renderer.h', '#pragma once\n')
        self.target('hyperion_a', directory)
        self.target('hyperion_renderer', 'Source/Runtime/Renderer')
        self.target('a_tests', directory, [source], test=True)
        self.link('a_tests', 'hyperion_renderer')
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])
        # Acceptance implementations can also be explicitly compiled by a production target.
        self.targets['hyperion_a']['sources'].append(source)
        errors = CheckBoundaries.check(self.root, self.targets)[0]
        self.assertTrue(any('Support.h:1: hyperion_a missing direct' in error for error in errors))
        self.link('hyperion_a', 'hyperion_renderer')
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])

    def test_nested_private_header_uses_actual_production_targets(self):
        directory = 'Source/Plugins/A'
        helper = directory + '/Private/Support.h'
        nested = directory + '/Private/Nested.h'
        source = directory + '/Private/Owner.cpp'
        self.write(source, '#include "Support.h"\n')
        self.write(helper, '#include "Nested.h"\n')
        self.write(nested, '#include "Hyperion/B/B.h"\n')
        self.write('Source/Runtime/B/Public/Hyperion/B/B.h', '#pragma once\n')
        self.target('hyperion_a', directory)
        self.target('hyperion_consumer', directory, [source])
        self.target('hyperion_b', 'Source/Runtime/B')
        self.link('hyperion_consumer', 'hyperion_b')
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])
        self.targets['hyperion_a']['sources'].append(helper)
        self.assertTrue(any('Nested.h:1: hyperion_a missing direct' in error for error in
                            CheckBoundaries.check(self.root, self.targets)[0]))

    def test_explicit_and_unselected_test_headers(self):
        directory = 'Source/Plugins/A'
        header = directory + '/Tests/Support.h'
        self.write(header, '#include "Hyperion/B/B.h"\n')
        self.write('Source/Runtime/B/Public/Hyperion/B/B.h', '#pragma once\n')
        self.target('hyperion_a', directory)
        self.target('hyperion_b', 'Source/Runtime/B')
        self.target('a_tests', directory, [header], test=True)
        self.link('a_tests', 'hyperion_b')
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])
        del self.targets['a_tests']
        self.assertEqual(CheckBoundaries.check(self.root, self.targets)[0], [])
        self.targets['hyperion_a']['sources'].append(header)
        self.assertTrue(any('Support.h:1: hyperion_a missing direct' in error for error in
                            CheckBoundaries.check(self.root, self.targets)[0]))

    def test_test_header_preserves_vendor_and_private_isolation(self):
        directory = 'Source/Runtime/A'
        source = directory + '/Tests/Scenario.cpp'
        self.write(source, '#include "Support.h"\n')
        self.write(directory + '/Tests/Support.h', '#include <SDL3/SDL.h>\n#include "../../B/Private/B.h"\n')
        self.write('Source/Runtime/B/Private/B.h', '#pragma once\n')
        self.target('hyperion_a', directory)
        self.target('hyperion_b', 'Source/Runtime/B')
        self.target('a_tests', directory, [source], test=True)
        errors = CheckBoundaries.check(self.root, self.targets)[0]
        self.assertTrue(any('native/vendor include' in error and 'Tests/Support.h' in error for error in errors))
        self.assertTrue(any('private header crosses' in error and 'Tests/Support.h' in error for error in errors))

    def test_explicit_test_source_cannot_relax_public_header_isolation(self):
        directory = 'Source/Runtime/A'
        header = directory + '/Public/Hyperion/A/A.h'
        self.write(header, '#include "Support/TestSupport.h"\n')
        self.write('Source/Tests/Support/TestSupport.h', '#pragma once\n')
        self.target('hyperion_a', directory)
        self.target('a_tests', directory, [header], test=True)
        self.assertTrue(any('private header crosses' in error and 'Public/Hyperion/A/A.h' in error for error in
                            CheckBoundaries.check(self.root, self.targets)[0]))

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

    def test_render_controls_rejects_indirect_rendering_dependencies(self):
        self.target('hyperion_render_controls', 'Source/Runtime/RenderControls')
        self.target('hyperion_middle', 'Source/Runtime/Middle')
        self.link('hyperion_render_controls', 'hyperion_middle', 'PUBLIC')
        for name, directory in (('hyperion_render', 'Source/Runtime/Renderer'),
                                ('hyperion_rhi', 'Source/Runtime/RHI'),
                                ('hyperion_native', 'Source/Backends/Native')):
            with self.subTest(target=name):
                self.target(name, directory)
                self.link('hyperion_middle', name)
                errors = CheckBoundaries.graph_errors(self.root, self.targets)
                self.assertTrue(any('hyperion_render_controls: CPU domain reaches rendering' in error
                                    and 'hyperion_middle' in error and 'PUBLIC' in error for error in errors))
                self.targets['hyperion_middle']['links'].clear()

    def test_rhi_rejects_direct_and_transitive_assets(self):
        self.target('hyperion_rhi', 'Source/Runtime/RHI')
        self.target('hyperion_middle', 'Source/Runtime/Middle')
        self.target('hyperion_assets', 'Source/Runtime/Assets')
        self.link('hyperion_rhi', 'hyperion_middle', 'PUBLIC')
        self.link('hyperion_middle', 'hyperion_assets')
        errors = CheckBoundaries.graph_errors(self.root, self.targets)
        self.assertTrue(any('RHI reaches Assets services' in error and 'hyperion_middle' in error
                            and 'PUBLIC' in error and 'CMakeLists.txt:1' in error for error in errors))
        self.targets['hyperion_rhi']['links'].clear()
        self.link('hyperion_rhi', 'hyperion_assets', 'PUBLIC')
        self.assertTrue(any('RHI reaches Assets services' in error
                            for error in CheckBoundaries.graph_errors(self.root, self.targets)))

    def test_image_data_has_no_owned_dependencies(self):
        self.target('hyperion_image_data', 'Source/Runtime/ImageData')
        self.target('hyperion_reflection', 'Source/Runtime/Reflection')
        self.target('hyperion_rhi', 'Source/Runtime/RHI')
        self.link('hyperion_rhi', 'hyperion_image_data', 'PUBLIC')
        self.assertEqual(CheckBoundaries.graph_errors(self.root, self.targets), [])
        self.link('hyperion_image_data', 'hyperion_reflection', 'INTERFACE')
        self.assertTrue(any('ImageData must be independent' in error
                            for error in CheckBoundaries.graph_errors(self.root, self.targets)))

    def test_non_owned_wrapper_cannot_hide_owned_links(self):
        self.target('hyperion_assets', 'Source/Runtime/Assets')
        self.target('hyperion_rhi', 'Source/Runtime/RHI')
        self.command('Source/Runtime/Assets', 'add_library', 'hyperion_assets', 'INTERFACE')
        dependency = self.write('cmake/Dependencies.cmake', '# fixture\n')
        self.trace.append({'file': str(dependency), 'line': 1, 'cmd': 'add_library',
                           'args': ['hyp_bridge', 'INTERFACE']})
        self.command('Source/Runtime/RHI', 'add_library', 'hyperion_rhi', 'INTERFACE')
        self.command('Source/Runtime/RHI', 'target_link_libraries', 'hyperion_rhi', 'INTERFACE', 'hyp_bridge')
        for link in ('hyperion_assets', '$<BUILD_INTERFACE:hyperion_assets>'):
            with self.subTest(link=link):
                self.trace.append({'file': str(dependency), 'line': 2, 'cmd': 'target_link_libraries',
                                   'args': ['hyp_bridge', 'INTERFACE', link]})
                with self.assertRaisesRegex(ValueError, 'non-owned target hyp_bridge links owned targets'):
                    self.collect()
                self.trace.pop()

    def test_image_data_rejects_non_owned_links(self):
        self.target('hyperion_image_data', 'Source/Runtime/ImageData')
        self.command('Source/Runtime/ImageData', 'add_library', 'hyperion_image_data', 'INTERFACE')
        self.command('Source/Runtime/ImageData', 'target_link_libraries', 'hyperion_image_data',
                     'INTERFACE', 'hyp_codec')
        with self.assertRaisesRegex(ValueError, 'ImageData must be independent'):
            self.collect()

    def test_non_owned_graph_properties_cannot_hide_owned_links(self):
        self.target('hyperion_assets', 'Source/Runtime/Assets')
        self.command('Source/Runtime/Assets', 'add_library', 'hyperion_assets', 'INTERFACE')
        dependency = self.write('cmake/Dependencies.cmake', '# fixture\n')
        self.trace.append({'file': str(dependency), 'line': 1, 'cmd': 'set_target_properties',
                           'args': ['hyp_bridge', 'PROPERTIES', 'INTERFACE_LINK_LIBRARIES',
                                    '$<BUILD_INTERFACE:hyperion_assets>']})
        with self.assertRaisesRegex(ValueError, 'unsupported graph property mutation'):
            self.collect()

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
