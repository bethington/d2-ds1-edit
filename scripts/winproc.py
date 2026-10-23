"""Process-launch helpers shared by the test and equivalence scripts.

Two things automated runs must not do on a developer's desktop:

* flash a console window for every child process (Windows creates one for a
  console program started from a GUI-less parent unless told otherwise), and
* put the editor's own window in front of whatever the developer is doing.

``no_window()`` supplies the first as ``**kwargs`` for subprocess.run/Popen.
``offscreen_env()`` supplies the second: it sets DS1EDIT_TEST_OFFSCREEN=1,
which the editor honours (frameless, off-screen, non-activating, hidden from
the taskbar) wherever it creates its display. Performance measurements pass
``visible=True`` because Windows may throttle an occluded window.
"""

import os
import subprocess

OFFSCREEN_VAR = "DS1EDIT_TEST_OFFSCREEN"


def no_window_flags(os_name=None):
    """CREATE_NO_WINDOW on Windows, 0 elsewhere."""
    if (os.name if os_name is None else os_name) == "nt":
        return getattr(subprocess, "CREATE_NO_WINDOW", 0x08000000)
    return 0


def no_window(os_name=None):
    """kwargs for subprocess.run/Popen; empty off Windows."""
    flags = no_window_flags(os_name)
    return {"creationflags": flags} if flags else {}


def offscreen_env(env=None, visible=False):
    """A copy of env (default os.environ) with the offscreen switch set,
    or explicitly cleared when visible is requested."""
    out = dict(os.environ if env is None else env)
    if visible:
        out.pop(OFFSCREEN_VAR, None)
    else:
        out[OFFSCREEN_VAR] = "1"
    return out
