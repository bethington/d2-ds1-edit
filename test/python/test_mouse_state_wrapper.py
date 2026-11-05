"""Editor code reads the mouse only through the wrappers in ui/compat.h.

On macOS the pointer comes from Cocoa (mac_pointer.m) and Allegro's mouse driver
is never installed. Allegro dereferences its driver in al_get_mouse_state() and
friends without checking it, so a direct call crashes there. The Launch Profile,
Project Settings and Map New/Clone dialogs once did exactly that and segfaulted
on open. The wrappers (a5_get_mouse_state, a5_set_mouse_xy,
a5_register_mouse_events) pick the Cocoa pointer, Allegro's driver, or an empty
state. A dialog's loop runs only in the GUI, which no headless test can drive,
so the rule is pinned here.
"""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "src"

# The wrappers themselves, and the Cocoa pointer they hand off to.
ALLOWED = {
    SRC / "ui" / "compat.h",
    SRC / "mac_pointer.h",
    SRC / "mac_pointer.m",
}

# Calls that go through Allegro's mouse driver. Reading a state struct someone
# else filled (al_mouse_button_down, al_get_mouse_state_axis) is safe and is
# not listed.
DRIVER_CALLS = re.compile(
    r"\bal_(get_mouse_state|set_mouse_xy|set_mouse_z|set_mouse_w"
    r"|get_mouse_event_source|get_mouse_cursor_position)\s*\(")

COMMENTS = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)


def without_comments(text):
    """Blank out C comments, keeping newlines so line numbers still match."""
    return COMMENTS.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def sources():
    for pattern in ("*.c", "*.h", "*.m"):
        yield from SRC.rglob(pattern)


class MouseStateWrapperTests(unittest.TestCase):
    def test_no_direct_driver_calls_outside_the_wrappers(self):
        found = []
        for path in sorted(sources()):
            if path in ALLOWED:
                continue
            code = without_comments(path.read_text(encoding="utf-8", errors="replace"))
            for match in DRIVER_CALLS.finditer(code):
                line = code.count("\n", 0, match.start()) + 1
                found.append(f"{path.relative_to(ROOT)}:{line}: {match.group(0)}")
        self.assertEqual(found, [], "use the a5_* wrappers in ui/compat.h instead:\n"
                         + "\n".join(found))

    def test_the_dialogs_read_the_pointer_through_the_wrapper(self):
        for name in ("fields_dialog.c", "map_dialog.c"):
            code = without_comments((SRC / "ui" / name).read_text(encoding="utf-8"))
            self.assertTrue("a5_get_mouse_state(&a5_ms_state)" in code,
                            f"{name} doesn't read the pointer with a5_get_mouse_state()")

    def test_the_scan_sees_a_direct_call(self):
        # Guard the guard: a call the regex or the comment stripper missed
        # would let the first test pass vacuously.
        code = without_comments("/* al_get_mouse_state(&s); */\n"
                                "   al_get_mouse_state(&a5_ms_state);\n")
        hits = DRIVER_CALLS.findall(code)
        self.assertEqual(hits, ["get_mouse_state"])


if __name__ == "__main__":
    unittest.main()
