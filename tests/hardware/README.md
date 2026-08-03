# Kiểm thử phần cứng Blue Pill

Các test trong thư mục này chỉ chạy khi firmware đã được flash lên STM32F103C8
và USART1 được nối với USB-UART 3,3 V.

```bash
python -m pip install -r requirements-hardware.txt
make serial-smoke PORT=/dev/ttyUSB0
```
