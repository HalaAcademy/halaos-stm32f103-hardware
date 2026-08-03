/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    halaos_objects.c
 * @brief   Process, IPC, file descriptor và VFS object manager của HalaOS.
 * @details Module quản lý các object bằng fixed pool, không dùng heap. Process có PCB,
 *          PID, ownership và có thể liên kết với TCB động để chạy entry đã đăng ký.
 *          IPC hỗ trợ handle có generation, blocking waiter, timeout và wakeup. VFS
 *          dùng mount table, path resolver và open-file object dùng chung cho tmpfs,
 *          devfs, procfs và appfs.
 */
#include "halaos/internal/halaos_internal.h"

/* ==========================================================================
 *                            PRIVATE DEFINITIONS
 * ========================================================================== */
/* Resource profile được chọn tại compile time. Các lab không cần toàn bộ
 * object pool của full system, vì vậy giảm pool thật thay vì chỉ đổi nhãn build. */
#if defined(HALAOS_LAB_BOOT)
#define HOS_MAX_PROCESSES 2u
#define HOS_MAX_FILES 1u
#define HOS_MAX_OPEN_FILES 2u
#define HOS_MAX_FD 3u
#define HOS_MAX_IPC 1u
#elif defined(HALAOS_LAB_SCHEDULER)
#define HOS_MAX_PROCESSES 3u
#define HOS_MAX_FILES 1u
#define HOS_MAX_OPEN_FILES 3u
#define HOS_MAX_FD 4u
#define HOS_MAX_IPC 1u
#elif defined(HALAOS_LAB_PROCESS_IPC)
#define HOS_MAX_PROCESSES 8u
#define HOS_MAX_FILES 1u
#define HOS_MAX_OPEN_FILES 4u
#define HOS_MAX_FD 5u
#define HOS_MAX_IPC 4u
#elif defined(HALAOS_LAB_VFS_POSIX)
#define HOS_MAX_PROCESSES 6u
#define HOS_MAX_FILES 4u
#define HOS_MAX_OPEN_FILES 8u
#define HOS_MAX_FD 6u
#define HOS_MAX_IPC 3u
#elif defined(HALAOS_LAB_COMPILER_VM)
#define HOS_MAX_PROCESSES 4u
#define HOS_MAX_FILES 2u
#define HOS_MAX_OPEN_FILES 4u
#define HOS_MAX_FD 5u
#define HOS_MAX_IPC 2u
#elif defined(HALAOS_HARDWARE_CONSTRAINED)
#define HOS_MAX_PROCESSES 5u
#define HOS_MAX_FILES 2u
#define HOS_MAX_OPEN_FILES 5u
#define HOS_MAX_FD 5u
#define HOS_MAX_IPC 2u
#else
#define HOS_MAX_PROCESSES 8u
#define HOS_MAX_FILES 4u
#define HOS_MAX_OPEN_FILES 8u
#define HOS_MAX_FD 6u
#define HOS_MAX_IPC 4u
#endif
#define HOS_FILE_DATA 96u
#define HOS_IPC_DATA 128u
#define HOS_CONSOLE_CAT_LIMIT 256u
#define HOS_INVALID_TASK 0xFFu

#define HOS_O_CREAT 1u
#define HOS_O_TRUNC 2u

#define HOS_PROC_FREE 0u
#define HOS_PROC_READY 1u
#define HOS_PROC_STOPPED 2u
#define HOS_PROC_EXITED 3u

#define HOS_IPC_QUEUE 1u
#define HOS_IPC_PIPE 2u
#define HOS_IPC_SEM 3u
#define HOS_IPC_MUTEX 4u
#define HOS_IPC_EVENT 5u

#define HOS_WAIT_NONE 0u
#define HOS_WAIT_RECEIVE 1u
#define HOS_WAIT_SEMAPHORE 2u
#define HOS_WAIT_MUTEX 3u
#define HOS_WAIT_PENDING 0x7FFFFFFEu

#define HOS_NODE_TMP 1u
#define HOS_NODE_DEV_NULL 2u
#define HOS_NODE_DEV_ZERO 3u
#define HOS_NODE_DEV_CONSOLE 4u
#define HOS_NODE_DEV_UART1 5u
#define HOS_NODE_DEV_GPIO_PC13 6u
#define HOS_NODE_PROC_VERSION 7u
#define HOS_NODE_PROC_UPTIME 8u
#define HOS_NODE_PROC_MEMINFO 9u
#define HOS_NODE_PROC_BOOTINFO 10u
#define HOS_NODE_PROC_PROCESSES 11u
#define HOS_NODE_PROC_SCHEDSTAT 12u
#define HOS_NODE_PROC_INTERRUPTS 13u
#define HOS_NODE_PROC_IPC 14u
#define HOS_NODE_PROC_DTB 15u
#define HOS_NODE_APPS_ACTIVE 16u

#define HOS_FS_DEV 1u
#define HOS_FS_PROC 2u
#define HOS_FS_TMP 3u
#define HOS_FS_APPS 4u

#define HOS_ENTRY_PRINT 1u
#define HOS_ENTRY_COUNT 2u
#define HOS_ENTRY_SLEEP_EXIT 3u
#define HOS_ENTRY_IPC_SEND 4u

/* ==========================================================================
 *                              PRIVATE TYPES
 * ========================================================================== */
/** @brief PCB nhỏ gọn cho process giáo dục. */
typedef struct {
    u16 pid;
    u16 ppid;
    u16 generation;
    u8 state;
    u8 exit_status;
    u8 fd_count;
    u8 ipc_count;
    u8 task_slot;
    u8 entry;
    u8 wait_task;
    u8 reserved;
    u16 progress;
    u16 wait_caller;
    u32 argument;
    u32 *wait_frame;
    i32 *wait_status;
    char name[12];
} HalaProcess;

/** @brief IPC object có một waiter blocking xác định. */
typedef struct {
    u8 used;
    u8 type;
    u16 generation;
    u16 owner;
    u8 head;
    u8 tail;
    u8 count;
    u8 wait_task;
    u8 wait_kind;
    u8 reserved;
    u16 wait_pid;
    u8 data[HOS_IPC_DATA];
    i32 value;
    u16 lock_owner;
    u16 reserved2;
    u32 wait_deadline;
    u32 *wait_frame;
    u8 *wait_byte;
} HalaIpcObject;

/** @brief File dữ liệu thật của tmpfs. */
typedef struct {
    u8 used;
    u8 reserved;
    u16 size;
    char path[16];
    u8 data[HOS_FILE_DATA];
} HalaVfsFile;

/** @brief Open-file description được nhiều FD tham chiếu. */
typedef struct {
    u8 used;
    u8 kind;
    u8 node;
    u8 flags;
    u8 refs;
    u8 reserved;
    u16 offset;
    u16 owner;
} HalaOpenFile;

