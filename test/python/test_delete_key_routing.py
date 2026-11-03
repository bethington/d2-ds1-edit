"""The Delete key must never reach map deletion.

One press of Delete used to run both the tile delete and ds1_manager_delete, so
removing a few tiles also removed the map's file and its LvlPrest.txt slot.
test_input_edges.c covers what a press routes to. The call into the map delete
sits in the main loop, which no headless test can run, so its only route is
pinned here instead: the browser's context menu, through a confirmation.
"""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
INTERFACE = (ROOT / "src" / "ui" / "interface.c").read_text(encoding="utf-8")


def function_body(source, name):
    """Return the text of a brace-at-column-0 C function definition."""
    match = re.search(rf"^static \w+ {name}\([^)]*\)\n\{{\n(.*?)^\}}", source,
                      re.MULTILINE | re.DOTALL)
    if match is None:
        raise AssertionError(f"{name} is not defined in interface.c")
    return match.group(1)


class MapDeleteRouteTests(unittest.TestCase):
    def test_interface_never_reads_the_delete_keys_directly(self):
        # Routing goes through input_delete_hits_tiles(), which has a test.
        self.assertNotRegex(INTERFACE, r"\bKEY_DEL(_PAD)?\b")

    def test_map_delete_is_called_only_by_the_confirming_helper(self):
        calls = re.findall(r"\bds1_manager_delete\s*\(", INTERFACE)
        self.assertEqual(len(calls), 1)
        self.assertIn("ds1_manager_delete(",
                      function_body(INTERFACE, "interface_delete_map"))

    def test_map_delete_asks_first_and_stops_unless_the_answer_is_yes(self):
        body = function_body(INTERFACE, "interface_delete_map")
        asked = body.index("ALLEGRO_MESSAGEBOX_YES_NO")
        declined = body.index("!= 1")
        deleted = body.index("ds1_manager_delete(")
        self.assertLess(asked, declined)
        self.assertLess(declined, deleted)

    def test_permanent_backup_delete_asks_first_too(self):
        body = function_body(INTERFACE, "interface_delete_backup")
        self.assertLess(body.index("ALLEGRO_MESSAGEBOX_YES_NO"),
                        body.index("ds1_manager_delete_permanent("))

    def test_helpers_are_only_reached_from_the_browser_menu(self):
        main_loop = INTERFACE[INTERFACE.index("void interfac_user_handler("):]
        menu_start = main_loop.index("Right-click in sidebar")
        menu_end = main_loop.index("Update hover selection")
        for name in ("interface_delete_map(", "interface_delete_backup("):
            self.assertEqual(main_loop.count(name), 1, name)
            self.assertTrue(menu_start < main_loop.index(name) < menu_end, name)


class MapDeleteOrderTests(unittest.TestCase):
    """Deleting a New map backs it up, and then clears its slot."""

    def setUp(self):
        manager = (ROOT / "src" / "core" / "ds1_manager.c").read_text(encoding="utf-8")
        match = re.search(r"^int ds1_manager_delete\([^)]*\)\n\{\n(.*?)^\}", manager,
                          re.MULTILINE | re.DOTALL)
        self.body = match.group(1)

    def test_the_backup_comes_before_the_table_changes(self):
        steps = [self.body.index(step) for step in (
            "lvlprest_find_file_slot(", "map_ops_backup_file(", "map_ops_unregister(")]
        self.assertEqual(steps, sorted(steps))

    def test_a_failed_table_update_puts_the_file_back(self):
        after = self.body[self.body.index("map_ops_unregister("):]
        self.assertIn("map_ops_restore_file(", after)


if __name__ == "__main__":
    unittest.main()
