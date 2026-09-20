"""Shared submitted-scene and GUI validation for RenderDoc XML captures."""
from collections import Counter
import re


def verify_capture(document, experiment):
    # Recorders interleave chunks: track marker scopes per command list, then count
    # draws only when that list is submitted. GUI draws cannot stand in for a scene.
    markers = {}
    recorded = {}
    submitted = Counter()
    chunks = []
    for node in document.iter('chunk'):
        name = node.get('name', '')
        chunks.append(name)
        command_list = node.findtext("ResourceId[@name='pCommandList']", '')
        if name == 'ID3D12GraphicsCommandList::Reset':
            markers[command_list] = []
            recorded[command_list] = Counter()
        elif name == 'ID3D12GraphicsCommandList::BeginEvent':
            markers.setdefault(command_list, []).append(node.findtext("string[@name='MarkerText']", ''))
        elif name == 'ID3D12GraphicsCommandList::EndEvent':
            if markers.get(command_list):
                markers[command_list].pop()
        elif name == 'ID3D12GraphicsCommandList::DrawIndexedInstanced':
            indices = int(node.findtext("uint[@name='IndexCountPerInstance']", '0'))
            instances = int(node.findtext("uint[@name='InstanceCount']", '0'))
            if indices > 0 and instances > 0:
                recorded.setdefault(command_list, Counter()).update(markers.get(command_list, []))
        elif name == 'ID3D12CommandQueue::ExecuteCommandLists':
            for submitted_list in node.findall("array[@name='ppCommandLists']/ResourceId"):
                submitted.update(recorded.get(submitted_list.text, {}))
    # Ordinary sessions retain their structured identity. The forward pipeline owns stage names.
    scene_draws = sum(count for marker, count in submitted.items()
                      if re.fullmatch(r'(Scene [1-9]\d*/[1-9]\d*/[1-9]\d*/[1-9]\d*/Forward|Forward|Forward/HDR|Deferred/BasePass|Deferred/Compatibility|Scene/Transparent|Display/LegacyMaterials)/\d+', marker))
    assert scene_draws > 0, f'{experiment}: missing submitted session scene draws'
    if experiment == 'Shadows':
        for cascade in range(4):
            assert submitted[f'Shadow cascade {cascade}/0'] > 0, f'missing cascade {cascade} draws'
    # Shared GuiRenderer uses the session preparation path; legacy standalone renderers use Debug UI.
    assert submitted['GUI/0'] + submitted['Debug UI/0'] > 0, f'{experiment}: missing submitted GUI draws'
    assert any('Present' in name for name in chunks), f'{experiment}: missing Present'
    assert any('CreateGraphicsPipeline' in name for name in chunks), f'{experiment}: missing pipeline'
    return submitted
