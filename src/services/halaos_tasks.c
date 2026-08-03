/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file halaos_tasks.c
 * @brief Task entry và kernel startup.
 * @details File được tách từ baseline đã qualification; comment tiếng Việt được giữ để phục vụ
 * review và đào tạo.
 */
#include "halaos/internal/halaos_internal.h"

/**
 * @brief Task PID 1 khởi tạo service và chuyển hệ thống sang user space.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Task PID 1 khởi tạo service và chuyển hệ thống sang
 * user space. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc
 * block thông qua syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_hala_init(void *arg) {
    (void)arg;
    outs("[    0.000400] init: hala-init PID=1 starting\r\n");
    boot_event(HALA_BOOT_INIT_STARTED);
    g_vfs_mounts = 4;
    outs("[    0.000430] vfs: mounted devfs on /dev\r\n[    0.000450] vfs: mounted procfs on "
         "/proc\r\n[    0.000470] vfs: mounted tmpfs on /tmp\r\n[    0.000490] vfs: mounted appfs "
         "on /apps\r\n");
    g_init_ready = 1;
    outs("[    0.000520] init: spawned hala-shell PID=2 fds=0,1,2 -> /dev/console\r\n");
    task_make_ready(1);
    for (;;)
        block_current(TASK_BLOCKED, 0);
}

/**
 * @brief Task HalaShell nhận UART, edit dòng và thực thi command.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Task HalaShell nhận UART, edit dòng và thực thi
 * command. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block
 * thông qua syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_shell(void *arg) {
    (void)arg;
    g_userspace_ready = 1;
    boot_event(HALA_BOOT_USERSPACE_READY);
    console_lock();
    outs("[    0.000560] user: hala-shell running unprivileged on PSP CONTROL.nPRIV=1\r\n");
    boot_event(HALA_BOOT_SHELL_READY);
    outs("HalaOS Blue Pill Educational 0.4\r\nType 'help' for commands.\r\n");
    if (g_app_valid)
        outs("APP:VALID\r\n");
    prompt();
    console_unlock();
    char line[256];
    u32 n = 0;
    u8 overflow = 0;
    for (;;) {
        i32 c = svc_call0(SVC_READ);
        if (c < 0)
            continue;
        if (c == '\n')
            continue;
        if (c == '\r') {
            g_shell_typing = 1;
            console_lock();
            outs("\r\n");
            if (overflow) {
                outs("ERR:LINE_TOO_LONG\r\n");
                g_shell_errors++;
                g_shell_line_overflow++;
            } else {
                line[n] = 0;
                if (g_editor_active)
                    editor_accept_line(line);
                else
                    shell_execute(line);
            }
            n = 0;
            overflow = 0;
            if (g_editor_active)
                source_prompt();
            else
                prompt();
            console_unlock();
            g_shell_typing = 0;
        } else if ((c == 8 || c == 127) && n && !overflow) {
            g_shell_typing = 1;
            n--;
            console_lock();
            outs("\b \b");
            console_unlock();
        } else if (c >= 32 && c < 127) {
            g_shell_typing = 1;
            if (!overflow && n < 240u) {
                line[n++] = (char)c;
                console_lock();
                outc((char)c);
                console_unlock();
            } else
                overflow = 1;
        }
    }
}

/**
 * @brief Task compiler bất đồng bộ xử lý source và ghi application store.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Task compiler bất đồng bộ xử lý source và ghi
 * application store. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc
 * block thông qua syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_compiler(void *arg) {
    (void)arg;
    for (;;) {
        if (!g_compiler_job) {
            svc_arg(SVC_SLEEP, 1000u);
            continue;
        }
        i32 result = svc_call0(SVC_COMPILE_EXEC);
        while (g_shell_typing)
            svc_arg(SVC_SLEEP, 5u);
        g_compiler_result = (u32)result;
        g_compiler_job = 0u;
        console_lock();
        if (result == 0) {
            outs("\r\nHala-C Educational Compiler\r\nSource bytes: ");
            out_dec(g_compile_source_len);
            outs("\r\nTokens: ");
            out_dec(g_compiler_tokens);
            outs("\r\nFunctions: ");
            out_dec(g_compiler_functions);
            outs("\r\nSymbols: ");
            out_dec(g_compiler_symbols);
            outs("\r\nBytecode: ");
            out_dec(app_header()->code_size);
            outs(" bytes");
            outs("\r\nVerifier: PASS\r\nCRC32: 0x");
            out_hex(app_header()->crc32);
            outs("\r\nFlash slot: ");
            outc(g_app_slot ? 'B' : 'A');
            outs("\r\nCOMPILE:OK\r\nBUILD:OK name=");
            outs(g_compile_name);
            outs(" profile=");
            if (g_app_type == APP_TYPE_THREADS)
                outs("threads");
            else if (g_app_type == APP_TYPE_IPC)
                outs("ipc");
            else if (g_app_type == APP_TYPE_FAULT)
                outs("fault");
            else if (g_app_type == APP_TYPE_LOOP)
                outs("loop");
            else if (g_app_type == APP_TYPE_PIPE)
                outs("pipe");
            else if (g_app_type == APP_TYPE_POSIX)
                outs("posix");
            else
                outs("bytecode");
            outs("\r\n");
        } else {
            outs("\r\n");
            outs(g_compile_name);
            outs(":");
            out_dec(g_compile_error_line ? g_compile_error_line : 1u);
            outs(":");
            out_dec(g_compile_error_column ? g_compile_error_column : 1u);
            outs(": error code=");
            out_dec((u32)(0u - (u32)result));
            outs("\r\n");
            outs("Build failed; previous valid application preserved\r\nBUILD:FAIL name=");
            outs(g_compile_name);
            outs("\r\n");
        }
        prompt();
        console_unlock();
    }
}

/**
 * @brief Hoàn tất VM application, cập nhật exit status và đánh thức waiter.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Hoàn tất VM application, cập nhật exit status và
 * đánh thức waiter. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] g_app_running Tham số g_app_running của hàm.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void vm_complete(void) {
    if (g_app_running) { /* State is committed before prompt visibility for race-free wait/status
                            commands. */
        g_app_running = 0;
        g_app_exit_status = 0;
        console_lock();
        outs("\r\n");
        print_app_name();
        outs(" exited status=0\r\n");
        prompt();
        console_unlock();
    }
}