/** @brief Một mount point và filesystem ID dùng cho generic resolver. */
typedef struct {
    const char *path;
    const char *filesystem;
    u8 filesystem_id;
} HalaMount;

/* ==========================================================================
 *                         PRIVATE DATA DECLARATIONS
 * ========================================================================== */
static HalaProcess g_processes[HOS_MAX_PROCESSES];
static HalaIpcObject g_ipc_objects[HOS_MAX_IPC];
static HalaVfsFile g_vfs_files[HOS_MAX_FILES];
static HalaOpenFile g_open_files[HOS_MAX_OPEN_FILES];
static i32 g_fd_table[HOS_MAX_PROCESSES][HOS_MAX_FD];
static u16 g_next_pid = 3u;

static const HalaMount g_mounts[] = {{"/dev", "devfs", HOS_FS_DEV},
                                     {"/proc", "procfs", HOS_FS_PROC},
                                     {"/tmp", "tmpfs", HOS_FS_TMP},
                                     {"/apps", "appfs", HOS_FS_APPS}};

volatile u32 g_process_live, g_process_created, g_process_reaped, g_process_killed;
volatile u32 g_process_runs, g_process_exits, g_process_task_links;
volatile u32 g_generic_syscalls, g_ipc_objects_live, g_ipc_timeouts, g_ipc_wakeups;
volatile u32 g_vfs_file_count, g_vfs_open_count, g_vfs_resolves;
volatile u32 g_posix_api_pass, g_posix_api_fail;

/* ==========================================================================
 *                         PRIVATE HELPER FUNCTIONS
 * ========================================================================== */
static void copy_small(char *destination, const char *source, u32 capacity) {
    u32 index = 0u;
    if (capacity == 0u)
        return;
    while ((source != NULL) && (source[index] != '\0') && (index + 1u < capacity)) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static HalaProcess *process_by_pid(u16 pid) {
    for (u32 index = 0u; index < HOS_MAX_PROCESSES; ++index) {
        if ((g_processes[index].state != HOS_PROC_FREE) && (g_processes[index].pid == pid)) {
            return &g_processes[index];
        }
    }
    return NULL;
}

static HalaProcess *process_by_task(u8 task_slot) {
    for (u32 index = 2u; index < HOS_MAX_PROCESSES; ++index) {
        if ((g_processes[index].state != HOS_PROC_FREE) &&
            (g_processes[index].task_slot == task_slot))
            return &g_processes[index];
    }
    return NULL;
}

static u32 process_index(const HalaProcess *process) { return (u32)(process - g_processes); }

static int fd_slot(HalaProcess *process, i32 fd) {
    if ((process == NULL) || (fd < 0) || (fd >= (i32)HOS_MAX_FD))
        return -1;
    return g_fd_table[process_index(process)][fd];
}

static int path_component_valid(const char *text) {
    u32 length = str_len(text);
    if ((length == 0u) || (length >= 11u))
        return 0;
    for (u32 index = 0u; index < length; ++index) {
        char character = text[index];
        if (!(((character >= 'a') && (character <= 'z')) ||
              ((character >= 'A') && (character <= 'Z')) ||
              ((character >= '0') && (character <= '9')) || (character == '_') ||
              (character == '-') || (character == '.')))
            return 0;
    }
    return 1;
}

static int tmp_name_valid(const char *relative) {
    return (relative != NULL) && path_component_valid(relative);
}

static HalaProcess *process_allocate(u16 ppid, const char *name) {
    if ((name == NULL) || (name[0] == '\0') || (str_len(name) >= 12u))
        return NULL;
    if (process_by_pid(ppid) == NULL)
        return NULL;

    for (u32 index = 2u; index < HOS_MAX_PROCESSES; ++index) {
        HalaProcess *process = &g_processes[index];
        if (process->state != HOS_PROC_FREE)
            continue;

        u16 generation = (u16)(process->generation + 1u);
        if (generation == 0u)
            generation = 1u;
        mem_zero(process, sizeof(*process));
        process->generation = generation;
        process->pid = g_next_pid++;
        if (g_next_pid < 3u)
            g_next_pid = 3u;
        process->ppid = ppid;
        process->state = HOS_PROC_READY;
        process->task_slot = HOS_INVALID_TASK;
        process->wait_task = HOS_INVALID_TASK;
        copy_small(process->name, name, sizeof(process->name));
        for (u32 fd = 0u; fd < HOS_MAX_FD; ++fd)
            g_fd_table[index][fd] = -1;
        g_process_live++;
        g_process_created++;
        return process;
    }
    return NULL;
}

static void process_restore_task(u8 slot) {
    if ((slot != 11u) && (slot != 12u))
        return;
    g_tasks[slot].pid = (slot == 11u) ? 20u : 21u;
    g_tasks[slot].name = (slot == 11u) ? "app-worker-A" : "app-worker-B";
    g_tasks[slot].caps = CAP_UART;
    g_tasks[slot].state = TASK_BLOCKED;
}

static i32 process_claim_task(HalaProcess *process) {
    /* Hai TCB động được chia sẻ giữa application demo và process registry. Chỉ cấp
     * khi không có application demo đang chạy và slot thật sự đang BLOCKED. */
    if (g_app_running != 0u)
        return -16;
    for (u8 slot = 11u; slot <= 12u; ++slot) {
        if ((g_tasks[slot].state == TASK_BLOCKED) && (process_by_task(slot) == NULL)) {
            process->task_slot = slot;
            g_tasks[slot].pid = process->pid;
            g_tasks[slot].name = process->name;
            g_tasks[slot].caps = CAP_UART | CAP_PROCESS;
            g_process_task_links++;
            task_make_ready(slot);
            return slot;
        }
    }
    return -11;
}

static void process_release_task(HalaProcess *process) {
    if ((process != NULL) && (process->task_slot != HOS_INVALID_TASK)) {
        u8 slot = process->task_slot;
        process->task_slot = HOS_INVALID_TASK;
        process_restore_task(slot);
    }
}

static void ipc_clear_waiter(HalaIpcObject *object) {
    object->wait_task = HOS_INVALID_TASK;
    object->wait_kind = HOS_WAIT_NONE;
    object->wait_pid = 0u;
    object->wait_deadline = 0u;
    object->wait_frame = NULL;
    object->wait_byte = NULL;
}

static void ipc_wake_waiter(HalaIpcObject *object, i32 result, u8 value, int write_value) {
    u8 task_slot = object->wait_task;
    if (write_value && (object->wait_byte != NULL))
        *object->wait_byte = value;
    if (object->wait_frame != NULL)
        object->wait_frame[0] = (u32)result;
    ipc_clear_waiter(object);
    if (task_slot < MAX_TASKS) {
        task_make_ready(task_slot);
        g_ipc_wakeups++;
    }
}

static i32 ipc_handle(u32 index) {
    return (i32)(((u32)g_ipc_objects[index].generation << 8) | index);
}

static HalaIpcObject *ipc_from_handle(i32 handle) {
    u32 index = (u32)handle & 0xFFu;
    u32 generation = (u32)handle >> 8;
    if ((index >= HOS_MAX_IPC) || !g_ipc_objects[index].used ||
        (g_ipc_objects[index].generation != generation))
        return NULL;
    return &g_ipc_objects[index];
}

static HalaVfsFile *tmp_file_by_path(const char *path) {
    for (u32 index = 0u; index < HOS_MAX_FILES; ++index) {
        if (g_vfs_files[index].used && str_eq(g_vfs_files[index].path, path))
            return &g_vfs_files[index];
    }
    return NULL;
}

static i32 tmp_file_index(const HalaVfsFile *file) { return (i32)(file - g_vfs_files); }

static i32 allocate_fd(HalaProcess *process, i32 open_index) {
    u32 process_slot = process_index(process);
    for (u32 fd = 3u; fd < HOS_MAX_FD; ++fd) {
        if (g_fd_table[process_slot][fd] < 0) {
            g_fd_table[process_slot][fd] = open_index;
            process->fd_count++;
            return (i32)fd;
        }
    }
    return -24;
}

/**
 * @brief Chọn mount có prefix dài nhất và trả relative path trong filesystem.
 * @details Boundary sau mount phải là NUL hoặc `/`, vì vậy `/tmpx` không thể
 *          bị bind nhầm vào tmpfs. Resolver này là điểm vào chung cho open/list.
 */
static const HalaMount *vfs_resolve_mount(const char *path, const char **relative) {
    const HalaMount *best = NULL;
    u32 best_length = 0u;
    if ((path == NULL) || (path[0] != '/'))
        return NULL;
    for (u32 index = 0u; index < ARRAY_LEN(g_mounts); ++index) {
        const u32 length = str_len(g_mounts[index].path);
        if (!str_starts(path, g_mounts[index].path))
            continue;
        if ((path[length] != '\0') && (path[length] != '/'))
            continue;
        if (length > best_length) {
            best = &g_mounts[index];
            best_length = length;
        }
    }
    if (best == NULL)
        return NULL;
    const char *inside = path + best_length;
    if (*inside == '/')
        inside++;
    if (relative != NULL)
        *relative = inside;
    return best;
}

/** @brief Resolve node thuộc devfs/procfs/appfs bằng relative path. */
static int virtual_node(u8 filesystem_id, const char *relative, u8 *kind, u8 *node) {
    if ((filesystem_id == HOS_FS_DEV) && str_eq(relative, "null")) {
        *kind = HOS_NODE_DEV_NULL;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_DEV) && str_eq(relative, "zero")) {
        *kind = HOS_NODE_DEV_ZERO;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_DEV) && str_eq(relative, "console")) {
        *kind = HOS_NODE_DEV_CONSOLE;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_DEV) && str_eq(relative, "uart1")) {
        *kind = HOS_NODE_DEV_UART1;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_DEV) && str_eq(relative, "gpio/PC13")) {
        *kind = HOS_NODE_DEV_GPIO_PC13;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_PROC) && str_eq(relative, "version")) {
        *kind = HOS_NODE_PROC_VERSION;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_PROC) && str_eq(relative, "uptime")) {
        *kind = HOS_NODE_PROC_UPTIME;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_PROC) && str_eq(relative, "meminfo")) {
        *kind = HOS_NODE_PROC_MEMINFO;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_PROC) && str_eq(relative, "bootinfo")) {
        *kind = HOS_NODE_PROC_BOOTINFO;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_PROC) && str_eq(relative, "processes")) {
        *kind = HOS_NODE_PROC_PROCESSES;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_PROC) && str_eq(relative, "schedstat")) {
        *kind = HOS_NODE_PROC_SCHEDSTAT;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_PROC) && str_eq(relative, "interrupts")) {
        *kind = HOS_NODE_PROC_INTERRUPTS;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_PROC) && str_eq(relative, "ipc")) {
        *kind = HOS_NODE_PROC_IPC;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_PROC) && str_eq(relative, "dtb")) {
        *kind = HOS_NODE_PROC_DTB;
        *node = 0u;
        return 1;
    }
    if ((filesystem_id == HOS_FS_APPS) && str_eq(relative, "active")) {
        *kind = HOS_NODE_APPS_ACTIVE;
        *node = 0u;
        return 1;
    }
    return 0;
}

