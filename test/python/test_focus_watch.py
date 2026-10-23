"""scripts/perf/focus_watch.py: the pure parts (process tree, taskbar rule, summary)."""

import unittest

from scripts.perf import focus_watch as fw


class DescendantTests(unittest.TestCase):
    def test_walks_the_tree(self):
        pairs = [(1, 0), (10, 1), (11, 10), (12, 10), (20, 2), (13, 11)]
        self.assertEqual(fw.descendants(pairs, 10), {10, 11, 12, 13})

    def test_root_alone(self):
        self.assertEqual(fw.descendants([(5, 1)], 7), {7})

    def test_cycle_terminates(self):
        self.assertEqual(fw.descendants([(2, 1), (1, 2)], 1), {1, 2})


class LiveTreeTests(unittest.TestCase):
    def test_orphan_still_traces_through_dead_parent(self):
        parents = {}
        fw.update_tree(parents, [(1, 0), (10, 1), (11, 10)])
        alive = fw.update_tree(parents, [(1, 0), (11, 10)])  # 10 died, 11 lives
        self.assertEqual(fw.test_pids(parents, {1, 11}, 1), {1, 11})

    def test_reused_pid_is_not_a_test_process(self):
        parents = {}
        fw.update_tree(parents, [(1, 0), (10, 1)])
        # 10 exits; the OS hands pid 10 to an unrelated process under pid 5.
        alive = fw.update_tree(parents, [(1, 0), (10, 5), (5, 0)])
        self.assertEqual(fw.test_pids(parents, alive, 1), {1})


    def test_edge_to_an_unseen_parent_stays_untrusted_after_pid_reuse(self):
        parents = {}
        # 48816's parent (60200) is not in the snapshot: it may be a dead pid.
        fw.update_tree(parents, [(1, 0), (48816, 60200)])
        # Later pid 60200 appears under the test root (reuse).
        alive = fw.update_tree(parents, [(1, 0), (60200, 1), (48816, 60200)])
        self.assertEqual(fw.test_pids(parents, alive, 1), {1, 60200})

    def test_unseen_parent_edge_alone_is_ignored(self):
        parents = {}
        alive = fw.update_tree(parents, [(1, 0), (48816, 60200)])
        self.assertEqual(fw.test_pids(parents, alive, 1), {1})

class TaskbarRuleTests(unittest.TestCase):
    def test_plain_visible_unowned_window_is_listed(self):
        self.assertTrue(fw.is_taskbar_window(True, 0, False))

    def test_hidden_window_is_not(self):
        self.assertFalse(fw.is_taskbar_window(False, 0, False))

    def test_toolwindow_is_not(self):
        self.assertFalse(fw.is_taskbar_window(True, fw.WS_EX_TOOLWINDOW, False))

    def test_owned_window_is_not(self):
        self.assertFalse(fw.is_taskbar_window(True, 0, True))

    def test_appwindow_overrides(self):
        self.assertTrue(fw.is_taskbar_window(True, fw.WS_EX_APPWINDOW | fw.WS_EX_TOOLWINDOW, True))

    def test_noactivate_toolwindow_offscreen_case(self):
        ex = fw.WS_EX_NOACTIVATE | fw.WS_EX_TOOLWINDOW
        self.assertFalse(fw.is_taskbar_window(True, ex, False))


class SummaryTests(unittest.TestCase):
    def test_counts(self):
        w = {"hwnd": 5, "pid": 9, "title": "DS1", "class": "c", "taskbar": True, "rect": [0, 0, 1, 1]}
        w2 = {"hwnd": 6, "pid": 9, "title": "x", "class": "c", "taskbar": False, "rect": [0, 0, 1, 1]}
        samples = [
            {"t": 0, "fg_pid": 4, "fg_is_test": False, "windows": []},
            {"t": 1, "fg_pid": 9, "fg_is_test": True, "windows": [w, w2]},
            {"t": 2, "fg_pid": 9, "fg_is_test": True, "windows": [w]},
        ]
        s = fw.summarize(samples)
        self.assertEqual(s["samples"], 3)
        self.assertEqual(s["foreground_samples"], 2)
        self.assertEqual(s["foreground_pids"], [9])
        self.assertEqual(s["test_windows_seen"], 2)
        self.assertEqual(s["taskbar_windows"], 1)
        self.assertEqual(s["taskbar_window_titles"], ["DS1"])

    def test_clean_run(self):
        s = fw.summarize([{"t": 0, "fg_pid": 1, "fg_is_test": False, "windows": []}])
        self.assertEqual((s["foreground_samples"], s["taskbar_windows"]), (0, 0))


if __name__ == "__main__":
    unittest.main()
