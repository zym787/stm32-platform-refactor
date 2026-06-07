#!/usr/bin/env python3
"""Cross-platform JFlash driver for `make download` / `make flash-assets`.

This replaces the shell-only recipes that used to live in the Makefile
(`if [ ! -f ... ]; then ...`, `wslpath -w`, hardcoded `/mnt/c/...` JFlash
path).  Those only worked under WSL; on native Windows GnuWin32 make runs
recipes through cmd.exe and the sh syntax blew up (`! was unexpected`).

Python is genuinely OS-agnostic, so all the fragile bits — locating
JFlash.exe, converting paths into the form the (always-Windows) JFlash
binary expects, and the "rebuild first if the hex is missing" logic —
live here instead.  The Makefile just calls `uv run flash.py ...`, exactly
like it already does for mem_report / ota_encrypt / pack_assets.

Supported hosts
---------------
* Native Windows   — uv's Windows Python; paths are already Windows form,
                     JFlash.exe found under `C:\\Program Files\\SEGGER\\JLink_*`.
* WSL  (primary)   — Linux Python; `/mnt/c/...` JFlash.exe, paths converted
                     to Windows form via `wslpath -w` for the JFlash process.
* Native Linux     — falls back to SEGGER's Linux `JFlashExe`/`JFlash` on
                     PATH; no path conversion needed.

In every case `JFLASH_EXE` (env var) or `--jflash-exe` overrides discovery.

Subcommands
-----------
    flash.py download --build-dir DIR --target NAME --jflash-prj PRJ \
                      --make MAKE --app-dir DIR [--jobs N]
        Flash build/<target>.hex into the internal-flash APP slot.  The .hex
        carries its own load address, so no ,0xADDR is appended.  If the hex
        is missing, runs a PARALLEL rebuild (`make clean && make -jN`) first.

    flash.py assets --bin FILE --addr 0xADDR --jflash-prj PRJ
        Flash a raw .bin to the given JLink address (W25Q64 LVGL partition,
        remapped to 0x300000 by the custom .FLM).

All path arguments may be relative to the current working directory (the
Makefile invokes this from Tools/ with ../ prefixes); they are resolved to
absolute paths before any host-specific conversion.

Usage examples
--------------
    cd 01_APP/Tools
    uv run flash.py download --build-dir ../build --target helloworld \\
        --jflash-prj ../Tools/segger/jflash/stm32f411ce.jflash \\
        --make make --app-dir ..
    uv run flash.py --dry-run assets --bin ../build/assets.bin \\
        --addr 0x90000000 --jflash-prj ../Tools/segger/jflash/411_w25q64.jflash
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys


# --------------------------------------------------------------------------
# host detection
# --------------------------------------------------------------------------
def is_windows() -> bool:
    return os.name == "nt"


def is_wsl() -> bool:
    """True when running under the Linux-on-Windows subsystem.

    JFlash on WSL is still the *Windows* JFlash.exe under /mnt/c, reached via
    WSL's binfmt interop — so paths handed to it must be Windows form.
    """
    if is_windows():
        return False
    try:
        with open("/proc/version", "r", encoding="utf-8", errors="ignore") as fh:
            return "microsoft" in fh.read().lower()
    except OSError:
        return False


def runs_windows_jflash() -> bool:
    """True when the JFlash binary we drive is a Windows .exe (native Win or WSL)."""
    return is_windows() or is_wsl()


# --------------------------------------------------------------------------
# JFlash binary discovery
# --------------------------------------------------------------------------
# Candidate roots searched for SEGGER's JFlash, newest version first.  These
# are only defaults — JFLASH_EXE / --jflash-exe always win.
_WIN_GLOBS = [
    r"C:\Program Files\SEGGER\JLink*\JFlash.exe",
    r"C:\Program Files (x86)\SEGGER\JLink*\JFlash.exe",
]
_WSL_GLOBS = [
    "/mnt/c/Program Files/SEGGER/JLink*/JFlash.exe",
    "/mnt/c/Program Files (x86)/SEGGER/JLink*/JFlash.exe",
]


def _newest(matches):
    """Pick the highest JLink version dir (lexical sort is good enough for
    JLink_VNNNx names; mtime breaks ties / odd naming)."""
    if not matches:
        return None
    return sorted(matches)[-1]


def find_jflash(override: str) -> str:
    if override:
        return override
    env = os.environ.get("JFLASH_EXE", "").strip()
    if env:
        return env

    if is_windows():
        for pat in _WIN_GLOBS:
            hit = _newest(glob.glob(pat))
            if hit:
                return hit
    elif is_wsl():
        for pat in _WSL_GLOBS:
            hit = _newest(glob.glob(pat))
            if hit:
                return hit
    else:  # native Linux — SEGGER ships JFlashExe / JFlash on PATH
        for name in ("JFlashExe", "JFlash"):
            hit = shutil.which(name)
            if hit:
                return hit

    sys.exit(
        "flash.py: could not locate JFlash. Set JFLASH_EXE or pass "
        "--jflash-exe.\n"
        "  Windows: C:\\Program Files\\SEGGER\\JLink_Vxxx\\JFlash.exe\n"
        "  WSL:     /mnt/c/Program Files/SEGGER/JLink_Vxxx/JFlash.exe\n"
        "  Linux:   install J-Link, ensure JFlashExe is on PATH"
    )


# --------------------------------------------------------------------------
# path conversion — JFlash always wants paths in its own native form
# --------------------------------------------------------------------------
def to_jflash_path(path: str) -> str:
    """Resolve `path` to absolute, then convert to the form the JFlash binary
    expects: Windows form on native Windows / WSL, POSIX form on Linux."""
    absolute = os.path.abspath(path)

    if is_windows():
        # uv's Windows Python already produces backslash Windows paths.
        return absolute
    if is_wsl():
        try:
            win = subprocess.check_output(
                ["wslpath", "-w", absolute], text=True
            ).strip()
            return win if win else absolute
        except (OSError, subprocess.CalledProcessError):
            return absolute
    # native Linux JFlash takes the POSIX path as-is
    return absolute


# --------------------------------------------------------------------------
# JFlash invocation
# --------------------------------------------------------------------------
def run_jflash(jflash_exe, prj, open_arg, dry_run: bool) -> int:
    """Drive JFlash headless: open project, open image, auto (connect→erase→
    program→verify), then exit / close the window — no clicking required."""
    cmd = [
        jflash_exe,
        "-openprj" + to_jflash_path(prj),
        "-open" + open_arg,
        "-auto",
        "-exit",
    ]
    print("  JFlash: " + " ".join(cmd))
    if dry_run:
        return 0
    return subprocess.call(cmd)


# --------------------------------------------------------------------------
# subcommands
# --------------------------------------------------------------------------
def cmd_download(args) -> int:
    hex_path = os.path.join(args.build_dir, args.target + ".hex")

    if not os.path.isfile(hex_path):
        # No prereq on the make target on purpose — a prereq would force a
        # single-threaded build.  Rebuild in parallel here instead, mirroring
        # the VSCode "build" task (clean + make -jN → all, incl. mem-report).
        print(
            "  >> no firmware (%s) — parallel rebuild first" % hex_path
        )
        make = args.make or "make"
        app_dir = args.app_dir or "."
        if not args.dry_run:
            rc = subprocess.call([make, "clean"], cwd=app_dir)
            if rc == 0:
                rc = subprocess.call([make, "-j%d" % args.jobs], cwd=app_dir)
            if rc != 0:
                return rc
        else:
            print("  >> (dry-run) %s clean && %s -j%d  (cwd=%s)"
                  % (make, make, args.jobs, app_dir))

    jflash = find_jflash(args.jflash_exe)
    # .hex carries its own load address → no ,0xADDR suffix.
    open_arg = to_jflash_path(hex_path)
    print("  JFlash: program %s into APP slot" % open_arg)
    return run_jflash(jflash, args.jflash_prj, open_arg, args.dry_run)


def cmd_assets(args) -> int:
    if not os.path.isfile(args.bin) and not args.dry_run:
        sys.exit("flash.py: assets image not found: %s" % args.bin)
    jflash = find_jflash(args.jflash_exe)
    # raw .bin → must specify the JLink load address (remapped by the .FLM).
    open_arg = to_jflash_path(args.bin) + "," + args.addr
    print("  JFlash: program %s @ %s" % (to_jflash_path(args.bin), args.addr))
    return run_jflash(jflash, args.jflash_prj, open_arg, args.dry_run)


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------
def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--dry-run", action="store_true",
                   help="print what would run without invoking JFlash/make")
    p.add_argument("--jflash-exe", default="",
                   help="override JFlash binary (else $JFLASH_EXE or auto-detect)")
    sub = p.add_subparsers(dest="subcmd", required=True)

    d = sub.add_parser("download", help="flash <target>.hex into the APP slot")
    d.add_argument("--build-dir", required=True)
    d.add_argument("--target", required=True)
    d.add_argument("--jflash-prj", required=True)
    d.add_argument("--make", default="make",
                   help="make executable for the missing-firmware rebuild")
    d.add_argument("--app-dir", default=".",
                   help="dir to run the rebuild make from (APP project root)")
    d.add_argument("--jobs", type=int, default=16)
    d.set_defaults(func=cmd_download)

    a = sub.add_parser("assets", help="flash a raw .bin to a JLink address")
    a.add_argument("--bin", required=True)
    a.add_argument("--addr", default="0x90000000")
    a.add_argument("--jflash-prj", required=True)
    a.set_defaults(func=cmd_assets)

    args = p.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
