#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
"""Kiểm tra cấu trúc source Blue Pill trước khi phát hành."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
required = [
    "README.md", "LICENSE", "NOTICE", "VERSION", "CMakeLists.txt", "Makefile",
    "boards/bluepill/board.dts", "config/memory/stm32f103c8.ld",
    "config/source_files.json", "tools/build.py", "tools/dtsgen.py",
    "tools/hardware/flash_openocd.py", "tests/hardware/serial_smoke.py",
]
missing = [path for path in required if not (ROOT / path).is_file()]
source_manifest = json.loads((ROOT / "config/source_files.json").read_text(encoding="utf-8"))
missing_sources = [path for path in source_manifest["sources"] if not (ROOT / path).is_file()]
forbidden_paths = [
    "integration/media", "legacy", "build", "PACKAGE_MANIFEST.json", "PACKAGE_SCOPE.md",
]
found_forbidden = [path for path in forbidden_paths if (ROOT / path).exists()]
report = {
    "project": "HalaOS",
    "target": "STM32F103C8 Blue Pill",
    "missing": missing,
    "missingSources": missing_sources,
    "forbiddenPaths": found_forbidden,
    "sourceCount": len(source_manifest["sources"]),
}
report["pass"] = not missing and not missing_sources and not found_forbidden
print(json.dumps(report, ensure_ascii=False, indent=2))
raise SystemExit(0 if report["pass"] else 1)
