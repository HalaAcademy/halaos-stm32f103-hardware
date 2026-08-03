/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file halaos_internal.h
 * @brief Giao diện nội bộ dùng chung giữa các module HalaOS.
 * @details Header này chỉ dành cho kernel và service nội bộ; application không được include trực
 * tiếp.
 */
#ifndef HALAOS_INTERNAL_H
#define HALAOS_INTERNAL_H

#include "board_config.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int i32;
typedef unsigned long usize;
typedef unsigned long long u64;
typedef signed long long i64;
#define NULL ((void *)0)
#define REG32(a) (*(volatile u32 *)(a))
#define REG16(a) (*(volatile u16 *)(a))
#define ARRAY_LEN(a) ((u32)(sizeof(a) / sizeof((a)[0])))
#define STACK_PATTERN 0xA5A5A5A5u

/** @section HardwareRegisterMap Ánh xạ thanh ghi STM32F103 dùng nội bộ. */
#define RCC_APB2RSTR REG32(0x4002100Cu)
#define RCC_APB2ENR REG32(0x40021018u)
#define GPIOA_CRH REG32(0x40010804u)
#define GPIOC_CRH REG32(0x40011004u)
#define GPIOC_ODR REG32(0x4001100Cu)
#define GPIOC_BSRR REG32(0x40011010u)
#define GPIOC_BRR REG32(0x40011014u)
#define USART1_SR REG32(0x40013800u)
#define USART1_DR REG32(0x40013804u)
#define USART1_BRR REG32(0x40013808u)
#define USART1_CR1 REG32(0x4001380Cu)
#define FLASH_KEYR REG32(0x40022004u)
#define FLASH_SR REG32(0x4002200Cu)
#define FLASH_CR REG32(0x40022010u)
#define FLASH_AR REG32(0x40022014u)
#define IWDG_KR REG32(0x40003000u)
#define IWDG_PR REG32(0x40003004u)
#define IWDG_RLR REG32(0x40003008u)
#define NVIC_ISER1 REG32(0xE000E104u)
#define NVIC_ICER0 REG32(0xE000E180u)
#define NVIC_ICER1 REG32(0xE000E184u)
#define NVIC_ICPR0 REG32(0xE000E280u)
#define NVIC_ICPR1 REG32(0xE000E284u)
#define SCB_ICSR REG32(0xE000ED04u)
#define SCB_AIRCR REG32(0xE000ED0Cu)
#define SCB_CFSR REG32(0xE000ED28u)
#define SCB_HFSR REG32(0xE000ED2Cu)
#define SCB_MMFAR REG32(0xE000ED34u)
#define SCB_BFAR REG32(0xE000ED38u)
#define SCB_SHPR3 REG32(0xE000ED20u)
#define SYST_CSR REG32(0xE000E010u)
#define SYST_RVR REG32(0xE000E014u)
#define SYST_CVR REG32(0xE000E018u)

/** @section ApplicationStoreConfig Cấu hình Flash app store và transaction A/B. */
#define APP_STORE_BASE 0x0800E000u
#define APP_STORE_SIZE 8192u
#define APP_SLOT_SIZE 4096u
#define APP_SLOT_COUNT 2u
#define APP_HEADER_SIZE 32u
#define APP_MAGIC 0x48415050u
#define APP_STATUS_WRITING 0x7FFFu
#define APP_STATUS_VERIFYING 0x3FFFu
#define APP_STATUS_VALID 0x1FFFu

/** @section KernelStaticConfig Giới hạn task, policy và ngân sách real-time. */
#define MAX_TASKS 13u
#define APP_TYPE_BYTECODE 0u
#define HBC_PUSH_I32 0x01u
#define HBC_LOAD_LOCAL 0x02u
#define HBC_STORE_LOCAL 0x03u
#define HBC_ADD 0x04u
#define HBC_SUB 0x05u
#define HBC_MUL 0x06u
#define HBC_DIV 0x07u
#define HBC_LT 0x08u
#define HBC_LE 0x09u
#define HBC_GT 0x0Au
#define HBC_GE 0x0Bu
#define HBC_EQ 0x0Cu
#define HBC_NE 0x0Du
#define HBC_CALL 0x0Eu
#define HBC_RET 0x0Fu
#define HBC_GPIO 0x10u
#define HBC_SLEEP 0x11u
#define HBC_JMP 0x12u
#define HBC_JZ 0x13u
#define HBC_PRINT_INT 0x14u
#define HBC_POP 0x15u
#define HBC_WRITE 0x20u
#define HBC_HALT 0xFFu
#define APP_TYPE_THREADS 1u
#define APP_TYPE_IPC 2u
#define APP_TYPE_FAULT 3u
#define APP_TYPE_LOOP 4u
#define APP_TYPE_PIPE 5u
#define APP_TYPE_POSIX 6u
#define RT_LEVELS 16u
#define TICK_CYCLES 8000u
#define RT_WINDOW_TICKS 100u
#define RT_BUDGET_TICKS 80u