static u32 append_text(u8 *buffer, u32 capacity, u32 offset, const char *text) {
    while ((*text != '\0') && (offset < capacity))
        buffer[offset++] = (u8)*text++;
    return offset;
}

static u32 append_decimal(u8 *buffer, u32 capacity, u32 offset, u32 value) {
    char digits[10];
    u32 count = 0u;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while ((value != 0u) && (count < sizeof(digits)));
    while ((count > 0u) && (offset < capacity))
        buffer[offset++] = (u8)digits[--count];
    return offset;
}

static u32 virtual_content(u8 kind, u8 *buffer, u32 capacity) {
    u32 length = 0u;
    if (kind == HOS_NODE_PROC_VERSION)
        return append_text(buffer, capacity, 0u, "HalaOS Educational 0.5 Cortex-M3\n");
    if (kind == HOS_NODE_PROC_UPTIME) {
        length = append_decimal(buffer, capacity, length, g_ticks);
        return append_text(buffer, capacity, length, " ticks\n");
    }
    if (kind == HOS_NODE_PROC_MEMINFO)
        return append_text(buffer, capacity, 0u, "MemTotal: 20480\nAppStore: 8192\n");
    if (kind == HOS_NODE_DEV_GPIO_PC13)
        return append_text(buffer, capacity, 0u, ((GPIOC_ODR & (1u << 13)) != 0u) ? "1\n" : "0\n");
    if (kind == HOS_NODE_PROC_BOOTINFO) {
        length = append_text(buffer, capacity, 0u, "events=");
        length = append_decimal(buffer, capacity, length, g_boot_event_count);
        length = append_text(buffer, capacity, length, " userspace=");
        length = append_decimal(buffer, capacity, length, g_userspace_ready);
        return append_text(buffer, capacity, length, "\n");
    }
    if (kind == HOS_NODE_PROC_PROCESSES) {
        length = append_text(buffer, capacity, 0u, "live=");
        length = append_decimal(buffer, capacity, length, g_process_live);
        length = append_text(buffer, capacity, length, " created=");
        length = append_decimal(buffer, capacity, length, g_process_created);
        return append_text(buffer, capacity, length, "\n");
    }
    if (kind == HOS_NODE_PROC_SCHEDSTAT) {
        length = append_text(buffer, capacity, 0u, "ticks=");
        length = append_decimal(buffer, capacity, length, g_ticks);
        length = append_text(buffer, capacity, length, " switches=");
        length = append_decimal(buffer, capacity, length, g_switches);
        return append_text(buffer, capacity, length, "\n");
    }
    if (kind == HOS_NODE_PROC_INTERRUPTS) {
        length = append_text(buffer, capacity, 0u, "usart1=");
        length = append_decimal(buffer, capacity, length, g_uart_irq_count);
        return append_text(buffer, capacity, length, "\n");
    }
    if (kind == HOS_NODE_PROC_IPC) {
        length = append_text(buffer, capacity, 0u, "operations=");
        length = append_decimal(buffer, capacity, length, g_ipc_operations);
        length = append_text(buffer, capacity, length, " live=");
        length = append_decimal(buffer, capacity, length, g_ipc_objects_live);
        return append_text(buffer, capacity, length, "\n");
    }
    if (kind == HOS_NODE_PROC_DTB) {
        length = append_text(buffer, capacity, 0u, "valid=");
        length = append_decimal(buffer, capacity, length, g_dtb_valid);
        length = append_text(buffer, capacity, length, " drivers=");
        length = append_decimal(buffer, capacity, length, g_driver_count);
        return append_text(buffer, capacity, length, "\n");
    }
    if (kind == HOS_NODE_APPS_ACTIVE) {
        if (g_app_valid == 0u)
            return append_text(buffer, capacity, 0u, "(empty)\n");
        length = append_text(buffer, capacity, 0u, app_name());
        return append_text(buffer, capacity, length, ".happ\n");
    }
    return 0u;
}

