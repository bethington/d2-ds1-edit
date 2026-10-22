"""Inspect a DS1Edit release archive before anybody trusts or runs it.

``inspect_package`` reads every member header of a CPack ZIP/TGZ, rejects
unsafe names, links and duplicates, checks the declared layout, the absence of
game data and user configuration, the executable's architecture and embedded
version, the macOS app entry point, and the executable's runtime closure. Only
when every safety check passes does it extract the archive, and only into a
fresh directory it owns under ``out_dir``.

Results are ``ds1edit.case/1`` dictionaries (package B's result schema) plus
the archive hash and the exact entry inventory. A failed check is never
reported as a pass; a check that could not be evaluated is ``not_run`` or
``unavailable``, which blocks qualification exactly like a mismatch.

CLI::

    python -m scripts.equivalence.package_check --archive PATH \
        --platform windows-x86|linux-x86_64|macos-arm64 \
        --version VERSION --out-dir PATH

Exit status: 0 every case passed, 1 any case did not pass, 2 invalid usage.
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import plistlib
import posixpath
import re
import stat
import struct
import sys
import tarfile
import zipfile
from pathlib import Path
from typing import Callable, Iterable, Iterator

try:  # Package B's shared contracts module, when present.
    from scripts.equivalence.contracts import canonical_digest  # type: ignore
except Exception:  # pragma: no cover - depends on the checkout state
    try:
        from .contracts import canonical_digest  # type: ignore
    except Exception:
        def canonical_digest(value: dict) -> str:
            """Local stand-in matching package B's definition: UTF-8 JSON,
            sorted keys, compact separators, SHA-256, own ``digest`` omitted."""
            if isinstance(value, dict):
                value = {k: v for k, v in value.items() if k != "digest"}
            data = json.dumps(value, sort_keys=True, separators=(",", ":"),
                              ensure_ascii=False).encode("utf-8")
            return hashlib.sha256(data).hexdigest()

TOOL_VERSION = "package_check/1"
CASE_SCHEMA = "ds1edit.case/1"
REPORT_SCHEMA = "ds1edit.package_inspection/1"
STATUSES = ("pass", "mismatch", "unavailable", "not_run", "partial",
            "rejected_expected")
LANE = "native-package"

MAX_MEMBERS = 20000
MAX_TOTAL_BYTES = 2 * 1024 ** 3
CHUNK = 1024 * 1024

_VERSION_RE = re.compile(r"^\d+\.\d+\.\d+([.-][0-9A-Za-z.-]+)?$")
_DRIVE_RE = re.compile(r"^[A-Za-z]:")
_WIN_RESERVED = {"con", "prn", "aux", "nul",
                 *(f"com{i}" for i in range(1, 10)),
                 *(f"lpt{i}" for i in range(1, 10))}

# ---------------------------------------------------------------------------
# Platform policy
# ---------------------------------------------------------------------------

COMMON_REQUIRED = (
    "Ds1edit.ini.sample",
    "LICENSE",
    "NOTICE",
    "README.md",
    "assets/editor/ds1edit.dt1",
    "assets/editor/obj.txt",
    "assets/ui/cursor_green.png",
    "assets/act1_town.ini",
)
# (glob, minimum count) -- the subtrees must be present, not just one name.
COMMON_REQUIRED_GLOBS = (
    ("assets/ui/*.png", 10),
    ("assets/*.ini", 1),
)

# Game data, generated caches and per-user configuration. Matching is on the
# lower-cased path relative to the package root.
FORBIDDEN_GLOBS = (
    "*.mpq",
    "*.dc6", "*.dcc", "*.cof", "*.ds1", "*.dt1",
    "assets/tiles/*",
    "assets/editor/pal*.bin",
    "assets/editor/cmap*.bin",
    "assets/editor/gamma.dat",
    "assets/editor/version",
    "*.pdb",
    "stderr.txt", "stdout.txt",
)
FORBIDDEN_COMPONENTS = ("excel", "tiles", "cache", ".cache", "__pycache__",
                        "mpq_cache", "data")
# Exact relative paths exempt from FORBIDDEN_GLOBS (shipped editor assets).
FORBIDDEN_EXEMPT = ("assets/editor/ds1edit.dt1",)
# A user's own INI is not shipped; only the sample is. Any other *.ini outside
# assets/ (where the area definitions live) is local configuration.
USER_INI_NAMES = ("ds1edit.ini",)

# Windows DLLs supplied by every supported Windows (10/11) installation.
WINDOWS_OS_DLLS = frozenset(n.lower() for n in (
    "kernel32.dll", "kernelbase.dll", "ntdll.dll", "user32.dll", "gdi32.dll",
    "gdiplus.dll", "advapi32.dll", "shell32.dll", "shlwapi.dll", "ole32.dll",
    "oleaut32.dll", "comdlg32.dll", "comctl32.dll", "opengl32.dll",
    "glu32.dll", "winmm.dll", "ws2_32.dll", "version.dll", "imm32.dll",
    "setupapi.dll", "cfgmgr32.dll", "bcrypt.dll", "crypt32.dll",
    "uxtheme.dll", "dwmapi.dll", "psapi.dll", "msvcrt.dll", "ucrtbase.dll",
    "d3d9.dll", "d3d11.dll", "dxgi.dll", "dsound.dll", "dinput8.dll",
    "xinput1_4.dll", "xinput9_1_0.dll", "hid.dll", "rpcrt4.dll",
    "secur32.dll", "userenv.dll", "wininet.dll", "winhttp.dll",
    "shcore.dll", "powrprof.dll", "mfplat.dll", "mf.dll", "mfreadwrite.dll",
))
WINDOWS_OS_PREFIXES = ("api-ms-win-", "ext-ms-")

# Libraries the Linux package declares as system prerequisites (README:
# distribution Allegro 5.2 packages and the system OpenGL driver, which is
# never shipped) plus the C runtime.
LINUX_DECLARED_PREREQS = (
    r"liballegro(_[a-z]+)?\.so\.5\.2(\.\d+)*",
    r"libOpenGL\.so\.0", r"libGL\.so\.1", r"libGLX\.so\.0", r"libEGL\.so\.1",
    r"libc\.so\.6", r"libm\.so\.6", r"libdl\.so\.2", r"libpthread\.so\.0",
    r"librt\.so\.1", r"libz\.so\.1", r"libgcc_s\.so\.1",
    r"ld-linux-x86-64\.so\.2",
)
# macOS: system libraries plus Homebrew's Allegro (README prerequisite).
MACOS_DECLARED_PREREQS = (
    r"/usr/lib/.+",
    r"/System/Library/.+",
    r"/(opt/homebrew|usr/local)/(opt|Cellar)/allegro/.*liballegro(_[a-z]+)?\.5\.2(\.\d+)*\.dylib",
    r"/(opt/homebrew|usr/local)/lib/liballegro(_[a-z]+)?\.5\.2(\.\d+)*\.dylib",
)

PLATFORMS = {
    "windows-x86": {
        "system": "win32",
        "executable": "ds1edit.exe",
        "format": "pe",
        "machine": 0x014C,          # IMAGE_FILE_MACHINE_I386
        "archive": "zip",
        "app": False,
    },
    "linux-x86_64": {
        "system": "linux-x86_64",
        "executable": "ds1edit",
        "format": "elf",
        "machine": 62,              # EM_X86_64
        "elf_class": 2,             # ELFCLASS64
        "archive": "tar",
        "app": False,
    },
    "macos-arm64": {
        "system": "macos-arm64",
        "executable": "ds1edit",
        "format": "macho",
        "machine": 0x0100000C,      # CPU_TYPE_ARM64
        "archive": "tar",
        "app": True,
    },
}
APP_BUNDLE = "ds1edit.app"
APP_IDENTIFIER = "org.ds1edit.ds1edit"


def _policy(platform: str) -> dict:
    p = dict(PLATFORMS[platform])
    p.update({
        "platform": platform,
        "required": list(COMMON_REQUIRED),
        "required_globs": [list(g) for g in COMMON_REQUIRED_GLOBS],
        "forbidden_globs": list(FORBIDDEN_GLOBS),
        "forbidden_components": list(FORBIDDEN_COMPONENTS),
        "forbidden_exempt": list(FORBIDDEN_EXEMPT),
        "tool": TOOL_VERSION,
    })
    return p


# ---------------------------------------------------------------------------
# Archive reading
# ---------------------------------------------------------------------------

class ArchiveError(Exception):
    pass


class _Member:
    __slots__ = ("raw", "kind", "size", "mode", "link", "handle")

    def __init__(self, raw, kind, size, mode, link, handle):
        self.raw = raw          # name exactly as stored
        self.kind = kind        # file | dir | symlink | hardlink | special
        self.size = size
        self.mode = mode
        self.link = link        # link target (symlink/hardlink) or None
        self.handle = handle    # archive-specific object for reading


class _Archive:
    def __init__(self, path: Path):
        self.path = path
        self.format = None
        self._zip = None
        self._tar = None
        if zipfile.is_zipfile(path):
            self.format = "zip"
            self._zip = zipfile.ZipFile(path)
        else:
            try:
                self._tar = tarfile.open(path, mode="r:*")
            except (tarfile.TarError, OSError) as exc:
                raise ArchiveError(f"not a readable ZIP or tar archive: {exc}")
            self.format = "tar"

    def close(self):
        if self._zip is not None:
            self._zip.close()
        if self._tar is not None:
            self._tar.close()

    def members(self) -> list[_Member]:
        out = []
        if self._zip is not None:
            for info in self._zip.infolist():
                mode = (info.external_attr >> 16) & 0xFFFF
                ftype = stat.S_IFMT(mode)
                if info.filename.endswith("/") or info.is_dir():
                    kind = "dir"
                elif ftype == stat.S_IFLNK:
                    kind = "symlink"
                elif ftype not in (0, stat.S_IFREG):
                    kind = "special"
                else:
                    kind = "file"
                link = None
                if kind == "symlink":
                    if info.file_size > 4096:
                        raise ArchiveError(f"oversized symlink {info.filename!r}")
                    link = self._zip.read(info).decode("utf-8", "replace")
                out.append(_Member(info.filename, kind, info.file_size,
                                   mode & 0o7777, link, info))
        else:
            try:
                infos = self._tar.getmembers()
            except (tarfile.TarError, OSError, EOFError) as exc:
                raise ArchiveError(f"corrupt tar archive: {exc}")
            for info in infos:
                if info.isdir():
                    kind = "dir"
                elif info.issym():
                    kind = "symlink"
                elif info.islnk():
                    kind = "hardlink"
                elif info.isreg():
                    kind = "file"
                else:
                    kind = "special"
                link = info.linkname if kind in ("symlink", "hardlink") else None
                out.append(_Member(info.name, kind, info.size,
                                   info.mode & 0o7777, link, info))
        return out

    def open(self, member: _Member):
        if self._zip is not None:
            return self._zip.open(member.handle)
        f = self._tar.extractfile(member.handle)
        if f is None:
            raise ArchiveError(f"cannot read {member.raw!r}")
        return f

    def iter_chunks(self, member: _Member, budget: list) -> Iterator[bytes]:
        """Stream a member, enforcing its declared size and a global budget."""
        seen = 0
        with self.open(member) as f:
            while True:
                chunk = f.read(CHUNK)
                if not chunk:
                    break
                seen += len(chunk)
                budget[0] -= len(chunk)
                if seen > member.size:
                    raise ArchiveError(
                        f"{member.raw!r} expands beyond its declared size")
                if budget[0] < 0:
                    raise ArchiveError("archive expands beyond the size limit")
                yield chunk
        if seen != member.size:
            raise ArchiveError(f"{member.raw!r} is truncated")


# ---------------------------------------------------------------------------
# Name / link safety
# ---------------------------------------------------------------------------

def name_problem(raw: str) -> str | None:
    """Return why a stored member name is unsafe, or None if it is acceptable."""
    if raw == "" or raw in ("/", "./"):
        return "empty name"
    if "\x00" in raw:
        return "NUL in name"
    if "\\" in raw:
        return "backslash in name (Windows separator or UNC path)"
    if raw.startswith("//"):
        return "UNC-style name"
    if raw.startswith("/"):
        return "absolute name"
    if _DRIVE_RE.match(raw):
        return "drive-qualified name"
    body = raw[:-1] if raw.endswith("/") else raw
    for comp in body.split("/"):
        if comp == "":
            return "empty path component"
        if comp in (".", ".."):
            return "traversal component"
        if any(ord(c) < 32 for c in comp):
            return "control character in name"
        if comp.rstrip(" .") != comp:
            return "component ends with space or dot"
        if comp.split(".")[0].lower() in _WIN_RESERVED:
            return "Windows reserved device name"
        if ":" in comp:
            return "colon in name (alternate data stream or drive)"
    return None


def link_escapes(member_path: str, target: str) -> bool:
    """True if a link stored at ``member_path`` would resolve outside the
    archive root or outside the package's top-level directory."""
    if not target or target.startswith("/") or "\\" in target \
            or _DRIVE_RE.match(target):
        return True
    joined = posixpath.normpath(posixpath.join(posixpath.dirname(member_path),
                                               target))
    if joined == ".." or joined.startswith("../") or joined.startswith("/"):
        return True
    # The package root is the member's top directory, not the archive root.
    top = member_path.split("/", 1)[0]
    return "/" in member_path and joined.split("/", 1)[0] != top