/** @section LinkerSymbols Symbol do linker script cung cấp cho startup, image range và stack. */
extern u32 _sidata, _sdata, _edata, _sbss, _ebss, _estack, __msp_stack_bottom, __msp_stack_top;
extern u32 __hala_loader_start, __hala_loader_end, __hala_kernel_start, __hala_kernel_end;

/** @brief Boot manifest đầy đủ được Stage-0 xác minh trước khi chuyển quyền. */
typedef struct {
    u32 magic, version, header_size, flags;
    u32 loader_addr, loader_size, loader_crc;
    u32 dtb_addr, dtb_size, dtb_crc;
    u32 kernel_addr, kernel_size, kernel_crc;
    u32 entry_point, header_crc;
} BootManifest;

#ifndef HALAOS_MANIFEST_MAGIC
#define HALAOS_MANIFEST_MAGIC 0x48414C41u
#endif
#ifndef HALAOS_MANIFEST_VERSION
#define HALAOS_MANIFEST_VERSION 4u
#endif
#ifndef HALAOS_LOADER_ADDR
#define HALAOS_LOADER_ADDR 0u
#define HALAOS_LOADER_SIZE 0u
#define HALAOS_LOADER_CRC 0u
#define HALAOS_DTB_ADDR 0u
#define HALAOS_DTB_IMAGE_SIZE HALAOS_DTB_SIZE
#define HALAOS_DTB_IMAGE_CRC 0u
#define HALAOS_KERNEL_ADDR 0u
#define HALAOS_KERNEL_SIZE 0u
#define HALAOS_KERNEL_CRC 0u
#define HALAOS_KERNEL_ENTRY 0u
#define HALAOS_MANIFEST_HEADER_CRC 0u
#endif

/** @brief Thông tin Loader truyền cho kernel sau khi xác minh manifest và DTB. */
typedef struct {
    u32 magic;
    const unsigned char *dtb;
    u32 dtb_size;
    u32 memory_base;
    u32 memory_size;
    u32 console_base;
    u32 console_irq;
    const char *stdout_path;
    const char *init_path;
    u32 manifest_flags;
} HalaBootInfo;
/** @brief Crash record nằm trong .noinit để giữ thông tin qua reset.
 * @details Lưu boot ID, stage/event cuối, PC/LR của fault và tổng số user fault đã phục hồi.
 */
typedef struct {
    u32 magic;
    u32 boot_id;
    u32 last_stage;
    u32 last_event;
    u32 fault_pc;
    u32 fault_lr;
    u32 reset_reason;
    u32 user_fault_recoveries_total;
    u32 checksum;
} HalaCrashRecord;
/** @brief Pipe byte-stream vòng dung lượng 128 byte. */
typedef struct {
    u8 data[128];
    u16 read_pos, write_pos, count;
    u8 readers, writers;
} HalaPipe;
/** @brief Semaphore đếm tối giản dùng cho POSIX educational subset. */
typedef struct {
    i32 count;
    u16 waiters;
    u16 posts;
} HalaSemaphore;
/** @brief Mutex POSIX tối giản với thống kê lock/unlock và priority inheritance. */
typedef struct {
    u8 owner;
    u8 waiters;
    u16 locks;
    u16 unlocks;
    u16 pi_boosts;
} HalaPosixMutex;
/** @brief Một file tmpfs duy nhất nằm trong RAM để minh họa VFS. */
typedef struct {
    u8 used;
    u8 data[128];
    u16 size;
} HalaTmpFile;
/** @brief Giá trị property trả về từ parser compact DTB. */
typedef struct {
    u8 type;
    u16 length;
    const u8 *data;
} HalaDtbValue;
/** @brief Device/driver registry dùng cho lsdev và driver qualification. */
typedef struct {
    const char *name;
    const char *path;
    const char *compatible;
    u8 bound;
    u8 from_dtb;
} HalaDeviceInfo;

