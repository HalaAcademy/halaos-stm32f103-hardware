/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    hala_posix.c
 * @brief   User-space wrappers cho POSIX-inspired syscall ABI của HalaOS.
 * @details Mọi wrapper phát lệnh SVC; user code không truy cập trực tiếp kernel object.
 */
#include "halaos/user/hala_posix.h"
#include "halaos/internal/halaos_internal.h"

int hala_open(const char *path, unsigned flags) {
    return svc_ptr_arg(SVC_OPEN, path, str_len(path) + 1u, flags);
}

int hala_close(int fd) { return svc_arg(SVC_CLOSE, (u32)fd); }
hala_ssize_t hala_read(int fd, void *buffer, hala_size_t length) {
    return svc_args3(SVC_FD_READ, (u32)fd, (u32)(usize)buffer, length);
}
hala_ssize_t hala_write(int fd, const void *buffer, hala_size_t length) {
    return svc_args3(SVC_FD_WRITE, (u32)fd, (u32)(usize)buffer, length);
}
int hala_dup2(int oldfd, int newfd) { return svc_args2(SVC_DUP2, (u32)oldfd, (u32)newfd); }
int hala_spawn(const char *name) { return svc_ptr_arg(SVC_SPAWN, name, str_len(name) + 1u, 0u); }
int hala_posix_spawn(const char *name, unsigned entry, unsigned argument) {
    return svc_ptr_args2(SVC_SPAWN_EXEC, name, str_len(name) + 1u, entry, argument);
}
int hala_waitpid(int pid, int *status) { return svc_args2(SVC_WAIT, (u32)pid, (u32)(usize)status); }
int hala_kill(int pid, int status) { return svc_args2(SVC_KILL, (u32)pid, (u32)status); }
int hala_nanosleep(unsigned ticks) { return svc_arg(SVC_SLEEP, ticks); }

int hala_pthread_create(hala_pthread_t *thread, unsigned entry, unsigned argument) {
    static unsigned sequence;
    char name[12] = "pthread-0";
    if (thread == NULL)
        return -22;
    name[8] = (char)('0' + (sequence++ % 10u));
    int pid = hala_posix_spawn(name, entry, argument);
    if (pid > 0)
        *thread = pid;
    return pid > 0 ? 0 : pid;
}

int hala_pthread_join(hala_pthread_t thread, int *status) { return hala_waitpid(thread, status); }

void hala_pthread_exit(int status) {
    (void)svc_arg(SVC_EXIT, (u32)status);
    for (;;)
        (void)svc_call0(SVC_YIELD);
}

int hala_ipc_create(unsigned type, int initial) {
    return svc_args2(SVC_IPC_CREATE, type, (u32)initial);
}
int hala_ipc_close(int handle) { return svc_arg(SVC_IPC_CLOSE, (u32)handle); }
int hala_ipc_send(int handle, unsigned char value) {
    return svc_args2(SVC_IPC_SEND, (u32)handle, value);
}
int hala_ipc_receive(int handle, unsigned char *value, unsigned timeout_ticks) {
    return svc_args3(SVC_IPC_RECEIVE, (u32)handle, (u32)(usize)value, timeout_ticks);
}
int hala_sem_wait(int handle, unsigned timeout_ticks) {
    return svc_args2(SVC_SEM_WAIT, (u32)handle, timeout_ticks);
}
int hala_sem_post(int handle) { return svc_arg(SVC_SEM_POST, (u32)handle); }
int hala_mutex_lock(int handle, unsigned timeout_ticks) {
    return svc_args2(SVC_MUTEX_LOCK, (u32)handle, timeout_ticks);
}
int hala_mutex_unlock(int handle) { return svc_arg(SVC_MUTEX_UNLOCK, (u32)handle); }
int hala_event_create(int initial_state) {
    return hala_ipc_create(HALA_IPC_EVENT, initial_state ? 1 : 0);
}
int hala_event_wait(int handle, unsigned timeout_ticks) {
    unsigned char value = 0u;
    return hala_ipc_receive(handle, &value, timeout_ticks);
}
int hala_event_set(int handle) { return hala_ipc_send(handle, 1u); }
int hala_vfs_list(const char *path) {
    return svc_ptr_arg(SVC_VFS_LIST, path, str_len(path) + 1u, 0u);
}

/**
 * @brief Kiểm tra end-to-end public syscall API từ user mode.
 * @details Test đi qua public wrapper cho process, runnable process/pthread, IPC và VFS.
 */
#if defined(HALAOS_LAB_BOOT) || defined(HALAOS_LAB_SCHEDULER)
int hala_user_api_selftest(void) {
    /* Hai profile tối thiểu chủ ý không mang qualification workload đầy đủ. */
    return -95;
}
#else
int example_threads_main(void);
int example_ipc_main(void);
int example_vfs_main(void);

