"""Tests for scripts/equivalence/package_check.py using synthetic archives.

No real executables or game assets: executables are tiny hand-built PE, ELF
and Mach-O headers carrying just the fields the checker reads.
"""

import io
import json
import struct
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.equivalence import package_check as pc  # noqa: E402

VERSION = "2.1.1"

# ---------------------------------------------------------------------------
# Synthetic executables
# ---------------------------------------------------------------------------


def make_pe(machine=0x014C, imports=(), version=VERSION, pe32plus=False):
    opt_size = 240 if pe32plus else 224
    dd_off = 112 if pe32plus else 96
    sect_table = 0x40 + 24 + opt_size
    raw_ptr, vaddr = 0x200, 0x1000
    # Section body: descriptors, terminator, then the DLL name strings.
    ndesc = len(imports)
    names_off = (ndesc + 1) * 20
    body = bytearray(names_off)
    for i, name in enumerate(imports):
        struct.pack_into("<IIIII", body, i * 20, 0, 0, 0,
                         vaddr + len(body), 0)
        body += name.encode("ascii") + b"\x00"
    body += b"\x00" * (-len(body) % 16)
    data = bytearray(raw_ptr)
    data[0:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x40)
    data[0x40:0x44] = b"PE\x00\x00"
    struct.pack_into("<HHIIIHH", data, 0x44, machine, 1, 0, 0, 0, opt_size,
                     0x102)
    opt = 0x58
    struct.pack_into("<H", data, opt, 0x20B if pe32plus else 0x10B)
    struct.pack_into("<I", data, opt + dd_off - 4, 16)
    if imports:
        struct.pack_into("<II", data, opt + dd_off + 8, vaddr, names_off)
    data[sect_table:sect_table + 8] = b".idata\x00\x00"
    struct.pack_into("<IIII", data, sect_table + 8, len(body), vaddr,
                     len(body), raw_ptr)
    data += body
    if version:
        data += b"\x00ds1edit\x00" + version.encode() + b"\x00"
    return bytes(data)


def make_elf(machine=62, cls=2, needed=(), version=VERSION):
    if cls == 1:
        hdr = bytearray(52)
        hdr[0:4] = b"\x7fELF"
        hdr[4], hdr[5], hdr[6] = 1, 1, 1
        struct.pack_into("<HH", hdr, 16, 2, machine)
        data = hdr
    else:
        strtab = b"\x00" + b"".join(n.encode() + b"\x00" for n in needed)
        phoff, phnum = 64, 2
        dyn_off = phoff + 56 * phnum
        dyn = []
        pos = 1
        for n in needed:
            dyn.append((1, pos))
            pos += len(n) + 1
        str_off = dyn_off + 16 * (len(dyn) + 2)
        dyn += [(5, str_off), (0, 0)]
        total = str_off + len(strtab)
        data = bytearray(64)
        data[0:4] = b"\x7fELF"
        data[4], data[5], data[6] = 2, 1, 1
        struct.pack_into("<HHI", data, 16, 2, machine, 1)
        struct.pack_into("<Q", data, 32, phoff)
        struct.pack_into("<HH", data, 54, 56, phnum)
        data += struct.pack("<IIQQQQQQ", 1, 5, 0, 0, 0, total, total, 0x1000)
        data += struct.pack("<IIQQQQQQ", 2, 6, dyn_off, dyn_off, dyn_off,
                            16 * len(dyn), 16 * len(dyn), 8)
        for tag, val in dyn:
            data += struct.pack("<qQ", tag, val)
        data += strtab
    if version:
        data += b"\x00" + version.encode() + b"\x00"
    return bytes(data)


def make_macho(cputype=0x0100000C, dylibs=("/usr/lib/libSystem.B.dylib",),
               version=VERSION):
    cmds = b""
    for d in dylibs:
        name = d.encode() + b"\x00"
        name += b"\x00" * (-(24 + len(name)) % 8)
        cmds += struct.pack("<IIIIII", 0xC, 24 + len(name), 24, 2, 0, 0) + name
    hdr = struct.pack("<IIIIIIII", 0xFEEDFACF, cputype, 0, 2, len(dylibs),
                      len(cmds), 0, 0)
    data = hdr + cmds
    if version:
        data += b"\x00" + version.encode() + b"\x00"
    return data


