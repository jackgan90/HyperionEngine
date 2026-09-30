"""Verify shared selection framing through real CLI/MCP connections to Editor."""
import copy
import itertools
import math
import pathlib
import subprocess
import sys

from AutomationAcceptance import completed
from AttachmentAcceptance import Application, AttachedSession, ready


def request(info):
    return {"document": info["document"], "revision": info["revision"]}


def focus(camera):
    matrix = camera["world"]["values"]
    distance = camera["lens"]["focusDistance"]
    return [matrix[12 + index] - matrix[8 + index] * distance for index in range(3)]


def check_center(camera, expected):
    actual = focus(camera)
    assert all(math.isclose(left, right, abs_tol=1e-4) for left, right in zip(actual, expected)), actual


def check_clipping(camera, positions):
    matrix = camera["world"]["values"]
    for position in positions:
        for offset in itertools.product((-0.5, 0.5), repeat=3):
            depth = -sum((position[index] + offset[index] - matrix[12 + index]) * matrix[8 + index]
                         for index in range(3))
            assert camera["lens"]["near"] < depth < camera["lens"]["far"], (depth, camera)


def check_unchanged(agent, peer, before, info, selection):
    assert completed(peer.call("view.get"))["camera"] == before
    current = ready(agent)
    for key in ("document", "revision", "dirty", "canUndo", "canRedo"):
        assert current[key] == info[key], (key, info, current)
    assert completed(peer.call("scene.selection.get", **request(info))) == selection


def verify_framing(agent, peer):
    definition = agent.request("api.describe", {"operation": "view.frame_selection"})
    assert not definition["unavailable"], definition
    assert {"document", "revision"} <= set(definition["inputSchema"]["required"])
    assert "camera" in definition["outputSchema"]["properties"]
    found = agent.request("api.search", {"query": "frame selection"})
    assert "view.frame_selection" in {item["id"] for item in found["items"]}, found
    info = ready(agent)
    catalog = completed(agent.call("scene.placement.list"))
    cube = next(item for item in catalog["items"] if item["label"] == "Cube" and not item["unavailable"])
    handles = []
    for position in ({"x": -300, "y": 0, "z": 0}, {"x": 300, "y": 1, "z": 0}):
        placed = completed(agent.wait(agent.call("scene.placement.place", **request(info),
                                                object=cube["id"], position=position)))
        handles.append(placed["handle"])
        info = ready(agent)
    completed(agent.wait(agent.call("scene.placement.place", **request(info), object=cube["id"],
                                    position={"x": -300, "y": 0, "z": -60})))
    info = ready(agent)
    original = completed(agent.call("view.get"))["camera"]
    original = copy.deepcopy(original)
    original["world"]["values"] = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 2, 20, 1]
    original["lens"]["far"] = 5000
    completed(agent.call("view.set", **request(info), camera=original))
    selection = completed(agent.call("scene.selection.set", **request(info), handles=handles))
    definition = agent.request("api.describe", {"operation": "scene.selection.select_all"})
    assert not definition["unavailable"] and "count" in definition["outputSchema"]["properties"], definition
    found = peer.request("api.search", {"query": "scene.selection.select_all"})
    assert "scene.selection.select_all" in {item["id"] for item in found["items"]}, found
    summary = completed(peer.call("scene.selection.select_all", **request(info)))
    all_selection = completed(agent.call("scene.selection.get", **request(info)))
    assert int(summary["count"]) == len(all_selection["handles"]) >= len(handles)
    assert summary["primary"] == handles[-1] == all_selection["primary"]
    current = ready(agent)
    assert all(current[key] == info[key] for key in ("document", "revision", "dirty", "canUndo", "canRedo"))
    selection = completed(agent.call("scene.selection.set", **request(info), handles=handles))
    multiple = completed(agent.call("view.frame_selection", **request(info)))["camera"]
    check_center(multiple, [0, 0.5, 0])
    assert multiple["lens"]["far"] >= original["lens"]["far"], (original, multiple)
    assert multiple["lens"]["verticalRadians"] == original["lens"]["verticalRadians"]
    assert multiple["world"]["values"][:12] == original["world"]["values"][:12]
    check_unchanged(agent, peer, multiple, info, selection)

    completed(peer.call("view.set", **request(info), camera=original))
    completed(peer.call("scene.selection.set", **request(info), handles=handles[::-1]))
    reversed_camera = completed(peer.call("view.frame_selection", **request(info)))["camera"]
    assert reversed_camera == multiple
    completed(agent.call("scene.selection.set", **request(info), handles=[handles[0]]))
    assert completed(peer.call("view.get"))["camera"] == multiple
    single = completed(agent.call("view.frame_selection", **request(info)))["camera"]
    check_center(single, [-300, 0, 0])
    assert single["lens"]["focusDistance"] < multiple["lens"]["focusDistance"]
    assert single["lens"]["far"] >= multiple["lens"]["far"], (multiple, single)
    check_clipping(single, [(-300, 0, -60)])
    retreated = copy.deepcopy(single)
    retreated["world"]["values"][14] += 100
    completed(agent.call("view.set", **request(info), camera=retreated))
    check_clipping(completed(peer.call("view.get"))["camera"], [(-300, 0, 0), (-300, 0, -60)])
    completed(agent.call("view.set", **request(info), camera=single))

    selection = completed(agent.call("scene.selection.get", **request(info)))
    for arguments, code in ((dict(request(info), document="expired-document"), "stale_document"),
                            (dict(request(info), revision=str(int(info["revision"]) - 1)), "stale_revision")):
        result = agent.call("view.frame_selection", **arguments)
        assert result["error"]["code"] == code, result
        check_unchanged(agent, peer, single, info, selection)
    malformed = agent.call("view.frame_selection", document=info["document"], revision="bad")
    assert malformed["error"]["code"] == "invalid_arguments", malformed
    check_unchanged(agent, peer, single, info, selection)

    empty = completed(agent.call("scene.selection.set", **request(info), handles=[]))
    assert completed(agent.call("view.frame_selection", **request(info)))["camera"] == single
    check_unchanged(agent, peer, single, info, empty)
    completed(agent.call("view.frame_scene", **request(info)))
    scene = completed(peer.call("view.get"))["camera"]
    check_center(scene, [0, 0.5, -30])
    assert scene["lens"]["far"] >= single["lens"]["far"], (single, scene)
    check_clipping(scene, [(-300, 0, 0), (300, 1, 0), (-300, 0, -60)])
    return info


