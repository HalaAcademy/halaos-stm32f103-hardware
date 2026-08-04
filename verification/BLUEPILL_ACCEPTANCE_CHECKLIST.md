# Blue Pill acceptance checklist

- [ ] Clean build và SHA-256 đã ghi.
- [ ] Flash verify thành công bằng ST-Link.
- [x] Boot log đầy đủ qua USART1 115200 8N1. (Minh chứng: [Complete Boot](../docs/hardware/UART_DEMO_EVIDENCE.md#bước-03-chu-kỳ-khởi-động-hoàn-tất-complete-boot))
- [x] Reset qua lệnh shell hoạt động bằng SYSRESETREQ. (Minh chứng: [Process Control](../docs/hardware/UART_DEMO_EVIDENCE.md#bước-08-kiểm-soát-tiến-trình-máy-ảo))
- [x] WFI idle hoạt động và không treo interrupt. (Minh chứng: [Memory/Stack](../docs/hardware/UART_DEMO_EVIDENCE.md#bước-06-trạng-thái-tiến-trình--bộ-nhớ-ramstack))
- [x] Scheduler workloads đạt ngưỡng đã phê duyệt. (Minh chứng: [Scheduler Workloads](../docs/hardware/UART_DEMO_EVIDENCE.md#bước-12-đánh-giá-bộ-lập-lịch-scheduler-workloads))
- [x] Process/thread/syscall/IPC không leak. (Minh chứng: [Threads](../docs/hardware/UART_DEMO_EVIDENCE.md#bước-09-ứng-dụng-đa-luồng-song-song-threads) và [IPC](../docs/hardware/UART_DEMO_EVIDENCE.md#bước-10-giao-tiếp--đồng-bộ-liên-luồng-ipc))
- [x] Fault user không dừng kernel. (Minh chứng: [Crash Containment](../docs/hardware/UART_DEMO_EVIDENCE.md#bước-13-xử-lý-và-chứa-lỗi-người-dùng-crash-containment))
- [x] Persistence qua reset đạt. (Minh chứng: [Persistence](../docs/hardware/UART_DEMO_EVIDENCE.md#bước-14-lưu-trữ-ứng-dụng-persistence-qua-reset))
- [ ] Power-loss transaction test đạt.
- [ ] Timing và current có raw capture.