def plist(version=VERSION, executable="ds1edit"):
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>{executable}</string>
<key>CFBundleIdentifier</key><string>org.ds1edit.ds1edit</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>{version}</string>
<key>CFBundleVersion</key><string>{version}</string>
</dict></plist>""".encode()


# ---------------------------------------------------------------------------
# Synthetic packages
# ---------------------------------------------------------------------------

SYSTEMS = {"windows-x86": "win32", "linux-x86_64": "linux-x86_64",
           "macos-arm64": "macos-arm64"}


def root_name(platform, version=VERSION):
    return f"DS1Edit-{version}-{SYSTEMS[platform]}"


def good_files(platform, version=VERSION):
    files = {
        "Ds1edit.ini.sample": b"[ds1edit]\n",
        "LICENSE": b"MIT\n",
        "NOTICE": b"notice\n",
        "README.md": b"# DS1Edit\n",
        "assets/editor/ds1edit.dt1": b"editor tiles",
        "assets/editor/obj.txt": b"objects",
        "assets/act1_town.ini": b"[area]\n",
        "assets/act5_town.ini": b"[area]\n",
    }
    for i in range(12):
        files[f"assets/ui/bit{i}.png"] = b"\x89PNG" + bytes([i])
    files["assets/ui/cursor_green.png"] = b"\x89PNGc"
    if platform == "windows-x86":
        files["ds1edit.exe"] = make_pe(
            imports=("KERNEL32.dll", "allegro-5.2.dll", "VCRUNTIME140.dll",
                     "api-ms-win-crt-runtime-l1-1-0.dll"), version=version)
        files["allegro-5.2.dll"] = make_pe(
            imports=("KERNEL32.dll", "VCRUNTIME140.dll", "OPENGL32.dll"),
            version=None)
        files["vcruntime140.dll"] = make_pe(imports=("KERNEL32.dll",),
                                            version=None)
    elif platform == "linux-x86_64":
        files["ds1edit"] = make_elf(needed=("liballegro.so.5.2", "libc.so.6"),
                                    version=version)
    else:
        files["ds1edit"] = make_macho(
            dylibs=("/usr/lib/libSystem.B.dylib",
                    "/opt/homebrew/opt/allegro/lib/liballegro.5.2.dylib"),
            version=version)
        files["ds1edit.app/Contents/Info.plist"] = plist(version)
        files["ds1edit.app/Contents/MacOS/ds1edit"] = files["ds1edit"]
        files["ds1edit.app/Contents/_CodeSignature/CodeResources"] = b"<plist/>"
    return files


EXEC_NAMES = ("ds1edit", "ds1edit.app/Contents/MacOS/ds1edit")


def write_zip(path, members):
    """members: list of (name, bytes) or (name, ("symlink", target))."""
    with zipfile.ZipFile(path, "w") as z:
        for name, data in members:
            zi = zipfile.ZipInfo("x")
            zi.filename = name            # keep the raw name
            if isinstance(data, tuple):
                zi.create_system = 3
                zi.external_attr = (0o120777 << 16)
                z.writestr(zi, data[1])
            else:
                zi.external_attr = (0o100644 << 16)
                z.writestr(zi, data)
    return path


def write_tar(path, members):
    """members: list of (name, bytes) or (name, (kind, target))."""
    with tarfile.open(path, "w:gz") as t:
        for name, data in members:
            ti = tarfile.TarInfo(name)
            if isinstance(data, tuple):
                kind, target = data
                ti.type = tarfile.SYMTYPE if kind == "symlink" else (
                    tarfile.LNKTYPE if kind == "hardlink" else tarfile.FIFOTYPE)
                ti.linkname = target
                t.addfile(ti)
            else:
                ti.size = len(data)
                base = name.rsplit("/", 1)[-1]
                ti.mode = 0o755 if name.endswith(EXEC_NAMES) else 0o644
                del base
                t.addfile(ti, io.BytesIO(data))
    return path


def build(tmp, platform, files=None, extra=(), version=VERSION, name=None,
          root=None):
    files = good_files(platform, version) if files is None else files
    root = root or root_name(platform, version)
    members = [(f"{root}/{k}", v) for k, v in files.items()] + list(extra)
    if platform == "windows-x86":
        path = Path(tmp) / (name or f"{root_name(platform, version)}.zip")
        return write_zip(path, members)
    path = Path(tmp) / (name or f"{root_name(platform, version)}.tar.gz")
    return write_tar(path, members)


def status(report, check):
    for c in report["cases"]:
        if c["id"].endswith("." + check):
            return c["status"]
    raise KeyError(check)


class PackageCheckCase(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory(prefix="pkg check ")
        self.tmp = Path(self._tmp.name)
        self.out = self.tmp / "out dir"

    def tearDown(self):
        self._tmp.cleanup()

    def inspect(self, archive, platform, version=VERSION, out=None):
        return pc.inspect_package(archive, platform, version, out or self.out)

    def assert_blocked(self, report, check):
        self.assertEqual(status(report, check), "mismatch", self.dump(report))
        self.assertNotEqual(report["status"], "pass")

    def dump(self, report):
        return json.dumps([(c["id"], c["status"], c["failures"][:3])
                           for c in report["cases"]], indent=1)


class GoodPackages(PackageCheckCase):
    def test_each_platform_passes_and_extracts(self):
        for platform in pc.PLATFORMS:
            with self.subTest(platform=platform):
                out = self.tmp / f"out {platform}"
                arc = build(self.tmp, platform)
                r = self.inspect(arc, platform, out=out)
                self.assertEqual(r["status"], "pass", self.dump(r))
                root = Path(r["extracted_root"])
                self.assertEqual(root, (out / "extracted").resolve())
                exe = pc.PLATFORMS[platform]["executable"]
                top = root / root_name(platform)
                self.assertEqual((top / exe).read_bytes(),
                                 good_files(platform)[exe])
                self.assertEqual(len(r["entries"]),
                                 len(good_files(platform)))
                self.assertTrue(all(e["sha256"] for e in r["entries"]))

    def test_case_schema_and_report_identity(self):
        arc = build(self.tmp, "windows-x86")
        r = self.inspect(arc, "windows-x86")
        import hashlib
        self.assertEqual(r["archive"]["sha256"],
                         hashlib.sha256(arc.read_bytes()).hexdigest())
        self.assertEqual(r["digest"], pc.canonical_digest(r))
        ids = [c["id"] for c in r["cases"]]
        self.assertEqual(len(ids), len(set(ids)))
        for c in r["cases"]:
            self.assertEqual(c["schema"], "ds1edit.case/1")
            self.assertIn(c["status"], pc.STATUSES)
            self.assertTrue(c["required"])
            self.assertEqual(c["input_sha256"], r["archive"]["sha256"])
            for key in ("lane", "platform", "contract_digest",
                        "expected_sha256", "actual_sha256", "coverage",
                        "failures", "provenance", "limitations"):
                self.assertIn(key, c)

    def test_release_staged_name_accepted(self):
        arc = build(self.tmp, "linux-x86_64",
                    name="ds1edit-v2.1.1-linux-x86_64.tar.gz")
        self.assertEqual(self.inspect(arc, "linux-x86_64")["status"], "pass")

    def test_cli_exit_codes(self):
        arc = build(self.tmp, "windows-x86")
        args = ["--archive", str(arc), "--platform", "windows-x86",
                "--version", VERSION, "--out-dir", str(self.out)]
        from contextlib import redirect_stdout, redirect_stderr
        sink = io.StringIO()
        with redirect_stdout(sink), redirect_stderr(sink):
            self.assertEqual(pc.main(args), 0)
            self.assertTrue((self.out / "package-inspection.json").is_file())
            args[-1] = str(self.tmp / "second")
            args[5] = "9.9.9"
            self.assertEqual(pc.main(args), 1)
            args[5] = "not a version"
            self.assertEqual(pc.main(args), 2)
            self.assertEqual(pc.main(["--platform", "amiga"]), 2)

    def test_invalid_arguments_raise(self):
        arc = build(self.tmp, "windows-x86")
        with self.assertRaises(ValueError):
            self.inspect(arc, "windows-arm64")
        with self.assertRaises(ValueError):
            self.inspect(arc, "windows-x86", version="2.1")


class RequiredMembers(PackageCheckCase):
    def test_each_missing_required_member_fails(self):
        cases = {
            "executable": ["ds1edit.exe"],
            "ui assets": [k for k in good_files("windows-x86")
                          if k.startswith("assets/ui/")],
            "editor asset": ["assets/editor/ds1edit.dt1"],
            "editor objects": ["assets/editor/obj.txt"],
            "area ini": ["assets/act1_town.ini", "assets/act5_town.ini"],
            "sample ini": ["Ds1edit.ini.sample"],
            "license": ["LICENSE"],
            "notice": ["NOTICE"],
            "readme": ["README.md"],
        }
        for label, drop in cases.items():
            with self.subTest(missing=label):
                files = good_files("windows-x86")
                for d in drop:
                    del files[d]
                d = self.tmp / label
                d.mkdir()
                r = self.inspect(build(d, "windows-x86", files),
                                 "windows-x86", out=d / "out")
                self.assert_blocked(r, "required_members")

    def test_missing_executable_marks_dependent_checks_not_run(self):
        files = good_files("linux-x86_64")
        del files["ds1edit"]
        r = self.inspect(build(self.tmp, "linux-x86_64", files),
                         "linux-x86_64")
        self.assertEqual(status(r, "executable_header"), "not_run")
        self.assertEqual(status(r, "runtime_closure"), "not_run")
        self.assertNotEqual(r["status"], "pass")


class ForbiddenMembers(PackageCheckCase):
    def test_game_data_caches_and_user_config_rejected(self):
        bad = [
            "d2data.mpq",
            "assets/patch_d2.MPQ",
            "assets/tiles/act1/town/floor.dt1",
            "assets/excel/levels.txt",
            "assets/editor/pal0.bin",
            "assets/editor/cmap2_0_0_abc_g22.bin",
            "assets/editor/gamma.dat",
            "assets/editor/version",
            "assets/cache/index.bin",
            "Ds1edit.ini",
            "config/ds1edit.INI",
            "maps/town.ds1",
            "assets/sprites/monster.dcc",
            "stderr.txt",
        ]
        for name in bad:
            with self.subTest(member=name):
                files = good_files("windows-x86")
                files[name] = b"x"
                d = self.tmp / name.replace("/", "_")
                d.mkdir()
                r = self.inspect(build(d, "windows-x86", files),
                                 "windows-x86", out=d / "out")
                self.assert_blocked(r, "forbidden_members")

    def test_shipped_editor_dt1_is_not_game_data(self):
        r = self.inspect(build(self.tmp, "windows-x86"), "windows-x86")
        self.assertEqual(status(r, "forbidden_members"), "pass")


class UnsafeArchives(PackageCheckCase):
    UNSAFE = [
        "../escape.txt",
        "DS1Edit-2.1.1-win32/../../escape.txt",
        "/abs/escape.txt",
        "C:/escape.txt",
        "C:escape.txt",
        "\\\\server\\share\\escape.txt",
        "//server/share/escape.txt",
        "DS1Edit-2.1.1-win32\\..\\escape.txt",
        "DS1Edit-2.1.1-win32//double.txt",
        "DS1Edit-2.1.1-win32/./dot.txt",
        "DS1Edit-2.1.1-win32/CON.txt",
        "DS1Edit-2.1.1-win32/trailing. ",
        "DS1Edit-2.1.1-win32/stream.txt:ads",
    ]

    def check_refused(self, r, check):
        self.assert_blocked(r, check)
        self.assertEqual(status(r, "extraction"), "not_run")
        self.assertIsNone(r["extracted_root"])
        self.assertFalse((self.out / "extracted").exists())
        self.assertFalse((self.tmp / "escape.txt").exists())

    def test_unsafe_names_in_zip(self):
        for name in self.UNSAFE:
            with self.subTest(name=name):
                d = self.tmp / f"z{self.UNSAFE.index(name)}"
                d.mkdir()
                arc = build(d, "windows-x86", extra=[(name, b"evil")])
                self.out = d / "out"
                self.check_refused(self.inspect(arc, "windows-x86"),
                                   "member_names")

    def test_unsafe_names_in_tar(self):
        for name in ("../escape.txt", "/abs/escape.txt",
                     "DS1Edit-2.1.1-linux-x86_64/../../escape.txt",
                     "C:/escape.txt"):
            with self.subTest(name=name):
                d = self.tmp / f"t{abs(hash(name))}"
                d.mkdir()
                arc = build(d, "linux-x86_64", extra=[(name, b"evil")])
                self.out = d / "out"
                self.check_refused(self.inspect(arc, "linux-x86_64"),
                                   "member_names")

    def test_duplicate_members(self):
        root = root_name("windows-x86")
        for dup in (f"{root}/LICENSE", f"{root}/license",
                    f"{root}/ASSETS/act1_town.ini"):
            with self.subTest(dup=dup):
                d = self.tmp / f"d{abs(hash(dup))}"
                d.mkdir()
                import warnings
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore")
                    arc = build(d, "windows-x86", extra=[(dup, b"other")])
                self.out = d / "out"
                self.check_refused(self.inspect(arc, "windows-x86"),
                                   "member_uniqueness")

    def test_symlink_escape_tar(self):
        root = root_name("linux-x86_64")
        for target in ("../../../etc/passwd", "/etc/passwd",
                       "../../outside", "C:/Windows/win.ini"):
            with self.subTest(target=target):
                d = self.tmp / f"s{abs(hash(target))}"
                d.mkdir()
                arc = build(d, "linux-x86_64", extra=[
                    (f"{root}/assets/link", ("symlink", target))])
                self.out = d / "out"
                r = self.inspect(arc, "linux-x86_64")
                self.check_refused(r, "member_types")
                msgs = [f["message"] for c in r["cases"]
                        for f in c["failures"]]
                self.assertIn("symlink escapes the package root", msgs)

    def test_symlink_escape_zip(self):
        root = root_name("windows-x86")
        arc = build(self.tmp, "windows-x86", extra=[
            (f"{root}/assets/link", ("symlink", "../../../escape.txt"))])
        self.check_refused(self.inspect(arc, "windows-x86"), "member_types")

    def test_in_root_symlink_hardlink_and_fifo_rejected(self):
        root = root_name("linux-x86_64")
        for kind, target in (("symlink", "LICENSE"),
                             ("hardlink", f"{root}/LICENSE"),
                             ("fifo", "")):
            with self.subTest(kind=kind):
                d = self.tmp / kind
                d.mkdir()
                arc = build(d, "linux-x86_64",
                            extra=[(f"{root}/extra", (kind, target))])
                self.out = d / "out"
                self.check_refused(self.inspect(arc, "linux-x86_64"),
                                   "member_types")

    def test_empty_archives(self):
        for platform, maker, name in (
                ("windows-x86", write_zip, "DS1Edit-2.1.1-win32.zip"),
                ("linux-x86_64", write_tar,
                 "DS1Edit-2.1.1-linux-x86_64.tar.gz")):
            with self.subTest(platform=platform):
                d = self.tmp / platform
                d.mkdir()
                arc = maker(d / name, [])
                self.out = d / "out"
                r = self.inspect(arc, platform)
                self.assertEqual(status(r, "archive"), "mismatch")
                self.assertEqual(status(r, "required_members"), "mismatch")
                self.assertEqual(status(r, "extraction"), "not_run")
                self.assertEqual(r["status"], "mismatch")

    def test_missing_and_corrupt_archive(self):
        r = self.inspect(self.tmp / "absent.zip", "windows-x86")
        self.assertEqual(status(r, "archive"), "unavailable")
        self.assertTrue(all(c["status"] == "not_run"
                            for c in r["cases"][1:]))
        bad = self.tmp / "DS1Edit-2.1.1-win32.zip"
        bad.write_bytes(b"PK\x03\x04 this is not really a zip")
        r = self.inspect(bad, "windows-x86")
        self.assertEqual(status(r, "archive"), "mismatch")
        self.assertNotEqual(r["status"], "pass")

    def test_wrong_archive_format_for_platform(self):
        files = good_files("windows-x86")
        path = self.tmp / "DS1Edit-2.1.1-win32.zip"
        write_tar(path, [(f"{root_name('windows-x86')}/{k}", v)
                         for k, v in files.items()])
        self.assert_blocked(self.inspect(path, "windows-x86"), "archive")

    def test_reused_extraction_dir_is_not_overwritten(self):
        sentinel = self.out / "extracted" / "keep.txt"
        sentinel.parent.mkdir(parents=True)
        sentinel.write_text("mine")
        r = self.inspect(build(self.tmp, "windows-x86"), "windows-x86")
        self.assertEqual(status(r, "extraction"), "unavailable")
        self.assertNotEqual(r["status"], "pass")
        self.assertEqual(sentinel.read_text(), "mine")
        self.assertEqual(list(sentinel.parent.iterdir()), [sentinel])


class Executables(PackageCheckCase):
    def test_wrong_architecture(self):
        cases = [
            ("windows-x86", "ds1edit.exe", make_pe(machine=0x8664,
                                                   pe32plus=True)),
            ("windows-x86", "ds1edit.exe", make_pe(machine=0xAA64,
                                                   pe32plus=True)),
            ("windows-x86", "ds1edit.exe", make_elf()),
            ("linux-x86_64", "ds1edit", make_elf(machine=183)),
            ("linux-x86_64", "ds1edit", make_elf(machine=3, cls=1)),
            ("linux-x86_64", "ds1edit", make_pe()),
            ("macos-arm64", "ds1edit", make_macho(cputype=0x01000007)),
            ("macos-arm64", "ds1edit", b"#!/bin/sh\n2.1.1\x00"),
        ]
        for i, (platform, exe, blob) in enumerate(cases):
            with self.subTest(i=i, platform=platform):
                files = good_files(platform)
                files[exe] = blob
                d = self.tmp / str(i)
                d.mkdir()
                r = self.inspect(build(d, platform, files), platform,
                                 out=d / "out")
                self.assert_blocked(r, "executable_header")

    def test_wrong_architecture_dll_in_closure(self):
        files = good_files("windows-x86")
        files["vcruntime140.dll"] = make_pe(machine=0x8664, pe32plus=True,
                                            imports=("KERNEL32.dll",))
        r = self.inspect(build(self.tmp, "windows-x86", files), "windows-x86")
        self.assert_blocked(r, "runtime_closure")

    def test_missing_runtime_dll(self):
        for drop in ("vcruntime140.dll", "allegro-5.2.dll"):
            with self.subTest(drop=drop):
                files = good_files("windows-x86")
                del files[drop]
                d = self.tmp / drop
                d.mkdir()
                r = self.inspect(build(d, "windows-x86", files),
                                 "windows-x86", out=d / "out")
                self.assert_blocked(r, "runtime_closure")
                failed = [f["actual"] for c in r["cases"]
                          for f in c["failures"] if c["id"].endswith(
                              "runtime_closure")]
                self.assertIn(drop.upper() if drop.startswith("vc") else drop,
                              [x.upper() if x.startswith("V") else x
                               for x in failed])

    def test_linux_undeclared_shared_library(self):
        files = good_files("linux-x86_64")
        files["ds1edit"] = make_elf(needed=("liballegro.so.5.2",
                                            "libsecret-thing.so.3"))
        r = self.inspect(build(self.tmp, "linux-x86_64", files),
                         "linux-x86_64")
        self.assert_blocked(r, "runtime_closure")

    def test_linux_executable_bit_required(self):
        arc = self.tmp / f"{root_name('linux-x86_64')}.tar.gz"
        with tarfile.open(arc, "w:gz") as t:
            for k, v in good_files("linux-x86_64").items():
                ti = tarfile.TarInfo(f"{root_name('linux-x86_64')}/{k}")
                ti.size, ti.mode = len(v), 0o644
                t.addfile(ti, io.BytesIO(v))
        self.assert_blocked(self.inspect(arc, "linux-x86_64"),
                            "executable_header")

    def test_macos_unbundled_rpath_dylib(self):
        files = good_files("macos-arm64")
        files["ds1edit"] = make_macho(dylibs=("@rpath/liballegro.5.2.dylib",))
        r = self.inspect(build(self.tmp, "macos-arm64", files), "macos-arm64")
        self.assert_blocked(r, "runtime_closure")


class Versions(PackageCheckCase):
    def test_package_of_other_version(self):
        for platform in pc.PLATFORMS:
            with self.subTest(platform=platform):
                d = self.tmp / platform
                d.mkdir()
                arc = build(d, platform, version="2.1.0")
                r = self.inspect(arc, platform, version=VERSION,
                                 out=d / "out")
                self.assert_blocked(r, "layout")
                self.assert_blocked(r, "executable_version")

    def test_renamed_archive_of_other_version(self):
        # Right file name and top directory, but the executable is old.
        files = good_files("windows-x86")
        files["ds1edit.exe"] = make_pe(version="2.1.0")
        r = self.inspect(build(self.tmp, "windows-x86", files), "windows-x86")
        self.assertEqual(status(r, "layout"), "pass")
        self.assert_blocked(r, "executable_version")

    def test_version_needs_exact_string(self):
        files = good_files("windows-x86")
        files["ds1edit.exe"] = make_pe(version="12.1.11")
        r = self.inspect(build(self.tmp, "windows-x86", files), "windows-x86",
                         version="2.1.1")
        self.assert_blocked(r, "executable_version")


class MacApp(PackageCheckCase):
    def mutate(self, **changes):
        files = good_files("macos-arm64")
        for k, v in changes.items():
            key = k.replace("__", "/").replace("_DOT_", ".")
            if v is None:
                del files[key]
            else:
                files[key] = v
        return self.inspect(build(self.tmp, "macos-arm64", files),
                            "macos-arm64")

    def test_missing_app_bundle(self):
        files = {k: v for k, v in good_files("macos-arm64").items()
                 if not k.startswith("ds1edit.app/")}
        r = self.inspect(build(self.tmp, "macos-arm64", files), "macos-arm64")
        self.assert_blocked(r, "required_members")
        self.assert_blocked(r, "macos_app")

    def test_bundle_version_must_match(self):
        r = self.mutate(**{"ds1edit_DOT_app__Contents__Info_DOT_plist":
                           plist(version="")})
        self.assert_blocked(r, "macos_app")

    def test_bundle_executable_name(self):
        r = self.mutate(**{"ds1edit_DOT_app__Contents__Info_DOT_plist":
                           plist(executable="other")})
        self.assert_blocked(r, "macos_app")

    def test_bundle_binary_architecture(self):
        r = self.mutate(**{"ds1edit_DOT_app__Contents__MacOS__ds1edit":
                           make_macho(cputype=0x01000007)})
        self.assert_blocked(r, "macos_app")

    def test_bundle_signature_present(self):
        r = self.mutate(**{
            "ds1edit_DOT_app__Contents___CodeSignature__CodeResources": None})
        self.assert_blocked(r, "macos_app")

    def test_other_platforms_do_not_require_app(self):
        r = self.inspect(build(self.tmp, "linux-x86_64"), "linux-x86_64")
        self.assertFalse(any(c["id"].endswith("macos_app")
                             for c in r["cases"]))


class HeaderParsers(unittest.TestCase):
    def test_pe_imports_round_trip(self):
        info = pc.parse_pe(make_pe(imports=("A.dll", "b.DLL")))
        self.assertEqual(info["machine"], 0x014C)
        self.assertEqual(info["imports"], ["A.dll", "b.DLL"])

    def test_elf_needed_round_trip(self):
        info = pc.parse_elf(make_elf(needed=("liba.so.1", "libb.so.2")))
        self.assertEqual(info["needed"], ["liba.so.1", "libb.so.2"])

    def test_macho_dylibs_round_trip(self):
        info = pc.parse_macho(make_macho(dylibs=("/usr/lib/libz.1.dylib",)))
        self.assertEqual(info["dylibs"], ["/usr/lib/libz.1.dylib"])

    def test_truncated_headers_are_errors(self):
        for blob in (b"MZ", make_pe()[:0x60], b"\x7fELF\x02\x01",
                     b"\xcf\xfa\xed\xfe\x0c\x00\x00\x01\x00"):
            with self.subTest(blob=blob[:8]):
                with self.assertRaises((pc.HeaderError, struct.error)):
                    pc.parse_executable(blob)


if __name__ == "__main__":
    unittest.main()