# ---------------------------------------------------------------------------
# Executable headers
# ---------------------------------------------------------------------------

class HeaderError(Exception):
    pass


def _u16(b, o):
    if o + 2 > len(b):
        raise HeaderError("truncated header")
    return struct.unpack_from("<H", b, o)[0]


def _u32(b, o):
    if o + 4 > len(b):
        raise HeaderError("truncated header")
    return struct.unpack_from("<I", b, o)[0]


def _cstr(b, o, limit=512):
    if o < 0 or o >= len(b):
        raise HeaderError("string offset out of range")
    end = b.find(b"\x00", o, o + limit)
    if end < 0:
        raise HeaderError("unterminated string")
    return b[o:end].decode("ascii", "replace")


def parse_pe(data: bytes) -> dict:
    """Machine type plus normal and delay-load imported DLL names."""
    if data[:2] != b"MZ":
        raise HeaderError("missing MZ signature")
    pe = _u32(data, 0x3C)
    if data[pe:pe + 4] != b"PE\x00\x00":
        raise HeaderError("missing PE signature")
    machine = _u16(data, pe + 4)
    nsect = _u16(data, pe + 6)
    opt_size = _u16(data, pe + 20)
    opt = pe + 24
    magic = _u16(data, opt)
    if magic == 0x10B:
        dd = opt + 96
    elif magic == 0x20B:
        dd = opt + 112
    else:
        raise HeaderError(f"unknown optional header magic {magic:#x}")
    ndirs = _u32(data, dd - 4)
    sections = []
    st = opt + opt_size
    for i in range(nsect):
        s = st + 40 * i
        if s + 24 > len(data):
            raise HeaderError("truncated section table")
        vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, s + 8)
        sections.append((vaddr, max(vsize, rsize), rptr, rsize))

    def rva(r):
        for vaddr, span, rptr, rsize in sections:
            if vaddr <= r < vaddr + span and r - vaddr < rsize:
                return rptr + (r - vaddr)
        raise HeaderError(f"RVA {r:#x} outside sections")

    def dir_entry(i):
        if i >= ndirs:
            return 0, 0
        return _u32(data, dd + 8 * i), _u32(data, dd + 8 * i + 4)

    imports, delay = [], []
    irva, isize = dir_entry(1)
    if irva and isize:
        o = rva(irva)
        for _ in range(4096):
            if o + 20 > len(data):
                raise HeaderError("truncated import directory")
            fields = struct.unpack_from("<IIIII", data, o)
            if not any(fields):
                break
            imports.append(_cstr(data, rva(fields[3])))
            o += 20
    drva, dsize = dir_entry(13)
    if drva and dsize:
        o = rva(drva)
        for _ in range(4096):
            if o + 32 > len(data):
                raise HeaderError("truncated delay-import directory")
            fields = struct.unpack_from("<8I", data, o)
            if not any(fields):
                break
            attrs, name = fields[0], fields[1]
            # Old (VC6) descriptors hold VAs rather than RVAs.
            if not (attrs & 1):
                base = _u32(data, opt + 28) if magic == 0x10B else 0
                name -= base
            delay.append(_cstr(data, rva(name)))
            o += 32
    return {"format": "pe", "machine": machine, "pe32plus": magic == 0x20B,
            "imports": imports, "delay_imports": delay}


