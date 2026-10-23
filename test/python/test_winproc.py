"""scripts/winproc.py: no-console-window flags and the offscreen environment."""

import subprocess
import unittest
from unittest import mock

from scripts import winproc


class NoWindowTests(unittest.TestCase):
    def test_windows_gets_create_no_window(self):
        flags = winproc.no_window_flags("nt")
        self.assertEqual(flags, 0x08000000)
        self.assertEqual(winproc.no_window("nt"), {"creationflags": 0x08000000})

    def test_other_platforms_get_nothing(self):
        self.assertEqual(winproc.no_window_flags("posix"), 0)
        self.assertEqual(winproc.no_window("posix"), {})

    def test_kwargs_are_accepted_by_subprocess(self):
        import sys
        r = subprocess.run([sys.executable, "-c", "pass"], **winproc.no_window())
        self.assertEqual(r.returncode, 0)

    def test_default_follows_host(self):
        import os
        self.assertEqual(winproc.no_window_flags(), winproc.no_window_flags(os.name))


class OffscreenEnvTests(unittest.TestCase):
    def test_sets_switch_without_touching_input(self):
        base = {"A": "1"}
        out = winproc.offscreen_env(base)
        self.assertEqual(out["DS1EDIT_TEST_OFFSCREEN"], "1")
        self.assertEqual(out["A"], "1")
        self.assertNotIn("DS1EDIT_TEST_OFFSCREEN", base)

    def test_visible_clears_inherited_switch(self):
        out = winproc.offscreen_env({"DS1EDIT_TEST_OFFSCREEN": "1"}, visible=True)
        self.assertNotIn("DS1EDIT_TEST_OFFSCREEN", out)

    def test_default_base_is_os_environ(self):
        with mock.patch.dict("os.environ", {"X_TEST": "y"}):
            self.assertEqual(winproc.offscreen_env()["X_TEST"], "y")


if __name__ == "__main__":
    unittest.main()
