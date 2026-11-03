"""The properties panel's draw and click must agree on the read-only rows, and on
the row of the map that's open.

The panel draws through Allegro, which no headless test can run, so what connects
it to the rules in test_props_panel_rules.c is pinned in the source here.
"""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
PANEL = (ROOT / "src" / "ui" / "props_panel.c").read_text(encoding="utf-8")
BROWSER = (ROOT / "src" / "core" / "area_browser.c").read_text(encoding="utf-8")


def between(text, start, end):
    first = text.index(start)
    return text[first:text.index(end, first)]


class HeaderFields(unittest.TestCase):
    def test_each_drawn_header_row_asks_the_rule_for_its_own_field(self):
        section = between(PANEL, "if (pp->section_expanded[PPS_DS1_HEADER])",
                          "Section: Layout")
        asked = re.findall(r"pp_next_field_read_only = "
                           r"props_panel_header_field_read_only\((\d+)\);\s*"
                           r"y = pp_draw_field(?:_long|_int)?\(", section)
        self.assertEqual(asked, [str(i) for i in range(12)])

    def test_a_click_asks_the_same_rule_before_it_starts_an_edit(self):
        section = between(PANEL, "PP_CLICK_SECTION(PPS_DS1_HEADER)", "cur_sec = PPS_TXT_LAYOUT")
        rule = section.index("props_panel_header_field_read_only(fi)")
        self.assertLess(rule, section.index("pp->editing = TRUE"))

    def test_the_running_field_count_decides_nothing(self):
        draw = between(PANEL, "static int pp_draw_field(", "/* Draw a label:long field row")
        self.assertRegex(draw, r"int is_ro = pp_next_field_read_only;")


class FieldCount(unittest.TestCase):
    """An edit box is drawn on the row whose count matches the click's. Counting a
    collapsed section put version's box on the walls row."""

    def test_a_click_counts_only_the_rows_of_an_open_section(self):
        for name in ("#define PP_CLICK_FIELD(", "#define PP_CLICK_READ_ONLY_FIELD("):
            macro = between(PANEL, name, "return 1;")
            self.assertLess(macro.index("pp->section_expanded[cur_sec]"),
                            macro.index("pp_field_idx++"), name)


class OpenRow(unittest.TestCase):
    def test_the_panel_never_reads_the_highlight(self):
        self.assertNotIn("selected_entry", PANEL)

    def test_the_open_row_is_set_before_the_panel_reads_it(self):
        for function in ("int area_browser_switch_area(", "int area_browser_switch_single("):
            body = between(BROWSER, function, "\n}\n")
            self.assertLess(body.index("ab->loaded_entry ="),
                            body.index("area_browser_finish_load("), function)


if __name__ == "__main__":
    unittest.main()