def parse_elf(data: bytes) -> dict:
    if data[:4] != b"\x7fELF":
        raise HeaderError("missing ELF magic")
    cls, enc = data[4], data[5]
    if enc != 1:
        raise HeaderError("big-endian ELF is not a supported target")
    machine = _u16(data, 18)
    etype = _u16(data, 16)
    needed = []
    if cls == 2:
        phoff = struct.unpack_from("<Q", data, 32)[0]
        phentsize, phnum = _u16(data, 54), _u16(data, 56)
        phfmt, dynfmt, dynsz = "<IIQQQQQQ", "<qQ", 16
    elif cls == 1:
        phoff = _u32(data, 28)
        phentsize, phnum = _u16(data, 42), _u16(data, 44)
        phfmt, dynfmt, dynsz = "<IIIIIIII", "<iI", 8
    else:
        raise HeaderError("unknown ELF class")
    loads, dynamic = [], None
    for i in range(min(phnum, 256)):
        o = phoff + i * phentsize
        if o + struct.calcsize(phfmt) > len(data):
            raise HeaderError("truncated program headers")
        f = struct.unpack_from(phfmt, data, o)
        if cls == 2:
            ptype, offset, vaddr, filesz = f[0], f[2], f[3], f[5]
        else:
            ptype, offset, vaddr, filesz = f[0], f[1], f[2], f[4]
        if ptype == 1:
            loads.append((vaddr, offset, filesz))
        elif ptype == 2:
            dynamic = (offset, filesz)
    if dynamic:
        entries, strtab = [], None
        off, size = dynamic
        for i in range(size // dynsz):
            if off + (i + 1) * dynsz > len(data):
                break
            tag, val = struct.unpack_from(dynfmt, data, off + i * dynsz)
            if tag == 0:
                break
            if tag == 5:
                strtab = val
            entries.append((tag, val))
        if strtab is not None:
            stroff = None
            for vaddr, offset, filesz in loads:
                if vaddr <= strtab < vaddr + filesz:
                    stroff = offset + (strtab - vaddr)
            if stroff is None:
                raise HeaderError("DT_STRTAB outside loadable segments")
            needed = [_cstr(data, stroff + v) for t, v in entries if t == 1]
    return {"format": "elf", "machine": machine, "elf_class": cls,
            "needed": needed}


_LC_DYLIBS = {0xC, 0x80000018, 0x8000001F, 0x80000023}


def _parse_thin_macho(data: bytes, base: int) -> dict:
    magic = _u32(data, base)
    if magic != 0xFEEDFACF:
        raise HeaderError("not a 64-bit little-endian Mach-O")
    cputype = _u32(data, base + 4)
    ncmds, sizeofcmds = _u32(data, base + 16), _u32(data, base + 20)
    o, end = base + 32, base + 32 + sizeofcmds
    dylibs = []
    for _ in range(min(ncmds, 4096)):
        if o + 8 > min(end, len(data)):
            raise HeaderError("truncated load commands")
        cmd, cmdsize = _u32(data, o), _u32(data, o + 4)
        if cmdsize < 8:
            raise HeaderError("invalid load command size")
        if cmd in _LC_DYLIBS:
            dylibs.append(_cstr(data, o + _u32(data, o + 8)))
        o += cmdsize
    return {"cputype": cputype, "dylibs": dylibs}


def parse_macho(data: bytes) -> dict:
    if data[:4] in (b"\xca\xfe\xba\xbe",):
        nfat = struct.unpack_from(">I", data, 4)[0]
        slices = []
        for i in range(min(nfat, 16)):
            cpu, _sub, off, _size, _align = struct.unpack_from(
                ">IIIII", data, 8 + 20 * i)
            slices.append((cpu, off))
        thin = [_parse_thin_macho(data, off) for _, off in slices]
        arm = [t for t in thin if t["cputype"] == 0x0100000C]
        pick = arm[0] if arm else thin[0]
        return {"format": "macho", "machine": pick["cputype"],
                "slices": [t["cputype"] for t in thin], "dylibs": pick["dylibs"]}
    t = _parse_thin_macho(data, 0)
    return {"format": "macho", "machine": t["cputype"],
            "slices": [t["cputype"]], "dylibs": t["dylibs"]}


def parse_executable(data: bytes) -> dict:
    if data[:2] == b"MZ":
        return parse_pe(data)
    if data[:4] == b"\x7fELF":
        return parse_elf(data)
    if data[:4] in (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe",
                    b"\xce\xfa\xed\xfe"):
        return parse_macho(data)
    raise HeaderError("unrecognised executable format")


def embeds_version(data: bytes, version: str) -> bool:
    """The executable carries ``version`` as a standalone C string."""
    pat = rb"(?<![0-9A-Za-z.])" + re.escape(version.encode("ascii")) + rb"\x00"
    return re.search(pat, data) is not None


# ---------------------------------------------------------------------------
# Results
# ---------------------------------------------------------------------------

def _case(check: str, platform: str, status: str, *, policy_digest: str,
          archive_sha: str | None, expected, actual, failures=(),
          coverage=None, limitations=()) -> dict:
    assert status in STATUSES
    fails = []
    for f in failures:
        f = dict(f)
        f.setdefault("kind", check)
        f.setdefault("detail", f.get("message", ""))
        fails.append(f)
    cov = {"distinct_inputs_compared": 1 if archive_sha else 0}
    cov.update(coverage or {})
    return {
        "schema": CASE_SCHEMA,
        "id": f"package.{platform}.{check}",
        "lane": LANE,
        "platform": platform,
        "required": True,
        "status": status,
        "success_outcome": "pass",
        "contract_digest": policy_digest,
        "baseline_digest": None,
        "input_sha256": archive_sha,
        "expected_sha256": canonical_digest({"v": expected}),
        "actual_sha256": canonical_digest({"v": actual}),
        "expected": expected,
        "actual": actual,
        "coverage": cov,
        "failures": fails,
        "provenance": {"tool": TOOL_VERSION, "archive_sha256": archive_sha},
        "limitations": list(limitations),
    }


def _aggregate(cases: Iterable[dict]) -> str:
    st = [c["status"] for c in cases]
    if not st:
        return "not_run"
    for s in ("mismatch", "unavailable", "not_run", "partial"):
        if s in st:
            return s
    return "pass"


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(CHUNK), b""):
            h.update(chunk)
    return h.hexdigest()


