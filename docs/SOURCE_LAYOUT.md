# Source layout

```text
boards/bluepill/        Cấu hình board và Device Tree
config/                 Linker memory và danh sách source
include/halaos/         Public/internal headers
src/                    Firmware STM32F103C8
services/               Nằm trong src/services
scripts/                Build/verify helpers
tests/hardware/         Test chạy qua UART trên board thật
tools/hardware/         Flash/debug helpers
docs/hardware/          Wiring, flash, UART và qualification
out/                    Artifact sinh ra, không commit
```

Repository chỉ có một target: STM32F103C8 Blue Pill.