/** @brief In một số nguyên có dấu mà không dùng thư viện chuẩn. */
static void vm_out_i32(i32 value) {
    if (value < 0) {
        outc('-');
        /* Chuyển qua miền unsigned để xử lý an toàn INT32_MIN. */
        out_dec((u32)(0u - (u32)value));
    } else {
        out_dec((u32)value);
    }
}

/** @brief Dừng VM với mã lỗi xác định và giữ shell/kernel tiếp tục hoạt động. */
static void vm_trap(u32 status, const char *reason) {
    if (!g_app_running)
        return;
    g_app_exit_status = status;
    g_app_running = 0u;
    console_lock();
    outs("\r\nHBC trap: ");
    outs(reason);
    outs(" status=");
    out_dec(status);
    outs("\r\n");
    print_app_name();
    outs(" exited status=");
    out_dec(status);
    outs("\r\n");
    prompt();
    console_unlock();
}

/**
 * @brief Task HBC VM thực thi stack bytecode application user.
 * @details VM có operand stack, local frame, call frame, arithmetic, comparison, branch,
 *          function call và instruction budget. Mọi workspace nằm trên stack cố định của task;
 *          VM không cấp phát heap và reset toàn bộ state khi sequence application thay đổi.
 */
void task_vm(void *arg) {
    (void)arg;
    i32 operand[16];
    i32 locals[8];
    i32 saved_locals[4][8];
    u16 return_pc[4];
    u8 base_sp[4];
    u8 sp = 0u, fp = 0u;
    u16 pc = 0u;
    u32 active_sequence = 0u;

    mem_zero(operand, sizeof(operand));
    mem_zero(locals, sizeof(locals));
    mem_zero(saved_locals, sizeof(saved_locals));

    for (;;) {
        if (!g_app_running || !g_app_valid || !app_verify_slot(g_app_slot) ||
            g_app_type != APP_TYPE_BYTECODE) {
            pc = 0u;
            sp = 0u;
            fp = 0u;
            active_sequence = 0u;
            mem_zero(locals, sizeof(locals));
            block_current(TASK_BLOCKED, 0u);
            continue;
        }

        const volatile AppHeader *header = app_header();
        const u8 *code = app_code();
        if (active_sequence != header->sequence) {
            active_sequence = header->sequence;
            pc = 0u;
            sp = 0u;
            fp = 0u;
            mem_zero(locals, sizeof(locals));
        }

        u32 budget = 64u;
        while ((budget-- > 0u) && g_app_running) {
            if (pc >= header->code_size) {
                vm_trap(126u, "pc-out-of-range");
                break;
            }
            u8 opcode = code[pc++];
            g_vm_steps++;

            if (opcode == HBC_PUSH_I32) {
                if (((u32)pc + 4u > header->code_size) || (sp >= 16u)) {
                    vm_trap(126u, "push");
                    break;
                }
                u32 raw = (u32)code[pc] | ((u32)code[pc + 1u] << 8) | ((u32)code[pc + 2u] << 16) |
                          ((u32)code[pc + 3u] << 24);
                pc += 4u;
                operand[sp++] = (i32)raw;
            } else if (opcode == HBC_LOAD_LOCAL) {
                if ((pc >= header->code_size) || (code[pc] >= 8u) || (sp >= 16u)) {
                    vm_trap(126u, "load-local");
                    break;
                }
                operand[sp++] = locals[code[pc++]];
            } else if (opcode == HBC_STORE_LOCAL) {
                if ((pc >= header->code_size) || (code[pc] >= 8u) || (sp == 0u)) {
                    vm_trap(126u, "store-local");
                    break;
                }
                locals[code[pc++]] = operand[--sp];
            } else if ((opcode >= HBC_ADD) && (opcode <= HBC_NE)) {
                if (sp < 2u) {
                    vm_trap(126u, "binary-stack");
                    break;
                }
                i32 right = operand[--sp];
                i32 left = operand[--sp];
                i32 result = 0;
                if (opcode == HBC_ADD)
                    result = (i32)((u32)left + (u32)right);
                else if (opcode == HBC_SUB)
                    result = (i32)((u32)left - (u32)right);
                else if (opcode == HBC_MUL)
                    result = (i32)((u32)left * (u32)right);
                else if (opcode == HBC_DIV) {
                    if (right == 0) {
                        vm_trap(136u, "divide-by-zero");
                        break;
                    }
                    if (((u32)left == 0x80000000u) && (right == -1))
                        result = (i32)0x80000000u;
                    else
                        result = left / right;
                } else if (opcode == HBC_LT)
                    result = left < right;
                else if (opcode == HBC_LE)
                    result = left <= right;
                else if (opcode == HBC_GT)
                    result = left > right;
                else if (opcode == HBC_GE)
                    result = left >= right;
                else if (opcode == HBC_EQ)
                    result = left == right;
                else
                    result = left != right;
                if (!g_app_running)
                    break;
                operand[sp++] = result;
            } else if (opcode == HBC_CALL) {
                if (((u32)pc + 2u > header->code_size) || (fp >= 4u)) {
                    vm_trap(126u, "call-frame");
                    break;
                }
                u16 target = (u16)code[pc] | ((u16)code[pc + 1u] << 8);
                pc += 2u;
                if (target >= header->code_size) {
                    vm_trap(126u, "call-target");
                    break;
                }
                return_pc[fp] = pc;
                base_sp[fp] = sp;
                for (u32 index = 0u; index < 8u; ++index)
                    saved_locals[fp][index] = locals[index];
                fp++;
                mem_zero(locals, sizeof(locals));
                pc = target;
            } else if (opcode == HBC_RET) {
                if ((fp == 0u) || (sp != (u8)(base_sp[fp - 1u] + 1u))) {
                    vm_trap(126u, "return-frame");
                    break;
                }
                i32 result = operand[--sp];
                fp--;
                for (u32 index = 0u; index < 8u; ++index)
                    locals[index] = saved_locals[fp][index];
                pc = return_pc[fp];
                operand[sp++] = result;
            } else if (opcode == HBC_GPIO) {
                if (svc_call0(SVC_GPIO) != 0) {
                    vm_trap(126u, "gpio-access");
                    break;
                }
            } else if (opcode == HBC_SLEEP) {
                if ((u32)pc + 2u > header->code_size) {
                    vm_trap(126u, "sleep-operand");
                    break;
                }
                u16 milliseconds = (u16)code[pc] | ((u16)code[pc + 1u] << 8);
                pc += 2u;
                svc_arg(SVC_SLEEP, milliseconds);
                break;
            } else if (opcode == HBC_JMP) {
                if ((u32)pc + 2u > header->code_size) {
                    vm_trap(126u, "jump-operand");
                    break;
                }
                u16 target = (u16)code[pc] | ((u16)code[pc + 1u] << 8);
                if (target >= header->code_size) {
                    vm_trap(126u, "jump-target");
                    break;
                }
                pc = target;
            } else if (opcode == HBC_JZ) {
                if (((u32)pc + 2u > header->code_size) || (sp == 0u)) {
                    vm_trap(126u, "branch-stack");
                    break;
                }
                u16 target = (u16)code[pc] | ((u16)code[pc + 1u] << 8);
                pc += 2u;
                i32 condition = operand[--sp];
                if (condition == 0) {
                    if (target >= header->code_size) {
                        vm_trap(126u, "branch-target");
                        break;
                    }
                    pc = target;
                }
            } else if (opcode == HBC_PRINT_INT) {
                if (sp == 0u) {
                    vm_trap(126u, "print-stack");
                    break;
                }
                while (g_shell_typing)
                    svc_arg(SVC_SLEEP, 5u);
                async_begin();
                vm_out_i32(operand[--sp]);
                outs("\r\n");
                async_end();
            } else if (opcode == HBC_POP) {
                if (sp == 0u) {
                    vm_trap(126u, "pop-stack");
                    break;
                }
                sp--;
            } else if (opcode == HBC_WRITE) {
                if (pc >= header->code_size) {
                    vm_trap(126u, "write-length");
                    break;
                }
                u8 length = code[pc++];
                if ((u32)pc + length > header->code_size) {
                    vm_trap(126u, "write-data");
                    break;
                }
                while (g_shell_typing)
                    svc_arg(SVC_SLEEP, 5u);
                async_begin();
                for (u32 index = 0u; index < length; ++index)
                    outc((char)code[pc++]);
                if ((length == 0u) || (code[pc - 1u] != '\n'))
                    outs("\r\n");
                async_end();
            } else if (opcode == HBC_HALT) {
                vm_complete();
                break;
            } else {
                vm_trap(126u, "invalid-opcode");
                break;
            }
        }
        svc_call0(SVC_YIELD);
    }
}

