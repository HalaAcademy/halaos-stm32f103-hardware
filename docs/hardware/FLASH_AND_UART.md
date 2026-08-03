# Build, flash và UART

## Build

```bash
make build
```

## Flash

Cài OpenOCD và kết nối ST-Link:

```bash
make flash
```

Lệnh tương đương:

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg   -c "program out/build/halaos-bluepill.elf verify reset exit"
```

## UART

Mở terminal 115200 8N1 hoặc chạy:

```bash
python -m pip install -r requirements-hardware.txt
make serial-smoke PORT=/dev/ttyUSB0
```

Nếu không thấy log, kiểm tra GND chung, TX/RX nối chéo, BOOT0 và nguồn board.
