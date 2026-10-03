"""Verify native stacking and input during Editor close with an open asset window."""
import ctypes
from ctypes import wintypes
import pathlib
import sys
import tempfile
import time

from AttachmentAcceptance import Application, AttachedSession, ready
from AutomationAcceptance import completed


user32 = ctypes.WinDLL("user32", use_last_error=True)
enum_callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.EnumWindows.argtypes = [enum_callback, wintypes.LPARAM]
user32.GetWindow.argtypes = [wintypes.HWND, wintypes.UINT]
user32.GetWindow.restype = wintypes.HWND
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
for name in ("IsWindowEnabled", "IsWindowVisible", "IsZoomed", "IsIconic"):
    getattr(user32, name).argtypes = [wintypes.HWND]


def await_state(predicate):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError("Native close-modal state did not settle")


def windows(process):
    found = {}

    @enum_callback
    def visit(handle, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(handle, ctypes.byref(pid))
        if pid.value == process.pid:
            title = ctypes.create_unicode_buffer(256)
            user32.GetWindowTextW(handle, title, len(title))
            if title.value.startswith("Hyperion"):
                found["asset" if title.value == "Hyperion Asset Editor" else "main"] = handle
        return True

    user32.EnumWindows(visit, 0)
    return found


def above(upper, lower):
    current = user32.GetWindow(lower, 3)  # GW_HWNDPREV
    while current:
        if current == upper:
            return True
        current = user32.GetWindow(current, 3)
    return False


def exercise(cli, editor, output, action):
    app = Application(editor, output, f"close-modal-{action}", "", output,
                      frames=100000, hidden=False)
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), True)
        info = ready(agent)
        scene_path = output / f"Scene-{action}.hasset"
        completed(agent.wait(agent.call("scene.save", document=info["document"],
                                        revision=info["revision"], path=str(scene_path))))
        original = scene_path.read_bytes()
        info = ready(agent)
        completed(agent.call("scene.node.create", document=info["document"],
                             revision=info["revision"], name="Unsaved close-modal edit"))
        completed(agent.wait(agent.call("asset.open", path="/Engine/Models/Primitives/Cube.hasset")))
        await_state(lambda: "asset" in windows(app.process))
        handles = windows(app.process)
        main, asset = handles["main"], handles["asset"]
        user32.ShowWindow(main, 3)  # SW_MAXIMIZE
        await_state(lambda: user32.IsZoomed(main) and above(asset, main))
        for minimized in (False, True):
            if minimized:
                user32.ShowWindow(asset, 6)  # SW_MINIMIZE
                await_state(lambda: user32.IsIconic(asset))
            assert user32.PostMessageW(main, 0x0010, 0, 0)  # WM_CLOSE, as the native close button
            await_state(lambda: user32.IsWindowEnabled(main) and not user32.IsWindowEnabled(asset)
                        and above(main, asset))
            assert user32.IsZoomed(main) and user32.IsWindowVisible(main)
            assert not user32.GetWindow(main, 4)  # Main never becomes an owned auxiliary.
            assert bool(user32.IsIconic(asset)) == minimized
            capture = output / f"Modal-{action}-{minimized}.png"
            completed(agent.wait(agent.call("render.screenshot", path=str(capture), overwrite=True)))
            user32.ShowWindow(main, 6)  # Main minimizes while its modal flow stays active.
            await_state(lambda: user32.IsIconic(main) and not user32.IsWindowVisible(asset))
            completed(agent.call("application.close.request", action=3))  # Cancel
            assert not user32.IsWindowVisible(asset)
            user32.ShowWindow(main, 9)
            await_state(lambda: user32.IsWindowEnabled(asset) and user32.GetWindow(asset, 4) == main)
            assert ready(agent)["dirty"] and scene_path.read_bytes() == original
            assert user32.IsZoomed(main) and app.process.poll() is None
            if minimized:
                user32.ShowWindow(asset, 9)  # SW_RESTORE
                await_state(lambda: not user32.IsIconic(asset))
        assert user32.PostMessageW(main, 0x0010, 0, 0)
        await_state(lambda: not user32.IsWindowEnabled(asset) and above(main, asset))
        completed(agent.call("application.close.request", action=action))
        app.process.wait(timeout=30)
        assert app.process.returncode == 0, app.path.read_text(encoding="utf-8")
        assert (scene_path.read_bytes() != original) == (action == 1)
        print(f"PASS: close modal stacking, native input, cancel, maximization and action {action}", flush=True)
    except Exception:
        print("Native windows:", {
            name: {"handle": handle, "owner": user32.GetWindow(handle, 4),
                   "enabled": bool(user32.IsWindowEnabled(handle)),
                   "visible": bool(user32.IsWindowVisible(handle)),
                   "maximized": bool(user32.IsZoomed(handle)),
                   "minimized": bool(user32.IsIconic(handle))}
            for name, handle in windows(app.process).items()}, flush=True)
        print(app.path.read_text(encoding="utf-8"), flush=True)
        raise
    finally:
        if agent:
            agent.close()
        app.close()


if __name__ == "__main__":
    cli, editor, output = [pathlib.Path(value).resolve() for value in sys.argv[1:]]
    output.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="close-modal-", dir=output))
    for action in (1, 2):  # Save, discard
        exercise(cli, editor, work, action)
