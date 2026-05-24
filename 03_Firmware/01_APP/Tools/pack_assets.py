#!/usr/bin/env python3
"""LVGL asset packer.

Builds the binary blob that gets programmed into the W25Q64 LVGL sub-region
via JFlash + the custom .FLM (which remaps JLink address 0x90000000 to W25Q64
byte offset 0x300000 = LVGL partition start).

Output layout follows ``00_Config/inc/cfg_storage.h``:

    offset 0x000000  CFG_LVGL_ASSET_MAGIC          (4 B, little-endian)
    offset 0x000100  fen sprite                    (1920 B = 80x8x3)
    offset 0x000900  miao sprite                   (1050 B = 70x5x3)
    offset 0x000E00  time sprite                   (1200 B = 50x8x3)
    offset 0x002000  MDLBG background              (201600 B = 240x280x3)
    offset 0x034000  biaopan1 background           (120000 B = 200x200x3)
    ...              0xFF padding to next sector

Sprite + background bytes are extracted from the LVGL-converter
``_<name>_alpha_*.c`` files (and ``_biaopan1_200x200.c`` for the dial),
picking the ``#if LV_COLOR_DEPTH == ...`` branch that matches lv_conf.h.

The two large backgrounds (MDLBG / biaopan1) are NOT shipped in firmware
.rodata; the .c files live only in ``project/test_01/generated/images/``
which this script reads as the source of truth.  Three sprite .c files
live alongside this script's `--image-dir` (the regular lvgl_ui/images/).
"""

import argparse
import re
import sys
from pathlib import Path

# (cfg_macro_stem, c_filename, array_var_name, source_dir_kind)
#   source_dir_kind: "image-dir" → uses --image-dir
#                    "ext-dir"   → uses --ext-image-dir (the SquareLine project)
ASSETS = [
    ("FEN",         "_fen_alpha_80x8.c",         "_fen_alpha_80x8_map",         "image-dir"),
    ("MIAO",        "_miao_alpha_70x5.c",        "_miao_alpha_70x5_map",        "image-dir"),
    ("TIME",        "_time_alpha_50x8.c",        "_time_alpha_50x8_map",        "image-dir"),
    ("MDLBG",       "_MDLBG_alpha_240x280.c",    "_MDLBG_alpha_240x280_map",    "ext-dir"),
    ("BIAOPAN1",    "_biaopan1_200x200.c",       "_biaopan1_200x200_map",       "ext-dir"),
    ("WATCHDIGHT1", "_watchdight1_alpha_60x60.c","_watchdight1_alpha_60x60_map","image-dir"),
    ("WATCHDIGHT2", "_watchdight2_alpha_60x60.c","_watchdight2_alpha_60x60_map","image-dir"),
    ("WATCHDIGHT3", "_watchdight3_alpha_60x60.c","_watchdight3_alpha_60x60_map","image-dir"),
]

_CFG_RE = re.compile(
    r"#define\s+(CFG_(?:LVGL_ASSET|W25Q64)_[A-Z_0-9]+)\s+\(?\s*"
    r"(0[xX][0-9a-fA-F]+|\d+)U?L?\s*\)?"
)
_HEX_BYTE_RE = re.compile(r"\b0x([0-9a-fA-F]+)\b")


def parse_macros(header: Path) -> dict[str, int]:
    """Extract `#define CFG_..._X  (literal)` pairs from a header."""
    text = header.read_text(encoding="utf-8")
    return {m.group(1): int(m.group(2), 0) for m in _CFG_RE.finditer(text)}


def read_lv_color(conf: Path) -> tuple[int, int]:
    text = conf.read_text(encoding="utf-8")
    depth_m = re.search(r"^\s*#define\s+LV_COLOR_DEPTH\s+(\d+)", text, re.MULTILINE)
    swap_m = re.search(r"^\s*#define\s+LV_COLOR_16_SWAP\s+(\d+)", text, re.MULTILINE)
    if depth_m is None:
        sys.exit(f"LV_COLOR_DEPTH not found in {conf}")
    return int(depth_m.group(1)), (int(swap_m.group(1)) if swap_m else 0)


def _block_active(condition: str, depth: int, swap: int) -> bool:
    """Evaluate `LV_COLOR_DEPTH == X [&& ...]` style C-preprocessor guards."""
    expr = (condition.replace("LV_COLOR_DEPTH", str(depth))
                     .replace("LV_COLOR_16_SWAP", str(swap))
                     .replace("&&", " and ")
                     .replace("||", " or "))
    try:
        return bool(eval(expr, {"__builtins__": {}}))
    except Exception:
        return False


