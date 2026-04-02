"""
Convert golden BMP screenshots (8-bit paletted) to PNG (24-bit RGB).

Usage:
    python convert_golden_to_png.py [--dir test/golden/]

Converts all .bmp files in the directory to .png alongside them.
The PNGs are full-color using the embedded BMP palette.
"""

import os
import sys

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow")
    sys.exit(2)


def convert_bmp_to_png(bmp_path):
    """Convert a BMP to PNG, preserving colors via palette expansion."""
    png_path = os.path.splitext(bmp_path)[0] + ".png"
    img = Image.open(bmp_path)
    # Convert paletted to RGB for clean PNG output
    if img.mode == "P":
        img = img.convert("RGB")
    img.save(png_path, "PNG")
    return png_path


def main():
    golden_dir = "test/golden/"
    if len(sys.argv) > 1:
        if sys.argv[1] == "--dir" and len(sys.argv) > 2:
            golden_dir = sys.argv[2]
        else:
            golden_dir = sys.argv[1]

    if not os.path.isdir(golden_dir):
        print(f"ERROR: Directory not found: {golden_dir}")
        return 1

    count = 0
    for f in sorted(os.listdir(golden_dir)):
        if f.lower().endswith(".bmp"):
            bmp_path = os.path.join(golden_dir, f)
            png_path = convert_bmp_to_png(bmp_path)
            bmp_size = os.path.getsize(bmp_path)
            png_size = os.path.getsize(png_path)
            ratio = png_size / bmp_size * 100 if bmp_size > 0 else 0
            print(f"  {f} -> {os.path.basename(png_path)} ({bmp_size:,} -> {png_size:,} bytes, {ratio:.0f}%)")
            count += 1

    print(f"\nConverted {count} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