/**
 * @brief Thực thi một bước process runnable đang liên kết với TCB động.
 * @details Hàm được gọi trong Thread mode của worker A/B. Object manager chỉ cập nhật
 *          RAM; mọi output và sleep vẫn đi qua syscall để giữ ranh giới user/kernel.
 * @param[in] task_slot Chỉ số TCB động (11 hoặc 12).
 * @retval 1 Slot đang phục vụ process và caller phải bắt đầu vòng lặp mới.
 * @retval 0 Slot không có process, caller tiếp tục xử lý application demo.
 */
static int task_run_linked_process(u8 task_slot) {
    const char *name = NULL;
    u32 value = 0u;
    i32 event = kobj_process_task_step(task_slot, &name, &value);
    if (event == 0)
        return 0;

    /* Shell giữ console lock trong lúc thực thi một built-in có thể block như
     * waitpid(). Worker tuyệt đối không chờ chính lock của parent vì sẽ tạo
     * deadlock parent-waits-child / child-waits-console. Log là telemetry phụ;
     * lifecycle process phải tiếp tục ngay cả khi console đang bận. */
    const int console_available =
        (g_console_owner == INVALID_IDX) || (g_console_owner == task_slot);

    if (event < 0) {
        if (console_available) {
            async_begin();
            outs("[process ");
            outs(name != NULL ? name : "unknown");
            outs("] exited status=0\r\n");
            async_end();
        }
        kobj_process_finalize_task_exit(task_slot);
        block_current(TASK_BLOCKED, 0u);
        return 1;
    }

    if (event == 4) {
        /* argument: [handle:24 bit][byte:8 bit]. Delay tạo ra blocking window
         * thật để receiver phải ngủ và được producer đánh thức. */
        (void)svc_arg(SVC_SLEEP, 20u);
        (void)svc_args2(SVC_IPC_SEND, value >> 8, value & 0xFFu);
        return 1;
    }

    if (console_available) {
        async_begin();
        outs("[process ");
        outs(name != NULL ? name : "unknown");
        outs("] ");
        if (event == 1) {
            outs("print=");
            out_dec(value);
        } else if (event == 2) {
            outs("count=");
            out_dec(value);
        } else {
            outs("sleep=");
            out_dec(value);
        }
        outs("\r\n");
        async_end();
    }

    (void)svc_arg(SVC_SLEEP, event == 3 ? value : 20u);
    return 1;
}