static void ipc_release_owned(u16 pid) {
    for (u32 index = 0u; index < HOS_MAX_IPC; ++index) {
        HalaIpcObject *object = &g_ipc_objects[index];
        if (!object->used || (object->owner != pid))
            continue;
        if (object->wait_kind != HOS_WAIT_NONE)
            ipc_wake_waiter(object, -125, 0u, 0);
        u16 generation = object->generation;
        mem_zero(object, sizeof(*object));
        object->generation = generation;
        object->wait_task = HOS_INVALID_TASK;
        if (g_ipc_objects_live > 0u)
            g_ipc_objects_live--;
    }
}

static void fd_release_owned(HalaProcess *process) {
    u32 process_slot = process_index(process);
    for (u32 fd = 0u; fd < HOS_MAX_FD; ++fd) {
        i32 open_index = g_fd_table[process_slot][fd];
        if ((open_index >= 0) && (open_index < (i32)HOS_MAX_OPEN_FILES) &&
            g_open_files[open_index].used) {
            if (g_open_files[open_index].refs > 0u)
                g_open_files[open_index].refs--;
            if (g_open_files[open_index].refs == 0u) {
                mem_zero(&g_open_files[open_index], sizeof(g_open_files[open_index]));
                if (g_vfs_open_count > 0u)
                    g_vfs_open_count--;
            }
            g_fd_close_count++;
        }
        g_fd_table[process_slot][fd] = -1;
    }
    process->fd_count = 0u;
}

/**
 * @brief Thu hồi PCB zombie và đánh thức parent đang block trong waitpid.
 * @details Return value được ghi trực tiếp vào exception frame của parent. Parent
 *          chỉ được READY sau khi FD, IPC, TCB link và PCB đã được thu hồi, vì vậy
 *          khi SVC trả về thì lifecycle đã hoàn tất nhất quán.
 */
static void process_reap_and_wake(HalaProcess *process) {
    const u16 pid = process->pid;
    const u8 exit_status = process->exit_status;
    const u8 waiter = process->wait_task;
    u32 *frame = process->wait_frame;
    i32 *status = process->wait_status;

    fd_release_owned(process);
    ipc_release_owned(pid);
    process_release_task(process);
    if (status != NULL)
        *status = (i32)exit_status;
    if (frame != NULL)
        frame[0] = pid;

    u16 generation = process->generation;
    mem_zero(process, sizeof(*process));
    process->generation = generation;
    process->task_slot = HOS_INVALID_TASK;
    process->wait_task = HOS_INVALID_TASK;
    if (g_process_live > 0u)
        g_process_live--;
    g_process_reaped++;
    if (waiter < MAX_TASKS)
        task_make_ready(waiter);
}

/* ==========================================================================
 *                          PUBLIC PROCESS FUNCTIONS
 * ========================================================================== */
/** @brief Khởi tạo object manager, PID 1/PID 2 và mount table. */
void hala_objects_init(void) {
    mem_zero(g_processes, sizeof(g_processes));
    mem_zero(g_ipc_objects, sizeof(g_ipc_objects));
    mem_zero(g_vfs_files, sizeof(g_vfs_files));
    mem_zero(g_open_files, sizeof(g_open_files));
    for (u32 process = 0u; process < HOS_MAX_PROCESSES; ++process) {
        for (u32 fd = 0u; fd < HOS_MAX_FD; ++fd)
            g_fd_table[process][fd] = -1;
    }
    for (u32 index = 0u; index < HOS_MAX_IPC; ++index)
        g_ipc_objects[index].wait_task = HOS_INVALID_TASK;

    g_processes[0].pid = 1u;
    g_processes[0].generation = 1u;
    g_processes[0].state = HOS_PROC_READY;
    g_processes[0].task_slot = 10u;
    g_processes[0].wait_task = HOS_INVALID_TASK;
    copy_small(g_processes[0].name, "hala-init", sizeof(g_processes[0].name));
    g_processes[1].pid = 2u;
    g_processes[1].ppid = 1u;
    g_processes[1].generation = 1u;
    g_processes[1].state = HOS_PROC_READY;
    g_processes[1].task_slot = 1u;
    g_processes[1].wait_task = HOS_INVALID_TASK;
    copy_small(g_processes[1].name, "hala-shell", sizeof(g_processes[1].name));
    g_process_live = 2u;
    g_next_pid = 3u;
    g_vfs_mounts = ARRAY_LEN(g_mounts);
}

/** @brief Cấp một PCB chưa gắn executable; caller có thể quản lý lifecycle bằng kill/wait. */
i32 kobj_process_spawn(u16 ppid, const char *name) {
    HalaProcess *process = process_allocate(ppid, name);
    return process != NULL ? process->pid : -12;
}

/**
 * @brief Tạo process runnable và liên kết với một TCB động.
 * @param[in] ppid PID cha.
 * @param[in] name Tên process.
 * @param[in] entry Entry ID đã đăng ký, không phải function pointer tùy ý.
 * @param[in] argument Tham số cho entry.
 * @return PID dương khi thành công hoặc mã lỗi âm.
 */
i32 kobj_process_spawn_exec(u16 ppid, const char *name, u8 entry, u32 argument) {
    if ((entry < HOS_ENTRY_PRINT) || (entry > HOS_ENTRY_IPC_SEND))
        return -22;
    HalaProcess *process = process_allocate(ppid, name);
    if (process == NULL)
        return -12;
    process->entry = entry;
    process->argument = argument;
    i32 slot = process_claim_task(process);
    if (slot < 0) {
        u16 generation = process->generation;
        mem_zero(process, sizeof(*process));
        process->generation = generation;
        if (g_process_live > 0u)
            g_process_live--;
        return slot;
    }
    g_process_runs++;
    return process->pid;
}

