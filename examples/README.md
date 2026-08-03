# Các ví dụ minh họa tính năng HalaOS trên Board thật

Thư mục này chứa các file ví dụ C mẫu hướng dẫn đầy đủ cách sử dụng các tính năng và hệ thống API giao tiếp của HalaOS.

## Danh sách các tệp ví dụ:

1. **[ex01_threads_scheduler.c](ex01_threads_scheduler.c)**
   - **Tính năng:** Đa nhiệm & Quản lý Luồng (Pthreads).
   - **Nội dung:** Minh họa cách tạo luồng (`hala_pthread_create`), thu hồi tài nguyên luồng (`hala_pthread_join`), tạm dừng luồng (`hala_nanosleep`), và thoát luồng (`hala_pthread_exit`).

2. **[ex02_ipc_sync.c](ex02_ipc_sync.c)**
   - **Tính năng:** Giao tiếp & Đồng bộ hóa liên tiến trình (IPC).
   - **Nội dung:** Minh họa cách sử dụng đồng thời 4 mô hình đồng bộ tĩnh trong Kernel:
     - *Message Queue:* Truyền thông điệp byte giữa Producer và Consumer.
     - *Semaphore:* Điều phối trạng thái sẵn sàng của tài nguyên.
     - *Mutex:* Khóa độc quyền bảo vệ tài nguyên dùng chung.
     - *Event:* Kích hoạt luồng thông qua phát tín hiệu sự kiện.

3. **[ex03_vfs_proc_dev.c](ex03_vfs_proc_dev.c)**
   - **Tính năng:** Hệ thống tệp ảo (VFS) & Ngoại vi.
   - **Nội dung:** Minh họa giao tiếp chuẩn POSIX file API:
     - Mở, đọc thông tin phiên bản OS và Uptime từ phân vùng hệ thống tệp tiến trình `/proc`.
     - Ghi tín hiệu điều khiển trực tiếp đảo trạng thái (nháy LED) cổng vật lý `PC13` qua file thiết bị ngoại vi `/dev/gpio/PC13`.
