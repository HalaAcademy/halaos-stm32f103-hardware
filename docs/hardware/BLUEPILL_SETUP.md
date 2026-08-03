# Chuẩn bị Blue Pill

## Board hỗ trợ

Target hiện tại là STM32F103C8T6 Blue Pill với 64 KiB Flash và 20 KiB SRAM.
Các board clone có thể khác chất lượng crystal, USB pull-up hoặc dung lượng Flash
thực tế; contributor phải ghi rõ model board trong report.

## Kết nối SWD

- ST-Link SWDIO → PA13/SWDIO.
- ST-Link SWCLK → PA14/SWCLK.
- GND chung.
- 3,3 V reference theo debugger/board.
- BOOT0 ở mức 0 để boot từ Flash.

## Kết nối UART

- PA9 TX → RX USB-UART.
- PA10 RX ← TX USB-UART.
- GND chung.
- Chỉ dùng logic 3,3 V.