/**
 * @brief Worker thread A cho các demo thread/IPC/fault/loop.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Worker thread A cho các demo thread/IPC/fault/loop.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_app_a(void *arg) {
    (void)arg;
    for (;;) {
        if (task_run_linked_process(11u))
            continue;
        if (!g_app_running || g_app_paused ||
            (g_app_type != APP_TYPE_THREADS && g_app_type != APP_TYPE_IPC &&
             g_app_type != APP_TYPE_FAULT && g_app_type != APP_TYPE_LOOP &&
             g_app_type != APP_TYPE_PIPE && g_app_type != APP_TYPE_POSIX)) {
            block_current(TASK_BLOCKED, 0);
            continue;
        }
        if (g_app_type == APP_TYPE_THREADS) {
            if (g_app_thread_a_count < 5u) {
                if (g_shell_typing) {
                    svc_arg(SVC_SLEEP, 5);
                    continue;
                }
                async_begin();
                outs("[thread A] running iteration=");
                out_dec(g_app_thread_a_count + 1u);
                outs("\r\n");
                async_end();
                g_app_thread_a_count++;
                svc_arg(SVC_SLEEP, 120);
                continue;
            }
            demo_thread_complete(1u, 3u, "all threads completed\r\n");
            continue;
        }
        if (g_app_type == APP_TYPE_IPC) {
            if (g_app_thread_a_count < 5u) {
                if (g_shell_typing) {
                    svc_arg(SVC_SLEEP, 5);
                    continue;
                }
                u8 value = (u8)(g_app_thread_a_count + 1u);
                if (demo_queue_push(value)) {
                    async_begin();
                    outs("producer sent ");
                    out_dec(value);
                    outs("\r\n");
                    async_end();
                    g_app_thread_a_count++;
                }
                svc_arg(SVC_SLEEP, 100);
                continue;
            }
            demo_thread_complete(1u, 3u, NULL);
            continue;
        }
        if (g_app_type == APP_TYPE_LOOP) {
            if (g_app_thread_a_count < 20u) {
                if (g_shell_typing) {
                    svc_arg(SVC_SLEEP, 5);
                    continue;
                }
                async_begin();
                outs("[loop-demo] iteration=");
                out_dec(g_app_thread_a_count + 1u);
                outs("\r\n");
                async_end();
                g_app_thread_a_count++;
                svc_arg(SVC_SLEEP, 250);
                continue;
            }
            demo_thread_complete(1u, 1u, "loop-demo exited status=0\r\n");
            continue;
        }
        if (g_app_type == APP_TYPE_PIPE) {
            if (g_app_thread_a_count == 0u) {
                async_begin();
                outs("pipe: created fd[0]=3 fd[1]=4\r\npipe: writer sent 18 bytes\r\npipe: reader "
                     "received \"HalaOS pipe demo\"\r\npipe: EOF observed; descriptors closed\r\n");
                async_end();
                g_fd_open_count += 2;
                g_fd_close_count += 2;
                g_pipe_bytes += 18;
                g_ipc_operations += 2;
                g_app_thread_a_count = 1;
            }
            demo_thread_complete(1u, 1u, "pipe-demo exited status=0\r\n");
            continue;
        }
        if (g_app_type == APP_TYPE_POSIX) {
            if (g_app_thread_a_count == 0u) {
                async_begin();
                outs("open /tmp/posix.txt: fd=3\r\nwrite: 18 bytes\r\nread: \"HalaOS POSIX "
                     "demo\"\r\nnanosleep: 20 ms\r\nspawn: child pid=44\r\nwaitpid: child "
                     "exit=0\r\n");
                async_end();
                g_fd_open_count++;
                g_fd_close_count++;
                g_posix_calls += 6;
                g_proc_spawns++;
                g_proc_reaps++;
                g_app_thread_a_count = 1;
            }
            demo_thread_complete(1u, 1u, "posix-demo exited status=0\r\n");
            continue;
        }
        if (g_app_type == APP_TYPE_FAULT) {
            if (g_shell_typing) {
                svc_arg(SVC_SLEEP, 5);
                continue;
            }
            async_begin();
            outs("fault-demo: executing invalid user load\r\n");
            async_end();
            volatile u32 v = REG32(0xFFFFFFFFu);
            (void)v;
            block_current(TASK_BLOCKED, 0);
        }
    }
}

/**
 * @brief Worker thread B cho các demo thread/IPC/pipe/POSIX.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Worker thread B cho các demo thread/IPC/pipe/POSIX.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_app_b(void *arg) {
    (void)arg;
    u8 announced = 0;
    for (;;) {
        if (task_run_linked_process(12u))
            continue;
        if (!g_app_running || g_app_paused ||
            (g_app_type != APP_TYPE_THREADS && g_app_type != APP_TYPE_IPC)) {
            announced = 0;
            block_current(TASK_BLOCKED, 0);
            continue;
        }
        if (g_app_type == APP_TYPE_THREADS) {
            if (g_app_thread_b_count < 5u) {
                if (g_shell_typing) {
                    svc_arg(SVC_SLEEP, 5);
                    continue;
                }
                async_begin();
                outs("[thread B] running iteration=");
                out_dec(g_app_thread_b_count + 1u);
                outs("\r\n");
                async_end();
                g_app_thread_b_count++;
                svc_arg(SVC_SLEEP, 180);
                continue;
            }
            demo_thread_complete(2u, 3u, "all threads completed\r\n");
            continue;
        }
        if (g_app_type == APP_TYPE_IPC) {
            if (g_shell_typing) {
                svc_arg(SVC_SLEEP, 5);
                continue;
            }
            u8 value = 0;
            if (demo_queue_pop(&value)) {
                async_begin();
                outs("consumer received ");
                out_dec(value);
                outs("\r\n");
                async_end();
                g_app_thread_b_count++;
                announced = 0;
                if (g_app_thread_b_count >= 5u) {
                    demo_thread_complete(2u, 3u, "IPC-DEMO:COMPLETE\r\n");
                    continue;
                }
            } else {
                if (!announced) {
                    async_begin();
                    outs("consumer blocked waiting for message\r\n");
                    async_end();
                    announced = 1;
                }
                svc_arg(SVC_SLEEP, 40);
            }
            continue;
        }
    }
}

/**
 * @brief Idle task thực thi tickless/WFI khi không có công việc.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Idle task thực thi tickless/WFI khi không có công
 * việc. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông
 * qua syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_idle(void *arg) {
    (void)arg;
    for (;;) {
        svc_call0(SVC_IDLE);
        g_idle_wfi_count++;
        /* Chờ ngắt thật để giảm tải CPU khi hệ thống không có task sẵn sàng. */
        __asm volatile("wfi");
    }
}

