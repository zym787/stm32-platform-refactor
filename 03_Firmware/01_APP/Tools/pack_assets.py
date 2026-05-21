#!/usr/bin/env python3
"""LVGL asset packer.

Builds the binary blob that gets programmed into the W25Q64 LVGL sub-region
via JFlash + the custom .FLM (which remaps JLink address 0x90000000 to W25Q64
byte offset 0x300000 = LVGL partition start).

Output layout follows ``00_Config/inc/cfg_storage.h``:

    offset 0x000000  CFG_LVGL_ASSET_MAGIC          (4 B, little-endian)
    offset 0x000100  fen sprite                    (1050 B)
    offset 0x000600  miao sprite                   (1050 B)
    offset 0x000B00  time sprite                   ( 600 B)
    offset 0x002000  biaopan1 background           (172800 B = 240x240x3)
    ...              0xFF padding to next sector

Sprite + background bytes are extracted from the LVGL-converter
``_<name>_alpha_*.c`` files, picking the ``#if LV_COLOR_DEPTH == ...``
branch that matches lv_conf.h.
"""

import argparse
import re
import sys
from pathlib import Path

# (cfg_macro_stem, c_filename, array_var_name)
ASSETS = [
    ("FEN",      "_fen_alpha_70x5.c",       "_fen_alpha_70x5_map"),
    ("MIAO",     "_miao_alpha_70x5.c",      "_miao_alpha_70x5_map"),
    ("TIME",     "_time_alpha_40x5.c",      "_time_alpha_40x5_map"),
    ("BIAOPAN1", "_biaopan1_alpha_240x240.c", "_biaopan1_alpha_240x240_map"),
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
    ap.add_argument("--config",    required=True, type=Path)
    ap.add_argument("--lv-conf",   required=True, type=Path)
    ap.add_argument("--image-dir", required=True, type=Path)
    ap.add_argument("--output",    required=True, type=Path)
    args = ap.parse_args()

    cfg = parse_macros(args.config)
    try:
        magic       = cfg["CFG_LVGL_ASSET_MAGIC"]
        magic_off   = cfg["CFG_LVGL_ASSET_MAGIC_OFFSET"]
        magic_size  = cfg["CFG_LVGL_ASSET_MAGIC_SIZE"]
        sector_size = cfg["CFG_W25Q64_SECTOR_SIZE"]
        bp1_off     = cfg["CFG_LVGL_ASSET_BIAOPAN1_OFFSET"]
        bp1_w       = cfg["CFG_LVGL_ASSET_BIAOPAN1_W"]
        bp1_h       = cfg["CFG_LVGL_ASSET_BIAOPAN1_H"]
        bp1_px      = cfg["CFG_LVGL_ASSET_BIAOPAN1_PX_SIZE"]
    except KeyError as e:
        sys.exit(f"missing macro {e} in {args.config}")
    if magic_size != 4:
        sys.exit("magic must be 4 bytes")

    # cfg_storage.h defines CFG_LVGL_ASSET_BIAOPAN1_SIZE as W*H*PX_SIZE.  The
    # parser only resolves literal-valued macros, so synthesise it here so
    # the per-asset lookup loop below finds the expected size.
    cfg["CFG_LVGL_ASSET_BIAOPAN1_SIZE"] = bp1_w * bp1_h * bp1_px

    depth, swap = read_lv_color(args.lv_conf)

    # Round footprint up to sector — JFlash erases per sector, predictable size.
    footprint = bp1_off + cfg["CFG_LVGL_ASSET_BIAOPAN1_SIZE"]
    aligned = -(-footprint // sector_size) * sector_size
    buf = bytearray(b"\xff" * aligned)
    buf[magic_off:magic_off + 4] = magic.to_bytes(4, "little")

    print(f"  LV_COLOR_DEPTH={depth}, LV_COLOR_16_SWAP={swap}", file=sys.stderr)
    print(f"  magic 0x{magic:08x} @ 0x{magic_off:06x}  4 B", file=sys.stderr)
    for name, fname, var in ASSETS:
        offset = cfg[f"CFG_LVGL_ASSET_{name}_OFFSET"]
        size   = cfg[f"CFG_LVGL_ASSET_{name}_SIZE"]
        data   = extract_array_bytes(args.image_dir / fname, var, depth, swap)
        if len(data) != size:
            sys.exit(f"{name}: extracted {len(data)} bytes, expected {size}")
        buf[offset:offset + size] = data
        print(f"  {name.lower():<5} @ 0x{offset:06x}  {size:5d} B  ({fname})",
              file=sys.stderr)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(buf)
    print(f"  wrote {len(buf)} B → {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
