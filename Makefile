# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
PYTHON ?= python3
PROFILE ?= hardware
ELF ?= out/build/halaos-bluepill.elf
PORT ?= /dev/ttyUSB0
OPENOCD ?= openocd

.PHONY: all build qualification bad-manifest bad-dtb audit verify reproducible flash serial-smoke clean

all: build audit verify

build:
	$(PYTHON) tools/build.py --profile $(PROFILE) --out $(ELF)

qualification:
	$(PYTHON) tools/build.py --profile qualification --out out/build/halaos-bluepill-qualification.elf

bad-manifest:
	$(PYTHON) tools/build.py --profile hardware --bad-manifest --out out/build/halaos-bluepill-bad-manifest.elf

bad-dtb:
	$(PYTHON) tools/build.py --profile hardware --bad-dtb --out out/build/halaos-bluepill-bad-dtb.elf

audit:
	$(PYTHON) tools/resource_audit.py $(ELF)

verify:
	$(PYTHON) scripts/verify_repository.py

reproducible:
	$(PYTHON) tools/reproducible_build_check.py

flash: build
	$(PYTHON) tools/hardware/flash_openocd.py --elf $(ELF) --openocd $(OPENOCD)

serial-smoke:
	$(PYTHON) tests/hardware/serial_smoke.py --port $(PORT)

clean:
	rm -rf out/build out/test