/** @brief Mã sự kiện chuẩn cho chuỗi boot Reset → User Space. */
enum {
    HALA_BOOT_RESET_ENTER = 1,
    HALA_BOOT_STARTUP_COMPLETE,
    HALA_BOOT_STAGE0_ENTER,
    HALA_BOOT_MANIFEST_VALID,
    HALA_BOOT_LOADER_ENTER,
    HALA_BOOT_DTB_VALID,
    HALA_BOOT_KERNEL_ENTER,
    HALA_BOOT_MEMORY_READY,
    HALA_BOOT_DRIVERS_READY,
    HALA_BOOT_SCHED_READY,
    HALA_BOOT_SYSCALL_READY,
    HALA_BOOT_IPC_READY,
    HALA_BOOT_VFS_READY,
    HALA_BOOT_PROCESS_READY,
    HALA_BOOT_INIT_STARTED,
    HALA_BOOT_USERSPACE_READY,
    HALA_BOOT_SHELL_READY
};

#define CAP_UART (1u << 0)
#define CAP_GPIO (1u << 1)
#define CAP_COMPILER (1u << 2)
#define CAP_APP (1u << 3)
#define CAP_SCHED_ADMIN (1u << 4)
#define CAP_PROCESS (1u << 5)
#define TASK_READY 0u
#define TASK_SLEEP 1u
#define TASK_BLOCKED 2u
#define TASK_ZOMBIE 3u
#define TASK_STOPPED 4u
#define POLICY_IDLE 0u
#define POLICY_FIFO 1u
#define POLICY_RR 2u
#define POLICY_NORMAL 3u
#define POLICY_BATCH 4u
#define POLICY_DEADLINE 5u
#define INVALID_IDX 0xFFu
#define TF_DYNAMIC (1u << 0)
#define TF_LOAD_ENABLED (1u << 1)

/**
 * @brief Task Control Block của HalaOS.
 * @details Chứa context stack, accounting, deadline/fair scheduling, PID, stack telemetry,
 * state, policy, priority, liên kết ready/sleep queue, capability và tên task.
 */
typedef struct Tcb {
    u32 *sp;
    u32 *stack_low;
    u32 *stack_high;
    u32 wake;
    u32 caps;
    u64 runtime;
    u64 vruntime;
    u64 virtual_deadline;
    u64 abs_deadline;
    u64 next_release;
    u32 deadline_period;
    u32 deadline_runtime;
    u32 deadline_remaining;
    u32 slice_used;
    u32 switches;
    u32 voluntary;
    u32 involuntary;
    u32 deadline_misses;
    u32 throttle_count;
    u16 pid;
    u16 weight;
    u16 stack_peak;
    u8 state;
    u8 policy;
    u8 base_prio;
    u8 effective_prio;
    u8 quantum;
    u8 quantum_left;
    u8 ready_next;
    u8 sleep_next;
    u32 sleep_delta;
    u8 flags;
    const char *name;
} Tcb;

/** @brief Mutex kernel có owner, wait queue và bộ đếm priority inheritance. */
typedef struct {
    u8 owner;
    u8 wait_head;
    u32 lock_count;
    u32 pi_boosts;
} HalaMutex;
/** @brief Resource slot của process trong fixed pool. */
typedef struct {
    u8 used;
    u8 handles;
    u8 timers;
    u8 queues;
    u16 generation;
    u16 owner;
} ProcSlot;
/** @brief Header application lưu trong mỗi Flash slot. */
typedef struct {
    u16 status, version;
    u32 magic;
    u16 code_size, flags;
    u32 crc32, sequence;
    u32 reserved[3];
} AppHeader;

