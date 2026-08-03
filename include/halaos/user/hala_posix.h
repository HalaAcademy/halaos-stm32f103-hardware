/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    hala_posix.h
 * @brief   API user-space POSIX-inspired được HalaOS hỗ trợ chính thức.
 * @details Đây là educational subset, không tuyên bố POSIX compliant. Mọi API
 *          chuyển qua SVC và không cho user code truy cập trực tiếp kernel object.
 */
#ifndef HALA_POSIX_H
#define HALA_POSIX_H

typedef signed int hala_ssize_t;
typedef unsigned int hala_size_t;
typedef int hala_pthread_t;

#define HALA_O_CREAT 1u
#define HALA_O_TRUNC 2u
#define HALA_IPC_QUEUE 1u
#define HALA_IPC_PIPE 2u
#define HALA_IPC_SEM 3u
#define HALA_IPC_MUTEX 4u
#define HALA_IPC_EVENT 5u

/** @brief Entry registry hợp lệ cho posix_spawn/pthread_create giáo dục. */
#define HALA_ENTRY_PRINT 1u
#define HALA_ENTRY_COUNT 2u
#define HALA_ENTRY_SLEEP_EXIT 3u
#define HALA_ENTRY_IPC_SEND 4u

int hala_open(const char *path, unsigned flags);
int hala_close(int fd);
hala_ssize_t hala_read(int fd, void *buffer, hala_size_t length);
hala_ssize_t hala_write(int fd, const void *buffer, hala_size_t length);
int hala_dup2(int oldfd, int newfd);

/** @brief Cấp PCB nhưng chưa gắn executable; phù hợp test lifecycle/ownership. */
int hala_spawn(const char *name);
/** @brief Tạo process runnable bằng entry ID đã đăng ký và argument. */
int hala_posix_spawn(const char *name, unsigned entry, unsigned argument);
int hala_waitpid(int pid, int *status);
int hala_kill(int pid, int status);

/** @brief Ngủ theo kernel tick; task thật sự rời ready queue. */
int hala_nanosleep(unsigned ticks);

/** @brief Tạo educational pthread bằng process/TCB động. */
int hala_pthread_create(hala_pthread_t *thread, unsigned entry, unsigned argument);
int hala_pthread_join(hala_pthread_t thread, int *status);
void hala_pthread_exit(int status);

int hala_ipc_create(unsigned type, int initial);
int hala_ipc_close(int handle);
int hala_ipc_send(int handle, unsigned char value);
int hala_ipc_receive(int handle, unsigned char *value, unsigned timeout_ticks);
int hala_sem_wait(int handle, unsigned timeout_ticks);
int hala_sem_post(int handle);
int hala_mutex_lock(int handle, unsigned timeout_ticks);
int hala_mutex_unlock(int handle);
int hala_event_create(int initial_state);
int hala_event_wait(int handle, unsigned timeout_ticks);
int hala_event_set(int handle);
int hala_vfs_list(const char *path);
int hala_user_api_selftest(void);

#endif
