/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    ex02_ipc_sync.c
 * @brief   Ví dụ minh họa các cơ chế IPC và đồng bộ hóa trong HalaOS.
 * @details File hướng dẫn sử dụng bốn cơ chế IPC chính của HalaOS bao gồm
 *          Message Queue, Semaphore, Mutex và Event.
 */

#include "halaos/user/hala_posix.h"

/* Biến tài nguyên dùng chung bảo vệ bởi Mutex */
static volatile int g_shared_counter = 0;

/* Các handle của đối tượng IPC */
static int g_queue_handle = -1;
static int g_sem_handle = -1;
static int g_mutex_handle = -1;
static int g_event_handle = -1;

/**
 * @brief Luồng sản xuất (Producer Task).
 * @details Gửi dữ liệu vào hàng đợi (Queue), bảo vệ tài nguyên chung bằng Mutex,
 *          và báo hiệu hoàn thành công việc qua Semaphore và Event.
 * @param[in] arg Không sử dụng.
 */
static void producer_thread(void *arg) {
    (void)arg;
    int console_fd;
    unsigned char value_to_send = 0xAAu;
    
    /* 1. Gửi dữ liệu vào Message Queue (Không Timeout) */
    int ret = hala_ipc_send(g_queue_handle, value_to_send);
    console_fd = hala_open("/dev/console", 0u);
    if (console_fd >= 0) {
        if (ret == 1) {
            char msg[] = "[Producer] Đã gửi giá trị 0xAA vào Hàng đợi.\r\n";
            hala_write(console_fd, msg, sizeof(msg) - 1u);
        } else {
            char msg[] = "[Producer] Gửi dữ liệu thất bại.\r\n";
            hala_write(console_fd, msg, sizeof(msg) - 1u);
        }
        hala_close(console_fd);
    }
    
    /* Trì hoãn một chút trước khi truy cập tài nguyên dùng chung */
    hala_nanosleep(20u);

    /* 2. Độc quyền truy cập và tăng Counter bằng Mutex */
    ret = hala_mutex_lock(g_mutex_handle, 100u); /* Timeout 100 ticks */
    if (ret == 0) {
        g_shared_counter = 42; /* Thay đổi tài nguyên chung */
        
        console_fd = hala_open("/dev/console", 0u);
        if (console_fd >= 0) {
            char msg[] = "[Producer] Đã khóa Mutex và gán g_shared_counter = 42\r\n";
            hala_write(console_fd, msg, sizeof(msg) - 1u);
            hala_close(console_fd);
        }
        
        hala_mutex_unlock(g_mutex_handle); /* Giải phóng khóa */
    }

    /* 3. Phát tín hiệu hoàn thành chu kỳ qua Semaphore */
    hala_sem_post(g_sem_handle);

    /* 4. Kích hoạt Event báo hiệu kết thúc công việc */
    hala_event_set(g_event_handle);

    hala_pthread_exit(0);
}

/**
 * @brief Luồng tiêu thụ (Consumer Task).
 * @details Chờ nhận dữ liệu từ Queue, đợi Semaphore sẵn sàng để kiểm tra counter,
 *          và chờ Event xác nhận kết thúc.
 * @param[in] arg Không sử dụng.
 */