/**
 * @brief Thực thi một bước entry của process đang gắn với task slot.
 * @details Task worker gọi hàm này rồi tự xuất log qua syscall. Hàm chỉ thao tác RAM,
 *          không truy cập peripheral từ Thread mode unprivileged.
 * @retval 0 Không có process runnable trên slot.
 * @retval 1 Entry PRINT cần in `value`.
 * @retval 2 Entry COUNT cần in số bước `value`.
 * @retval 3 Entry SLEEP cần ngủ `value` tick.
 * @retval -1 Process vừa hoàn thành và task cần block.
 */
i32 kobj_process_task_step(u8 task_slot, const char **name, u32 *value) {
    HalaProcess *process = process_by_task(task_slot);
    if ((process == NULL) || (process->state != HOS_PROC_READY))
        return 0;
    if (name != NULL)
        *name = process->name;

    if (process->entry == HOS_ENTRY_PRINT) {
        if (process->progress == 0u) {
            process->progress = 1u;
            if (value != NULL)
                *value = process->argument;
            return 1;
        }
    } else if (process->entry == HOS_ENTRY_COUNT) {
        u32 limit = process->argument;
        if (limit > 16u)
            limit = 16u;
        if (process->progress < limit) {
            process->progress++;
            if (value != NULL)
                *value = process->progress;
            return 2;
        }
    } else if (process->entry == HOS_ENTRY_SLEEP_EXIT) {
        if (process->progress == 0u) {
            process->progress = 1u;
            if (value != NULL)
                *value = process->argument;
            return 3;
        }
    } else if (process->entry == HOS_ENTRY_IPC_SEND) {
        if (process->progress == 0u) {
            process->progress = 1u;
            if (value != NULL)
                *value = process->argument;
            return 4;
        }
    }

    process->state = HOS_PROC_EXITED;
    process->exit_status = 0u;
    g_process_exits++;
    return -1;
}

/**
 * @brief Hoàn tất exit sau khi worker đã in event cuối cùng.
 * @details Nếu parent đang chờ, process được reap và parent được đánh thức. Nếu
 *          chưa có waiter, PCB được giữ ở trạng thái EXITED như zombie.
 */
void kobj_process_finalize_task_exit(u8 task_slot) {
    HalaProcess *process = process_by_task(task_slot);
    if ((process != NULL) && (process->state == HOS_PROC_EXITED) &&
        (process->wait_task != HOS_INVALID_TASK))
        process_reap_and_wake(process);
}

/** @brief Đánh dấu process hiện hành EXITED khi user gọi pthread_exit/SVC_EXIT. */
i32 kobj_process_exit_current(u16 pid, u8 status) {
    HalaProcess *process = process_by_pid(pid);
    if ((process == NULL) || (process->task_slot == HOS_INVALID_TASK) || (pid <= 2u))
        return 0;
    process->state = HOS_PROC_EXITED;
    process->exit_status = status;
    g_process_exits++;
    if (process->wait_task != HOS_INVALID_TASK)
        process_reap_and_wake(process);
    return 1;
}

/** @brief Kết thúc process, thu hồi FD/IPC và giữ zombie đến khi parent wait. */
i32 kobj_process_kill(u16 caller, u16 pid, u8 status) {
    HalaProcess *process = process_by_pid(pid);
    if ((process == NULL) || (pid <= 2u))
        return -3;
    if ((caller != process->ppid) && (caller != 1u) && (caller != 2u))
        return -13;
    fd_release_owned(process);
    ipc_release_owned(pid);
    process->ipc_count = 0u;
    process->state = HOS_PROC_EXITED;
    process->exit_status = status;
    if (process->task_slot != HOS_INVALID_TASK)
        g_tasks[process->task_slot].state = TASK_BLOCKED;
    g_process_killed++;
    g_process_exits++;
    return 0;
}

/** @brief Dừng hoặc tiếp tục process runnable bằng state PCB và TCB tương ứng. */
i32 kobj_process_stop(u16 caller, u16 pid, int resume) {
    HalaProcess *process = process_by_pid(pid);
    if ((process == NULL) || (pid <= 2u) || (process->task_slot == HOS_INVALID_TASK))
        return -3;
    if ((caller != process->ppid) && (caller != 1u) && (caller != 2u))
        return -13;
    if (resume) {
        if (process->state != HOS_PROC_STOPPED)
            return -22;
        process->state = HOS_PROC_READY;
        task_make_ready(process->task_slot);
    } else {
        if (process->state != HOS_PROC_READY)
            return -22;
        process->state = HOS_PROC_STOPPED;
        g_tasks[process->task_slot].state = TASK_STOPPED;
    }
    return 0;
}

/**
 * @brief Waitpid có blocking semantics qua exception frame của parent.
 * @details Nếu child chưa exit, kernel lưu frame/status pointer, chuyển parent sang
 *          BLOCKED và trả sentinel nội bộ. Child exit sẽ ghi PID/status vào frame,
 *          reap resource rồi đánh thức parent. Sentinel không lộ ra user-space.
 */
i32 kobj_process_wait_blocking(u16 caller, u16 pid, i32 *status, u32 *frame) {
    HalaProcess *process = process_by_pid(pid);
    if ((process == NULL) || (process->ppid != caller))
        return -10;
    if (process->state == HOS_PROC_EXITED) {
        if (status != NULL)
            *status = process->exit_status;
        fd_release_owned(process);
        ipc_release_owned(pid);
        process_release_task(process);
        u16 generation = process->generation;
        mem_zero(process, sizeof(*process));
        process->generation = generation;
        process->task_slot = HOS_INVALID_TASK;
        process->wait_task = HOS_INVALID_TASK;
        if (g_process_live > 0u)
            g_process_live--;
        g_process_reaped++;
        return pid;
    }
    if ((process->state != HOS_PROC_READY) && (process->state != HOS_PROC_STOPPED))
        return -11;
    if (process->wait_task != HOS_INVALID_TASK)
        return -16;

    process->wait_task = (u8)g_current_index;
    process->wait_caller = caller;
    process->wait_frame = frame;
    process->wait_status = status;
    block_current(TASK_BLOCKED, 0u);
    return (i32)HOS_WAIT_PENDING;
}

/** @brief Immediate wait helper cho internal code không có exception frame. */
i32 kobj_process_wait(u16 caller, u16 pid, i32 *status) {
    HalaProcess *process = process_by_pid(pid);
    if ((process == NULL) || (process->ppid != caller))
        return -10;
    if (process->state != HOS_PROC_EXITED)
        return -11;
    if (status != NULL)
        *status = process->exit_status;
    fd_release_owned(process);
    ipc_release_owned(pid);
    process_release_task(process);
    u16 generation = process->generation;
    mem_zero(process, sizeof(*process));
    process->generation = generation;
    process->task_slot = HOS_INVALID_TASK;
    process->wait_task = HOS_INVALID_TASK;
    if (g_process_live > 0u)
        g_process_live--;
    g_process_reaped++;
    return pid;
}

