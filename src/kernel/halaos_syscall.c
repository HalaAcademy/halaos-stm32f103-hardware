/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file halaos_syscall.c
 * @brief SVC dispatcher và user/kernel syscall wrappers.
 * @details File được tách từ baseline đã qualification; comment tiếng Việt được giữ để phục vụ
 * review và đào tạo.
 */
#include "halaos/internal/halaos_internal.h"

/**
 * @brief Kiểm tra chuỗi NUL-terminated nằm hoàn toàn trong vùng user hợp lệ.
 * @details Kernel không dùng str_len() trực tiếp trên con trỏ user trước khi xác minh bounds,
 *          tránh đọc vượt vùng SRAM/Flash khi application truyền chuỗi lỗi.
 */
static int user_cstr_valid(const char *text, u32 length) {
    if ((text == NULL) || (length == 0u) || (length > 32u) || !ptr_readable(text, length)) {
        return 0;
    }

    for (u32 i = 0u; i < length; ++i) {
        if (text[i] == '\0') {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Giải mã số syscall, kiểm tra quyền/con trỏ và thực thi dịch vụ kernel.
 * @details Mọi API process, IPC và VFS user-space đều đi qua dispatcher này. Kernel xác minh
 *          capability, ownership và bounds trước khi gọi object manager đặc quyền.
 */
__attribute__((used, noinline)) void hala_svc_dispatch(u32 *frame) {
    const u8 svc = ((const u8 *)(usize)frame[6])[-2];
    const u16 caller_pid = g_current != NULL ? g_current->pid : 0u;
    i32 ret = 0;

    switch (svc) {
    case SVC_PUTC:
        if (!current_has(CAP_UART)) {
            g_capability_denials++;
            ret = -13;
        } else {
            uart_putc_priv((char)frame[0]);
        }
        break;
    case SVC_SLEEP:
        block_current(TASK_SLEEP, frame[0]);
        break;
    case SVC_YIELD:
        g_current->voluntary++;
        pend_resched();
        break;
    case SVC_READ:
        if (!current_has(CAP_UART)) {
            g_capability_denials++;
            ret = -13;
        } else {
            ret = rx_pop();
            if (ret < 0)
                block_current(TASK_BLOCKED, 0);
        }
        break;
    case SVC_GPIO:
        if (!current_has(CAP_GPIO)) {
            g_capability_denials++;
            ret = -13;
        } else {
            led_toggle_priv();
        }
        break;
    case SVC_UPTIME:
        ret = (i32)g_ticks;
        break;
    case SVC_QUEUE_COMPILE:
        if (!current_has(CAP_COMPILER)) {
            g_capability_denials++;
            ret = -13;
        } else if (!ptr_in_sram((void *)(usize)frame[0], frame[1])) {
            g_pointer_denials++;
            ret = -14;
        } else if (frame[1] >= sizeof(g_compile_source)) {
            ret = -21;
        } else if (g_compiler_job) {
            ret = -16;
        } else {
            mem_copy(g_compile_source, (void *)(usize)frame[0], frame[1]);
            g_compile_source_len = (u16)frame[1];
            g_compile_source[frame[1]] = 0;
            g_compiler_job = 1;
            task_make_ready(2);
            ret = 1;
        }
        break;
    case SVC_COMPILE_EXEC:
        if ((task_index_from_ptr(g_current) != 2u) || !current_has(CAP_COMPILER)) {
            g_capability_denials++;
            ret = -13;
        } else {
            ret = compile_hala_c((const char *)g_compile_source, g_compile_source_len);
        }
        break;
    case SVC_APP_RUN:
        if (!current_has(CAP_APP)) {
            g_capability_denials++;
            ret = -13;
        } else if (!g_app_valid || !app_verify_slot(g_app_slot)) {
            ret = -2;
        } else {
            demo_start(app_header()->flags);
        }
        break;
    case SVC_APP_STOP:
        demo_stop();
        break;
    case SVC_REBOOT:
        reboot_priv();
        break;
    case SVC_EXIT:
        /* Process runnable dùng PCB/TCB link: lưu exit status trong PCB và block task.
         * Task hệ thống/demo không có PCB động vẫn theo đường ZOMBIE cũ. */
        if (kobj_process_exit_current(caller_pid, (u8)frame[0]) != 0) {
            block_current(TASK_BLOCKED, 0u);
        } else {
            g_current->state = TASK_ZOMBIE;
            if (is_rt_policy(g_current->policy))
                rt_remove((u8)g_current_index);
            pend_resched();
        }
        break;
    case SVC_LOAD:
        if (!current_has(CAP_SCHED_ADMIN)) {
            g_capability_denials++;
            ret = -13;
        } else {
            load_enable(frame[0]);
        }
        break;
    case SVC_PROC_STRESS:
        if (!current_has(CAP_PROCESS)) {
            g_capability_denials++;
            ret = -13;
        } else {
            ret = proc_stress(frame[0]);
        }
        break;
    case SVC_PI_TEST:
        if (!current_has(CAP_SCHED_ADMIN)) {
            g_capability_denials++;
            ret = -13;
        } else {
            wait_queue_model_test();
            ret = pi_selftest() ? 0 : -1;
        }
        break;
    case SVC_IDLE:
        enter_idle_tickless();
        break;
    case SVC_FAULT_TEST:
        if (!current_has(CAP_SCHED_ADMIN)) {
            g_capability_denials++;
            ret = -13;
        } else {
            g_fault_inject = 1;
            g_tasks[8].flags |= TF_LOAD_ENABLED;
            task_make_ready(8);
        }
        break;
    case SVC_IPC_TEST:
        ret = ipc_posix_selftest();
        break;
    case SVC_POSIX_TEST:
        ret = kobj_selftest();
        break;
    case SVC_WRITE:
        if (!current_has(CAP_UART)) {
            g_capability_denials++;
            ret = -13;
        } else if (!ptr_readable((const void *)(usize)frame[0], frame[1])) {
            g_pointer_denials++;
            ret = -14;
        } else {
            const char *p = (const char *)(usize)frame[0];
            for (u32 i = 0u; i < frame[1]; ++i)
                uart_putc_priv(p[i]);
            g_posix_calls++;
            ret = (i32)frame[1];
        }
        break;
    case SVC_CONSOLE_LOCK:
        if ((g_console_owner == INVALID_IDX) || (g_console_owner == (u8)g_current_index)) {
            g_console_owner = (u8)g_current_index;
        } else {
            ret = -16;
        }
        break;
    case SVC_CONSOLE_UNLOCK:
        if (g_console_owner == (u8)g_current_index)
            g_console_owner = INVALID_IDX;
        else
            ret = -1;
        break;
    case SVC_DTB_LIST:
        hala_dtb_console_list();
        break;
    case SVC_DTB_GET:
        if (!ptr_in_sram((void *)(usize)frame[0], frame[1]) || frame[1] < 4u) {
            g_pointer_denials++;
            ret = -14;
        } else {
            char *p = (char *)(usize)frame[0];
            u32 first = 0u;
            while ((first < frame[1]) && (p[first] != '\0'))
                first++;
            if ((first + 2u > frame[1]) || !user_cstr_valid(p + first + 1u, frame[1] - first - 1u))
                ret = -22;
            else
                ret = hala_dtb_console_get(p, p + first + 1u);
        }
        break;
    case SVC_DEVICE_LIST:
        hala_driver_console_list();
        break;

    case SVC_OPEN:
        if (!current_has(CAP_PROCESS)) {
            g_capability_denials++;
            ret = -13;
        } else if (!user_cstr_valid((const char *)(usize)frame[0], frame[1])) {
            g_pointer_denials++;
            ret = -14;
        } else {
            ret = kobj_vfs_open(caller_pid, (const char *)(usize)frame[0], frame[2]);
            g_generic_syscalls++;
        }
        break;
    case SVC_CLOSE:
        ret = kobj_vfs_close(caller_pid, (i32)frame[0]);
        g_generic_syscalls++;
        break;
    case SVC_FD_READ:
        if (!ptr_in_sram((void *)(usize)frame[1], frame[2])) {
            g_pointer_denials++;
            ret = -14;
        } else {
            ret = kobj_vfs_read(caller_pid, (i32)frame[0], (u8 *)(usize)frame[1], frame[2]);
            g_generic_syscalls++;
        }
        break;
    case SVC_FD_WRITE:
        if (!ptr_readable((const void *)(usize)frame[1], frame[2])) {
            g_pointer_denials++;
            ret = -14;
        } else {
            ret = kobj_vfs_write(caller_pid, (i32)frame[0], (const u8 *)(usize)frame[1], frame[2]);
            g_generic_syscalls++;
        }
        break;
    case SVC_DUP2:
        ret = kobj_vfs_dup2(caller_pid, (i32)frame[0], (i32)frame[1]);
        g_generic_syscalls++;
        break;
    case SVC_SPAWN:
        if (!current_has(CAP_PROCESS)) {
            g_capability_denials++;
            ret = -13;
        } else if (!user_cstr_valid((const char *)(usize)frame[0], frame[1])) {
            g_pointer_denials++;
            ret = -14;
        } else {
            ret = kobj_process_spawn(caller_pid, (const char *)(usize)frame[0]);
            g_generic_syscalls++;
        }
        break;
    case SVC_SPAWN_EXEC:
        if (!current_has(CAP_PROCESS)) {
            g_capability_denials++;
            ret = -13;
        } else if (!user_cstr_valid((const char *)(usize)frame[0], frame[1])) {
            g_pointer_denials++;
            ret = -14;
        } else {
            ret = kobj_process_spawn_exec(caller_pid, (const char *)(usize)frame[0], (u8)frame[2],
                                          frame[3]);
            g_generic_syscalls++;
        }
        break;
    case SVC_WAIT:
        if ((frame[1] != 0u) && !ptr_in_sram((void *)(usize)frame[1], sizeof(i32))) {
            g_pointer_denials++;
            ret = -14;
        } else {
            ret = kobj_process_wait_blocking(caller_pid, (u16)frame[0], (i32 *)(usize)frame[1],
                                             frame);
            g_generic_syscalls++;
        }
        break;
    case SVC_KILL:
        ret = kobj_process_kill(caller_pid, (u16)frame[0], (u8)frame[1]);
        g_generic_syscalls++;
        break;
    case SVC_IPC_CREATE:
        ret = kobj_ipc_create(caller_pid, (u8)frame[0], (i32)frame[1]);
        g_generic_syscalls++;
        break;
    case SVC_IPC_CLOSE:
        ret = kobj_ipc_close(caller_pid, (i32)frame[0]);
        g_generic_syscalls++;
        break;
    case SVC_IPC_SEND:
        ret = kobj_ipc_send(caller_pid, (i32)frame[0], (u8)frame[1]);
        g_generic_syscalls++;
        break;
    case SVC_IPC_RECEIVE:
        if (!ptr_in_sram((void *)(usize)frame[1], 1u)) {
            g_pointer_denials++;
            ret = -14;
        } else {
            ret = kobj_ipc_receive_wait(caller_pid, (i32)frame[0], (u8 *)(usize)frame[1], frame[2],
                                        frame);
            g_generic_syscalls++;
        }
        break;
    case SVC_SEM_WAIT:
        ret = kobj_sem_wait_blocking(caller_pid, (i32)frame[0], frame[1], frame);
        g_generic_syscalls++;
        break;
    case SVC_SEM_POST:
        ret = kobj_sem_post(caller_pid, (i32)frame[0]);
        g_generic_syscalls++;
        break;
    case SVC_MUTEX_LOCK:
        ret = kobj_mutex_lock_blocking(caller_pid, (i32)frame[0], frame[1], frame);
        g_generic_syscalls++;
        break;
    case SVC_MUTEX_UNLOCK:
        ret = kobj_mutex_unlock(caller_pid, (i32)frame[0]);
        g_generic_syscalls++;
        break;
    case SVC_PROCESS_LIST:
        kobj_process_console_list();
        break;
    case SVC_OBJECTS_SELFTEST:
        ret = kobj_selftest();
        break;
    case SVC_VFS_LIST:
        if (frame[0] == 0u)
            kobj_vfs_console_list("/tmp");
        else if (!user_cstr_valid((const char *)(usize)frame[0], frame[1])) {
            g_pointer_denials++;
            ret = -14;
        } else
            kobj_vfs_console_list((const char *)(usize)frame[0]);
        break;
    case SVC_VFS_CAT:
        if (!user_cstr_valid((const char *)(usize)frame[0], frame[1])) {
            g_pointer_denials++;
            ret = -14;
        } else
            ret = kobj_vfs_console_cat((const char *)(usize)frame[0]);
        break;
    case SVC_VFS_MOUNTS:
        kobj_vfs_console_mounts();
        break;
    default:
        ret = -38;
        break;
    }

    /* Blocking IPC giữ exception frame trên PSP. Producer/timer sẽ ghi return
     * value vào frame trước khi đánh thức task; dispatcher không được ghi đè. */
    if ((u32)ret == HOS_SYSCALL_PENDING)
        return;
    frame[0] = (u32)ret;
}

/**
 * @brief Assembly wrapper phục vụ syscall và khởi động task đầu tiên.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Assembly wrapper phục vụ syscall và khởi động task
 * đầu tiên. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8. Hàm chạy trong exception/interrupt context; không được gọi
 * trực tiếp từ application.
 * @pre Exception được kích hoạt bởi lệnh SVC hợp lệ.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((naked)) void SVC_Handler(void) {
    __asm volatile(
        /* Bước 1: Chọn exception frame của caller và giải mã immediate SVC. */
        "tst lr,#4\n"
        "ite eq\n"
        "mrseq r0,msp\n"
        "mrsne r0,psp\n"
        "ldr r1,[r0,#24]\n"
        "ldrb r2,[r1,#-2]\n"
        "cmp r2,#16\n"
        "beq 3f\n"

        /* Bước 2: Dispatcher C có thể block task hiện hành. Giữ EXC_RETURN
         * trên MSP vì lệnh BL sẽ thay đổi LR. */
        "push {lr}\n"
        "bl hala_svc_dispatch\n"
        "pop {lr}\n"

        /* Bước 3: Nếu syscall chỉ đọc/ghi thông thường, trả về caller. */
        "ldr r3,=g_need_resched\n"
        "ldr r2,[r3]\n"
        "cbz r2,2f\n"

        /* Bước 4: Thực hiện context switch ngay trong SVC. Không phụ thuộc
         * tail-chaining của emulator; hardware cũng nhận cùng semantics. */
        "mrs r0,psp\n"
        "ldr r1,=0x20000020\n"
        "cmp r0,r1\n"
        "blo 4f\n"
        "ldr r1,=0x20005000\n"
        "cmp r0,r1\n"
        "bhi 4f\n"
        "ldr r3,=g_current\n"
        "ldr r2,[r3]\n"
        "stmdb r0!,{r4-r11}\n"
        "str r0,[r2]\n"
        "push {lr}\n"
        "bl hala_schedule_next\n"
        "pop {lr}\n"
        "ldr r3,=g_current\n"
        "ldr r1,[r3]\n"
        "ldr r0,[r1]\n"
        "ldmia r0!,{r4-r11}\n"
        "msr psp,r0\n"
        /* Xóa PendSV đã được pend_resched() đặt trước đó để tránh switch kép. */
        "ldr r1,=0xE000ED04\n"
        "ldr r2,=0x08000000\n"
        "str r2,[r1]\n"
        "2:\n"
        "bx lr\n"

        /* Bước 5: SVC #16 tạo context task đầu tiên và bật SysTick. */
        "3:\n"
        "ldr r3,=g_current\n"
        "ldr r1,[r3]\n"
        "ldr r0,[r1]\n"
        "ldmia r0!,{r4-r11}\n"
        "msr psp,r0\n"
        "movs r0,#3\n"
        "msr control,r0\n"
        "isb\n"
        "ldr r1,=0xE000E014\n"
        "ldr r2,=7999\n"
        "str r2,[r1]\n"
        "ldr r1,=0xE000E018\n"
        "movs r2,#0\n"
        "str r2,[r1]\n"
        "ldr r1,=0xE000E010\n"
        "movs r2,#7\n"
        "str r2,[r1]\n"
        /* Task đầu tiên đã có PSP hợp lệ; mở lại toàn bộ IRQ/PendSV. */
        "movs r2,#0\n"
        "msr basepri,r2\n"
        "ldr r0,=0xFFFFFFFD\n"
        "bx r0\n"

        "4:\n"
        "b hala_bad_psp\n");
}

/** @brief Gọi SVC không có tham số từ user mode. */
i32 svc_call0(u8 n) {
    register u32 r0 __asm("r0") = 0u;
    switch (n) {
    case SVC_PUTC:
        __asm volatile("svc #0" : "+r"(r0)::"memory");
        break;
    case SVC_YIELD:
        __asm volatile("svc #2" : "+r"(r0)::"memory");
        break;
    case SVC_READ:
        __asm volatile("svc #3" : "+r"(r0)::"memory");
        break;
    case SVC_GPIO:
        __asm volatile("svc #4" : "+r"(r0)::"memory");
        break;
    case SVC_UPTIME:
        __asm volatile("svc #5" : "+r"(r0)::"memory");
        break;
    case SVC_APP_RUN:
        __asm volatile("svc #7" : "+r"(r0)::"memory");
        break;
    case SVC_APP_STOP:
        __asm volatile("svc #8" : "+r"(r0)::"memory");
        break;
    case SVC_REBOOT:
        __asm volatile("svc #9" : "+r"(r0)::"memory");
        break;
    case SVC_COMPILE_EXEC:
        __asm volatile("svc #11" : "+r"(r0)::"memory");
        break;
    case SVC_PI_TEST:
        __asm volatile("svc #14" : "+r"(r0)::"memory");
        break;
    case SVC_IDLE:
        __asm volatile("svc #15" : "+r"(r0)::"memory");
        break;
    case SVC_FAULT_TEST:
        __asm volatile("svc #17" : "+r"(r0)::"memory");
        break;
    case SVC_IPC_TEST:
        __asm volatile("svc #18" : "+r"(r0)::"memory");
        break;
    case SVC_POSIX_TEST:
        __asm volatile("svc #19" : "+r"(r0)::"memory");
        break;
    case SVC_CONSOLE_LOCK:
        __asm volatile("svc #21" : "+r"(r0)::"memory");
        break;
    case SVC_CONSOLE_UNLOCK:
        __asm volatile("svc #22" : "+r"(r0)::"memory");
        break;
    case SVC_DTB_LIST:
        __asm volatile("svc #23" : "+r"(r0)::"memory");
        break;
    case SVC_DEVICE_LIST:
        __asm volatile("svc #25" : "+r"(r0)::"memory");
        break;
    case SVC_PROCESS_LIST:
        __asm volatile("svc #42" : "+r"(r0)::"memory");
        break;
    case SVC_OBJECTS_SELFTEST:
        __asm volatile("svc #43" : "+r"(r0)::"memory");
        break;
    case SVC_VFS_LIST:
        __asm volatile("svc #44" : "+r"(r0)::"memory");
        break;
    case SVC_VFS_MOUNTS:
        __asm volatile("svc #47" : "+r"(r0)::"memory");
        break;
    default:
        return -38;
    }
    return (i32)r0;
}

/** @brief Gọi SVC có một tham số số nguyên từ user mode. */
i32 svc_arg(u8 n, u32 a) {
    register u32 r0 __asm("r0") = a;
    switch (n) {
    case SVC_SLEEP:
        __asm volatile("svc #1" : "+r"(r0)::"memory");
        break;
    case SVC_LOAD:
        __asm volatile("svc #12" : "+r"(r0)::"memory");
        break;
    case SVC_PROC_STRESS:
        __asm volatile("svc #13" : "+r"(r0)::"memory");
        break;
    case SVC_CLOSE:
        __asm volatile("svc #27" : "+r"(r0)::"memory");
        break;
    case SVC_IPC_CLOSE:
        __asm volatile("svc #35" : "+r"(r0)::"memory");
        break;
    case SVC_SEM_POST:
        __asm volatile("svc #39" : "+r"(r0)::"memory");
        break;
    case SVC_MUTEX_UNLOCK:
        __asm volatile("svc #41" : "+r"(r0)::"memory");
        break;
    default:
        return -38;
    }
    return (i32)r0;
}

/** @brief Gọi SVC có con trỏ và kích thước buffer từ user mode. */
i32 svc_ptr(u8 n, const char *p, u32 len) {
    register u32 r0 __asm("r0") = (u32)(usize)p;
    register u32 r1 __asm("r1") = len;
    if (n == SVC_QUEUE_COMPILE)
        __asm volatile("svc #6" : "+r"(r0) : "r"(r1) : "memory");
    else if (n == SVC_WRITE)
        __asm volatile("svc #20" : "+r"(r0) : "r"(r1) : "memory");
    else if (n == SVC_DTB_GET)
        __asm volatile("svc #24" : "+r"(r0) : "r"(r1) : "memory");
    else
        return -38;
    return (i32)r0;
}

/** @brief Gọi SVC với hai thanh ghi đối số. */
i32 svc_args2(u8 n, u32 a, u32 b) {
    register u32 r0 __asm("r0") = a;
    register u32 r1 __asm("r1") = b;
    switch (n) {
    case SVC_DUP2:
        __asm volatile("svc #30" : "+r"(r0) : "r"(r1) : "memory");
        break;
    case SVC_WAIT:
        __asm volatile("svc #32" : "+r"(r0) : "r"(r1) : "memory");
        break;
    case SVC_KILL:
        __asm volatile("svc #33" : "+r"(r0) : "r"(r1) : "memory");
        break;
    case SVC_IPC_CREATE:
        __asm volatile("svc #34" : "+r"(r0) : "r"(r1) : "memory");
        break;
    case SVC_IPC_SEND:
        __asm volatile("svc #36" : "+r"(r0) : "r"(r1) : "memory");
        break;
    case SVC_SEM_WAIT:
        __asm volatile("svc #38" : "+r"(r0) : "r"(r1) : "memory");
        break;
    case SVC_MUTEX_LOCK:
        __asm volatile("svc #40" : "+r"(r0) : "r"(r1) : "memory");
        break;
    default:
        return -38;
    }
    return (i32)r0;
}

/** @brief Gọi SVC với ba thanh ghi đối số. */
i32 svc_args3(u8 n, u32 a, u32 b, u32 c) {
    register u32 r0 __asm("r0") = a;
    register u32 r1 __asm("r1") = b;
    register u32 r2 __asm("r2") = c;
    switch (n) {
    case SVC_FD_READ:
        __asm volatile("svc #28" : "+r"(r0) : "r"(r1), "r"(r2) : "memory");
        break;
    case SVC_FD_WRITE:
        __asm volatile("svc #29" : "+r"(r0) : "r"(r1), "r"(r2) : "memory");
        break;
    case SVC_IPC_RECEIVE:
        __asm volatile("svc #37" : "+r"(r0) : "r"(r1), "r"(r2) : "memory");
        break;
    default:
        return -38;
    }
    return (i32)r0;
}

/** @brief Gọi SVC với chuỗi, độ dài và một đối số bổ sung. */
i32 svc_ptr_arg(u8 n, const char *p, u32 len, u32 arg) {
    register u32 r0 __asm("r0") = (u32)(usize)p;
    register u32 r1 __asm("r1") = len;
    register u32 r2 __asm("r2") = arg;
    switch (n) {
    case SVC_OPEN:
        __asm volatile("svc #26" : "+r"(r0) : "r"(r1), "r"(r2) : "memory");
        break;
    case SVC_SPAWN:
        __asm volatile("svc #31" : "+r"(r0) : "r"(r1), "r"(r2) : "memory");
        break;
    case SVC_VFS_LIST:
        __asm volatile("svc #44" : "+r"(r0) : "r"(r1), "r"(r2) : "memory");
        break;
    case SVC_VFS_CAT:
        __asm volatile("svc #45" : "+r"(r0) : "r"(r1), "r"(r2) : "memory");
        break;
    default:
        return -38;
    }
    return (i32)r0;
}

/** @brief Gọi SVC với chuỗi, độ dài và hai đối số số nguyên (r2/r3). */
i32 svc_ptr_args2(u8 n, const char *p, u32 len, u32 a, u32 b) {
    register u32 r0 __asm("r0") = (u32)(usize)p;
    register u32 r1 __asm("r1") = len;
    register u32 r2 __asm("r2") = a;
    register u32 r3 __asm("r3") = b;
    if (n == SVC_SPAWN_EXEC) {
        __asm volatile("svc #46" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory");
    } else
        return -38;
    return (i32)r0;
}