def extract_array_bytes(c_file: Path, var: str, depth: int, swap: int) -> bytes:
    src = c_file.read_text(encoding="utf-8")
    decl = re.search(rf"\b{re.escape(var)}\s*\[\s*\]\s*=\s*\{{", src)
    if decl is None:
        sys.exit(f"array `{var}` not found in {c_file}")
    body_end = src.find("};", decl.end())
    if body_end < 0:
        sys.exit(f"unterminated array `{var}` in {c_file}")
    body = src[decl.end():body_end]

    out: list[int] = []
    active = True  # outside any #if/#endif: include
    for line in body.splitlines():
        s = line.strip()
        if s.startswith("#if "):
            active = _block_active(s[4:], depth, swap)
        elif s.startswith("#endif"):
            active = True
        elif active:
            for m in _HEX_BYTE_RE.finditer(s):
                v = int(m.group(1), 16)
                if v > 0xFF:
                    sys.exit(f"non-byte literal 0x{v:x} in {c_file}")
                out.append(v)
    return bytes(out)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--config",        required=True, type=Path)
    ap.add_argument("--lv-conf",       required=True, type=Path)
    ap.add_argument("--image-dir",     required=True, type=Path,
                    help="lvgl_ui/images dir (sprite .c files compiled into firmware)")
    ap.add_argument("--ext-image-dir", required=True, type=Path,
                    help="project/test_01/generated/images dir (large bg .c files)")
    ap.add_argument("--output",        required=True, type=Path)
    args = ap.parse_args()

    cfg = parse_macros(args.config)
    try:
        magic       = cfg["CFG_LVGL_ASSET_MAGIC"]
        magic_off   = cfg["CFG_LVGL_ASSET_MAGIC_OFFSET"]
        magic_size  = cfg["CFG_LVGL_ASSET_MAGIC_SIZE"]
        sector_size = cfg["CFG_W25Q64_SECTOR_SIZE"]
    except KeyError as e:
        sys.exit(f"missing macro {e} in {args.config}")
    if magic_size != 4:
        sys.exit("magic must be 4 bytes")

    # cfg_storage.h defines per-asset SIZE as W*H*PX_SIZE.  The macro parser
    # only resolves literal-valued macros, so synthesise composite SIZEs here.
    for name, *_ in ASSETS:
        w = cfg.get(f"CFG_LVGL_ASSET_{name}_W")
        h = cfg.get(f"CFG_LVGL_ASSET_{name}_H")
        px = cfg.get(f"CFG_LVGL_ASSET_{name}_PX_SIZE")
        if w is not None and h is not None and px is not None:
            cfg[f"CFG_LVGL_ASSET_{name}_SIZE"] = w * h * px

    depth, swap = read_lv_color(args.lv_conf)

    # Round footprint up to sector — JFlash erases per sector, predictable size.
    last_off = max(cfg[f"CFG_LVGL_ASSET_{name}_OFFSET"] for name, *_ in ASSETS)
    last_name = next(name for name, *_ in ASSETS
                     if cfg[f"CFG_LVGL_ASSET_{name}_OFFSET"] == last_off)
    footprint = last_off + cfg[f"CFG_LVGL_ASSET_{last_name}_SIZE"]
    aligned = -(-footprint // sector_size) * sector_size
    buf = bytearray(b"\xff" * aligned)
    buf[magic_off:magic_off + 4] = magic.to_bytes(4, "little")

    print(f"  LV_COLOR_DEPTH={depth}, LV_COLOR_16_SWAP={swap}", file=sys.stderr)
    print(f"  magic 0x{magic:08x} @ 0x{magic_off:06x}  4 B", file=sys.stderr)
    for name, fname, var, src_kind in ASSETS:
        offset = cfg[f"CFG_LVGL_ASSET_{name}_OFFSET"]
        size   = cfg[f"CFG_LVGL_ASSET_{name}_SIZE"]
        src_dir = args.image_dir if src_kind == "image-dir" else args.ext_image_dir
        data   = extract_array_bytes(src_dir / fname, var, depth, swap)
        if len(data) != size:
            sys.exit(f"{name}: extracted {len(data)} bytes, expected {size}")
        buf[offset:offset + size] = data
        print(f"  {name.lower():<8} @ 0x{offset:06x}  {size:6d} B  ({fname})",
              file=sys.stderr)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(buf)
    print(f"  wrote {len(buf)} B → {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