/** @brief In process table từ object manager thật. */
void kobj_process_console_list(void) {
    uart_puts_priv("PID PPID STATE FD IPC TASK NAME\r\n");
    for (u32 index = 0u; index < HOS_MAX_PROCESSES; ++index) {
        HalaProcess *process = &g_processes[index];
        if (process->state == HOS_PROC_FREE)
            continue;
        uart_dec_priv(process->pid);
        uart_putc_priv(' ');
        uart_dec_priv(process->ppid);
        uart_putc_priv(' ');
        uart_puts_priv(process->state == HOS_PROC_READY     ? "READY"
                       : process->state == HOS_PROC_STOPPED ? "STOPPED"
                                                            : "EXITED");
        uart_putc_priv(' ');
        uart_dec_priv(process->fd_count);
        uart_putc_priv(' ');
        uart_dec_priv(process->ipc_count);
        uart_putc_priv(' ');
        if (process->task_slot == HOS_INVALID_TASK)
            uart_putc_priv('-');
        else
            uart_dec_priv(process->task_slot);
        uart_putc_priv(' ');
        uart_puts_priv(process->name);
        uart_puts_priv("\r\n");
    }
}

/* ==========================================================================
 *                            PUBLIC IPC FUNCTIONS
 * ========================================================================== */
i32 kobj_ipc_create(u16 pid, u8 type, i32 initial) {
    HalaProcess *process = process_by_pid(pid);
    if ((process == NULL) || (type < HOS_IPC_QUEUE) || (type > HOS_IPC_EVENT))
        return -22;
    for (u32 index = 0u; index < HOS_MAX_IPC; ++index) {
        HalaIpcObject *object = &g_ipc_objects[index];
        if (object->used)
            continue;
        u16 generation = (u16)(object->generation + 1u);
        if (generation == 0u)
            generation = 1u;
        mem_zero(object, sizeof(*object));
        object->used = 1u;
        object->type = type;
        object->generation = generation;
        object->owner = pid;
        object->value = initial;
        object->wait_task = HOS_INVALID_TASK;
        process->ipc_count++;
        g_ipc_objects_live++;
        return ipc_handle(index);
    }
    return -12;
}

i32 kobj_ipc_close(u16 pid, i32 handle) {
    HalaIpcObject *object = ipc_from_handle(handle);
    if (object == NULL)
        return -9;
    if (object->owner != pid)
        return -13;
    if (object->wait_kind != HOS_WAIT_NONE)
        ipc_wake_waiter(object, -125, 0u, 0);
    HalaProcess *process = process_by_pid(pid);
    if ((process != NULL) && (process->ipc_count > 0u))
        process->ipc_count--;
    u16 generation = object->generation;
    mem_zero(object, sizeof(*object));
    object->generation = generation;
    object->wait_task = HOS_INVALID_TASK;
    if (g_ipc_objects_live > 0u)
        g_ipc_objects_live--;
    return 0;
}

i32 kobj_ipc_send(u16 pid, i32 handle, u8 value) {
    (void)pid;
    HalaIpcObject *object = ipc_from_handle(handle);
    if (object == NULL)
        return -9;
    if (object->type == HOS_IPC_EVENT) {
        if (object->wait_kind == HOS_WAIT_RECEIVE)
            ipc_wake_waiter(object, 1, 1u, 1);
        else
            object->value = 1;
        g_ipc_operations++;
        return 1;
    }
    if ((object->type != HOS_IPC_QUEUE) && (object->type != HOS_IPC_PIPE))
        return -9;
    if (object->wait_kind == HOS_WAIT_RECEIVE) {
        ipc_wake_waiter(object, 1, value, 1);
        g_ipc_operations++;
        return 1;
    }
    if (object->count >= HOS_IPC_DATA)
        return -11;
    object->data[object->tail] = value;
    object->tail = (u8)((object->tail + 1u) % HOS_IPC_DATA);
    object->count++;
    g_ipc_operations++;
    return 1;
}

i32 kobj_ipc_receive(u16 pid, i32 handle, u8 *output, u32 timeout) {
    (void)pid;
    HalaIpcObject *object = ipc_from_handle(handle);
    if ((object == NULL) || (output == NULL))
        return -9;
    if (object->type == HOS_IPC_EVENT) {
        if (object->value <= 0) {
            if (timeout != 0u)
                g_ipc_timeouts++;
            return timeout != 0u ? -110 : -11;
        }
        object->value = 0;
        *output = 1u;
        g_ipc_operations++;
        return 1;
    }
    if ((object->type != HOS_IPC_QUEUE) && (object->type != HOS_IPC_PIPE))
        return -9;
    if (object->count == 0u) {
        if (timeout != 0u)
            g_ipc_timeouts++;
        return timeout != 0u ? -110 : -11;
    }
    *output = object->data[object->head];
    object->head = (u8)((object->head + 1u) % HOS_IPC_DATA);
    object->count--;
    g_ipc_operations++;
    return 1;
}

/** @brief Phiên bản syscall có thể block task đến khi có byte hoặc timeout. */
i32 kobj_ipc_receive_wait(u16 pid, i32 handle, u8 *output, u32 timeout, u32 *frame) {
    i32 immediate = kobj_ipc_receive(pid, handle, output, 0u);
    if ((immediate != -11) || (timeout == 0u))
        return immediate;
    HalaIpcObject *object = ipc_from_handle(handle);
    if ((object == NULL) || (object->wait_kind != HOS_WAIT_NONE))
        return -16;
    object->wait_task = (u8)g_current_index;
    object->wait_kind = HOS_WAIT_RECEIVE;
    object->wait_pid = pid;
    object->wait_deadline = g_ticks + timeout;
    object->wait_frame = frame;
    object->wait_byte = output;
    g_wait_ops++;
    block_current(TASK_BLOCKED, 0u);
    return (i32)HOS_WAIT_PENDING;
}

i32 kobj_sem_wait(u16 pid, i32 handle, u32 timeout) {
    (void)pid;
    HalaIpcObject *object = ipc_from_handle(handle);
    if ((object == NULL) || (object->type != HOS_IPC_SEM))
        return -9;
    if (object->value <= 0) {
        if (timeout != 0u)
            g_ipc_timeouts++;
        return timeout != 0u ? -110 : -11;
    }
    object->value--;
    return 0;
}