# ---------------------------------------------------------------------------
# Inspection
# ---------------------------------------------------------------------------

def _closure_windows(files: dict, exe_rel: str, read: Callable[[str], bytes],
                     machine: int = 0x014C) -> tuple[list, dict]:
    """Walk PE imports from the executable through packaged DLLs."""
    packaged = {posixpath.basename(r).lower(): r for r in files
                if posixpath.dirname(r) == posixpath.dirname(exe_rel)
                and r.lower().endswith(".dll")}
    failures, graph = [], {}
    queue, seen = [exe_rel], set()
    while queue:
        rel = queue.pop(0)
        if rel in seen:
            continue
        seen.add(rel)
        try:
            info = parse_pe(read(rel))
        except HeaderError as exc:
            failures.append({"path": rel, "message": f"unparseable PE: {exc}"})
            continue
        if info["machine"] != machine:
            failures.append({"path": rel, "field": "machine",
                             "expected": hex(machine),
                             "actual": hex(info["machine"]),
                             "message": f"{rel} is built for another "
                                        "architecture"})
        deps = info["imports"] + info["delay_imports"]
        graph[rel] = deps
        for dep in deps:
            low = dep.lower()
            if low in packaged:
                queue.append(packaged[low])
            elif low in WINDOWS_OS_DLLS or low.startswith(WINDOWS_OS_PREFIXES):
                continue
            else:
                kind = "delay-load" if dep in info["delay_imports"] else "import"
                failures.append({
                    "path": rel, "field": kind, "expected": "packaged or OS DLL",
                    "actual": dep,
                    "message": f"{rel} needs {dep}, which is neither packaged "
                               f"nor supplied by Windows"})
    unused = sorted(set(packaged.values()) - seen)
    return failures, {"graph": graph, "unreferenced_dlls": unused}


