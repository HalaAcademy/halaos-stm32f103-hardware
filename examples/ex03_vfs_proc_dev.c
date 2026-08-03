/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    ex03_vfs_proc_dev.c
 * @brief   Ví dụ minh họa sử dụng Hệ thống tệp ảo (VFS), /proc và /dev trong HalaOS.
 * @details File hướng dẫn mở, đọc thông tin hệ thống từ procfs (/proc) và
 *          tương tác với thiết bị ngoại vi qua devfs (/dev/gpio/PC13).
 */

#include "halaos/user/hala_posix.h"

/**
 * @brief Đọc thông tin hệ thống từ phân vùng procfs.
 * @details Mở tệp tin `/proc/version` và `/proc/uptime`, đọc và in ra console.
 * @param[in] console_fd File descriptor của màn hình console đầu ra.
 */
static void read_system_info(int console_fd) {
    char buffer[128];
    int fd;
    hala_ssize_t bytes_read;

    /* 
     * 1. Đọc tệp tin /proc/version.
     * Tệp tin này sinh động nội dung phiên bản OS trực tiếp từ Kernel State.
     */
    fd = hala_open("/proc/version", 0u);
    if (fd >= 0) {
        bytes_read = hala_read(fd, buffer, sizeof(buffer) - 1u);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            
            hala_write(console_fd, "Thông tin phiên bản OS: \r\n", 25u);
            hala_write(console_fd, buffer, (hala_size_t)bytes_read);
            hala_write(console_fd, "\r\n", 2u);
        }
        hala_close(fd);
    } else {
        hala_write(console_fd, "Không mở được /proc/version.\r\n", 30u);
    }

    /* 
     * 2. Đọc tệp tin /proc/uptime.
     * Tệp tin này hiển thị số tick đã trôi qua kể từ khi khởi động hệ thống.
     */
    fd = hala_open("/proc/uptime", 0u);
    if (fd >= 0) {
        bytes_read = hala_read(fd, buffer, sizeof(buffer) - 1u);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            
            hala_write(console_fd, "Thời gian uptime (ticks): ", 26u);
            hala_write(console_fd, buffer, (hala_size_t)bytes_read);
            hala_write(console_fd, "\r\n", 2u);
        }
        hala_close(fd);
    }
}

/**
 * @brief Điều khiển LED trên board thông qua file thiết bị ngoại vi devfs.
 * @details Mở file điều khiển thiết bị chân GPIO `/dev/gpio/PC13`, thực hiện
 *          nháy LED 3 lần và đóng file thiết bị.
 * @param[in] console_fd File descriptor của console ghi log.
 */
static void control_hardware_led(int console_fd) {
    int led_fd;
    unsigned char toggle_val = 1u;

    hala_write(console_fd, "Đang nháy LED trạng thái trên chân PC13 (3 lần)...\r\n", 53u);

    /* 
     * Mở tệp tin thiết bị GPIO LED PC13 ảo.
     * Việc ghi dữ liệu (bất kể giá trị nào) vào file này sẽ kích hoạt Driver
     * thực hiện đảo trạng thái chân GPIO vật lý của chip STM32F103.
     */
    led_fd = hala_open("/dev/gpio/PC13", 0u);
    if (led_fd >= 0) {
        for (int i = 0; i < 6; ++i) {
            /* Ghi lệnh đảo trạng thái cổng chân LED */
            hala_write(led_fd, &toggle_val, 1u);
            
            /* Dừng luồng 50 ticks (~500ms) tạo hiệu ứng chớp tắt */
            hala_nanosleep(50u);
        }
        hala_close(led_fd);
        hala_write(console_fd, "Hoàn tất điều khiển LED.\r\n", 27u);
    } else {
        hala_write(console_fd, "Không tìm thấy file thiết bị LED PC13.\r\n", 40u);
    }
}

/**
 * @brief Hàm chạy chính của ví dụ VFS.
 * @return 0 nếu hoàn tất không lỗi.
 */
int example_vfs_main(void) {
    int console_fd = hala_open("/dev/console", 0u);
    if (console_fd < 0) {
        return -1;
    }

    char start_msg[] = "========================================\r\n"
                       "Bắt đầu ví dụ VFS, /proc và /dev...\r\n"
                       "========================================\r\n";
    hala_write(console_fd, start_msg, sizeof(start_msg) - 1u);

    /* Thực hiện các bài test VFS */
    read_system_info(console_fd);
    control_hardware_led(console_fd);

    char end_msg[] = "========================================\r\n"
                     "Kết thúc ví dụ VFS.\r\n"
                     "========================================\r\n";
    hala_write(console_fd, end_msg, sizeof(end_msg) - 1u);
    
    hala_close(console_fd);
    return 0;
}
