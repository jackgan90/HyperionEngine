"""Exercise reflected storage operations in Editor, restart, and absent-provider targets."""
import os
import pathlib
import sys

from AutomationAcceptance import Session, completed
from AttachmentAcceptance import Application, AttachedSession
from TestEnvironment import test_output_root


def stop(app, session):
    completed(session.call("application.close.request"))
    session.close()
    app.finish()


def run(cli, editor, root):
    output = test_output_root() / "Storage"
    output.mkdir(parents=True, exist_ok=True)
    assets = output / "EmptyGame"
    assets.mkdir()
    os.environ.pop("HYP_DISCOVERY_ROOT", None)
    fake_system = output / "UnusedSystemLocalAppData"
    os.environ["LOCALAPPDATA"] = str(fake_system)
    standalone = Session(cli)
    try:
        assert standalone.request("api.describe", {"operation": "application.storage.get"})["unavailable"]
        assert standalone.call("application.storage.get")["error"]["code"] == "unavailable"
    finally:
        standalone.close()
    app = Application(editor, output, "seed", "", assets, frames=0)
    session = AttachedSession(cli, app.target(), True)
    try:
        state = completed(session.call("application.storage.get"))
        assert state["userDataOverride"] and state["cacheOverride"], state
        schema = session.request("api.describe", {"operation": "application.storage.set"})
        assert set(schema["inputSchema"]["required"]) == {"revision", "roots"}, schema
        state = completed(session.call("application.storage.set", revision=state["revision"], roots=state["active"]))
        assert not state["restartRequired"], state
        active = state["active"]
        locator = pathlib.Path(state["settingsFile"])
        assert locator.is_file()
        assert list((locator.parent / "Discovery").rglob("*.json"))
        assert not fake_system.exists()
        stop(app, session)
        session = None
    finally:
        if session:
            session.close()
        app.close()
    os.environ.pop("HYP_USER_DATA_ROOT")
    os.environ.pop("HYP_CACHE_ROOT")
    app = Application(editor, output, "edit", "", assets, frames=0)
    session = AttachedSession(cli, app.target(), False)
    roots = {"userDataRoot": str(output / "RedirectedUser"), "cacheRoot": str(output / "RedirectedCache")}
    try:
        state = completed(session.call("application.storage.get"))
        assert state["active"] == active and not state["userDataOverride"], state
        other_game = output / "OtherGame"
        other_game.mkdir()
        content = completed(session.call("content.root.get"))
        completed(session.call("content.root.set", generation=content["generation"], directory=str(other_game)))
        overlap = session.call("application.storage.set", revision=state["revision"],
                               roots={"userDataRoot": active["userDataRoot"], "cacheRoot": str(other_game)})
        assert overlap["error"]["code"] == "invalid_arguments", overlap
        invalid = session.call("application.storage.set", revision=state["revision"],
                               roots={"userDataRoot": "relative", "cacheRoot": roots["cacheRoot"]})
        assert invalid["error"]["code"] == "invalid_arguments", invalid
        updated = completed(session.call("application.storage.set", revision=state["revision"], roots=roots))
        assert updated["active"] == active and updated["restartRequired"], updated
        for field in roots:
            assert pathlib.Path(updated["next"][field]) == pathlib.Path(roots[field]), updated
        stale = session.call("application.storage.set", revision=state["revision"], roots=roots)
        assert stale["error"]["code"] == "stale_revision", stale
        blocked = output / "Blocked"
        blocked.write_text("not a directory")
        rejected = session.call("application.storage.set", revision=updated["revision"],
                                roots={"userDataRoot": str(blocked), "cacheRoot": roots["cacheRoot"]})
        assert rejected["error"]["code"] == "save_failed", rejected
        assert completed(session.call("application.storage.get")) == updated
        stop(app, session)
        session = None
    finally:
        if session:
            session.close()
        app.close()
    for name, extra in (("restart", ()), ("disabled", ("--disable-plugin", "storage"))):
        app = Application(editor, output, name, "", assets, frames=0, extra=extra)
        session = AttachedSession(cli, app.target(), True)
        try:
            if extra:
                assert session.request("api.describe", {"operation": "application.storage.get"})["unavailable"]
                assert session.call("application.storage.get")["error"]["code"] == "unavailable"
            else:
                state = completed(session.call("application.storage.get"))
                assert not state["restartRequired"], state
                for field in roots:
                    assert pathlib.Path(state["active"][field]) == pathlib.Path(roots[field]), state
                assert list(pathlib.Path(state["logDirectory"]).rglob("*.log"))
            stop(app, session)
            session = None
        finally:
            if session:
                session.close()
            app.close()
    print("Storage discovery, schemas, CLI/MCP edits, failures, restart and disabled-provider checks passed")


if __name__ == "__main__":
    run(*map(pathlib.Path, sys.argv[1:]))