i32 kobj_sem_wait_blocking(u16 pid, i32 handle, u32 timeout, u32 *frame) {
    i32 immediate = kobj_sem_wait(pid, handle, 0u);
    if ((immediate != -11) || (timeout == 0u))
        return immediate;
    HalaIpcObject *object = ipc_from_handle(handle);
    if ((object == NULL) || (object->wait_kind != HOS_WAIT_NONE))
        return -16;
    object->wait_task = (u8)g_current_index;
    object->wait_kind = HOS_WAIT_SEMAPHORE;
    object->wait_pid = pid;
    object->wait_deadline = g_ticks + timeout;
    object->wait_frame = frame;
    g_wait_ops++;
    block_current(TASK_BLOCKED, 0u);
    return (i32)HOS_WAIT_PENDING;
}

i32 kobj_sem_post(u16 pid, i32 handle) {
    (void)pid;
    HalaIpcObject *object = ipc_from_handle(handle);
    if ((object == NULL) || (object->type != HOS_IPC_SEM))
        return -9;
    if (object->wait_kind == HOS_WAIT_SEMAPHORE)
        ipc_wake_waiter(object, 0, 0u, 0);
    else
        object->value++;
    return 0;
}

i32 kobj_mutex_lock(u16 pid, i32 handle, u32 timeout) {
    HalaIpcObject *object = ipc_from_handle(handle);
    if ((object == NULL) || (object->type != HOS_IPC_MUTEX))
        return -9;
    if ((object->lock_owner != 0u) && (object->lock_owner != pid)) {
        if (timeout != 0u)
            g_ipc_timeouts++;
        return timeout != 0u ? -110 : -16;
    }
    object->lock_owner = pid;
    return 0;
}

i32 kobj_mutex_lock_blocking(u16 pid, i32 handle, u32 timeout, u32 *frame) {
    i32 immediate = kobj_mutex_lock(pid, handle, 0u);
    if ((immediate != -16) || (timeout == 0u))
        return immediate;
    HalaIpcObject *object = ipc_from_handle(handle);
    if ((object == NULL) || (object->wait_kind != HOS_WAIT_NONE))
        return -16;
    object->wait_task = (u8)g_current_index;
    object->wait_kind = HOS_WAIT_MUTEX;
    object->wait_pid = pid;
    object->wait_deadline = g_ticks + timeout;
    object->wait_frame = frame;
    g_wait_ops++;
    block_current(TASK_BLOCKED, 0u);
    return (i32)HOS_WAIT_PENDING;
}

i32 kobj_mutex_unlock(u16 pid, i32 handle) {
    HalaIpcObject *object = ipc_from_handle(handle);
    if ((object == NULL) || (object->type != HOS_IPC_MUTEX))
        return -9;
    if (object->lock_owner != pid)
        return -13;
    if (object->wait_kind == HOS_WAIT_MUTEX) {
        object->lock_owner = object->wait_pid;
        ipc_wake_waiter(object, 0, 0u, 0);
    } else
        object->lock_owner = 0u;
    return 0;
}

/** @brief Được gọi mỗi SysTick để hoàn tất waiter đã hết timeout. */
void kobj_ipc_tick(u32 now) {
    for (u32 index = 0u; index < HOS_MAX_IPC; ++index) {
        HalaIpcObject *object = &g_ipc_objects[index];
        if (!object->used || (object->wait_kind == HOS_WAIT_NONE))
            continue;
        if ((i32)(now - object->wait_deadline) >= 0) {
            g_ipc_timeouts++;
            ipc_wake_waiter(object, -110, 0u, 0);
        }
    }
}

/* ==========================================================================
 *                             PUBLIC VFS FUNCTIONS
 * ========================================================================== */
i32 kobj_vfs_open(u16 pid, const char *path, u32 flags) {
    HalaProcess *process = process_by_pid(pid);
    if ((process == NULL) || (path == NULL) || (path[0] != '/'))
        return -22;

    u8 kind = 0u, node = 0u;
    HalaVfsFile *tmp = NULL;
    const char *relative = NULL;
    const HalaMount *mount = vfs_resolve_mount(path, &relative);
    if (mount == NULL)
        return -2;

    if (mount->filesystem_id == HOS_FS_TMP) {
        if (!tmp_name_valid(relative))
            return -22;
        kind = HOS_NODE_TMP;
        tmp = tmp_file_by_path(path);
        if ((tmp == NULL) && ((flags & HOS_O_CREAT) == 0u))
            return -2;
        if (tmp == NULL) {
            for (u32 index = 0u; index < HOS_MAX_FILES; ++index) {
                if (!g_vfs_files[index].used) {
                    tmp = &g_vfs_files[index];
                    mem_zero(tmp, sizeof(*tmp));
                    tmp->used = 1u;
                    copy_small(tmp->path, path, sizeof(tmp->path));
                    g_vfs_file_count++;
                    break;
                }
            }
            if (tmp == NULL)
                return -28;
        }
        if ((flags & HOS_O_TRUNC) != 0u)
            tmp->size = 0u;
        node = (u8)tmp_file_index(tmp);
    } else if (!virtual_node(mount->filesystem_id, relative, &kind, &node))
        return -2;

    g_vfs_resolves++;
    for (u32 index = 0u; index < HOS_MAX_OPEN_FILES; ++index) {
        HalaOpenFile *open = &g_open_files[index];
        if (open->used)
            continue;
        mem_zero(open, sizeof(*open));
        open->used = 1u;
        open->kind = kind;
        open->node = node;
        open->flags = (u8)flags;
        open->refs = 1u;
        open->owner = pid;
        i32 fd = allocate_fd(process, (i32)index);
        if (fd < 0) {
            mem_zero(open, sizeof(*open));
            return fd;
        }
        g_vfs_open_count++;
        g_fd_open_count++;
        return fd;
    }
    return -24;
}

i32 kobj_vfs_close(u16 pid, i32 fd) {
    HalaProcess *process = process_by_pid(pid);
    i32 open_index = fd_slot(process, fd);
    if ((open_index < 0) || (open_index >= (i32)HOS_MAX_OPEN_FILES))
        return -9;
    HalaOpenFile *open = &g_open_files[open_index];
    if (!open->used)
        return -9;
    g_fd_table[process_index(process)][fd] = -1;
    if (process->fd_count > 0u)
        process->fd_count--;
    if (open->refs > 0u)
        open->refs--;
    if (open->refs == 0u) {
        mem_zero(open, sizeof(*open));
        if (g_vfs_open_count > 0u)
            g_vfs_open_count--;
    }
    g_fd_close_count++;
    return 0;
}