static void consumer_thread(void *arg) {
    (void)arg;
    int console_fd;
    unsigned char received_val = 0;
    int ret;

    /* 1. Đợi nhận dữ liệu từ Queue (Timeout tối đa 200 ticks) */
    ret = hala_ipc_receive(g_queue_handle, &received_val, 200u);
    console_fd = hala_open("/dev/console", 0u);
    if (console_fd >= 0) {
        if (ret == 1 && received_val == 0xAAu) {
            char msg[] = "[Consumer] Đã nhận chính xác dữ liệu 0xAA từ Hàng đợi.\r\n";
            hala_write(console_fd, msg, sizeof(msg) - 1u);
        } else {
            char msg[] = "[Consumer] Lỗi hoặc hết thời gian chờ Queue.\r\n";
            hala_write(console_fd, msg, sizeof(msg) - 1u);
        }
        hala_close(console_fd);
    }

    /* 2. Đợi Semaphore sẵn sàng (Producer đã hoàn thành việc ghi dữ liệu) */
    ret = hala_sem_wait(g_sem_handle, 200u);
    if (ret == 0) {
        /* Đọc tài nguyên chung dưới sự bảo vệ của Mutex */
        ret = hala_mutex_lock(g_mutex_handle, 50u);
        if (ret == 0) {
            int local_val = g_shared_counter;
            hala_mutex_unlock(g_mutex_handle);

            console_fd = hala_open("/dev/console", 0u);
            if (console_fd >= 0) {
                char msg[] = "[Consumer] Đọc counter từ Mutex: 00\r\n";
                msg[32] = (char)('0' + (local_val / 10));
                msg[33] = (char)('0' + (local_val % 10));
                hala_write(console_fd, msg, sizeof(msg) - 1u);
                hala_close(console_fd);
            }
        }
    }

    /* 3. Đợi sự kiện Event được set */
    ret = hala_event_wait(g_event_handle, 100u);
    console_fd = hala_open("/dev/console", 0u);
    if (console_fd >= 0) {
        if (ret == 1) {
            char msg[] = "[Consumer] Đã nhận tín hiệu Event hoàn thành.\r\n";
            hala_write(console_fd, msg, sizeof(msg) - 1u);
        } else {
            char msg[] = "[Consumer] Không nhận được Event.\r\n";
            hala_write(console_fd, msg, sizeof(msg) - 1u);
        }
        hala_close(console_fd);
    }

    hala_pthread_exit(0);
}

/**
 * @brief Chương trình chính khởi tạo các đối tượng IPC và chạy ví dụ.
 * @return 0 nếu thành công, mã âm nếu thất bại.
 */
int example_ipc_main(void) {
    hala_pthread_t prod_thread, cons_thread;
    int status;
    int console_fd;

    console_fd = hala_open("/dev/console", 0u);
    if (console_fd >= 0) {
        char msg[] = "========================================\r\n"
                     "Bắt đầu ví dụ IPC & Đồng bộ hóa...\r\n"
                     "========================================\r\n";
        hala_write(console_fd, msg, sizeof(msg) - 1u);
        hala_close(console_fd);
    }

    /*
     * Bước 1: Khởi tạo các đối tượng giao tiếp và đồng bộ.
     * Kernel cung cấp một bể chứa tĩnh đối tượng IPC (g_ipc_objects).
     */
    g_queue_handle = hala_ipc_create(HALA_IPC_QUEUE, 0);
    g_sem_handle   = hala_ipc_create(HALA_IPC_SEM, 0); /* Khởi tạo sem = 0 */
    g_mutex_handle = hala_ipc_create(HALA_IPC_MUTEX, 0);
    g_event_handle = hala_event_create(0);              /* Khởi tạo event = 0 */

    if (g_queue_handle < 0 || g_sem_handle < 0 || g_mutex_handle < 0 || g_event_handle < 0) {
        return -1; /* Lỗi khởi tạo IPC */
    }

    /* Bước 2: Tạo luồng Producer và Consumer */
    hala_pthread_create(&prod_thread, (unsigned)producer_thread, 0u);
    hala_pthread_create(&cons_thread, (unsigned)consumer_thread, 0u);

    /* Bước 3: Chờ cả hai luồng kết thúc */
    hala_pthread_join(prod_thread, &status);
    hala_pthread_join(cons_thread, &status);

    /* Bước 4: Thu hồi các đối tượng IPC để trả lại vùng nhớ tĩnh */
    hala_ipc_close(g_queue_handle);
    hala_ipc_close(g_sem_handle);
    hala_ipc_close(g_mutex_handle);
    hala_ipc_close(g_event_handle);

    console_fd = hala_open("/dev/console", 0u);
    if (console_fd >= 0) {
        char msg[] = "Hoàn thành ví dụ IPC.\r\n";
        hala_write(console_fd, msg, sizeof(msg) - 1u);
        hala_close(console_fd);
    }

    return 0;
}
