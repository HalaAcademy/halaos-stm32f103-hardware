# Hạn chế hiện tại

- Chưa hoàn thành qualification trên Blue Pill thật.
- Chưa đo SysTick jitter, PendSV latency, IRQ latency và UART overrun.
- Chưa kiểm tra brownout và mất nguồn trong lúc ghi Flash.
- Chưa có dữ liệu Flash endurance và power-cycle dài hạn.
- Không có MMU, virtual memory, `fork`, `mmap`, dynamic linker hoặc socket đầy đủ.
- POSIX chỉ là tập API giáo dục có giới hạn.
- Flash và SRAM gần giới hạn, nên tính năng mới phải có resource budget.
- Một số source còn lớn và cần tiếp tục tách module.