enum {
    SVC_PUTC = 0,
    SVC_SLEEP = 1,
    SVC_YIELD = 2,
    SVC_READ = 3,
    SVC_GPIO = 4,
    SVC_UPTIME = 5,
    SVC_QUEUE_COMPILE = 6,
    SVC_APP_RUN = 7,
    SVC_APP_STOP = 8,
    SVC_REBOOT = 9,
    SVC_EXIT = 10,
    SVC_COMPILE_EXEC = 11,
    SVC_LOAD = 12,
    SVC_PROC_STRESS = 13,
    SVC_PI_TEST = 14,
    SVC_IDLE = 15,
    SVC_FAULT_TEST = 17,
    SVC_IPC_TEST = 18,
    SVC_POSIX_TEST = 19,
    SVC_WRITE = 20,
    SVC_CONSOLE_LOCK = 21,
    SVC_CONSOLE_UNLOCK = 22,
    SVC_DTB_LIST = 23,
    SVC_DTB_GET = 24,
    SVC_DEVICE_LIST = 25,
    SVC_OPEN = 26,
    SVC_CLOSE = 27,
    SVC_FD_READ = 28,
    SVC_FD_WRITE = 29,
    SVC_DUP2 = 30,
    SVC_SPAWN = 31,
    SVC_WAIT = 32,
    SVC_KILL = 33,
    SVC_IPC_CREATE = 34,
    SVC_IPC_CLOSE = 35,
    SVC_IPC_SEND = 36,
    SVC_IPC_RECEIVE = 37,
    SVC_SEM_WAIT = 38,
    SVC_SEM_POST = 39,
    SVC_MUTEX_LOCK = 40,
    SVC_MUTEX_UNLOCK = 41,
    SVC_PROCESS_LIST = 42,
    SVC_OBJECTS_SELFTEST = 43,
    SVC_VFS_LIST = 44,
    SVC_VFS_CAT = 45,
    SVC_SPAWN_EXEC = 46,
    SVC_VFS_MOUNTS = 47
};
#define HOS_SYSCALL_PENDING 0x7FFFFFFEu
#define SVC_PIPE SVC_IPC_CREATE

/* ==========================================================================
 *                         INTERNAL GLOBAL STATE
 * ========================================================================== */
extern void (*const vectors[64])(void);
extern const BootManifest g_boot_manifest;
extern HalaBootInfo g_boot_info;
extern volatile u32 g_startup_data_probe, g_startup_bss_probe, g_startup_data_ok, g_startup_bss_ok,
    g_startup_noinit_boots;