def _closure_posix(platform, info: dict, files: dict) -> tuple[list, dict]:
    if platform == "linux-x86_64":
        needs, pats = info.get("needed", []), LINUX_DECLARED_PREREQS
        packaged = {posixpath.basename(r) for r in files}
    else:
        needs, pats = info.get("dylibs", []), MACOS_DECLARED_PREREQS
        packaged = {posixpath.basename(r) for r in files}
    failures = []
    for dep in needs:
        if dep.startswith("@"):
            if posixpath.basename(dep) not in packaged:
                failures.append({"field": "dylib", "actual": dep,
                                 "message": f"{dep} is not packaged"})
            continue
        if posixpath.basename(dep) in packaged:
            continue
        if not any(re.fullmatch(p, dep) for p in pats):
            failures.append({"field": "needed", "actual": dep,
                             "message": f"{dep} is not packaged and not a "
                                        "declared system prerequisite"})
    return failures, {"needed": needs, "declared_prerequisites": list(pats)}


def inspect_package(archive: Path, platform: str, expected_version: str,
                    out_dir: Path) -> dict:
    if platform not in PLATFORMS:
        raise ValueError(f"unknown platform {platform!r}")
    if not _VERSION_RE.match(expected_version or ""):
        raise ValueError(f"invalid version {expected_version!r}")
    archive = Path(archive)
    out_dir = Path(out_dir)
    policy = _policy(platform)
    pdigest = canonical_digest(policy)
    cases: list[dict] = []
    report = {
        "schema": REPORT_SCHEMA,
        "tool": TOOL_VERSION,
        "platform": platform,
        "expected_version": expected_version,
        "policy_digest": pdigest,
        "archive": {"path": str(archive), "name": archive.name,
                    "sha256": None, "size": None, "format": None},
        "root": None,
        "entries": [],
        "extracted_root": None,
        "cases": cases,
    }

    def add(check, status, expected=None, actual=None, **kw):
        cases.append(_case(check, platform, status, policy_digest=pdigest,
                           archive_sha=report["archive"]["sha256"],
                           expected=expected, actual=actual, **kw))

    later = ("member_names", "member_uniqueness", "member_types", "layout",
             "required_members", "forbidden_members", "executable_header",
             "executable_version", "runtime_closure", "extraction")
    if PLATFORMS[platform]["app"]:
        later = later[:-1] + ("macos_app", "extraction")

    def finish():
        report["status"] = _aggregate(cases)
        report["digest"] = canonical_digest(report)
        return report

    # -- archive identity ---------------------------------------------------
    if not archive.is_file():
        add("archive", "unavailable", "readable archive", "missing",
            failures=[{"message": f"{archive} does not exist"}])
        for c in later:
            add(c, "not_run", failures=[{"message": "archive unavailable"}])
        return finish()
    report["archive"]["sha256"] = _sha256_file(archive)
    report["archive"]["size"] = archive.stat().st_size
    try:
        arc = _Archive(archive)
        members = arc.members()
    except (ArchiveError, zipfile.BadZipFile, OSError) as exc:
        add("archive", "mismatch", "readable archive", "unreadable",
            failures=[{"message": str(exc)}])
        for c in later:
            add(c, "not_run", failures=[{"message": "archive unreadable"}])
        return finish()
    try:
        return _inspect_members(arc, members, platform, expected_version,
                                out_dir, report, add, finish)
    finally:
        arc.close()


