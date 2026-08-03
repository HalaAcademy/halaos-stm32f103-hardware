/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file halaos_state.c
 * @brief Sở hữu toàn bộ fixed-pool và telemetry state dùng chung của HalaOS.
 * @details Việc tập trung ownership tại một translation unit giúp tránh multiple definition và làm
 * rõ nơi sở hữu dữ liệu.
 */
#include "halaos/internal/halaos_internal.h"

/** @brief Bảng vector exception/IRQ của Cortex-M3.
 * @details Entry 0 chứa MSP ban đầu; các entry còn lại trỏ tới handler tương ứng.
 */
__attribute__((used, section(".isr_vector"))) void (*const vectors[64])(void) = {
    [0] = (void (*)(void))(&_estack),
    [1] = Reset_Handler,
    [2] = Default_Handler,
    [3] = HardFault_Handler,
    [4] = Default_Handler,
    [5] = Default_Handler,
    [6] = Default_Handler,
    [7] = Default_Handler,
    [8] = Default_Handler,
    [9] = Default_Handler,
    [10] = Default_Handler,
    [11] = SVC_Handler,
    [12] = Default_Handler,
    [13] = Default_Handler,
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,
    [16 ... 52] = Default_Handler,
    [53] = USART1_IRQHandler,
    [54 ... 63] = Default_Handler};

/** @brief Boot manifest được build hai pass để chứa range và CRC image thật. */
__attribute__((used, section(".hala_manifest"))) const BootManifest g_boot_manifest = {
    HALAOS_MANIFEST_MAGIC, HALAOS_MANIFEST_VERSION, sizeof(BootManifest),      0x0000000Fu,
    HALAOS_LOADER_ADDR,    HALAOS_LOADER_SIZE,      HALAOS_LOADER_CRC,         HALAOS_DTB_ADDR,
    HALAOS_DTB_IMAGE_SIZE, HALAOS_DTB_IMAGE_CRC,    HALAOS_KERNEL_ADDR,        HALAOS_KERNEL_SIZE,
    HALAOS_KERNEL_CRC,     HALAOS_KERNEL_ENTRY,     HALAOS_MANIFEST_HEADER_CRC};

/** @brief Startup probes dùng qualification để chứng minh .data/.bss/.noinit. */
volatile u32 g_startup_data_probe = 0x13579BDFu;
volatile u32 g_startup_bss_probe;
volatile u32 g_startup_data_ok, g_startup_bss_ok;
__attribute__((section(".noinit.bootlog"), used)) volatile u32 g_startup_noinit_boots;
HalaBootInfo g_boot_info;

volatile u32 g_dtb_valid;
volatile u32 g_user_fault_recoveries;
volatile u16 g_boot_events[32];
volatile u8 g_boot_event_count;
__attribute__((section(".noinit.bootlog"), used)) HalaCrashRecord g_crash_record;

Tcb g_tasks[MAX_TASKS];
volatile Tcb *g_current;
volatile u32 g_current_index;
volatile u32 g_ticks, g_switches, g_need_resched, g_preempt_count, g_tick_step = 1u;
volatile u32 g_scheduler_calls, g_scheduler_max_scan, g_idle_wfi_count, g_tickless_entries,
    g_tickless_skipped;
volatile u32 g_rt_window_start, g_rt_used, g_rt_throttled, g_rt_throttle_count;
volatile u32 g_capability_denials, g_pointer_denials, g_vm_steps, g_compile_count, g_app_valid,
    g_app_running, g_bad_psp, g_bad_task, g_bytecode_rejects;
volatile u32 g_wakeup_count, g_lost_wakeup, g_wait_ops, g_pi_boosts, g_pi_restores, g_proc_spawns,
    g_proc_reaps, g_proc_leaks;
volatile u32 g_fault_count, g_fault_pc, g_fault_lr, g_kernel_faults, g_last_fault_pid;
volatile u32 g_last_fault_task = INVALID_IDX;
volatile u32 g_uart_irq_count, g_uart_wakeups, g_uart_bytes, g_rx_overflow, g_app_slot,
    g_app_fail_stage, g_app_recovery_count;
volatile u32 g_rr_rotations, g_fifo_ticks, g_fair_picks, g_deadline_picks, g_deadline_misses,
    g_deadline_throttles;
volatile u32 g_rr_a_ticks, g_rr_b_ticks, g_fair_a_ticks, g_fair_b_ticks, g_deadline_ticks,
    g_shell_ticks, g_compiler_ticks, g_vm_ticks;
volatile u32 g_load_mode, g_test_done, g_compiler_job, g_compiler_result, g_compiler_peak,
    g_fault_inject;
volatile u32 g_driver_count, g_vfs_mounts, g_init_ready, g_userspace_ready;
volatile u32 g_shell_commands, g_shell_errors, g_shell_line_overflow, g_stack_corruption,
    g_in_systick, g_shell_typing;
volatile u32 g_ipc_operations, g_ipc_lost, g_pipe_bytes, g_posix_calls, g_fd_open_count,
    g_fd_close_count;
volatile u8 g_console_owner = INVALID_IDX;
volatile u32 g_app_type, g_app_thread_done, g_thread_creates, g_thread_joins, g_thread_exits,
    g_thread_faults, g_thread_leaks, g_app_paused, g_app_exit_status;
volatile u32 g_app_thread_a_count, g_app_thread_b_count, g_demo_queue_sent, g_demo_queue_received;
u8 g_demo_queue[8];
volatile u8 g_demo_q_head, g_demo_q_tail, g_demo_q_count;
volatile u8 g_editor_active;
u16 g_editor_len;
char g_editor_name[12];
u8 g_editor_source[300];
char g_compile_name[12] = "app0";
HalaPipe g_pipe;
HalaSemaphore g_sem;
HalaPosixMutex g_posix_mutex;
HalaTmpFile g_tmp_file;
u8 g_rt_head[RT_LEVELS], g_rt_tail[RT_LEVELS];
u16 g_rt_bitmap;
u8 g_sleep_head = INVALID_IDX;
u8 rx_ring[128];
volatile u16 rx_head, rx_tail;
u8 g_compile_source[300];
u16 g_compile_source_len;
u8 g_compact[256];
u8 g_flash_staging[258];
ProcSlot g_proc_pool[4];
HalaMutex g_test_mutex = {INVALID_IDX, INVALID_IDX, 0, 0};

/**
 * @section TaskStacks Stack tĩnh cho toàn bộ task; không sử dụng heap động.
 * @details Các stack được đặt trong section NOLOAD riêng ở đầu SRAM. Cách bố trí
 *          này giữ PSP của task tách khỏi object pool và vùng MSP, đồng thời làm
 *          memory map ổn định khi kích thước PCB/VFS/IPC thay đổi qua các phase.
 */
__attribute__((section(".task_stacks"), aligned(8))) u32 stack_idle[128], stack_init[192],
    stack_shell[768], stack_compiler[320], stack_vm[256], stack_app_a[96], stack_app_b[96];
__attribute__((section(".task_stacks"), aligned(8))) u32 stack_rr_a[128], stack_rr_b[128],
    stack_fifo[128], stack_fair_a[128], stack_fair_b[128], stack_deadline[128];
