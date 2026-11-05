"""The properties panel must not edit LvlPrest's File cells.

ds1_manager_txt_set_cell applies no rules, so an edit there could leave a blank or
0 cell below Files, which crashes the game. The panel draws through Allegro, which
no headless test can run, so the rule is pinned in the source here.
"""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
PANEL = (ROOT / "src" / "ui" / "props_panel.c").read_text(encoding="utf-8")


def layout_section_of_click_handler():
    """The click handler's walk over the Layout section's rows."""
    start = PANEL.index("PP_CLICK_SECTION(PPS_TXT_LAYOUT)")
    end = PANEL.index("cur_sec = PPS_TXT_ROOMSIZE", start)
    return PANEL[start:end]


class FileCellsAreReadOnly(unittest.TestCase):
    def test_a_click_on_a_file_cell_starts_no_edit(self):
        section = layout_section_of_click_handler()
        self.assertNotRegex(section, r"PP_CLICK_FIELD\([^)]*fcol")
        self.assertIn("PP_CLICK_READ_ONLY_FIELD()", section)

    def test_a_read_only_row_stays_in_step_with_the_draw(self):
        macro = re.search(r"#define PP_CLICK_READ_ONLY_FIELD\(\)(.*?)\n\s*\n", PANEL, re.DOTALL)
        self.assertIsNotNone(macro)
        self.assertIn("pp_field_idx++", macro.group(1))
        self.assertNotIn("pp->editing", macro.group(1))

    def test_a_file_cell_is_drawn_as_read_only(self):
        start = PANEL.index("/* File1-6 slots */")
        end = PANEL.index("Section: Room/Size", start)
        self.assertIn("pp_next_field_read_only = 1", PANEL[start:end])

    def test_the_rest_of_the_section_can_still_be_edited(self):
        self.assertRegex(layout_section_of_click_handler(),
                         r'PP_CLICK_FIELD\(PFS_LVLPREST, lvlprest_def, "LevelId"')


if __name__ == "__main__":
    unittest.main()
