#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
"""Kiểm tra Flash/RAM của firmware STM32F103C8 Blue Pill."""
from pathlib import Path
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
elf = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "out/build/halaos-bluepill.elf"
(ROOT / "out/build").mkdir(parents=True, exist_ok=True)
if not elf.is_file():
    raise SystemExit(f"missing ELF: {elf}")
import shutil
size_tool = shutil.which("llvm-size") or shutil.which("size") or "size"
proc = subprocess.run([size_tool, str(elf)], text=True, capture_output=True, check=True)
match = re.search(r"\n\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)", proc.stdout)
if not match:
    raise SystemExit("cannot parse size")
text, data, bss, dec = map(int, match.groups())
flash = text + data
# GNU/LLVM size gộp các section NOLOAD như task stacks, crash log và MSP vào BSS.
# Vì vậy data+bss đã là tổng RAM tĩnh thực tế của image, không được trừ/cộng MSP lần hai.
ram = data + bss
physical_ram = 20 * 1024
free_ram = physical_ram - ram
minimum_free_ram = 512
report = {
    "target": "STM32F103C8 Blue Pill",
    "elf": str(elf),
    "text": text,
    "data": data,
    "bssIncludingNoLoad": bss,
    "firmwareFlashBytes": flash,
    "firmwareFlashBudget": 57344,
    "flashPass": flash <= 57344,
    "totalStaticRamBytes": ram,
    "physicalRamBytes": physical_ram,
    "freeRamBytes": free_ram,
    "minimumFreeRamBytes": minimum_free_ram,
    "ramPass": ram <= physical_ram and free_ram >= minimum_free_ram,
    "mspReserveBytes": 2048,
    "appStoreBytes": 8192,
    "appStoreBase": "0x0800E000",
    "note": "BSS from size includes task stacks, noinit and MSP NOLOAD sections.",
}
report["pass"] = report["flashPass"] and report["ramPass"]
(ROOT / "out/build/resource-report.json").write_text(
    json.dumps(report, indent=2) + "\n", encoding="utf-8"
)
print(json.dumps(report, indent=2))
raise SystemExit(0 if report["pass"] else 1)
