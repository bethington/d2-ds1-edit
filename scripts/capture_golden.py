"""
Capture golden screenshots for migration testing.

Usage:
    python capture_golden.py [--core|--full]

Runs the Allegro 4 build in headless mode against test maps and saves
screenshots to test/golden/ for later comparison.
"""

import os
import sys
import subprocess

# Paths relative to the project root (script assumes it's run from project root or bin/)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
BIN_DIR = os.path.join(PROJECT_ROOT, "bin")
ASSETS_DIR = os.path.join(BIN_DIR, "assets")
GOLDEN_DIR = os.path.join(PROJECT_ROOT, "test", "golden")
EXE = os.path.join(BIN_DIR, "ds1edit.exe")

# Core test suite: 6 maps covering all 5 act palettes + indoor/outdoor
CORE_MAPS = [
    "act1_town",
    "act1_cave",
    "act2_desert",
    "act3_jungle",
    "act4_fortress",
    "act5_town",
]


def parse_ini_first_entry(ini_path):
    """Parse an INI file and return the first valid entry as (lvltype_id, def, ds1_path)."""
    with open(ini_path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or line.startswith(";"):
                continue
            parts = line.split()
            if len(parts) >= 3:
                return parts[0], parts[1], parts[2]
    return None


def capture_screenshot(map_name, lvltype_id, lvlprest_def, ds1_path, output_path):
    """Run the editor in headless mode to capture a screenshot."""
    cmd = [
        EXE,
        ds1_path,
        lvltype_id,
        lvlprest_def,
        "--headless",
        output_path,
    ]
    print(f"  Capturing {map_name}...")
    print(f"    cmd: {' '.join(cmd)}")

    try:
        result = subprocess.run(
            cmd,
            cwd=BIN_DIR,
            capture_output=True,
            text=True,
            timeout=120,
        )
    except subprocess.TimeoutExpired:
        print(f"    TIMEOUT (>120s, skipping)")
        return False

    if result.returncode != 0:
        print(f"    FAILED (exit code {result.returncode})")
        # Print last few lines of stdout for debugging
        lines = result.stdout.strip().split("\n")
        for line in lines[-5:]:
            print(f"      {line}")
        return False

    if os.path.exists(output_path):
        size = os.path.getsize(output_path)
        # Also convert to PNG
        try:
            from PIL import Image
            png_path = os.path.splitext(output_path)[0] + ".png"
            img = Image.open(output_path)
            if img.mode == "P":
                img = img.convert("RGB")
            img.save(png_path, "PNG")
            png_size = os.path.getsize(png_path)
            print(f"    OK ({size:,} bytes BMP, {png_size:,} bytes PNG)")
        except ImportError:
            print(f"    OK ({size:,} bytes, no PNG - install Pillow)")
        return True
    else:
        print(f"    FAILED (output file not created)")
        return False


def get_all_ini_names():
    """Get all INI file basenames (without .ini) from the assets directory."""
    names = []
    for f in sorted(os.listdir(ASSETS_DIR)):
        if f.endswith(".ini"):
            names.append(f[:-4])  # strip .ini
    return names


def main():
    mode = "--core"
    skip_existing = False
    for arg in sys.argv[1:]:
        if arg in ("--core", "--full"):
            mode = arg
        elif arg == "--skip-existing":
            skip_existing = True

    if mode == "--full":
        map_names = get_all_ini_names()
        print(f"Full suite: {len(map_names)} maps")
    else:
        map_names = CORE_MAPS
        print(f"Core suite: {len(map_names)} maps")

    os.makedirs(GOLDEN_DIR, exist_ok=True)

    succeeded = 0
    failed = 0
    skipped = 0

    for map_name in map_names:
        ini_path = os.path.join(ASSETS_DIR, f"{map_name}.ini")
        if not os.path.exists(ini_path):
            print(f"  SKIP {map_name} (INI not found)")
            skipped += 1
            continue

        entry = parse_ini_first_entry(ini_path)
        if entry is None:
            print(f"  SKIP {map_name} (no valid entries)")
            skipped += 1
            continue

        lvltype_id, lvlprest_def, ds1_path = entry
        output_path = os.path.join(GOLDEN_DIR, f"{map_name}.bmp")

        if skip_existing and os.path.exists(output_path):
            print(f"  SKIP {map_name} (already exists)")
            skipped += 1
            continue

        if capture_screenshot(map_name, lvltype_id, lvlprest_def, ds1_path, output_path):
            succeeded += 1
        else:
            failed += 1

    print(f"\nResults: {succeeded} succeeded, {failed} failed, {skipped} skipped")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