def _inspect_members(arc, members, platform, version, out_dir, report, add,
                     finish):
    plat = PLATFORMS[platform]
    report["archive"]["format"] = arc.format
    afail = []
    if not members:
        afail.append({"message": "archive has no members"})
    if len(members) > MAX_MEMBERS:
        afail.append({"message": f"more than {MAX_MEMBERS} members"})
    if arc.format != plat["archive"]:
        afail.append({"field": "format", "expected": plat["archive"],
                      "actual": arc.format,
                      "message": "unexpected archive format for platform"})
    declared = sum(m.size for m in members if m.kind == "file")
    if declared > MAX_TOTAL_BYTES:
        afail.append({"message": "declared size exceeds limit"})
    add("archive", "mismatch" if afail else "pass", "non-empty archive",
        {"members": len(members), "format": arc.format,
         "declared_bytes": declared}, failures=afail)

    # -- names ---------------------------------------------------------------
    nfail = []
    for m in members:
        why = name_problem(m.raw)
        if why:
            nfail.append({"path": m.raw, "message": why})
    add("member_names", "mismatch" if nfail else "pass",
        "relative, normalized names", f"{len(nfail)} unsafe",
        failures=nfail, coverage={"members": len(members)})
    names_ok = not nfail

    # -- duplicates (exact and case-folded: Windows/macOS fold case) ---------
    dfail, seen_exact, seen_fold = [], {}, {}
    for m in members:
        key = m.raw.rstrip("/")
        if key in seen_exact:
            dfail.append({"path": m.raw, "message": "duplicate member"})
        elif key.casefold() in seen_fold:
            dfail.append({"path": m.raw,
                          "message": f"case-insensitive duplicate of "
                                     f"{seen_fold[key.casefold()]!r}"})
        seen_exact[key] = m
        seen_fold.setdefault(key.casefold(), m.raw)
    add("member_uniqueness", "mismatch" if dfail else "pass", "unique names",
        f"{len(dfail)} duplicates", failures=dfail)

    # -- link / special types --------------------------------------------------
    tfail = []
    for m in members:
        if m.kind == "symlink":
            if link_escapes(m.raw.rstrip("/"), m.link or ""):
                tfail.append({"path": m.raw, "actual": m.link,
                              "message": "symlink escapes the package root"})
            else:
                tfail.append({"path": m.raw, "actual": m.link,
                              "message": "symlink is not part of the declared "
                                         "package layout"})
        elif m.kind == "hardlink":
            tfail.append({"path": m.raw, "actual": m.link,
                          "message": "hard link member"})
        elif m.kind == "special":
            tfail.append({"path": m.raw, "message": "device/fifo/special member"})
    add("member_types", "mismatch" if tfail else "pass",
        "regular files and directories only", f"{len(tfail)} rejected",
        failures=tfail)
    safe = names_ok and not dfail and not tfail and not afail

    # -- inventory + layout ----------------------------------------------------
    expected_root = f"DS1Edit-{version}-{plat['system']}"
    tops = sorted({m.raw.split("/", 1)[0] for m in members if m.raw})
    lfail = []
    if len(tops) != 1:
        lfail.append({"field": "top-level", "expected": [expected_root],
                      "actual": tops,
                      "message": "package must have exactly one top directory"})
    elif tops[0] != expected_root:
        lfail.append({"field": "top-level", "expected": expected_root,
                      "actual": tops[0],
                      "message": "top directory does not name the expected "
                                 "version/platform"})
    stem = report["archive"]["name"]
    for suffix in (".zip", ".tgz", ".tar.gz", ".tar"):
        if stem.lower().endswith(suffix):
            stem = stem[: -len(suffix)]
            break
    # CPack's own name, or the name release.yml stages it under.
    names = {expected_root, f"ds1edit-v{version}-{platform}",
             f"ds1edit-{version}-{platform}"}
    if stem not in names:
        lfail.append({"field": "archive-name", "expected": sorted(names),
                      "actual": report["archive"]["name"],
                      "message": "archive name does not name the expected "
                                 "version/platform"})
    report["root"] = expected_root

    # Locate members under the package's single top directory even when its
    # name is wrong, so a version mismatch still reports the other checks.
    prefix = (tops[0] if len(tops) == 1 else expected_root) + "/"
    files: dict[str, _Member] = {}
    for m in members:
        rel = m.raw[len(prefix):] if m.raw.startswith(prefix) else None
        entry = {"name": m.raw, "type": m.kind, "size": m.size,
                 "mode": oct(m.mode), "link": m.link, "sha256": None}
        report["entries"].append(entry)
        if rel and m.kind == "file":
            files[rel.rstrip("/")] = m
    add("layout", "mismatch" if lfail else "pass", expected_root, tops,
        failures=lfail)

    budget = [MAX_TOTAL_BYTES]
    hashes: dict[str, str] = {}
    head_cache: dict[str, bytes] = {}

    def read(rel: str) -> bytes:
        if rel not in head_cache:
            head_cache[rel] = b"".join(arc.iter_chunks(files[rel], budget))
        return head_cache[rel]

    hash_error = None
    if names_ok and not afail:
        try:
            for entry, m in zip(report["entries"], members):
                if m.kind == "file":
                    h = hashlib.sha256()
                    for chunk in arc.iter_chunks(m, budget):
                        h.update(chunk)
                    entry["sha256"] = hashes[m.raw] = h.hexdigest()
        except (ArchiveError, zipfile.BadZipFile, OSError, EOFError,
                tarfile.TarError) as exc:
            hash_error = str(exc)
            safe = False
            cases_archive = report["cases"][0]
            cases_archive["status"] = "mismatch"
            cases_archive["failures"].append({"message": hash_error})
        budget[0] = MAX_TOTAL_BYTES

    # -- required ----------------------------------------------------------------
    rfail = []
    exe_rel = plat["executable"]
    required = [exe_rel] + list(COMMON_REQUIRED)
    if plat["app"]:
        required += [f"{APP_BUNDLE}/Contents/Info.plist",
                     f"{APP_BUNDLE}/Contents/MacOS/ds1edit"]
    for r in required:
        if r not in files:
            rfail.append({"path": r, "message": "required member missing"})
    for pattern, minimum in COMMON_REQUIRED_GLOBS:
        n = sum(1 for f in files if fnmatch.fnmatchcase(f, pattern)
                and "/" not in f[len(pattern.split("*")[0]):])
        if n < minimum:
            rfail.append({"path": pattern, "expected": minimum, "actual": n,
                          "message": "too few members"})
    add("required_members", "mismatch" if rfail else "pass",
        required, sorted(files), failures=rfail,
        coverage={"required": len(required) + len(COMMON_REQUIRED_GLOBS)})

    # -- forbidden -----------------------------------------------------------------
    ffail = []
    for m in members:
        rel = m.raw[len(prefix):] if m.raw.startswith(prefix) else m.raw
        rel = rel.rstrip("/")
        low = rel.lower()
        if not low or low in FORBIDDEN_EXEMPT:
            continue
        comps = low.split("/")
        hit = next((g for g in FORBIDDEN_GLOBS if fnmatch.fnmatchcase(low, g)
                    or fnmatch.fnmatchcase(comps[-1], g)), None)
        if hit is None and any(c in FORBIDDEN_COMPONENTS for c in comps[:-1]
                               + ([comps[-1]] if m.kind == "dir" else [])):
            hit = "forbidden directory"
        if hit is None and comps[-1] in USER_INI_NAMES:
            hit = "user INI"
        if hit is None and comps[-1].endswith(".ini") and comps[0] != "assets":
            hit = "local INI outside assets/"
        if hit:
            ffail.append({"path": m.raw, "field": hit,
                          "message": "forbidden package content"})
    add("forbidden_members", "mismatch" if ffail else "pass",
        "no game data, caches or user configuration",
        f"{len(ffail)} forbidden", failures=ffail)

    # -- executable ------------------------------------------------------------------
    exe_info = None
    if exe_rel in files and not hash_error:
        try:
            exe_data = read(exe_rel)
            exe_info = parse_executable(exe_data)
        except (HeaderError, ArchiveError, struct.error) as exc:
            add("executable_header", "mismatch", plat["format"], None,
                failures=[{"path": exe_rel, "message": str(exc)}])
            exe_data = None
        if exe_info is not None:
            hfail = []
            if exe_info["format"] != plat["format"]:
                hfail.append({"field": "format", "expected": plat["format"],
                              "actual": exe_info["format"]})
            elif exe_info["machine"] != plat["machine"]:
                hfail.append({"field": "machine",
                              "expected": hex(plat["machine"]),
                              "actual": hex(exe_info["machine"])})
            elif plat["format"] == "elf" and \
                    exe_info["elf_class"] != plat["elf_class"]:
                hfail.append({"field": "elf_class",
                              "expected": plat["elf_class"],
                              "actual": exe_info["elf_class"]})
            elif plat["format"] == "pe" and exe_info["pe32plus"]:
                hfail.append({"field": "pe_magic", "expected": "PE32",
                              "actual": "PE32+"})
            if plat["format"] == "elf" and exe_rel in files and \
                    files[exe_rel].mode & 0o111 == 0:
                hfail.append({"field": "mode", "expected": "executable bit",
                              "actual": oct(files[exe_rel].mode)})
            add("executable_header", "mismatch" if hfail else "pass",
                {"format": plat["format"], "machine": hex(plat["machine"])},
                {"format": exe_info["format"],
                 "machine": hex(exe_info["machine"])}, failures=hfail)
        if exe_data is not None:
            ok = embeds_version(exe_data, version)
            add("executable_version", "pass" if ok else "mismatch", version,
                "embedded" if ok else "absent",
                failures=[] if ok else [{
                    "path": exe_rel, "expected": version,
                    "message": "executable does not embed the expected "
                               "version string"}])
        else:
            add("executable_version", "not_run",
                failures=[{"message": "executable unreadable"}])
    else:
        why = "executable missing" if exe_rel not in files else hash_error
        add("executable_header", "not_run", failures=[{"message": why}])
        add("executable_version", "not_run", failures=[{"message": why}])

    # -- runtime closure -----------------------------------------------------------------
    if exe_info is not None and exe_info["format"] == plat["format"]:
        try:
            if plat["format"] == "pe":
                cfail, detail = _closure_windows(files, exe_rel, read,
                                                 plat["machine"])
                lim = ["OS DLL allowlist assumes Windows 10 or later (UCRT "
                       "is part of the OS)",
                       "static import closure only; LoadLibrary-time "
                       "dependencies are exercised by native startup tests"]
            else:
                cfail, detail = _closure_posix(platform, exe_info, files)
                lim = ["system prerequisites are declared, not shipped; their "
                       "own closure is the platform package manager's"]
        except (HeaderError, ArchiveError, struct.error) as exc:
            cfail, detail, lim = [{"message": str(exc)}], {}, []
        add("runtime_closure", "mismatch" if cfail else "pass",
            "all imports packaged or OS/declared", detail, failures=cfail,
            limitations=lim)
    else:
        add("runtime_closure", "not_run",
            failures=[{"message": "no parseable executable for platform"}])

    # -- macOS app -------------------------------------------------------------------------
    if plat["app"]:
        afails = []
        plist_rel = f"{APP_BUNDLE}/Contents/Info.plist"
        bin_rel = f"{APP_BUNDLE}/Contents/MacOS/ds1edit"
        sig_rel = f"{APP_BUNDLE}/Contents/_CodeSignature/CodeResources"
        actual = {}
        if plist_rel in files and not hash_error:
            try:
                pl = plistlib.loads(read(plist_rel))
                actual = {k: pl.get(k) for k in (
                    "CFBundleExecutable", "CFBundleIdentifier",
                    "CFBundleShortVersionString", "CFBundleVersion",
                    "CFBundlePackageType")}
            except Exception as exc:  # plistlib raises several types
                afails.append({"path": plist_rel, "message": f"bad plist: {exc}"})
        else:
            afails.append({"path": plist_rel, "message": "missing Info.plist"})
        want = {"CFBundleExecutable": "ds1edit",
                "CFBundleIdentifier": APP_IDENTIFIER,
                "CFBundleShortVersionString": version,
                "CFBundleVersion": version,
                "CFBundlePackageType": "APPL"}
        if actual:
            for k, v in want.items():
                if actual.get(k) != v:
                    afails.append({"path": plist_rel, "field": k,
                                   "expected": v, "actual": actual.get(k)})
        if bin_rel in files and not hash_error:
            try:
                bi = parse_executable(read(bin_rel))
                if bi["format"] != "macho" or bi["machine"] != plat["machine"]:
                    afails.append({"path": bin_rel, "field": "machine",
                                   "expected": hex(plat["machine"]),
                                   "actual": hex(bi["machine"])})
            except HeaderError as exc:
                afails.append({"path": bin_rel, "message": str(exc)})
            if files[bin_rel].mode & 0o111 == 0:
                afails.append({"path": bin_rel, "field": "mode",
                               "expected": "executable bit"})
            if exe_rel in hashes and hashes.get(prefix + bin_rel) != \
                    hashes.get(prefix + exe_rel):
                # A bundle is the same binary; codesign may rewrite it, so a
                # difference is recorded, not failed.
                actual["bundle_binary_differs_from_cli"] = True
        else:
            afails.append({"path": bin_rel, "message": "missing app binary"})
        if sig_rel not in files:
            afails.append({"path": sig_rel,
                           "message": "bundle lacks its ad-hoc signature"})
        add("macos_app", "mismatch" if afails else "pass", want, actual,
            failures=afails,
            limitations=["signature presence only; codesign --verify must run "
                         "on macOS"])

    # -- extraction --------------------------------------------------------------------------
    blocking = [c["id"] for c in report["cases"]
                if c["status"] != "pass" and c["id"].split(".")[-1] in
                ("archive", "member_names", "member_uniqueness", "member_types")]
    if not safe or blocking:
        add("extraction", "not_run",
            failures=[{"message": "extraction refused: archive failed "
                                  "safety checks", "actual": blocking}])
        return finish()
    try:
        root = _extract(arc, members, out_dir, hashes)
        report["extracted_root"] = str(root)
        add("extraction", "pass", "all members extracted",
            {"files": len(hashes)})
    except (ArchiveError, OSError, ValueError) as exc:
        add("extraction", "unavailable" if isinstance(exc, FileExistsError)
            else "mismatch", failures=[{"message": str(exc)}])
    return finish()


