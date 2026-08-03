---
id: HOS-DOC-ARCH-001
title: Kiến trúc HalaOS
version: 0.3.0
status: approved
owner: HALA Academy
last_reviewed: 2026-08-03
---

# Kiến trúc HalaOS

```text
Reset/Startup → Stage-0 → Loader → Compact DTB → Kernel
              → Scheduler/Task/Syscall → Services → HalaShell/Application
```

## Luật phụ thuộc

- `arch` và `platform` sở hữu code phụ thuộc Cortex-M3/STM32F1.
- Kernel không phụ thuộc parser lệnh shell.
- Service chỉ truy cập phần cứng qua internal driver/kernel API.
- Application không include `halaos_internal.h`.

## Trạng thái refactor

Source đã được tách thành nhiều translation unit thực. `halaos_internal.h` vẫn là compatibility aggregator và phải tiếp tục được chia theo module trong các phase sau.
