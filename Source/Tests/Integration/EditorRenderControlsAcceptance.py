"""Rendering HUD GUI parity and scene-owned light shadow persistence."""
import copy
import json
import pathlib
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "Automation"))
from AutomationAcceptance import completed
from AttachmentAcceptance import Application, AttachedSession, ready
from DiscoveryEnvironment import ensure_isolated_discovery


LIGHT = "hyperion.scenedirectionallight"


def version(agent):
    info = ready(agent)
    return {key: info[key] for key in ("document", "revision")}


def read_light(agent, handle):
    return completed(agent.call(f"scene.component.{LIGHT}.get", **version(agent),
                                handle=handle, component=LIGHT))


def write_light(agent, handle, value):
    return completed(agent.call(f"scene.component.{LIGHT}.set", **version(agent),
                                handles=[handle], component=LIGHT, value=value))


def wait_contact(agent, expected, shadow_bytes=None):
    deadline = time.monotonic() + 20
    while True:
        stats = completed(agent.call("render.statistics"))
        assert int(stats["device"]["validationErrors"]) == 0, stats
        if (stats["pipeline"]["contactShadows"] == expected
                and (shadow_bytes is None or int(stats["pipeline"]["shadowTextureBytes"]) == shadow_bytes)):
            return stats
        assert time.monotonic() < deadline, stats
        time.sleep(.02)


def viewport_controls(agent):
    original = ready(agent)
    for mode in range(7):
        state = completed(agent.call("view.set", **version(agent),
                                    options={"statusHud": True, "profilingHud": True,
                                             "profilingCategories": 1 << (mode % 6), "visualizer": mode,
                                             "exposure": .05 if mode == 0 else 1.25}))
        assert state["options"]["visualizer"] == mode, state
        assert state["options"]["profilingCategories"] == 1 << (mode % 6), state
    for category in (64, 128, 255):
        state = completed(agent.call("view.set", **version(agent), options={"profilingCategories": category}))
        assert state["options"]["profilingCategories"] == category, state
    for options in ({"visualizer": -1}, {"visualizer": 7}, {"visualizer": 4294967295},
                    {"profilingCategories": 256}, {"exposure": 0}):
        before = completed(agent.call("view.get"))
        result = agent.call("view.set", **version(agent), options=options)
        assert result["error"]["code"] == "invalid_arguments", result
        assert completed(agent.call("view.get")) == before
    settings = completed(agent.call("render.settings.get"))
    settings["values"]["pipeline"] = "forward"
    changed = completed(agent.call("render.settings.set", revision=settings["revision"], values=settings["values"]))
    # A latent Deferred visualizer must not prevent changing unrelated HUD options in Forward.
    completed(agent.call("view.set", **version(agent), options={"statusHud": False, "profilingCategories": 63}))
    before_rejection = completed(agent.call("render.settings.get"))
    before_view = completed(agent.call("view.get"))
    assert before_rejection["values"]["debugMode"] == 6
    result = agent.call("view.set", **version(agent), options={"visualizer": 2})
    assert result["error"]["code"] == "unavailable", result
    assert completed(agent.call("render.settings.get")) == before_rejection
    assert completed(agent.call("view.get")) == before_view
    changed["values"]["pipeline"] = "deferred"
    restored = completed(agent.call("render.settings.set", revision=changed["revision"], values=changed["values"]))
    assert restored["values"]["debugMode"] == 6
    assert completed(agent.call("view.get"))["options"]["visualizer"] == 6
    completed(agent.call("view.set", **version(agent), options={"visualizer": 0, "statusHud": True}))
    assert ready(agent)["revision"] == original["revision"] and not ready(agent)["dirty"]


