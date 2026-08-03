# HalaOS

HalaOS là hệ điều hành giáo dục mã nguồn mở dành cho **STM32F103C8 Blue Pill**
(ARM Cortex-M3, 64 KiB Flash, 20 KiB SRAM, không MMU), được phát triển bởi
**HALA Academy**.

Website: https://hala.edu.vn

> Trạng thái: `v0.1.0-alpha`. Firmware build được cho Blue Pill

## Mục tiêu học tập

```text
Reset → Startup → Stage-0 → Loader → Device Tree → Kernel → Driver
→ Scheduler → Process → Syscall → IPC → VFS → Shell → Compiler → VM → App
```

## Phần cứng cần có

- STM32F103C8 Blue Pill.
- ST-Link V2 hoặc debugger tương thích SWD.
- USB-UART mức logic 3,3 V.
- Dây nối và nguồn 3,3 V/5 V phù hợp với board.

USART1 console:

```text
Blue Pill PA9  (TX) → RX của USB-UART
Blue Pill PA10 (RX) ← TX của USB-UART
GND                 ↔ GND
115200 baud, 8N1
```

## Build

Yêu cầu: `clang`, `lld`, `llvm-objcopy`, `nm`, `size`, Python 3.

```bash
make build
make audit
make reproducible
```

Artifact:

```text
out/build/halaos-bluepill.elf
out/build/halaos-bluepill.bin
out/build/halaos-bluepill.map
```

## Flash bằng ST-Link và OpenOCD

```bash
make flash
```

Hoặc:

```bash
python tools/hardware/flash_openocd.py \
  --elf out/build/halaos-bluepill.elf
```

## Kiểm tra UART trên board thật

```bash
python -m pip install -r requirements-hardware.txt
make serial-smoke PORT=/dev/ttyUSB0
```

Trên Windows có thể dùng `PORT=COM5`.

## Tài liệu

- `docs/hardware/BLUEPILL_SETUP.md`
- `docs/hardware/FLASH_AND_UART.md`
- `docs/hardware/HARDWARE_TEST_PLAN.md`
- `docs/development/BUILD.md`
- `docs/architecture/ARCHITECTURE.md`
- `docs/project/KNOWN_LIMITATIONS.md`

## Giới hạn

- Không phải Linux.
- Không tương thích POSIX đầy đủ.
- Không có MMU hoặc virtual memory.
- Flash và SRAM gần giới hạn của STM32F103C8.
- Chưa có hardware qualification đầy đủ về timing, power-loss và endurance.

## License

Apache License 2.0. Xem `LICENSE`, `NOTICE` và `TRADEMARKS.md`.
