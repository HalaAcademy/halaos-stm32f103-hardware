# Blue Pill acceptance checklist

- [ ] Clean build và SHA-256 đã ghi.
- [ ] Flash verify thành công bằng ST-Link.
- [ ] Boot log đầy đủ qua USART1 115200 8N1.
- [ ] Reset qua lệnh shell hoạt động bằng SYSRESETREQ.
- [ ] WFI idle hoạt động và không treo interrupt.
- [ ] Scheduler workloads đạt ngưỡng đã phê duyệt.
- [ ] Process/thread/syscall/IPC không leak.
- [ ] Fault user không dừng kernel.
- [ ] Persistence qua reset đạt.
- [ ] Power-loss transaction test đạt.
- [ ] Timing và current có raw capture.