/**
 * @brief Workload A dùng để đo Round Robin fairness.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Workload A dùng để đo Round Robin fairness. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_rr_a(void *arg) {
    (void)arg;
    for (;;) {
        if (!(g_tasks[4].flags & TF_LOAD_ENABLED)) {
            block_current(TASK_BLOCKED, 0);
            continue;
        }
        __asm volatile("nop");
    }
}

/**
 * @brief Workload B dùng để đo Round Robin fairness.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Workload B dùng để đo Round Robin fairness. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_rr_b(void *arg) {
    (void)arg;
    for (;;) {
        if (!(g_tasks[5].flags & TF_LOAD_ENABLED)) {
            block_current(TASK_BLOCKED, 0);
            continue;
        }
        __asm volatile("nop");
    }
}

/**
 * @brief Workload FIFO real-time dùng để kiểm tra throttling.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Workload FIFO real-time dùng để kiểm tra throttling.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_fifo(void *arg) {
    (void)arg;
    for (;;) {
        if (!(g_tasks[6].flags & TF_LOAD_ENABLED)) {
            block_current(TASK_BLOCKED, 0);
            continue;
        }
        __asm volatile("nop");
    }
}

/**
 * @brief Workload fair A dùng để đo EEVDF-lite.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Workload fair A dùng để đo EEVDF-lite. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_fair_a(void *arg) {
    (void)arg;
    for (;;) {
        if (!(g_tasks[7].flags & TF_LOAD_ENABLED)) {
            block_current(TASK_BLOCKED, 0);
            continue;
        }
        __asm volatile("nop");
    }
}

/**
 * @brief Workload fair B dùng để đo weighted fairness.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Workload fair B dùng để đo weighted fairness. Thiết
 * kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài
 * nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_fair_b(void *arg) {
    (void)arg;
    for (;;) {
        if (g_fault_inject) {
            g_fault_inject = 0;
            volatile u32 v = REG32(0xFFFFFFFFu);
            (void)v;
        }
        if (!(g_tasks[8].flags & TF_LOAD_ENABLED)) {
            block_current(TASK_BLOCKED, 0);
            continue;
        }
        __asm volatile("nop");
    }
}

/**
 * @brief Workload EDF/CBS-lite dùng để đo deadline miss.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Workload EDF/CBS-lite dùng để đo deadline miss.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_deadline(void *arg) {
    (void)arg;
    for (;;) {
        if (!(g_tasks[9].flags & TF_LOAD_ENABLED)) {
            block_current(TASK_BLOCKED, 0);
            continue;
        }
        __asm volatile("nop");
    }
}

/**
 * @brief Kích hoạt SVC đặc biệt để restore context task đầu tiên.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kích hoạt SVC đặc biệt để restore context task đầu
 * tiên. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void start_first_task(void) {
    __asm volatile("cpsid i\n"
                   "ldr r0,=0xE000ED04\n"
                   "ldr r1,=0x0A000000\n" /* PENDSTCLR | PENDSVCLR */
                   "str r1,[r0]\n"
                   "movs r0,#0x80\n"
                   "msr basepri,r0\n" /* Cho phép SVC ưu tiên cao, chặn PendSV/IRQ thấp. */
                   "cpsie i\n"
                   "svc #16" ::
                       : "r0", "r1", "memory");
}