def author_shadows(agent, output):
    schema = agent.request("types.describe", {"type": LIGHT})
    assert "shadowSettings" in schema["properties"], schema
    for type_id in ("hyperion.light.shadows", "hyperion.light.directional-shadows", "hyperion.contact.settings"):
        assert agent.request("types.describe", {"type": type_id})["x-hyperion-type"] == type_id
    for type_id in ("hyperion.scenepointlight", "hyperion.scenespotlight"):
        assert "shadowSettings" not in agent.request("types.describe", {"type": type_id})["properties"]
        local = completed(agent.call("scene.node.create", **version(agent), name="Unsupported shadows",
                                     components=[type_id]))["handle"]
        value = completed(agent.call(f"scene.component.{type_id}.get", **version(agent),
                                      handle=local, component=type_id))
        value["shadowSettings"] = {"contact": {"enabled": True}}
        before = version(agent)
        result = agent.call(f"scene.component.{type_id}.set", **before, handles=[local], component=type_id, value=value)
        assert result["error"]["code"] == "invalid_arguments", result
        assert version(agent) == before
        completed(agent.call("scene.undo", **version(agent)))
        assert not ready(agent)["dirty"]
    main = completed(agent.call("scene.lighting.get"))["shadowDirectionalLight"]
    value = read_light(agent, main)
    assert value["shadowSettings"] is None, value
    settings = completed(agent.call("render.settings.get"))["values"]
    shadow = {"directional": settings["shadows"], "contact": settings["contact"]}
    shadow["directional"].pop("previewViewport", None)
    shadow["directional"]["resolution"] = 1024
    shadow["contact"].update(enabled=True, steps=24, debugMode=0)
    value["shadowSettings"] = shadow
    changed = write_light(agent, main, value)
    assert changed["dirty"] and read_light(agent, main)["shadowSettings"] == shadow
    wait_contact(agent, True, 4 * 1024 * 1024 * 4)
    before = version(agent)
    invalid = copy.deepcopy(value)
    invalid["shadowSettings"]["directional"]["resolution"] = 32
    response = agent.call(f"scene.component.{LIGHT}.set", **before, handles=[main], component=LIGHT, value=invalid)
    assert response["error"]["code"] == "invalid_arguments", response
    assert version(agent) == before and read_light(agent, main) == value
    completed(agent.call("scene.undo", **version(agent)))
    assert read_light(agent, main)["shadowSettings"] is None and not ready(agent)["dirty"]
    wait_contact(agent, False, 4 * 2048 * 2048 * 4)
    completed(agent.call("scene.redo", **version(agent)))
    assert read_light(agent, main)["shadowSettings"] == shadow
    wait_contact(agent, True, 4 * 1024 * 1024 * 4)
    other = completed(agent.call("scene.node.create", **version(agent), name="Second sun", components=[LIGHT]))["handle"]
    other_value = read_light(agent, other)
    other_value["priority"] = 10
    write_light(agent, other, other_value)
    assert completed(agent.call("scene.lighting.get"))["shadowDirectionalLight"] == other
    wait_contact(agent, False, 4 * 2048 * 2048 * 4)
    value["priority"] = 20
    write_light(agent, main, value)
    assert completed(agent.call("scene.lighting.get"))["shadowDirectionalLight"] == main
    wait_contact(agent, True)
    path = output / "Authored.hasset"
    saved = completed(agent.wait(agent.call("scene.save", **version(agent), path=str(path))))
    assert not saved["dirty"] and path.is_file()
    return path, shadow


def automation(cli, editor, root, output):
    app = Application(editor, output, "authoring", "/Game/Scenes/Showcase.hasset", root.parent / "HyperionAssets", frames=0)
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), True)
        viewport_controls(agent)
        path, expected = author_shadows(agent, output)
        completed(agent.call("application.close.request"))
        app.finish()
    finally:
        if agent:
            agent.close()
        app.close()
    for disabled in (False, True):
        extra = ("--disable-plugin", "contact-shadows") if disabled else ()
        app = Application(editor, output, f"reopen-{disabled}", path, root.parent / "HyperionAssets", frames=0, extra=extra)
        agent = None
        try:
            agent = AttachedSession(cli, app.target(), False)
            ready(agent)
            main = completed(agent.call("scene.lighting.get"))["shadowDirectionalLight"]
            assert read_light(agent, main)["shadowSettings"] == expected
            assert not ready(agent)["dirty"]
            wait_contact(agent, not disabled)
            completed(agent.call("application.close.request"))
            app.finish()
        finally:
            if agent:
                agent.close()
            app.close()


def gui(editor, root, output):
    report = output / "Gui.json"
    args = [str(editor), "--asset-root", str(root.parent / "HyperionAssets"), "--hidden",
            "--scene", "/Game/Scenes/Showcase.hasset", "--exercise-render-controls", str(output),
            "--layout", str(output / "Gui.ini"), "--ui-preferences", str(output / "Scale.ini"),
            "--editor-preferences", str(output / "Preferences.ini"), "--report", str(report)]
    result = subprocess.run(args, cwd=root, capture_output=True, text=True, timeout=100)
    (output / "Gui.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    data = json.loads(report.read_text(encoding="utf-8"))
    assert data["render_controls_verified"] and not data["document_dirty"], data
    assert data["validation_errors"] == 0, data
    for name in ("Hud", "Settings", "Light", "Narrow", "Visibility", "Batching"):
        assert (output / f"{name}.png").stat().st_size > 1000


if __name__ == "__main__":
    ensure_isolated_discovery()
    cli, editor, root = [pathlib.Path(value).resolve() for value in sys.argv[1:]]
    parent = root / "out" / "editor-render-controls"
    parent.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(prefix="acceptance-", dir=parent))
    print(f"Render controls evidence: {output}", flush=True)
    gui(editor, root, output)
    automation(cli, editor, root, output)
    print("Render controls GUI, HUD layout, light history, native persistence and feature absence passed")
