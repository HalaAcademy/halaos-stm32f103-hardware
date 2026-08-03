#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
"""Kiểm tra build HalaOS tái lập ở hai đường dẫn source độc lập."""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COPY_ITEMS = ["tools/build.py", "src", "include", "config", "examples"]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(dst: Path) -> None:
    for rel in COPY_ITEMS:
        source = ROOT / rel
        target = dst / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            shutil.copytree(source, target)
        else:
            shutil.copy2(source, target)


def build(root: Path) -> dict[str, object]:
    out = root / "out/build/halaos.elf"
    out.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    proc = subprocess.run(
        [sys.executable, "tools/build.py", "--profile", "hardware", "--out", str(out)],
        cwd=root,
        text=True,
        capture_output=True,
        env=env,
        timeout=120,
    )
    binary = out.with_suffix(".bin")
    return {
        "returncode": proc.returncode,
        "elf": str(out),
        "bin": str(binary),
        "elfSha256": sha256(out) if out.exists() else None,
        "binSha256": sha256(binary) if binary.exists() else None,
        "stdoutTail": proc.stdout[-2000:],
        "stderrTail": proc.stderr[-2000:],
    }


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="halaos-repro-a-") as a, \
         tempfile.TemporaryDirectory(prefix="halaos-repro-b-") as b:
        root_a, root_b = Path(a), Path(b)
        prepare(root_a)
        prepare(root_b)
        result_a = build(root_a)
        result_b = build(root_b)
        elf_identical = (
            result_a["returncode"] == 0
            and result_b["returncode"] == 0
            and result_a["elfSha256"] == result_b["elfSha256"]
        )
        bin_identical = (
            result_a["returncode"] == 0
            and result_b["returncode"] == 0
            and result_a["binSha256"] == result_b["binSha256"]
        )
        payload = {
            "ok": bool(elf_identical and bin_identical),
            "elfIdentical": elf_identical,
            "binIdentical": bin_identical,
            "buildA": result_a,
            "buildB": result_b,
        }
        print(json.dumps(payload, ensure_ascii=False, indent=2))
        return 0 if payload["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