def _extract(arc: _Archive, members: list, out_dir: Path,
             hashes: dict) -> Path:
    """Extract validated members into a fresh directory owned by this run."""
    out_dir.mkdir(parents=True, exist_ok=True)
    out_dir = out_dir.resolve(strict=True)
    root = out_dir / "extracted"
    root.mkdir()                                  # FileExistsError if reused
    root = root.resolve(strict=True)
    if root.parent != out_dir:
        raise ValueError("extraction root resolved outside out_dir")
    budget = [MAX_TOTAL_BYTES]
    for m in members:
        rel = m.raw.rstrip("/")
        dest = root.joinpath(*rel.split("/"))
        parent = dest if m.kind == "dir" else dest.parent
        parent.mkdir(parents=True, exist_ok=True)
        rparent = parent.resolve(strict=True)
        if rparent != root and root not in rparent.parents:
            raise ValueError(f"{m.raw!r} resolves outside the extraction root")
        if m.kind == "dir":
            continue
        h = hashlib.sha256()
        # "xb": never overwrite or follow anything already at the path.
        with open(rparent / dest.name, "xb") as f:
            for chunk in arc.iter_chunks(m, budget):
                h.update(chunk)
                f.write(chunk)
        if hashes.get(m.raw) not in (None, h.hexdigest()):
            raise ArchiveError(f"{m.raw!r} changed between reads")
        if os.name == "posix" and m.mode & 0o111:
            os.chmod(rparent / dest.name, 0o755)
    return root


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="package_check", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--archive", required=True, type=Path)
    ap.add_argument("--platform", required=True, choices=sorted(PLATFORMS))
    ap.add_argument("--version", required=True)
    ap.add_argument("--out-dir", required=True, type=Path)
    try:
        args = ap.parse_args(argv)
    except SystemExit as exc:
        return 2 if exc.code else 0
    try:
        report = inspect_package(args.archive, args.platform, args.version,
                                 args.out_dir)
    except ValueError as exc:
        print(f"package_check: {exc}", file=sys.stderr)
        return 2
    args.out_dir.mkdir(parents=True, exist_ok=True)
    out = args.out_dir / "package-inspection.json"
    out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                   encoding="utf-8")
    for c in report["cases"]:
        line = f"{c['status']:<12} {c['id']}"
        print(line)
        for f in c["failures"][:20]:
            print(f"    - {f.get('path', '')} {f.get('message', '')} "
                  f"{('-> ' + str(f['actual'])) if 'actual' in f else ''}".rstrip())
        if len(c["failures"]) > 20:
            print(f"    ... {len(c['failures']) - 20} more")
    print(f"overall: {report['status']}  archive sha256 "
          f"{report['archive']['sha256']}  report {out}")
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
