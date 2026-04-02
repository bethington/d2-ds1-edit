"""
Compare a test screenshot against a golden reference image.

Usage:
    python compare_golden.py <reference.bmp> <test.bmp> [--tolerance N]

Exits 0 if images match within tolerance, 1 if they differ.
Reports pixel difference statistics.
"""

import os
import sys

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow")
    sys.exit(2)


def compare_images(ref_path, test_path, tolerance=0):
    """Compare two images pixel-by-pixel.

    Args:
        ref_path: Path to reference (golden) image
        test_path: Path to test image
        tolerance: Max per-channel difference allowed (0 = exact match)

    Returns:
        (match, stats_dict)
    """
    ref = Image.open(ref_path).convert("RGB")
    test = Image.open(test_path).convert("RGB")

    if ref.size != test.size:
        return False, {
            "error": f"Size mismatch: reference {ref.size} vs test {test.size}",
            "ref_size": ref.size,
            "test_size": test.size,
        }

    ref_data = ref.tobytes()
    test_data = test.tobytes()
    total_pixels = ref.size[0] * ref.size[1]
    total_channels = total_pixels * 3

    diff_pixels = 0
    max_diff = 0
    total_diff = 0

    for i in range(0, total_channels, 3):
        r_diff = abs(ref_data[i] - test_data[i])
        g_diff = abs(ref_data[i + 1] - test_data[i + 1])
        b_diff = abs(ref_data[i + 2] - test_data[i + 2])
        pixel_max = max(r_diff, g_diff, b_diff)

        if pixel_max > tolerance:
            diff_pixels += 1

        if pixel_max > max_diff:
            max_diff = pixel_max

        total_diff += r_diff + g_diff + b_diff

    pct = (diff_pixels / total_pixels) * 100.0 if total_pixels > 0 else 0.0
    avg_diff = total_diff / total_channels if total_channels > 0 else 0.0

    stats = {
        "total_pixels": total_pixels,
        "diff_pixels": diff_pixels,
        "diff_pct": pct,
        "max_channel_diff": max_diff,
        "avg_channel_diff": avg_diff,
        "tolerance": tolerance,
    }

    return diff_pixels == 0, stats


def main():
    if len(sys.argv) < 3:
        print("Usage: compare_golden.py <reference.bmp> <test.bmp> [--tolerance N]")
        return 2

    ref_path = sys.argv[1]
    test_path = sys.argv[2]
    tolerance = 0

    for i, arg in enumerate(sys.argv):
        if arg == "--tolerance" and i + 1 < len(sys.argv):
            tolerance = int(sys.argv[i + 1])

    if not os.path.exists(ref_path):
        print(f"ERROR: Reference image not found: {ref_path}")
        return 2

    if not os.path.exists(test_path):
        print(f"ERROR: Test image not found: {test_path}")
        return 2

    match, stats = compare_images(ref_path, test_path, tolerance)

    if "error" in stats:
        print(f"FAIL: {stats['error']}")
        return 1

    if match:
        print(
            f"PASS: {stats['total_pixels']:,} pixels, "
            f"max channel diff = {stats['max_channel_diff']}, "
            f"tolerance = {tolerance}"
        )
        return 0
    else:
        print(
            f"FAIL: {stats['diff_pixels']:,} / {stats['total_pixels']:,} pixels differ "
            f"({stats['diff_pct']:.2f}%), "
            f"max channel diff = {stats['max_channel_diff']}, "
            f"avg = {stats['avg_channel_diff']:.2f}, "
            f"tolerance = {tolerance}"
        )
        return 1


if __name__ == "__main__":
    sys.exit(main())