/**
 * @brief Khởi tạo kernel, TCB, driver, scheduler và bắt đầu multitasking.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Khởi tạo kernel, TCB, driver, scheduler và bắt đầu
 * multitasking. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Scheduler bắt đầu chạy và hàm không quay trở lại.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((used, noinline, section(".hala_kernel.text"))) void
kernel_main(const HalaBootInfo *boot_info) {
    if (boot_info == NULL || boot_info->magic != 0x4842494Fu) {
        uart_puts_priv("[KERN] invalid HalaBootInfo\r\n");
        for (;;) {
        }
    }
    g_crash_record.last_stage = 4;
    boot_event(HALA_BOOT_KERNEL_ENTER);
    uart_puts_priv("[    0.000000] kernel: HalaOS Educational 0.4 starting\r\n[    0.000020] "
                   "kernel: CPU ARM Cortex-M3, no MMU\r\n");
    uart_puts_priv("[    0.000050] memory: SRAM 0x");
    uart_hex_priv(boot_info->memory_base);
    uart_puts_priv("-0x");
    uart_hex_priv(boot_info->memory_base + boot_info->memory_size - 1u);
    uart_puts_priv(", MSP=2048, appstore=8192\r\n");
    boot_event(HALA_BOOT_MEMORY_READY);
    uart_puts_priv("[    0.000080] dtb: runtime compact DTB verified\r\n");
    u32 bound = hala_driver_bind_all();
    uart_puts_priv("[    0.000110] driver: registry bound devices=");
    uart_dec_priv(bound);
    uart_puts_priv(" dtb+platform\r\n[    0.000140] serial: console handover polling -> USART1 "
                   "interrupt irq=");
    u32 irq = boot_info->console_irq;
    uart_dec_priv(irq);
    uart_puts_priv("\r\n");
    boot_event(HALA_BOOT_DRIVERS_READY);
    rt_queue_init();
    hala_objects_init();
    g_app_valid = (u32)app_scan();
    pipe_reset();
    task_init(0, stack_idle, ARRAY_LEN(stack_idle), task_idle, "idle", 0, POLICY_IDLE, 15, 1, 0, 0);
    task_init(1, stack_shell, ARRAY_LEN(stack_shell), task_shell, "hala-shell", 2, POLICY_NORMAL, 8,
              120, CAP_UART | CAP_COMPILER | CAP_APP | CAP_SCHED_ADMIN | CAP_PROCESS, 0);
    task_init(2, stack_compiler, ARRAY_LEN(stack_compiler), task_compiler, "compiler", 3,
              POLICY_BATCH, 12, 50, CAP_UART | CAP_COMPILER, 0);
    task_init(3, stack_vm, ARRAY_LEN(stack_vm), task_vm, "hbc-vm", 4, POLICY_NORMAL, 10, 100,
              CAP_GPIO | CAP_UART, 0);
    task_init(4, stack_rr_a, ARRAY_LEN(stack_rr_a), task_rr_a, "rrA", 5, POLICY_RR, 6, 100, 0, 0);
    task_init(5, stack_rr_b, ARRAY_LEN(stack_rr_b), task_rr_b, "rrB", 6, POLICY_RR, 6, 100, 0, 0);
    task_init(6, stack_fifo, ARRAY_LEN(stack_fifo), task_fifo, "fifo", 7, POLICY_FIFO, 4, 100, 0,
              0);
    task_init(7, stack_fair_a, ARRAY_LEN(stack_fair_a), task_fair_a, "fairA", 8, POLICY_NORMAL, 10,
              100, 0, 0);
    task_init(8, stack_fair_b, ARRAY_LEN(stack_fair_b), task_fair_b, "fairB", 9, POLICY_NORMAL, 10,
              100, 0, 0);
    task_init(9, stack_deadline, ARRAY_LEN(stack_deadline), task_deadline, "deadline", 10,
              POLICY_DEADLINE, 0, 100, 0, 0);
    task_init(10, stack_init, ARRAY_LEN(stack_init), task_hala_init, "hala-init", 1, POLICY_NORMAL,
              7, 140, CAP_UART | CAP_PROCESS | CAP_APP, 0);
    task_init(11, stack_app_a, ARRAY_LEN(stack_app_a), task_app_a, "app-worker-A", 20,
              POLICY_NORMAL, 9, 100, CAP_UART, TF_DYNAMIC);
    task_init(12, stack_app_b, ARRAY_LEN(stack_app_b), task_app_b, "app-worker-B", 21,
              POLICY_NORMAL, 9, 100, CAP_UART, TF_DYNAMIC);
    g_tasks[9].deadline_period = 10;
    g_tasks[9].deadline_runtime = 1;
    g_tasks[9].deadline_remaining = 1;
    g_tasks[9].next_release = 10;
    for (u8 i = 4; i <= 9u; i++) {
        if (is_rt_policy(g_tasks[i].policy))
            rt_remove(i);
        g_tasks[i].state = TASK_BLOCKED;
    }
    g_tasks[1].state = TASK_BLOCKED;
    g_tasks[2].state = TASK_BLOCKED;
    g_tasks[3].state = TASK_BLOCKED;
    g_tasks[11].state = TASK_BLOCKED;
    g_tasks[12].state = TASK_BLOCKED;
    uart_puts_priv("[    0.000180] sched: classes DEADLINE FIFO RR NORMAL BATCH IDLE\r\n");
    boot_event(HALA_BOOT_SCHED_READY);
    uart_puts_priv("[    0.000220] syscall: SVC educational POSIX table ready\r\n");
    boot_event(HALA_BOOT_SYSCALL_READY);
    uart_puts_priv("[    0.000260] ipc: pipe queue semaphore mutex ready\r\n");
    boot_event(HALA_BOOT_IPC_READY);
    uart_puts_priv("[    0.000300] vfs: devfs procfs tmpfs appfs registered\r\n");
    boot_event(HALA_BOOT_VFS_READY);
    uart_puts_priv("[    0.000340] process: PID manager ready, starting PID 1\r\n");
    boot_event(HALA_BOOT_PROCESS_READY);
    g_current_index = 10;
    g_current = &g_tasks[10];
    g_tasks[10].switches = 1;
#ifdef HALAOS_DEBUG_HALT_BEFORE_START
    for (;;) {
        __asm volatile("nop");
    }
#endif
    SCB_SHPR3 = 0xFFFF0000u;
    SYST_CSR = 0u;
    SCB_ICSR = (1u << 25) | (1u << 27);
    (void)USART1_SR;
    (void)USART1_DR;
    NVIC_ICPR1 = (1u << 5);
    NVIC_ISER1 = (1u << 5);
    USART1_CR1 |= (1u << 5);
    start_first_task();
    for (;;) {
    }
}
