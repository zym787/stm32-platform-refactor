#!/usr/bin/env python3
"""OTA image encryption tool — produces .mxxx files for the bootloader's
exA_to_exB_AES decryption path (00_Bootloader/Tasks/Bootmanager/src/
bootmanager.c).

Format the bootloader expects:

    plain = [ 12 B reserved (zeros) | 4 B app_size LE | actual .bin | 0xFF pad ]
    cipher = AES-256-CBC(plain, key = s_key_256, iv = s_iv_default)

Notes
-----
* key/iv are the hardcoded constants from bootmanager.c (32×{0x31,0x32}
  alternation for the key, 16 bytes of same for the iv).  These match the
  upstream reference design verbatim — if you ever rotate them, update
  this script at the same time.
* `actual .bin` is the unmodified output of `arm-none-eabi-objcopy -O
  binary` (i.e. `build/helloworld.bin`).  Do NOT use the .hex or .elf.
* Padding goes to a 16-byte boundary; the size field tells the bootloader
  exactly how many bytes of decrypted output to keep, so the pad is
  silently dropped downstream.

Usage
-----
    cd 01_APP/Tools
    uv run ota_encrypt.py ../build/helloworld.bin ../build/helloworld.mxxx

(Or from the project Makefile: `make ota-image`.)

`pycryptodome` is declared in Tools/pyproject.toml so uv auto-installs it
into the project's managed virtualenv on first invocation — no manual
`pip install` needed.
"""

import struct
import sys

from Crypto.Cipher import AES

# Must match bootmanager.c s_key_256 / s_iv_default exactly.
KEY = bytes([0x31, 0x32] * 16)   # 32 B
IV  = bytes([0x31, 0x32] * 8)    # 16 B
HEADER_RESERVED_LEN = 12
BLOCK_SIZE = 16


def encrypt(in_path: str, out_path: str) -> None:
    with open(in_path, "rb") as f:
        payload = f.read()

    app_size = len(payload)
    if app_size == 0:
        raise ValueError(f"{in_path} is empty")

    # First 16 B: 12 zero bytes + 4 B LE app_size.  Bootloader's
    # exA_to_exB_AES reads bytes 12..15 of the first decrypted block.
    header = b"\x00" * HEADER_RESERVED_LEN + struct.pack("<I", app_size)

    # Pad raw payload to 16-byte boundary with 0xFF so AES-CBC can encrypt
    # cleanly.  Bootloader trims to app_size on the way out so the pad is
    # invisible to the running APP.
    pad_len = (BLOCK_SIZE - (app_size % BLOCK_SIZE)) % BLOCK_SIZE
    padded_payload = payload + b"\xFF" * pad_len

    plain = header + padded_payload
    assert len(plain) % BLOCK_SIZE == 0

    cipher = AES.new(KEY, AES.MODE_CBC, IV)
    encrypted = cipher.encrypt(plain)

    with open(out_path, "wb") as f:
        f.write(encrypted)

    print(f"in  : {in_path}  ({app_size} B)")
    print(f"out : {out_path}  ({len(encrypted)} B = 16 B hdr + {len(padded_payload)} B padded payload)")
    print(f"pad : {pad_len} B of 0xFF appended before encryption")


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)
    encrypt(sys.argv[1], sys.argv[2])


if __name__ == "__main__":
    main()
