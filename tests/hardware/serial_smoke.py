#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
"""Đọc UART Blue Pill và xác nhận các marker boot bắt buộc."""
from pathlib import Path
import argparse
import json
import time

ROOT = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser()
ap.add_argument("--port", required=True, help="Ví dụ: /dev/ttyUSB0 hoặc COM5")
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--timeout", type=float, default=15.0)
ap.add_argument("--markers", type=Path, default=ROOT / "tests/hardware/expected_boot_markers.txt")
ap.add_argument("--log", type=Path, default=ROOT / "out/test/bluepill-uart.log")
args = ap.parse_args()
try:
    import serial
except ImportError as exc:
    raise SystemExit(
        "Thiếu pyserial. Cài bằng: python -m pip install -r requirements-hardware.txt"
    ) from exc
markers = [line.strip() for line in args.markers.read_text(encoding="utf-8").splitlines()
           if line.strip() and not line.lstrip().startswith("#")]
args.log.parent.mkdir(parents=True, exist_ok=True)
start = time.monotonic()
chunks: list[bytes] = []
with serial.Serial(args.port, args.baud, timeout=0.2) as port:
    while time.monotonic() - start < args.timeout:
        data = port.read(port.in_waiting or 1)
        if data:
            chunks.append(data)
            text = b"".join(chunks).decode("utf-8", errors="replace")
            if all(marker in text for marker in markers):
                break
raw = b"".join(chunks)
args.log.write_bytes(raw)
text = raw.decode("utf-8", errors="replace")
missing = [marker for marker in markers if marker not in text]
result = {
    "target": "STM32F103C8 Blue Pill", "port": args.port, "baud": args.baud,
    "bytes": len(raw), "markers": markers, "missing": missing, "pass": not missing,
    "log": str(args.log),
}
print(json.dumps(result, ensure_ascii=False, indent=2))
raise SystemExit(0 if result["pass"] else 1)