i32 kobj_vfs_write(u16 pid, i32 fd, const u8 *data, u32 length) {
    HalaProcess *process = process_by_pid(pid);
    i32 open_index = fd_slot(process, fd);
    if ((open_index < 0) || (data == NULL))
        return -9;
    HalaOpenFile *open = &g_open_files[open_index];
    if (open->kind == HOS_NODE_DEV_NULL)
        return (i32)length;
    if ((open->kind == HOS_NODE_DEV_CONSOLE) || (open->kind == HOS_NODE_DEV_UART1)) {
        for (u32 index = 0u; index < length; ++index)
            uart_putc_priv((char)data[index]);
        return (i32)length;
    }
    if (open->kind == HOS_NODE_DEV_GPIO_PC13) {
        if (length == 0u)
            return 0;
        if (data[0] == (u8)'1')
            GPIOC_BSRR = (1u << 13);
        else if (data[0] == (u8)'0')
            GPIOC_BRR = (1u << 13);
        else
            return -22;
        return (i32)length;
    }
    if (open->kind != HOS_NODE_TMP)
        return -30;
    HalaVfsFile *file = &g_vfs_files[open->node];
    u32 room = HOS_FILE_DATA - open->offset;
    if (length > room)
        length = room;
    mem_copy(file->data + open->offset, data, length);
    open->offset = (u16)(open->offset + length);
    if (open->offset > file->size)
        file->size = open->offset;
    return (i32)length;
}

i32 kobj_vfs_read(u16 pid, i32 fd, u8 *data, u32 length) {
    HalaProcess *process = process_by_pid(pid);
    i32 open_index = fd_slot(process, fd);
    if ((open_index < 0) || (data == NULL))
        return -9;
    HalaOpenFile *open = &g_open_files[open_index];

    if (open->kind == HOS_NODE_DEV_NULL)
        return 0;
    if (open->kind == HOS_NODE_DEV_ZERO) {
        mem_zero(data, length);
        open->offset = (u16)(open->offset + length);
        return (i32)length;
    }
    if (open->kind == HOS_NODE_TMP) {
        HalaVfsFile *file = &g_vfs_files[open->node];
        u32 left = file->size > open->offset ? file->size - open->offset : 0u;
        if (length > left)
            length = left;
        mem_copy(data, file->data + open->offset, length);
        open->offset = (u16)(open->offset + length);
        return (i32)length;
    }

    u8 content[96];
    u32 content_length = virtual_content(open->kind, content, sizeof(content));
    u32 left = content_length > open->offset ? content_length - open->offset : 0u;
    if (length > left)
        length = left;
    mem_copy(data, content + open->offset, length);
    open->offset = (u16)(open->offset + length);
    return (i32)length;
}

i32 kobj_vfs_dup2(u16 pid, i32 oldfd, i32 newfd) {
    HalaProcess *process = process_by_pid(pid);
    i32 open_index = fd_slot(process, oldfd);
    if ((open_index < 0) || (newfd < 0) || (newfd >= (i32)HOS_MAX_FD))
        return -9;
    if (oldfd == newfd)
        return newfd;
    u32 process_slot = process_index(process);
    if (g_fd_table[process_slot][newfd] >= 0) {
        i32 result = kobj_vfs_close(pid, newfd);
        if (result != 0)
            return result;
    }
    g_fd_table[process_slot][newfd] = open_index;
    g_open_files[open_index].refs++;
    process->fd_count++;
    g_fd_open_count++;
    return newfd;
}

/** @brief Liệt kê mount hoặc directory bằng VFS registry thật. */
void kobj_vfs_console_list(const char *path) {
    if ((path == NULL) || str_eq(path, "/")) {
        for (u32 index = 0u; index < ARRAY_LEN(g_mounts); ++index) {
            uart_puts_priv(g_mounts[index].path + 1);
            uart_putc_priv(' ');
        }
        return;
    }
    if (str_eq(path, "/tmp")) {
        for (u32 index = 0u; index < HOS_MAX_FILES; ++index)
            if (g_vfs_files[index].used) {
                uart_puts_priv(g_vfs_files[index].path + 5);
                uart_putc_priv(' ');
            }
    } else if (str_eq(path, "/dev"))
        uart_puts_priv("console uart1 null zero gpio/PC13 ");
    else if (str_eq(path, "/proc"))
        uart_puts_priv("version uptime meminfo bootinfo processes schedstat interrupts ipc dtb ");
    else if (str_eq(path, "/apps"))
        uart_puts_priv("active ");
}

/** @brief In mount table thật. */
void kobj_vfs_console_mounts(void) {
    for (u32 index = 0u; index < ARRAY_LEN(g_mounts); ++index) {
        uart_puts_priv(g_mounts[index].filesystem);
        uart_putc_priv(' ');
        uart_puts_priv(g_mounts[index].path);
        uart_puts_priv("\r\n");
    }
}

i32 kobj_vfs_console_cat(const char *path) {
    i32 fd = kobj_vfs_open(2u, path, 0u);
    if (fd < 0)
        return fd;
    u8 buffer[24];
    i32 total = 0;
    int truncated = 0;
    for (;;) {
        u32 request = sizeof(buffer);
        if ((u32)total + request > HOS_CONSOLE_CAT_LIMIT)
            request = HOS_CONSOLE_CAT_LIMIT - (u32)total;
        if (request == 0u) {
            truncated = 1;
            break;
        }
        i32 count = kobj_vfs_read(2u, fd, buffer, request);
        if (count <= 0)
            break;
        for (i32 index = 0; index < count; ++index)
            uart_putc_priv((char)buffer[index]);
        total += count;
    }
    (void)kobj_vfs_close(2u, fd);
    if (truncated)
        uart_puts_priv("\r\n[stream truncated at 256 bytes]\r\n");
    return total;
}

/* ==========================================================================
 *                         QUALIFICATION SELF-TEST
 * ========================================================================== */
/** @brief Qualification object/API dùng implementation thật, không tăng counter giả. */
i32 kobj_selftest(void) {
    const u16 shell = 2u;
    g_posix_api_pass = 0u;
    g_posix_api_fail = 0u;
    i32 pid = kobj_process_spawn(shell, "qa-child");
    if (pid < 0)
        goto fail;

    i32 queue = kobj_ipc_create((u16)pid, HOS_IPC_QUEUE, 0);
    if ((queue < 0) || (kobj_ipc_send((u16)pid, queue, 0x5Au) != 1))
        goto fail;
    u8 value = 0u;
    if ((kobj_ipc_receive((u16)pid, queue, &value, 0u) != 1) || (value != 0x5Au))
        goto fail;
    if (kobj_ipc_receive((u16)pid, queue, &value, 5u) != -110)
        goto fail;
    if (kobj_ipc_close((u16)pid, queue) != 0)
        goto fail;

    i32 fd = kobj_vfs_open((u16)pid, "/tmp/qa", HOS_O_CREAT | HOS_O_TRUNC);
    const u8 message[3] = {'O', 'K', '!'};
    if ((fd < 0) || (kobj_vfs_write((u16)pid, fd, message, 3u) != 3) ||
        (kobj_vfs_close((u16)pid, fd) != 0))
        goto fail;

    if (kobj_process_kill(shell, (u16)pid, 7u) != 0)
        goto fail;
    i32 status = -1;
    if ((kobj_process_wait(shell, (u16)pid, &status) != pid) || (status != 7))
        goto fail;
    g_posix_api_pass = 1u;
    return 0;

fail:
    g_posix_api_fail++;
    return -1;
}
