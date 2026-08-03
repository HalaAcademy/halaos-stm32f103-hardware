#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
"""Flash ELF HalaOS lên Blue Pill bằng ST-Link và OpenOCD."""
from pathlib import Path
import argparse
import shutil
import subprocess

ap = argparse.ArgumentParser()
ap.add_argument("--elf", type=Path, default=Path("out/build/halaos-bluepill.elf"))
ap.add_argument("--openocd", default="openocd")
ap.add_argument("--interface", default="interface/stlink.cfg")
ap.add_argument("--target", default="target/stm32f1x.cfg")
args = ap.parse_args()

if not args.elf.is_file():
    raise SystemExit(f"Không tìm thấy ELF: {args.elf}")
exe = shutil.which(args.openocd)
if not exe:
    raise SystemExit("Không tìm thấy OpenOCD trong PATH")
cmd = [exe, "-f", args.interface, "-f", args.target,
       "-c", f"program {args.elf.resolve()} verify reset exit"]
print("+", " ".join(str(item) for item in cmd))
raise SystemExit(subprocess.run(cmd).returncode)
