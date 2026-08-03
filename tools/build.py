#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
"""Build HalaOS cho STM32F103C8 Blue Pill thật."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]
SOURCE_MANIFEST = ROOT / "config/source_files.json"
SOURCES = [ROOT / item for item in json.loads(SOURCE_MANIFEST.read_text(encoding="utf-8"))["sources"]]

PROFILES = [
    "hardware",
    "qualification",
    "boot-lab",
    "scheduler-lab",
    "process-ipc-lab",
    "vfs-posix-lab",
    "compiler-vm-lab",
    "hardware-constrained",
]

ap = argparse.ArgumentParser(description="Build HalaOS for STM32F103C8 Blue Pill")
ap.add_argument("--profile", choices=PROFILES, default="hardware")
ap.add_argument("--bad-manifest", action="store_true", help="Alias của --boot-fault bad-header-crc")
ap.add_argument("--bad-dtb", action="store_true")
ap.add_argument("--boot-fault", choices=[
    "bad-magic", "bad-version", "bad-header-crc", "bad-loader-crc",
    "bad-dtb-crc", "bad-kernel-crc", "bad-entry", "bad-address", "zero-size",
])
ap.add_argument("--debug-halt-before-start", action="store_true")
ap.add_argument("--out", type=Path, default=ROOT / "out/build/halaos-bluepill.elf")
args = ap.parse_args()

cc = shutil.which("clang")
objcopy = shutil.which("llvm-objcopy") or shutil.which("arm-none-eabi-objcopy") or shutil.which("objcopy")
size_tool = shutil.which("llvm-size") or shutil.which("size")
nm_tool = shutil.which("llvm-nm") or shutil.which("nm")
if not cc or not objcopy or not nm_tool:
    raise SystemExit("clang, objcopy and nm are required")

args.out.parent.mkdir(parents=True, exist_ok=True)
metrics_dir = ROOT / "out/build"
metrics_dir.mkdir(parents=True, exist_ok=True)
canonical = "/halaos-src"
path_flags = [
    f"-ffile-prefix-map={ROOT}={canonical}",
    f"-fdebug-prefix-map={ROOT}={canonical}",
    f"-fmacro-prefix-map={ROOT}={canonical}",
]
profile_macro = args.profile.upper().replace("-", "_")


def common_flags() -> list[str]:
    cmd = [
        cc, "--target=armv7m-none-eabi", "-mcpu=cortex-m3", "-mthumb",
        "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
        "-fdata-sections", "-ffunction-sections", "-Wall", "-Wextra",
        "-Werror=implicit-function-declaration", "-flto", "-g", "-Oz",
        "-nostdlib", *path_flags,
    ]
    if args.profile == "qualification":
        cmd += ["-DHALAOS_QUALIFICATION=1"]
    cmd += ["-DHALAOS_TARGET_BLUEPILL=1", f"-DHALAOS_BUILD_PROFILE_{profile_macro}=1"]
    macros = {
        "boot-lab": "HALAOS_LAB_BOOT",
        "scheduler-lab": "HALAOS_LAB_SCHEDULER",
        "process-ipc-lab": "HALAOS_LAB_PROCESS_IPC",
        "vfs-posix-lab": "HALAOS_LAB_VFS_POSIX",
        "compiler-vm-lab": "HALAOS_LAB_COMPILER_VM",
        "hardware-constrained": "HALAOS_HARDWARE_CONSTRAINED",
    }
    if args.profile in macros:
        cmd += [f"-D{macros[args.profile]}=1"]
    if args.bad_dtb:
        cmd += ["-DHALAOS_BAD_DTB=1"]
    if args.debug_halt_before_start:
        cmd += ["-DHALAOS_DEBUG_HALT_BEFORE_START=1"]
    return cmd


def compile_elf(out: Path, defs: list[str], generation: bool = False) -> Path:
    map_path = out.with_suffix(".map")
    cmd = common_flags() + defs
    if generation:
        cmd += ["-DHALAOS_BOOT_CRC_GENERATION=1"]
    cmd += [str(path) for path in SOURCES]
    cmd += [
        "-I", str(ROOT / "include"), "-I", str(ROOT / "src/generated"),
        "-Wl,-T," + str(ROOT / "config/memory/stm32f103c8.ld"),
        "-Wl,--gc-sections", "-Wl,-Map=" + str(map_path),
        "-Wl,-u,Reset_Handler_C", "-Wl,-u,hala_loader_entry",
        "-Wl,-u,kernel_main", "-Wl,-u,hala_schedule_next",
        "-Wl,-u,hala_bad_psp", "-Wl,-u,HardFault_C",
        "-Wl,-u,hala_svc_dispatch", "-fuse-ld=lld", "-o", str(out),
    ]
    print("+", " ".join(cmd))
    subprocess.run(cmd, check=True)
    return map_path


def symbols(path: Path) -> dict[str, int]:
    output = subprocess.run([nm_tool, "-n", str(path)], text=True, capture_output=True, check=True).stdout
    result: dict[str, int] = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            try:
                result[parts[2]] = int(parts[0], 16)
            except ValueError:
                pass
    return result


def dump_section(elf: Path, name: str, dest: Path) -> bytes:
    if dest.exists():
        dest.unlink()
    subprocess.run([objcopy, "--dump-section", f"{name}={dest}", str(elf)], check=True)
    return dest.read_bytes()


def derive_actual(bootstrap: Path) -> dict[str, int]:
    sy = symbols(bootstrap)
    tmp = bootstrap.parent / (bootstrap.stem + "-sections")
    tmp.mkdir(exist_ok=True)
    loader = dump_section(bootstrap, ".hala_loader", tmp / "loader.bin")
    dtb = dump_section(bootstrap, ".hala_dtb", tmp / "dtb.bin")
    kernel = dump_section(bootstrap, ".hala_kernel", tmp / "kernel.bin")
    return {
        "magic": 0x48414C41, "version": 4, "header_size": 60, "flags": 0xF,
        "loader_addr": sy["__hala_loader_start"], "loader_size": len(loader),
        "loader_crc": zlib.crc32(loader) & 0xFFFFFFFF,
        "dtb_addr": sy["g_halaos_compact_dtb"], "dtb_size": len(dtb),
        "dtb_crc": zlib.crc32(dtb) & 0xFFFFFFFF,
        "kernel_addr": sy["__hala_kernel_start"], "kernel_size": len(kernel),
        "kernel_crc": zlib.crc32(kernel) & 0xFFFFFFFF,
        "entry_point": sy["kernel_main"] | 1,
    }


def apply_fault(actual: dict[str, int]) -> dict[str, int]:
    values = dict(actual)
    fault = args.boot_fault or ("bad-header-crc" if args.bad_manifest else None)
    if fault == "bad-magic": values["magic"] = 0
    elif fault == "bad-version": values["version"] = 99
    elif fault == "bad-loader-crc": values["loader_crc"] ^= 1
    elif fault == "bad-dtb-crc": values["dtb_crc"] ^= 1
    elif fault == "bad-kernel-crc": values["kernel_crc"] ^= 1
    elif fault == "bad-entry": values["entry_point"] &= ~1
    elif fault == "bad-address": values["loader_addr"] = 0x07000000
    elif fault == "zero-size": values["loader_size"] = 0
    ordered = [values[key] for key in [
        "magic", "version", "header_size", "flags", "loader_addr",
        "loader_size", "loader_crc", "dtb_addr", "dtb_size", "dtb_crc",
        "kernel_addr", "kernel_size", "kernel_crc", "entry_point",
    ]]
    values["header_crc"] = zlib.crc32(struct.pack("<14I", *ordered)) & 0xFFFFFFFF
    if fault == "bad-header-crc":
        values["header_crc"] ^= 1
    return values


def defs_from(values: dict[str, int]) -> list[str]:
    mapping = {
        "HALAOS_MANIFEST_MAGIC": "magic", "HALAOS_MANIFEST_VERSION": "version",
        "HALAOS_LOADER_ADDR": "loader_addr", "HALAOS_LOADER_SIZE": "loader_size",
        "HALAOS_LOADER_CRC": "loader_crc", "HALAOS_DTB_ADDR": "dtb_addr",
        "HALAOS_DTB_IMAGE_SIZE": "dtb_size", "HALAOS_DTB_IMAGE_CRC": "dtb_crc",
        "HALAOS_KERNEL_ADDR": "kernel_addr", "HALAOS_KERNEL_SIZE": "kernel_size",
        "HALAOS_KERNEL_CRC": "kernel_crc", "HALAOS_KERNEL_ENTRY": "entry_point",
        "HALAOS_MANIFEST_HEADER_CRC": "header_crc",
    }
    return [f"-D{name}=0x{values[key]:08X}u" for name, key in mapping.items()]


bootstrap = args.out.with_name(args.out.stem + ".bootstrap.elf")
compile_elf(bootstrap, [], generation=True)
actual = derive_actual(bootstrap)
manifest = apply_fault(actual)
map_path = compile_elf(args.out, defs_from(manifest))
for _ in range(3):
    next_actual = derive_actual(args.out)
    comparable = [
        "loader_addr", "loader_size", "loader_crc", "dtb_addr", "dtb_size",
        "dtb_crc", "kernel_addr", "kernel_size", "kernel_crc", "entry_point",
    ]
    if all(next_actual[key] == actual[key] for key in comparable):
        break
    actual = next_actual
    manifest = apply_fault(actual)
    map_path = compile_elf(args.out, defs_from(manifest))
else:
    raise SystemExit("boot image manifest did not converge")

bin_path = args.out.with_suffix(".bin")
subprocess.run([objcopy, "-O", "binary", str(args.out), str(bin_path)], check=True)
metrics: dict[str, object] = {
    "status": "PASS", "target": "STM32F103C8 Blue Pill", "profile": args.profile,
    "profileMacro": profile_macro, "elf": str(args.out), "map": str(map_path),
    "bin": str(bin_path), "elfSha256": hashlib.sha256(args.out.read_bytes()).hexdigest(),
    "binSha256": hashlib.sha256(bin_path.read_bytes()).hexdigest(),
    "manifest": manifest,
    "bootFault": args.boot_fault or ("bad-header-crc" if args.bad_manifest else None),
}
if size_tool:
    proc = subprocess.run([size_tool, str(args.out)], text=True, capture_output=True, check=True)
    print(proc.stdout, end="")
    match = re.search(r"\n\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)", proc.stdout)
    if match:
        metrics.update(text=int(match.group(1)), data=int(match.group(2)),
                       bss=int(match.group(3)), dec=int(match.group(4)))
(metrics_dir / "build-metrics.json").write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
print(json.dumps(metrics, indent=2))
