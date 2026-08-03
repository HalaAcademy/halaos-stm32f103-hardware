/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    ex01_threads_scheduler.c
 * @brief   Ví dụ minh họa quản lý luồng (Pthread) và lập lịch trong HalaOS.
 * @details File hướng dẫn cách tạo, điều phối, đồng bộ vòng đời luồng thông qua
 *          các POSIX-like API của HalaOS, giải thích các cơ chế lập lịch bên dưới.
 */

#include "halaos/user/hala_posix.h"

/* Các định nghĩa cho chu kỳ chạy của luồng mẫu */
#define LOOP_COUNT 5u
#define SLEEP_TICKS_A 50u /* Luồng A ngủ 50 ticks (~500ms) */
#define SLEEP_TICKS_B 80u /* Luồng B ngủ 80 ticks (~800ms) */

/**
 * @brief Hàm thực thi của Luồng con A.
 * @details Đếm số chu kỳ chạy, in thông tin ra console, và ngủ một khoảng thời gian tương đối.
 * @param[in] arg Đối số truyền từ hàm tạo luồng (ép kiểu từ con trỏ).
 */
static void thread_a_entry(void *arg) {
    (void)arg;

    for (unsigned int i = 0u; i < LOOP_COUNT; ++i) {
        /*
         * Mở rộng cửa sổ truyền để ghi log. Do UART được chia sẻ chung trong user mode,
         * việc sử dụng luồng có thể cạnh tranh tài nguyên log.
         */
        int fd = hala_open("/dev/console", 0u);
        if (fd >= 0) {
            char log_msg[] = "[Luồng A] Đang hoạt động, chu kỳ: \n";
            /* Sửa đổi ký tự in số chu kỳ để người dùng dễ theo dõi */
            log_msg[34] = (char)('0' + (i + 1u));
            hala_write(fd, log_msg, sizeof(log_msg) - 1u);
            hala_close(fd);
        }

        /*
         * Đưa luồng hiện hành vào trạng thái ngủ trong một số ticks nhất định.
         * SysTick Handler của kernel sẽ đánh thức luồng sau khi hết thời gian này.
         */
        hala_nanosleep(SLEEP_TICKS_A);
    }

    /* Thoát luồng con với mã trả về xác định (99) */
    hala_pthread_exit(99);
}

/**
 * @brief Hàm thực thi của Luồng con B.
 * @details Thực thi tương tự luồng A nhưng chu kỳ ngủ dài hơn để minh họa lập lịch phi đồng bộ.
 * @param[in] arg Đối số truyền từ hàm tạo luồng.
 */
static void thread_b_entry(void *arg) {
    (void)arg;

    for (unsigned int i = 0u; i < LOOP_COUNT; ++i) {
        int fd = hala_open("/dev/console", 0u);
        if (fd >= 0) {
            char log_msg[] = "[Luồng B] Đang hoạt động, chu kỳ: \n";
            log_msg[34] = (char)('0' + (i + 1u));
            hala_write(fd, log_msg, sizeof(log_msg) - 1u);
            hala_close(fd);
        }
        hala_nanosleep(SLEEP_TICKS_B);
    }
    hala_pthread_exit(100);
}

/**
 * @brief Chương trình chính điều phối vòng đời của Luồng.
 * @details Khởi tạo luồng A và B, theo dõi quá trình chạy và thu hồi (Join) tài nguyên luồng con.
 * @return 0 nếu toàn bộ luồng hoàn thành đúng mong đợi, ngược lại trả về mã lỗi âm.
 */
int example_threads_main(void) {
    hala_pthread_t thread_a = -1;
    hala_pthread_t thread_b = -1;
    int status_a = -1;
    int status_b = -1;
    int ret;

    int console_fd = hala_open("/dev/console", 0u);
    if (console_fd >= 0) {
        char start_msg[] = "========================================\r\n"
                           "Bắt đầu ví dụ Thread & Scheduler...\r\n"
                           "========================================\r\n";
        hala_write(console_fd, start_msg, sizeof(start_msg) - 1u);
        hala_close(console_fd);
    }

    /*
     * Bước 1: Tạo luồng A.
     * Hàm này gọi SVC_SPAWN_EXEC dưới Kernel để tạo một Task Control Block (TCB) động mới,
     * thiết lập stack riêng, và kích hoạt chuyển trạng thái thành READY.
     */
    ret = hala_pthread_create(&thread_a, (unsigned)thread_a_entry, 1u);
    if (ret != 0) {
        return ret; /* Trả về mã lỗi tạo luồng A */
    }

    /*
     * Bước 2: Tạo luồng B chạy song song.
     * Cả hai luồng A và B sẽ cạnh tranh khe thời gian CPU dưới sự điều phối của scheduler.
     */
    ret = hala_pthread_create(&thread_b, (unsigned)thread_b_entry, 2u);
    if (ret != 0) {
        return ret; /* Trả về mã lỗi tạo luồng B */
    }

    /*
     * Bước 3: Chờ luồng A kết thúc và giải phóng TCB (Join).
     * Hàm này block luồng chính hiện tại (shell) cho tới khi luồng con A chuyển sang trạng thái
     * ZOMBIE.
     */
    ret = hala_pthread_join(thread_a, &status_a);
    if (ret != 0) {
        return ret;
    }

    /* Bước 4: Chờ luồng B kết thúc. */
    ret = hala_pthread_join(thread_b, &status_b);
    if (ret != 0) {
        return ret;
    }

    /* In báo cáo kết quả nhận được từ hai luồng con */
    console_fd = hala_open("/dev/console", 0u);
    if (console_fd >= 0) {
        char report_msg[] = "Cả hai luồng đã hoàn tất.\r\n"
                            "Luồng A thoát với mã: 00\r\n"
                            "Luồng B thoát với mã: 000\r\n";
        /* Ép mã thoát số sang ký tự để hiển thị */
        report_msg[51] = (char)('0' + (status_a / 10));
        report_msg[52] = (char)('0' + (status_a % 10));
        report_msg[78] = (char)('0' + (status_b / 100));
        report_msg[79] = (char)('0' + ((status_b % 100) / 10));
        report_msg[80] = (char)('0' + (status_b % 10));

        hala_write(console_fd, report_msg, sizeof(report_msg) - 1u);
        hala_close(console_fd);
    }

    return 0;
}
