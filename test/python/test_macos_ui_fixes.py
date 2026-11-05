"""GUI fixes from the 2.2.1 macOS arm64 run that no headless test can drive.

Each of these runs only inside a dialog or a frame of the editor's window, so the
rule is pinned in the source, as test_delete_key_routing.py does:

- Native message boxes go through ds1_message_box(), because Allegro's macOS
  alert drops the heading ("The MPQ was built.") and shows only the title.
- A selected tile is drawn tinted with its selection colour again, not just
  half see-through, which made a selection hard to see.
- wmsg_main() puts back what was under its window, so a box that follows
  another doesn't show the first one's leftovers.
- New Project's folder picker on macOS can make a folder and shows its text.
"""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
COMMENTS = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)


def code(path):
    """The file's text with C comments blanked, keeping newlines."""
    text = (SRC / path).read_text(encoding="utf-8", errors="replace")
    return COMMENTS.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def function_body(text, name):
    """The body of a brace-at-column-0 C function definition."""
    match = re.search(rf"^[\w \*]*\b{name}\([^)]*\)\n\{{\n(.*?)^\}}", text,
                      re.MULTILINE | re.DOTALL)
    if match is None:
        raise AssertionError(f"{name} is not defined")
    return match.group(1)


class MessageBoxTests(unittest.TestCase):
    def test_only_the_wrapper_calls_allegros_message_box(self):
        found = []
        for path in sorted(SRC.rglob("*")):
            if path.suffix not in (".c", ".h", ".m") or path.name == "message_box.h":
                continue
            rel = path.relative_to(SRC)
            for match in re.finditer(r"\bal_show_native_message_box\s*\(", code(rel)):
                found.append(f"src/{rel}:{code(rel).count(chr(10), 0, match.start()) + 1}")
        self.assertEqual(found, [], "use ds1_message_box() from ui/message_box.h:\n"
                         + "\n".join(found))

    def test_macos_shows_the_heading_where_allegro_shows_the_title(self):
        wrapper = code("ui/message_box.h")
        apple = wrapper[wrapper.index("#ifdef __APPLE__"):wrapper.index("#endif")]
        self.assertIn("title = heading;", apple)
        self.assertIn('heading = "";', apple)


class SelectionTintTests(unittest.TestCase):
    def test_a_selected_tile_is_drawn_toward_its_colour(self):
        body = function_body(code("render/preview.c"), "wpreview_shape")
        self.assertIn("wpreview_silhouette(tmp_bmp, pal_color(col_idx)", body)
        self.assertIn("WPREVIEW_SELECT_MIX", body)

    def test_the_silhouette_takes_its_colour_and_the_tiles_coverage(self):
        body = function_body(code("render/preview.c"), "wpreview_silhouette")
        # rgb = colour * tile alpha, alpha = tile alpha: premultiplied, so the
        # blend over the map leaves the tile's transparent pixels alone.
        self.assertIn("al_set_separate_blender(ALLEGRO_ADD, ALLEGRO_ZERO, ALLEGRO_ALPHA,\n"
                      "                           ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_ZERO)", body)
        self.assertLess(body.index("al_store_state("), body.index("al_set_target_bitmap("))
        self.assertIn("al_restore_state(", body)

    def test_a_tile_under_the_mouse_stays_see_through(self):
        body = function_body(code("render/preview.c"), "wpreview_shape")
        mouse = body.index("COL_MOUSE")
        self.assertLess(mouse, body.index("pal_color(col_idx)"))
        self.assertIn("a5_trans_alpha", body[mouse:body.index("return;", mouse)])

    def test_the_mix_is_a_real_blend(self):
        text = code("render/preview.c")
        mix = float(re.search(r"#define WPREVIEW_SELECT_MIX\s+([0-9.]+)f", text).group(1))
        self.assertGreater(mix, 0.0)
        self.assertLess(mix, 1.0)


class MessageWindowTests(unittest.TestCase):
    def test_the_window_saves_what_was_under_it_before_drawing(self):
        body = function_body(code("ui/dialogs.c"), "wmsg_main")
        self.assertLess(body.index("wmsg_save_under("),
                        body.index("a5_rectfill(glb_ds1edit.screen_buff"))

    def test_the_window_puts_it_back_at_its_only_return(self):
        body = function_body(code("ui/dialogs.c"), "wmsg_main")
        first_draw = body.index("a5_rectfill(glb_ds1edit.screen_buff")
        self.assertEqual(body.count("return", first_draw), 1)
        self.assertLess(body.index("wmsg_restore_under("), body.rindex("return ret;"))


class FolderPickerTests(unittest.TestCase):
    def test_macos_uses_its_own_picker(self):
        body = function_body(code("ui/project_menu.c"), "pick_folder")
        apple = body[body.index("#elif defined(__APPLE__)"):body.index("#else")]
        self.assertIn("ds1_mac_pick_folder(title, initial, out, out_cap)", apple)

    def test_the_picker_can_make_a_folder_and_shows_the_text(self):
        picker = code("mac_folder_picker.m")
        self.assertIn("canCreateDirectories = YES", picker)
        self.assertIn("canChooseDirectories = YES", picker)
        self.assertIn("panel.message =", picker)
        self.assertIn('panel.prompt = @"Select"', picker)

    def test_the_picker_is_built_on_macos(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertRegex(cmake, r"add_library\(ds1edit_mac_pointer STATIC[^)]*src/mac_folder_picker\.m")


if __name__ == "__main__":
    unittest.main()