extern volatile u32 g_dtb_valid;
extern volatile u32 g_user_fault_recoveries;
extern volatile u16 g_boot_events[32];
extern volatile u8 g_boot_event_count;
extern HalaCrashRecord g_crash_record;
extern Tcb g_tasks[13];
extern volatile Tcb *g_current;
extern volatile u32 g_current_index;
extern volatile u32 g_ticks;
extern volatile u32 g_switches;
extern volatile u32 g_need_resched;
extern volatile u32 g_preempt_count;
extern volatile u32 g_tick_step;
extern volatile u32 g_scheduler_calls;
extern volatile u32 g_scheduler_max_scan;
extern volatile u32 g_idle_wfi_count;
extern volatile u32 g_tickless_entries;
extern volatile u32 g_tickless_skipped;
extern volatile u32 g_rt_window_start;
extern volatile u32 g_rt_used;
extern volatile u32 g_rt_throttled;
extern volatile u32 g_rt_throttle_count;
extern volatile u32 g_capability_denials;
extern volatile u32 g_pointer_denials;
extern volatile u32 g_vm_steps;
extern volatile u32 g_compile_count;
extern volatile u32 g_app_valid;
extern volatile u32 g_app_running;
extern volatile u32 g_bad_psp;
extern volatile u32 g_bad_task;
extern volatile u32 g_bytecode_rejects;
extern volatile u32 g_wakeup_count;
extern volatile u32 g_lost_wakeup;
extern volatile u32 g_wait_ops;
extern volatile u32 g_pi_boosts;
extern volatile u32 g_pi_restores;
extern volatile u32 g_proc_spawns;
extern volatile u32 g_proc_reaps;
extern volatile u32 g_proc_leaks;
extern volatile u32 g_fault_count;
extern volatile u32 g_fault_pc;
extern volatile u32 g_fault_lr;
extern volatile u32 g_kernel_faults;
extern volatile u32 g_last_fault_pid;
extern volatile u32 g_last_fault_task;
extern volatile u32 g_uart_irq_count;
extern volatile u32 g_uart_wakeups;
extern volatile u32 g_uart_bytes;
extern volatile u32 g_rx_overflow;
extern volatile u32 g_app_slot;
extern volatile u32 g_app_fail_stage;
extern volatile u32 g_app_recovery_count;
extern volatile u32 g_rr_rotations;
extern volatile u32 g_fifo_ticks;
extern volatile u32 g_fair_picks;
extern volatile u32 g_deadline_picks;
extern volatile u32 g_deadline_misses;
extern volatile u32 g_deadline_throttles;
extern volatile u32 g_rr_a_ticks;
extern volatile u32 g_rr_b_ticks;
extern volatile u32 g_fair_a_ticks;
extern volatile u32 g_fair_b_ticks;
extern volatile u32 g_deadline_ticks;
extern volatile u32 g_shell_ticks;
extern volatile u32 g_compiler_ticks;
extern volatile u32 g_vm_ticks;
extern volatile u32 g_load_mode;
extern volatile u32 g_test_done;
extern volatile u32 g_compiler_job;
extern volatile u32 g_compiler_result;
extern volatile u32 g_compiler_peak;
extern volatile u32 g_fault_inject;
extern volatile u32 g_driver_count;
extern volatile u32 g_vfs_mounts;
extern volatile u32 g_init_ready;
extern volatile u32 g_userspace_ready;
extern volatile u32 g_shell_commands;
extern volatile u32 g_shell_errors;
extern volatile u32 g_shell_line_overflow;
extern volatile u32 g_stack_corruption;
extern volatile u32 g_in_systick;
extern volatile u32 g_shell_typing;
extern volatile u32 g_ipc_operations;
extern volatile u32 g_ipc_lost;
extern volatile u32 g_pipe_bytes;
extern volatile u32 g_posix_calls;
extern volatile u32 g_fd_open_count;
extern volatile u32 g_fd_close_count;
extern volatile u8 g_console_owner;
extern volatile u32 g_app_type;
extern volatile u32 g_app_thread_done;
extern volatile u32 g_thread_creates;
extern volatile u32 g_thread_joins;
extern volatile u32 g_thread_exits;
extern volatile u32 g_thread_faults;
extern volatile u32 g_thread_leaks;
extern volatile u32 g_app_paused;
extern volatile u32 g_app_exit_status;
extern volatile u32 g_app_thread_a_count;
extern volatile u32 g_app_thread_b_count;
extern volatile u32 g_demo_queue_sent;
extern volatile u32 g_demo_queue_received;
extern u8 g_demo_queue[8];
extern volatile u8 g_demo_q_head;
extern volatile u8 g_demo_q_tail;
extern volatile u8 g_demo_q_count;
extern volatile u8 g_editor_active;
extern u16 g_editor_len;
extern char g_editor_name[12];
extern u8 g_editor_source[300];
extern char g_compile_name[12];
extern HalaPipe g_pipe;
extern HalaSemaphore g_sem;
extern HalaPosixMutex g_posix_mutex;
extern HalaTmpFile g_tmp_file;
extern u8 g_rt_head[16];
extern u8 g_rt_tail[16];
extern u16 g_rt_bitmap;
extern u8 g_sleep_head;
extern u8 rx_ring[128];
extern volatile u16 rx_head;
extern volatile u16 rx_tail;
extern u8 g_compile_source[300];
extern u16 g_compile_source_len;
extern u8 g_compact[256];
extern u8 g_flash_staging[258];
extern ProcSlot g_proc_pool[4];
extern HalaMutex g_test_mutex;
extern u32 stack_idle[128];
extern u32 stack_init[192];
extern u32 stack_shell[768];
extern u32 stack_compiler[320];
extern u32 stack_vm[256];
extern u32 stack_app_a[96];
extern u32 stack_app_b[96];
extern u32 stack_rr_a[128];
extern u32 stack_rr_b[128];
extern u32 stack_fifo[128];
extern u32 stack_fair_a[128];
extern u32 stack_fair_b[128];
extern u32 stack_deadline[128];

/* ==========================================================================
 *                       INTERNAL FUNCTION PROTOTYPES
 * ========================================================================== */