def verify_preview_and_fallbacks(agent, peer, info):
    camera_type = "hyperion.scene.camera"
    types = completed(agent.call("scene.component_types.list"))["components"]
    camera_type = next(item["type"] for item in types if "camera" in item["type"])
    node = completed(agent.call("scene.node.create", **request(info), name="Framing camera",
                                components=[camera_type]))
    info = ready(agent)
    selected = node["handle"]
    before = completed(agent.call("view.get"))["camera"]
    authored = completed(agent.call("scene.node.get", document=info["document"], handle=selected))
    completed(agent.call("view.preview_camera", **request(info), camera=selected))
    denied = agent.call("view.frame_selection", **request(info))
    assert denied["error"]["code"] == "unavailable", denied
    assert completed(peer.call("view.get"))["camera"] == before
    assert completed(peer.call("scene.node.get", document=info["document"], handle=selected)) == authored
    completed(agent.call("view.preview_camera", **request(info), camera=None))
    framed = completed(agent.call("view.frame_selection", **request(info)))["camera"]
    values = authored["world"]["values"]
    check_center(framed, values[12:15])
    assert framed["lens"]["focusDistance"] > 0
    info = ready(agent)
    group = completed(agent.call("scene.node.create", **request(info), name="Empty framing group"))
    info = ready(agent)
    before = completed(agent.call("view.frame_selection", **request(info)))["camera"]
    check_center(before, [0, 0, 0])
    assert group["handle"] == completed(peer.call("scene.selection.get", **request(info)))["primary"]


def workflow(cli, editor, assets, output):
    app = Application(editor, output, "selection-framing", "", assets, frames=0)
    sessions = []
    try:
        agent = AttachedSession(cli, app.target(), True)
        peer = AttachedSession(cli, app.instance, False)
        sessions += [agent, peer]
        info = verify_framing(agent, peer)
        verify_preview_and_fallbacks(agent, peer, info)
        completed(agent.call("application.close.request", action=2))
        for session in sessions:
            session.close()
        sessions.clear()
        app.finish()
    finally:
        for session in sessions:
            session.close()
        app.close()
    print("PASS: selection framing discovery, CLI/MCP parity, state invariance, stale/preview errors and normal shutdown")


if __name__ == "__main__":
    cli, editor, fixture, output = [pathlib.Path(value).resolve() for value in sys.argv[1:]]
    output.mkdir(parents=True, exist_ok=True)
    assets = output / "Assets"
    subprocess.run([str(fixture), str(assets)], check=True)
    workflow(cli, editor, assets / "Game", output)
