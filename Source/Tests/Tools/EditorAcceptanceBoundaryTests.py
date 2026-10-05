"""Positive and negative fixtures for Editor test ownership and build selection."""

import importlib.util
from pathlib import Path
import tempfile


module_path = Path(__file__).resolve().parents[1] / 'Integration/EditorAcceptanceBoundary.py'
spec = importlib.util.spec_from_file_location('editor_boundary', module_path)
boundary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(boundary)


def rejects(operation):
    try:
        operation()
    except AssertionError:
        return
    raise AssertionError('Invalid ownership or selection was accepted')


with tempfile.TemporaryDirectory(prefix='EditorBoundary-') as temporary:
    editor = Path(temporary)
    private = editor / 'Private'
    acceptance = editor / 'Tests/Acceptance'
    unit = editor / 'Tests/Unit'
    for directory in (private, acceptance, unit):
        directory.mkdir(parents=True)
    for name in ('EditorApplication.h', 'EditorAcceptanceDriver.h', 'EditorOptions.h',
                 'EditorAcceptanceUnavailable.cpp', 'EditorAcceptanceReport.cpp', 'Production.cpp'):
        (private / name).write_text('', encoding='utf-8')
    scenario = acceptance / 'ArbitraryScenario.cpp'
    scenario.write_text('', encoding='utf-8')
    header = acceptance / 'State.h'
    header.write_text('', encoding='utf-8')
    test = unit / 'Entry.cpp'
    test.write_text('', encoding='utf-8')
    report = private / 'EditorAcceptanceReport.cpp'
    unavailable = private / 'EditorAcceptanceUnavailable.cpp'
    boundary.check_sources(private)
    boundary.check_selection(private, {report, scenario}, True, 'ON fixture')
    boundary.check_selection(private, {report, unavailable}, False, 'OFF fixture')
    rejects(lambda: boundary.check_selection(private, {report}, True, 'missing arbitrary scenario'))
    rejects(lambda: boundary.check_selection(private, {report, unavailable, scenario}, False, 'OFF scenario leak'))
    rejects(lambda: boundary.check_selection(private, {report, unavailable, test}, False, 'OFF unit leak'))
    rejects(lambda: boundary.check_selection(private, {report, scenario, test}, True, 'unit compiled into plugin'))
    (private / 'Production.cpp').write_text('#include "Bridge.h"\n', encoding='utf-8')
    (private / 'Bridge.h').write_text('#include "../Tests/Acceptance/State.h"\n', encoding='utf-8')
    rejects(lambda: boundary.check_sources(private))

    (private / 'Production.cpp').write_text('', encoding='utf-8')
    adapters = private / 'Adapters'
    adapters.mkdir()
    nested = adapters / 'Production.cpp'
    nested.write_text('#include "../../Tests/Acceptance/State.h"\n', encoding='utf-8')
    rejects(lambda: boundary.check_sources(private))
    nested.write_text('', encoding='utf-8')
    public = editor / 'Public/Hyperion/Editor'
    public.mkdir(parents=True)
    (public / 'Entry.h').write_text('#include "../../../Tests/Acceptance/State.h"\n', encoding='utf-8')
    (private / 'Production.cpp').write_text('#include "Hyperion/Editor/Entry.h"\n', encoding='utf-8')
    rejects(lambda: boundary.check_sources(private))
    cases = acceptance / 'Cases'
    cases.mkdir()
    (cases / 'NestedScenario.cpp').write_text('', encoding='utf-8')
    rejects(lambda: boundary.check_selection(private, {report, scenario}, True, 'missing nested scenario'))
    boundary.check_selection(private, {report, scenario, cases / 'NestedScenario.cpp'}, True, 'complete nested scenario')

print('Editor acceptance ownership: 3 positive and 8 negative fixtures passed')
