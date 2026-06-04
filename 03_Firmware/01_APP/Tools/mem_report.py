#!/usr/bin/env python3
"""Memory report tool — Linux equivalent of mem_report.ps1.

Parses linker-script MEMORY regions, attributes ELF sections to regions by VMA,
then prints a three-part report: Memory Usage Summary, Per-File object table,
and Link Region View with bar graphs and delta tracking against the previous run.
"""

import sys
import os
import re
import json
import subprocess
import glob
from datetime import datetime


def parse_uint64(value: str) -> int:
    raw = value.strip()
    multiplier = 1
    if raw.upper().endswith('K'):
        multiplier = 1024
        raw = raw[:-1]
    elif raw.upper().endswith('M'):
        multiplier = 1024 * 1024
        raw = raw[:-1]

    if raw.lower().startswith('0x'):
        base = int(raw[2:], 16)
    else:
        base = int(raw)

    return base * multiplier


def make_bar(percent: float, length: int, fill: str, empty: str) -> str:
    fill_count = round((percent / 100.0) * length)
    fill_count = max(0, min(fill_count, length))
    return fill * fill_count + empty * (length - fill_count)


def format_delta(current_pct: float, current_used: int,
                 previous: dict | None) -> str:
    if previous is None or previous.get('pct') is None or previous.get('used') is None:
        return " (first run)"

    delta_pct = current_pct - float(previous['pct'])
    delta_bytes = current_used - int(previous['used'])

    if abs(delta_pct) < 0.05 and delta_bytes == 0:
        return " (no change)"

    if delta_pct > 0.0 or (delta_pct == 0.0 and delta_bytes > 0):
        return f" (+{abs(delta_pct):.1f}%, +{abs(delta_bytes)} bytes)"

    return f" (-{abs(delta_pct):.1f}%, -{abs(delta_bytes)} bytes)"


