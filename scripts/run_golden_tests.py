"""
Run golden screenshot comparison tests.

Renders maps using the current build's --headless mode and compares
against golden reference PNGs.

Usage:
    python run_golden_tests.py [--core|--full] [--tolerance N] [--update]

--update rewrites the reference images from the current build. Only do that
once you have confirmed the new output is right -- it blesses whatever the
build currently produces.

Default tolerance is 0 (exact match). Use --tolerance to allow for
animation non-determinism (e.g., --tolerance 255 ignores color diffs).
"""

import os
import sys
import subprocess
import tempfile

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow")
    sys.exit(2)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
BIN_DIR = os.path.join(PROJECT_ROOT, "bin")
ASSETS_DIR = os.path.join(BIN_DIR, "assets")
GOLDEN_DIR = os.path.join(PROJECT_ROOT, "test", "golden")
EXE = os.path.join(BIN_DIR, "ds1edit.exe")

# Import comparison function
sys.path.insert(0, SCRIPT_DIR)
from compare_golden import compare_images

CORE_MAPS = [
    "act1_town",
    "act1_cave",
    "act2_desert",
    "act3_jungle",
    "act4_fortress",
    "act5_town",
]


def parse_ini_first_entry(ini_path):
    with open(ini_path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or line.startswith(";"):
                continue
            parts = line.split()
            if len(parts) >= 3:
                return parts[0], parts[1], parts[2]
    return None


def get_all_ini_names():
    names = []
    for f in sorted(os.listdir(ASSETS_DIR)):
        if f.endswith(".ini"):
            names.append(f[:-4])
    return names


# Pinned so a reference means the same thing on every machine. Without it the
# render follows default_zoom from the local ds1edit.ini and the comparison
# fails on a size mismatch. 1:4 keeps the reference images small while still
# making a palette or tile regression obvious.
GOLDEN_ZOOM = "1:4"


def render_headless(lvltype_id, lvlprest_def, ds1_path, output_path):
    """Render a map in headless mode. Returns True on success."""
    cmd = [EXE, ds1_path, lvltype_id, lvlprest_def,
           "--zoom=" + GOLDEN_ZOOM, "--headless", output_path]
    try:
        result = subprocess.run(cmd, cwd=BIN_DIR, capture_output=True,
                                text=True, timeout=120)
        # Check file existence rather than exit code — the atexit handler
        # may return non-zero even when the render succeeded
        return os.path.exists(output_path) and os.path.getsize(output_path) > 0
    except subprocess.TimeoutExpired:
        return False


def bmp_to_png_path(bmp_path):
    """Convert a BMP to a temp PNG for comparison."""
    img = Image.open(bmp_path)
    if img.mode == "P":
        img = img.convert("RGB")
    png_path = os.path.splitext(bmp_path)[0] + ".png"
    img.save(png_path, "PNG")
    return png_path


def main():
    mode = "--core"
    tolerance = 0
    update = "--update" in sys.argv

    for arg in sys.argv[1:]:
        if arg in ("--core", "--full"):
            mode = arg
        elif arg == "--tolerance":
            pass  # next arg is the value
        elif sys.argv[sys.argv.index(arg) - 1] == "--tolerance" if arg.isdigit() else False:
            tolerance = int(arg)

    # Parse --tolerance N
    for i, arg in enumerate(sys.argv):
        if arg == "--tolerance" and i + 1 < len(sys.argv):
            tolerance = int(sys.argv[i + 1])

    if mode == "--full":
        map_names = get_all_ini_names()
    else:
        map_names = CORE_MAPS

    print(f"Golden screenshot tests ({'core' if mode == '--core' else 'full'}): "
          f"{len(map_names)} maps, tolerance={tolerance}, zoom={GOLDEN_ZOOM}"
          f"{', UPDATING REFERENCES' if update else ''}")
    print()

    passed = 0
    failed = 0
    skipped = 0

    with tempfile.TemporaryDirectory() as tmpdir:
        for map_name in map_names:
            # Check for golden reference (prefer PNG, fall back to BMP)
            golden_png = os.path.join(GOLDEN_DIR, f"{map_name}.png")
            golden_bmp = os.path.join(GOLDEN_DIR, f"{map_name}.bmp")
            if os.path.exists(golden_png):
                golden_path = golden_png
            elif os.path.exists(golden_bmp):
                golden_path = golden_bmp
            elif update:
                golden_path = golden_png   # about to be created
            else:
                print(f"  SKIP {map_name} (no golden reference)")
                skipped += 1
                continue

            # Parse INI
            ini_path = os.path.join(ASSETS_DIR, f"{map_name}.ini")
            if not os.path.exists(ini_path):
                print(f"  SKIP {map_name} (no INI file)")
                skipped += 1
                continue

            entry = parse_ini_first_entry(ini_path)
            if entry is None:
                print(f"  SKIP {map_name} (no valid INI entries)")
                skipped += 1
                continue

            lvltype_id, lvlprest_def, ds1_path = entry

            # Render
            test_bmp = os.path.join(tmpdir, f"{map_name}.bmp")
            if not render_headless(lvltype_id, lvlprest_def, ds1_path, test_bmp):
                print(f"  FAIL {map_name} (render failed)")
                failed += 1
                continue

            # Convert test BMP to PNG for comparison
            test_png = bmp_to_png_path(test_bmp)

            if update:
                # Store as an indexed PNG when the render fits in 256 colours,
                # which is lossless for these and keeps the repo small.
                img = Image.open(test_png)
                rgb = img.convert("RGB")
                if len(rgb.getcolors(maxcolors=256) or []) > 0:
                    rgb = rgb.convert("P", palette=Image.ADAPTIVE, colors=256)
                rgb.save(golden_path, "PNG", optimize=True)
                print(f"  UPDATED {map_name} ({img.size[0]}x{img.size[1]})")
                passed += 1
                continue

            # Compare
            match, stats = compare_images(golden_path, test_png, tolerance)

            if "error" in stats:
                print(f"  FAIL {map_name}: {stats['error']}")
                failed += 1
            elif match:
                print(f"  PASS {map_name} ({stats['total_pixels']:,} px, "
                      f"max diff={stats['max_channel_diff']})")
                passed += 1
            else:
                print(f"  FAIL {map_name}: {stats['diff_pixels']:,}/{stats['total_pixels']:,} "
                      f"pixels differ ({stats['diff_pct']:.2f}%), "
                      f"max diff={stats['max_channel_diff']}")
                failed += 1

    print(f"\nResults: {passed} passed, {failed} failed, {skipped} skipped")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