u32 crc32_bytes(const u8 *, u32);
u32 manifest_crc(void);
u32 str_len(const char *);
int str_eq(const char *, const char *);
int str_starts(const char *, const char *);
void mem_copy(void *, const void *, u32);
void mem_zero(void *, u32);
void copy_zero(void);
void uart_putc_priv(char);
void uart_puts_priv(const char *);
void uart_hex_priv(u32);
void uart_dec_priv(u32);
u32 rd32le(const u8 *);
void boot_event(u16);
int compact_dtb_validate(void);
int hala_dtb_validate_blob(void);
u32 hala_dtb_node_count(void);
const char *hala_dtb_node_path(u32);
int hala_dtb_get(const char *, const char *, HalaDtbValue *);
const char *hala_dtb_get_string(const char *, const char *);
int hala_dtb_get_u32(const char *, const char *, u32 *);
int hala_dtb_get_pair(const char *, const char *, u32 *, u32 *);
u32 hala_driver_bind_all(void);
u32 hala_device_count(void);
const HalaDeviceInfo *hala_device_at(u32);
void hala_dtb_console_list(void);
int hala_dtb_console_get(const char *, const char *);
void hala_driver_console_list(void);

void early_stage(const char *, const char *, u16);
void board_early_init(void);
void led_toggle_priv(void);
void reboot_priv(void);
u32 read_msp(void);
void Reset_Handler(void);
void fill_msp_pattern(void);
void Reset_Handler_C(void);
void hala_loader_entry(const BootManifest *manifest);
void Default_Handler(void);
int is_rt_policy(u8);
int is_fair_policy(u8);
u32 app_slot_addr(u32);
const volatile AppHeader *app_header_slot(u32);
const volatile AppHeader *app_header(void);
const u8 *app_code(void);
const char *app_name(void);
void name_copy(char *, const char *);
int name_valid(const char *);
int app_name_eq(const char *);
void rt_queue_init(void);
void rt_enqueue(u8);
void rt_remove(u8);
void rt_rotate(u8);
u8 highest_rt(void);
void sleep_remove(u8);
void sleep_insert(u8, u32);
void task_make_ready(u8);
void sleep_advance(u32);
u32 stack_used_words(const Tcb *);
u32 msp_used_bytes(void);
void update_stack_peak(Tcb *);
void stack_guard_check(Tcb *);
void task_exit_trap(void);
u32 *init_stack(u32 *, u32, void (*)(void *), void *);
void task_init(u8, u32 *, u32, void (*)(void *), const char *, u16, u8, u8, u16, u32, u8);
int task_precedes(const Tcb *, const Tcb *);
u8 pick_deadline(void);
u8 pick_fair(void);
u8 sched_pick_next(void);
void hala_schedule_next(void);
void pend_resched(void);
void block_current(u8, u32);
void hala_bad_psp(u32);
void PendSV_Handler(void);
void task_fault_trampoline(void);
void HardFault_C(u32 *, u32);
void HardFault_Handler(void);
void rt_account(u32);
void deadline_account(Tcb *, u32);
void SysTick_Handler(void);
void uart_wake_shell(void);
void USART1_IRQHandler(void);
int ptr_in_sram(const void *, u32);
int ptr_readable(const void *, u32);
int current_has(u32);
int rx_pop(void);
void flash_wait(void);
void flash_unlock(void);
void flash_erase_page(u32);
void flash_program_half(u32, u16);
int bytecode_verify(const u8 *, u16);
int app_verify_slot(u32);
int app_scan(void);
int app_write(const u8 *, u16, u16, const char *);
int compact_source(const char *, u32);
const char *find_text(const char *, const char *);
int hala_compile_general(const char *, u32, const char *);
extern volatile u32 g_compile_error_line, g_compile_error_column;
extern volatile i32 g_compile_error_code;
extern volatile u32 g_compiler_tokens, g_compiler_functions, g_compiler_symbols;
int compile_hala_c(const char *, u32);
int proc_alloc(void);
void proc_reap(u32);
int proc_stress(u32);
int pi_selftest(void);
void load_disable_all(void);
void load_enable(u32);
u32 next_event_ticks(void);
void enter_idle_tickless(void);
int wait_priority_better(u8, u8);
void wait_queue_model_test(void);
void pipe_reset(void);
int pipe_write_priv(const u8 *, u32);
int pipe_read_priv(u8 *, u32);
int ipc_posix_selftest(void);
void tmp_write(const char *, u32);
u32 task_index_from_ptr(const volatile Tcb *);
void demo_block_task(u8);
void demo_stop(void);
void demo_start(u32);
void demo_thread_complete(u32, u32, const char *);
int demo_queue_push(u8);
int demo_queue_pop(u8 *);
extern volatile u32 g_process_live, g_process_created, g_process_reaped, g_process_killed;
extern volatile u32 g_process_runs, g_process_exits, g_process_task_links;
extern volatile u32 g_generic_syscalls, g_ipc_objects_live, g_ipc_timeouts, g_ipc_wakeups;
extern volatile u32 g_vfs_file_count, g_vfs_open_count, g_vfs_resolves;
extern volatile u32 g_posix_api_pass, g_posix_api_fail;
void hala_objects_init(void);
i32 kobj_process_spawn(u16, const char *);
i32 kobj_process_spawn_exec(u16, const char *, u8, u32);
i32 kobj_process_task_step(u8, const char **, u32 *);
i32 kobj_process_exit_current(u16, u8);
i32 kobj_process_kill(u16, u16, u8);
i32 kobj_process_stop(u16, u16, int);
i32 kobj_process_wait(u16, u16, i32 *);
i32 kobj_process_wait_blocking(u16, u16, i32 *, u32 *);
void kobj_process_finalize_task_exit(u8);
void kobj_process_console_list(void);
i32 kobj_ipc_create(u16, u8, i32);
i32 kobj_ipc_close(u16, i32);
i32 kobj_ipc_send(u16, i32, u8);
i32 kobj_ipc_receive(u16, i32, u8 *, u32);
i32 kobj_ipc_receive_wait(u16, i32, u8 *, u32, u32 *);
i32 kobj_sem_wait(u16, i32, u32);
i32 kobj_sem_wait_blocking(u16, i32, u32, u32 *);
i32 kobj_sem_post(u16, i32);
i32 kobj_mutex_lock(u16, i32, u32);
i32 kobj_mutex_lock_blocking(u16, i32, u32, u32 *);
i32 kobj_mutex_unlock(u16, i32);
void kobj_ipc_tick(u32);
i32 kobj_vfs_open(u16, const char *, u32);
i32 kobj_vfs_close(u16, i32);
i32 kobj_vfs_write(u16, i32, const u8 *, u32);
i32 kobj_vfs_read(u16, i32, u8 *, u32);
i32 kobj_vfs_dup2(u16, i32, i32);
void kobj_vfs_console_list(const char *);
void kobj_vfs_console_mounts(void);
i32 kobj_vfs_console_cat(const char *);
i32 kobj_selftest(void);
void hala_svc_dispatch(u32 *);
void SVC_Handler(void);
i32 svc_call0(u8);
i32 svc_arg(u8, u32);
i32 svc_ptr(u8, const char *, u32);
i32 svc_args2(u8, u32, u32);
i32 svc_args3(u8, u32, u32, u32);
i32 svc_ptr_arg(u8, const char *, u32, u32);
i32 svc_ptr_args2(u8, const char *, u32, u32, u32);
void out_write(const char *, u32);
void outc(char);
void outs(const char *);
void out_hex(u32);
void out_dec(u32);
void console_lock(void);
void console_unlock(void);
void prompt(void);
void source_prompt(void);
void print_app_name(void);
void async_begin(void);
void async_end(void);
const char *state_name(u8);
const char *policy_name(u8);
void out_spaces(u32);
void out_field(const char *, u32);
void out_num_field(u32, u32);
int task_visible(u32);
u32 task_ppid(u32);
u32 task_tid(u32);
void print_task(u32);
void print_ps_table(void);
void print_threads_table(void);
void print_boot_events(void);
void print_schedstat(void);
void print_irqstat(void);
void print_boot_stage(const char *);
void print_dmesg(int);
void print_disasm(void);
void print_proc_path(const char *);
void print_dtb_nodes(void);
void print_dtb_property(const char *, const char *);
void shell_dtb_get(char *);
void print_device_registry(void);
void shell_command(char *);
void editor_accept_line(char *);
void shell_execute(char *);
void task_hala_init(void *);
void task_shell(void *);
void task_compiler(void *);
void vm_complete(void);
void task_vm(void *);
void task_app_a(void *);
void task_app_b(void *);
void task_idle(void *);
void task_rr_a(void *);
void task_rr_b(void *);
void task_fifo(void *);
void task_fair_a(void *);
void task_fair_b(void *);
void task_deadline(void *);
void start_first_task(void);
void kernel_main(const HalaBootInfo *boot_info);

#endif /* HALAOS_INTERNAL_H */
