# Kế hoạch đánh giá phần cứng Blue Pill

Mọi test phải ghi board ID, firmware SHA-256, wiring, tool version và raw UART.

## Nhóm test

1. Boot positive và manifest/DTB negative images.
2. UART RX/TX, burst và overrun.
3. SysTick jitter và PendSV context-switch latency.
4. Scheduler fairness và deadline trên clock thật.
5. Process, syscall, IPC và VFS stress.
6. User fault containment.
7. App-store reset và mất nguồn khi ghi Flash.
8. Power-cycle, brownout và Flash endurance.
9. WFI/tickless và dòng tiêu thụ.

Không dùng output `PASS` của firmware làm oracle duy nhất. Cần UART raw, trạng
thái runtime và thiết bị đo phù hợp.