int hala_user_api_selftest(void) {
    int r;
    r = example_threads_main();
    if (r != 0)
        return -101;
    r = example_ipc_main();
    if (r != 0)
        return -102;
    r = example_vfs_main();
    if (r != 0)
        return -103;

    unsigned char value = 0u;
    unsigned char readback[3] = {0u, 0u, 0u};
    const unsigned char payload[3] = {'A', 'P', 'I'};
    int status = -1;

    /* Bước 1: lifecycle PCB thuần để kiểm tra ownership kill/wait. */
    int pid = hala_spawn("api-child");
    if (pid < 0)
        return -1;

    /* Bước 2: IPC object và timeout non-blocking. Blocking path được test bằng
     * campaign riêng để producer có thể đánh thức waiter. */
    int queue = hala_ipc_create(HALA_IPC_QUEUE, 0);
    if ((queue < 0) || (hala_ipc_send(queue, 0x5Au) != 1) ||
        (hala_ipc_receive(queue, &value, 0u) != 1) || (value != 0x5Au) ||
        (hala_ipc_close(queue) != 0))
        return -2;

    /* Blocking queue: child producer ngủ 20 tick rồi gửi byte. Receive phải
     * block shell, producer ghi vào exception frame và đánh thức shell. */
    queue = hala_ipc_create(HALA_IPC_QUEUE, 0);
    if (queue < 0)
        return -16;
    int sender = hala_posix_spawn("api-send", HALA_ENTRY_IPC_SEND, ((unsigned)queue << 8) | 0xA5u);
    if (sender < 0)
        return -17;
    value = 0u;
    if ((hala_ipc_receive(queue, &value, 200u) != 1) || (value != 0xA5u))
        return -18;
    status = -1;
    if ((hala_waitpid(sender, &status) != sender) || (status != 0) || (hala_ipc_close(queue) != 0))
        return -19;

    int sem = hala_ipc_create(HALA_IPC_SEM, 1);
    if ((sem < 0) || (hala_sem_wait(sem, 0u) != 0) || (hala_sem_post(sem) != 0) ||
        (hala_ipc_close(sem) != 0))
        return -3;

    int mutex = hala_ipc_create(HALA_IPC_MUTEX, 0);
    if ((mutex < 0) || (hala_mutex_lock(mutex, 0u) != 0) || (hala_mutex_unlock(mutex) != 0) ||
        (hala_ipc_close(mutex) != 0))
        return -4;

    int event = hala_event_create(0);
    if ((event < 0) || (hala_event_set(event) != 1) || (hala_event_wait(event, 0u) != 1) ||
        (hala_ipc_close(event) != 0))
        return -22;

    /* Bước 3: FD/open-file object, dup2 và nhiều tmpfs file. */
    int fd = hala_open("/tmp/api", HALA_O_CREAT | HALA_O_TRUNC);
    if ((fd < 0) || (hala_write(fd, payload, sizeof(payload)) != (hala_ssize_t)sizeof(payload)))
        return -5;
    if (hala_dup2(fd, fd) != fd)
        return -6;
    int duplicate = hala_dup2(fd, 5);
    if ((duplicate != 5) || (hala_close(fd) != 0) || (hala_close(duplicate) != 0))
        return -6;

    if (hala_open("invalid", HALA_O_CREAT) != -22)
        return -7;
    int second = hala_open("/tmp/api2", HALA_O_CREAT | HALA_O_TRUNC);
    if ((second < 0) || (hala_write(second, payload, 1u) != 1) || (hala_close(second) != 0))
        return -7;

    fd = hala_open("/tmp/api", 0u);
    if ((fd < 0) || (hala_read(fd, readback, sizeof(readback)) != (hala_ssize_t)sizeof(readback)) ||
        (hala_close(fd) != 0) || (readback[0] != 'A') || (readback[1] != 'P') ||
        (readback[2] != 'I'))
        return -8;

    /* Bước 4: xác minh resolver mount và virtual node bằng lượt đọc hữu hạn. */
    unsigned char zeroes[4] = {1u, 1u, 1u, 1u};
    fd = hala_open("/dev/zero", 0u);
    if ((fd < 0) || (hala_read(fd, zeroes, sizeof(zeroes)) != (hala_ssize_t)sizeof(zeroes)) ||
        (hala_close(fd) != 0) || (zeroes[0] != 0u) || (zeroes[1] != 0u) || (zeroes[2] != 0u) ||
        (zeroes[3] != 0u))
        return -20;
    fd = hala_open("/proc/version", 0u);
    if ((fd < 0) || (hala_read(fd, readback, sizeof(readback)) != (hala_ssize_t)sizeof(readback)) ||
        (hala_close(fd) != 0) || (readback[0] != 'H'))
        return -21;

    /* Bước 5: process runnable và pthread dùng TCB động thật. */
    int runnable = hala_posix_spawn("api-run", HALA_ENTRY_COUNT, 2u);
    if (runnable < 0)
        return -9;
    /* Scheduler fair có thể đánh thức shell trước worker tùy virtual deadline.
     * Poll có sleep giữa các lần thử để xác minh lifecycle bất đồng bộ mà không
     * biến self-test thành busy-wait hoặc phụ thuộc một thời điểm cố định. */
    status = -1;
    int waited = -11;
    for (unsigned attempt = 0u; (attempt < 16u) && (waited == -11); ++attempt) {
        waited = hala_waitpid(runnable, &status);
        if (waited == -11)
            (void)hala_nanosleep(20u);
    }
    if ((waited != runnable) || (status != 0))
        return -11;

    hala_pthread_t thread = -1;
    if (hala_pthread_create(&thread, HALA_ENTRY_PRINT, 77u) != 0)
        return -12;
    status = -1;
    waited = -11;
    for (unsigned attempt = 0u; (attempt < 16u) && (waited == -11); ++attempt) {
        waited = hala_pthread_join(thread, &status);
        if (waited == -11)
            (void)hala_nanosleep(20u);
    }
    if ((waited != thread) || (status != 0))
        return -14;

    if ((hala_kill(pid, 7) != 0) || (hala_waitpid(pid, &status) != pid) || (status != 7))
        return -15;
    return 0;
}
#endif /* minimal lab qualification workload */