def main():
    elf_path = None
    ld_path = None
    size_tool = 'arm-none-eabi-size'
    fail_above = None  # fraction in [0, 1]; if any region exceeds, exit 2

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] in ('--elf', '-e') and i + 1 < len(args):
            elf_path = args[i + 1]; i += 2
        elif args[i] in ('--ld', '-l') and i + 1 < len(args):
            ld_path = args[i + 1]; i += 2
        elif args[i] in ('--size-tool', '-s') and i + 1 < len(args):
            size_tool = args[i + 1]; i += 2
        elif args[i] == '--fail-above' and i + 1 < len(args):
            fail_above = float(args[i + 1]); i += 2
        else:
            i += 1

    if not elf_path or not ld_path:
        print(f"Usage: {sys.argv[0]} --elf <elf> --ld <ldscript> "
              f"[--size-tool <size>] [--fail-above <frac>]", file=sys.stderr)
        sys.exit(1)

    if not os.path.exists(elf_path):
        print(f"ELF file not found: {elf_path}", file=sys.stderr)
        sys.exit(1)
    if not os.path.exists(ld_path):
        print(f"Linker script not found: {ld_path}", file=sys.stderr)
        sys.exit(1)

    # -------------------------------------------------------------------
    # 1. Parse all MEMORY regions from the linker script
    # -------------------------------------------------------------------
    with open(ld_path, 'r') as f:
        ld_text = f.read()

    region_re = re.compile(
        r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*:'
        r'\s*ORIGIN\s*=\s*([^,]+),\s*LENGTH\s*=\s*([^\s/]+)',
        re.MULTILINE
    )

    memories = []
    for m in region_re.finditer(ld_text):
        memories.append({
            'name':   m.group(1),
            'attr':   m.group(2),
            'origin': parse_uint64(m.group(3)),
            'length': parse_uint64(m.group(4)),
            'used':   0,
        })

    if not memories:
        print(f"No MEMORY regions parsed from linker script: {ld_path}",
              file=sys.stderr)
        sys.exit(1)

    # -------------------------------------------------------------------
    # 2. Attribute each ELF section to its memory region by VMA
    # -------------------------------------------------------------------
    result = subprocess.run([size_tool, '-A', elf_path],
                            capture_output=True, text=True)
    size_a_lines = result.stdout.splitlines()
    if result.stderr:
        size_a_lines.extend(result.stderr.splitlines())

    for line in size_a_lines:
        m = re.match(r'^\s*(\.\S+)\s+(\d+)\s+(\d+)\s*$', line)
        if not m:
            continue
        sec_size = int(m.group(2))
        sec_addr = int(m.group(3))
        if sec_size <= 0:
            continue

        for mem in memories:
            if mem['origin'] <= sec_addr < (mem['origin'] + mem['length']):
                mem['used'] += sec_size
                break

    # -------------------------------------------------------------------
    # 3. Memory Usage Summary
    # -------------------------------------------------------------------
    result = subprocess.run([size_tool, elf_path],
                            capture_output=True, text=True)
    size_lines = result.stdout.strip().splitlines()
    summary_line = size_lines[-1] if size_lines else ''
    summary_parts = summary_line.split()

    if len(summary_parts) < 3:
        print(f"Unexpected size output: {summary_line}", file=sys.stderr)
        sys.exit(1)

    text_size  = int(summary_parts[0])
    data_size  = int(summary_parts[1])
    flash_used = text_size + data_size

    # FLASH → first rx region without w
    load_memory = None
    for mem in memories:
        if 'r' in mem['attr'] and 'x' in mem['attr'] and 'w' not in mem['attr']:
            load_memory = mem
            break
    if not load_memory:
        for mem in memories:
            if 'r' in mem['attr'] and 'x' in mem['attr']:
                load_memory = mem
                break
    if not load_memory:
        load_memory = memories[0]

    # RAM → first w region
    rw_memory = None
    for mem in memories:
        if 'w' in mem['attr']:
            rw_memory = mem
            break
    if not rw_memory:
        rw_memory = memories[0]

    flash_total = load_memory['length']
    ram_total   = rw_memory['length']
    ram_used    = rw_memory['used']

    flash_pct = round((flash_used * 100.0) / flash_total, 1) if flash_total > 0 else 0.0
    ram_pct   = round((ram_used * 100.0) / ram_total,   1) if ram_total > 0 else 0.0

    print("=== Memory Usage Summary ===")
    print(f"RAM used  : {ram_used} / {ram_total} bytes ({ram_pct} %)")
    print(f"FLASH used: {flash_used} / {flash_total} bytes ({flash_pct} %)")

    # -------------------------------------------------------------------
    # 4. Per-file object table
    # -------------------------------------------------------------------
    elf_abs   = os.path.abspath(elf_path)
    build_dir = os.path.dirname(elf_abs)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    target_name = os.path.splitext(os.path.basename(elf_path))[0]

    state_file = os.path.join(script_dir,
                              f".mem_report_last_{target_name}.json")

    previous_state = None
    if os.path.exists(state_file):
        try:
            with open(state_file, 'r') as f:
                previous_state = json.load(f)
        except (json.JSONDecodeError, IOError):
            pass

    print()
    print("=== Per-File RAM/FLASH (object-based) ===")

    obj_files = sorted(
        glob.glob(os.path.join(build_dir, '*.o')),
        key=lambda x: os.path.basename(x)
    )
    rows = []

    for obj in obj_files:
        result = subprocess.run([size_tool, obj],
                                capture_output=True, text=True)
        lines = result.stdout.strip().splitlines()
        if not lines:
            continue
        parts = lines[-1].split()
        if len(parts) < 6:
            continue
        try:
            rows.append({
                'file':  os.path.basename(obj),
                'ram':   int(parts[1]) + int(parts[2]),   # data + bss
                'flash': int(parts[0]) + int(parts[1]),   # text + data
            })
        except (ValueError, IndexError):
            continue

    if rows:
        file_width = max(36, max(len(r['file']) for r in rows) + 2)
        sep = '-' * (file_width + 38)
        print(sep)
        print(f"{'FILE(s)'.ljust(file_width)} | {'RAM (byte)':>15} | "
              f"{'FLASH (byte)':>15}")
        print(sep)
        for r in rows:
            print(f"{r['file'].ljust(file_width)} | {r['ram']:>15} | "
                  f"{r['flash']:>15}")
        print(sep)

    # -------------------------------------------------------------------
    # 5. Link Region View — one entry per LD MEMORY region
    # -------------------------------------------------------------------
    bar_len = 48
    current_state = {
        'timestamp': datetime.now().strftime('%Y-%m-%dT%H:%M:%S'),
        'regions': {},
    }

    print()
    print("=== Link Region View (From LD) ===")

    for mem in memories:
        used_bytes  = mem['used']
        total_bytes = mem['length']
        if total_bytes <= 0:
            total_bytes = 1

        pct      = round((used_bytes * 100.0) / total_bytes, 1)
        used_kb  = used_bytes  / 1024.0
        total_kb = total_bytes / 1024.0

        fill_char      = 'O' if 'w' in mem['attr'] else '#'
        bar            = make_bar(pct, bar_len, fill_char, '_')
        section_prefix = 'RW' if 'w' in mem['attr'] else 'ER'
        section_label  = f"{section_prefix}_{mem['name']}"

        prev_snap = None
        if previous_state and 'regions' in previous_state:
            prev_snap = previous_state['regions'].get(section_label)

        delta = format_delta(pct, used_bytes, prev_snap)

        print()
        print(f"        {mem['name']:<18}[0x{mem['origin']:08X} | "
              f"0x{mem['length']:08X} ({mem['length']})]")
        print(f"                {section_label:<10}[0x{mem['origin']:08X}]"
              f"|{bar}| ( {used_kb:6.1f} KB / {total_kb:6.1f} KB )  "
              f"{pct:5.1f}%{delta}")

        current_state['regions'][section_label] = {
            'used':  used_bytes,
            'total': total_bytes,
            'pct':   pct,
        }

    print()
    # Flush before returning so the full report reaches the terminal ahead of
    # the process exit — VSCode's task banner otherwise races a buffered tail.
    sys.stdout.flush()

    try:
        with open(state_file, 'w') as f:
            json.dump(current_state, f, indent=2)
    except IOError:
        pass

    # --fail-above checks the load region (FLASH) only. Other regions are
    # excluded by design: RAM/BSS overflow is already caught by the linker, and
    # RTT-buffer regions are pre-sized to 100% by SEGGER tooling so checking
    # them would false-positive on every build.
    if fail_above is not None and load_memory is not None and load_memory['length'] > 0:
        frac = load_memory['used'] / load_memory['length']
        if frac > fail_above:
            print(
                f"ERROR: {load_memory['name']} usage {load_memory['used']}/"
                f"{load_memory['length']} bytes = {frac * 100:.1f}% "
                f"exceeded --fail-above={fail_above * 100:.1f}%",
                file=sys.stderr,
            )
            sys.exit(2)


if __name__ == '__main__':
    main()
