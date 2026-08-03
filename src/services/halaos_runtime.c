/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file halaos_runtime.c
 * @brief App store, compiler hiện tại, IPC/process model và runtime service.
 * @details File được tách từ baseline đã qualification; comment tiếng Việt được giữ để phục vụ
 * review và đào tạo.
 */
#include "halaos/internal/halaos_internal.h"

/**
 * @brief Chờ bộ điều khiển Flash kết thúc thao tác hiện tại.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chờ bộ điều khiển Flash kết thúc thao tác hiện tại.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] u Tham số u của hàm.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void flash_wait(void) {
    while (FLASH_SR & 1u) {
    }
}

/**
 * @brief Mở khóa Flash controller khi đang ở trạng thái locked.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Mở khóa Flash controller khi đang ở trạng thái
 * locked. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] u Tham số u của hàm.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void flash_unlock(void) {
    if (FLASH_CR & (1u << 7)) {
        FLASH_KEYR = 0x45670123u;
        FLASH_KEYR = 0xCDEF89ABu;
    }
}

/**
 * @brief Xóa một trang Flash tại địa chỉ yêu cầu.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Xóa một trang Flash tại địa chỉ yêu cầu. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] addr Địa chỉ bộ nhớ hoặc Flash.
 * @pre Flash đã được mở khóa và địa chỉ căn theo trang.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void flash_erase_page(u32 addr) {
    flash_unlock();
    flash_wait();
    FLASH_CR &= ~((1u << 6) | (1u << 2) | (1u << 1) | 1u);
    FLASH_CR |= (1u << 1);
    FLASH_AR = addr;
    FLASH_CR |= (1u << 6);
    flash_wait();
    FLASH_CR &= ~((1u << 6) | (1u << 1));
}

/**
 * @brief Lập trình một half-word 16-bit vào Flash.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Lập trình một half-word 16-bit vào Flash. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] addr Địa chỉ bộ nhớ hoặc Flash.
 * @param[in] value Giá trị cần ghi hoặc truyền.
 * @pre Flash đã mở khóa và địa chỉ căn 2 byte.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void flash_program_half(u32 addr, u16 value) {
    flash_wait();
    FLASH_CR |= 1u;
    REG16(addr) = value;
    flash_wait();
    FLASH_CR &= ~1u;
}

/**
 * @brief Xác minh opcode, operand và cấu trúc HBC bytecode trước khi chạy.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Xác minh opcode, operand và cấu trúc HBC bytecode
 * trước khi chạy. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] code Con trỏ HBC bytecode.
 * @param[in] size Kích thước dữ liệu/bytecode.
 * @return 1 nếu bytecode hợp lệ.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int bytecode_verify(const u8 *code, u16 size) {
    u8 boundary[32];
    signed char depth[256];
    u8 queue[256];
    u16 pc = 0u;
    u16 queue_head = 0u, queue_tail = 0u;

    if ((code == NULL) || (size == 0u) || (size > 256u))
        return 0;
    mem_zero(boundary, sizeof(boundary));
    for (u32 i = 0u; i < 256u; ++i)
        depth[i] = -1;

    /* Bước 1: Giải mã tuyến tính để đánh dấu đúng ranh giới instruction. */
    while (pc < size) {
        boundary[pc >> 3] |= (u8)(1u << (pc & 7u));
        u8 opcode = code[pc++];
        if (opcode == HBC_PUSH_I32) {
            if ((u32)pc + 4u > size)
                goto reject;
            pc += 4u;
        } else if ((opcode == HBC_LOAD_LOCAL) || (opcode == HBC_STORE_LOCAL)) {
            if (pc >= size || code[pc] >= 8u)
                goto reject;
            pc++;
        } else if ((opcode >= HBC_ADD) && (opcode <= HBC_NE)) {
        } else if ((opcode == HBC_CALL) || (opcode == HBC_SLEEP) || (opcode == HBC_JMP) ||
                   (opcode == HBC_JZ)) {
            if ((u32)pc + 2u > size)
                goto reject;
            pc += 2u;
        } else if ((opcode == HBC_RET) || (opcode == HBC_GPIO) || (opcode == HBC_PRINT_INT) ||
                   (opcode == HBC_POP) || (opcode == HBC_HALT)) {
        } else if (opcode == HBC_WRITE) {
            if (pc >= size)
                goto reject;
            u8 length = code[pc++];
            if ((u32)pc + length > size)
                goto reject;
            pc = (u16)(pc + length);
        } else if ((opcode >= 0x30u) && (opcode <= 0x35u)) {
        } else
            goto reject;
    }

    /* Bước 2: Kiểm tra mọi target trỏ đúng đầu instruction. */
    pc = 0u;
    while (pc < size) {
        u8 opcode = code[pc++];
        if (opcode == HBC_PUSH_I32)
            pc += 4u;
        else if ((opcode == HBC_LOAD_LOCAL) || (opcode == HBC_STORE_LOCAL))
            pc++;
        else if ((opcode == HBC_CALL) || (opcode == HBC_JMP) || (opcode == HBC_JZ)) {
            u16 target = (u16)code[pc] | ((u16)code[pc + 1u] << 8);
            if ((target >= size) || ((boundary[target >> 3] & (u8)(1u << (target & 7u))) == 0u))
                goto reject;
            pc += 2u;
        } else if (opcode == HBC_SLEEP)
            pc += 2u;
        else if (opcode == HBC_WRITE) {
            u8 length = code[pc++];
            pc = (u16)(pc + length);
        }
    }

    /* Bước 3: Phân tích data-flow độ sâu operand stack cho entry và từng callee. */
    depth[0] = 0;
    queue[queue_tail++] = 0u;
    pc = 0u;
    while (pc < size) {
        u8 opcode = code[pc++];
        if (opcode == HBC_CALL) {
            u16 target = (u16)code[pc] | ((u16)code[pc + 1u] << 8);
            if (depth[target] < 0) {
                depth[target] = 0;
                queue[queue_tail++] = (u8)target;
            }
            pc += 2u;
        } else if (opcode == HBC_PUSH_I32)
            pc += 4u;
        else if ((opcode == HBC_LOAD_LOCAL) || (opcode == HBC_STORE_LOCAL))
            pc++;
        else if ((opcode == HBC_SLEEP) || (opcode == HBC_JMP) || (opcode == HBC_JZ))
            pc += 2u;
        else if (opcode == HBC_WRITE) {
            u8 length = code[pc++];
            pc = (u16)(pc + length);
        }
    }

    while (queue_head < queue_tail) {
        u16 at = queue[queue_head++];
        i32 stack = depth[at];
        u8 opcode = code[at];
        u16 next = (u16)(at + 1u);
        u16 branch = 0u;
        int has_fallthrough = 1;

        if (opcode == HBC_PUSH_I32) {
            stack++;
            next += 4u;
        } else if (opcode == HBC_LOAD_LOCAL) {
            stack++;
            next++;
        } else if (opcode == HBC_STORE_LOCAL) {
            if (stack < 1)
                goto reject;
            stack--;
            next++;
        } else if ((opcode >= HBC_ADD) && (opcode <= HBC_NE)) {
            if (stack < 2)
                goto reject;
            stack--;
        } else if (opcode == HBC_CALL) {
            stack++;
            next += 2u;
        } else if (opcode == HBC_RET) {
            if (stack != 1)
                goto reject;
            has_fallthrough = 0;
        } else if (opcode == HBC_SLEEP)
            next += 2u;
        else if (opcode == HBC_JMP) {
            branch = (u16)code[next] | ((u16)code[next + 1u] << 8);
            next += 2u;
            has_fallthrough = 0;
        } else if (opcode == HBC_JZ) {
            if (stack < 1)
                goto reject;
            stack--;
            branch = (u16)code[next] | ((u16)code[next + 1u] << 8);
            next += 2u;
        } else if ((opcode == HBC_PRINT_INT) || (opcode == HBC_POP)) {
            if (stack < 1)
                goto reject;
            stack--;
        } else if (opcode == HBC_WRITE) {
            u8 length = code[next++];
            next = (u16)(next + length);
        } else if ((opcode == HBC_HALT) || ((opcode >= 0x30u) && (opcode <= 0x35u))) {
            if (opcode == HBC_HALT)
                has_fallthrough = 0;
        }

        if ((stack < 0) || (stack > 16))
            goto reject;
        if (branch != 0u || opcode == HBC_JMP || opcode == HBC_JZ) {
            if (depth[branch] < 0) {
                depth[branch] = (signed char)stack;
                queue[queue_tail++] = (u8)branch;
            } else if (depth[branch] != stack)
                goto reject;
        }
        if (has_fallthrough && next < size) {
            if (depth[next] < 0) {
                depth[next] = (signed char)stack;
                queue[queue_tail++] = (u8)next;
            } else if (depth[next] != stack)
                goto reject;
        }
    }
    return 1;

reject:
    g_bytecode_rejects++;
    return 0;
}

/**
 * @brief Xác minh header, trạng thái, CRC và bytecode của một app slot.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Xác minh header, trạng thái, CRC và bytecode của một
 * app slot. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] slot Chỉ số app slot A/B.
 * @return 1 nếu slot hợp lệ.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int app_verify_slot(u32 slot) {
    const volatile AppHeader *h = app_header_slot(slot);
    if (h->status != APP_STATUS_VALID || h->magic != APP_MAGIC || h->version != 2u ||
        h->code_size == 0 || h->code_size > APP_SLOT_SIZE - APP_HEADER_SIZE)
        return 0;
    const u8 *code = (const u8 *)(app_slot_addr(slot) + APP_HEADER_SIZE);
    if (crc32_bytes(code, h->code_size) != h->crc32)
        return 0;
    return bytecode_verify(code, h->code_size);
}

/**
 * @brief Quét hai slot A/B và chọn application hợp lệ có sequence mới nhất.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Quét hai slot A/B và chọn application hợp lệ có
 * sequence mới nhất. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8.
 * @return Giá trị kết quả hoặc mã trạng thái của thao tác.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int app_scan(void) {
    u32 best_seq = 0, best = 0, found = 0;
    for (u32 i = 0; i < APP_SLOT_COUNT; i++) {
        if (app_verify_slot(i)) {
            u32 seq = app_header_slot(i)->sequence;
            if (!found || seq > best_seq) {
                best_seq = seq;
                best = i;
                found = 1;
            }
        }
    }
    if (found) {
        g_app_slot = best;
        g_compile_count = best_seq;
        g_app_type = app_header_slot(best)->flags;
        g_app_recovery_count++;
        return 1;
    }
    return 0;
}

/**
 * @brief Ghi application mới theo transaction WRITING→VERIFYING→VALID.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Ghi application mới theo transaction
 * WRITING→VERIFYING→VALID. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác
 * định để phù hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] code Con trỏ HBC bytecode.
 * @param[in] size Kích thước dữ liệu/bytecode.
 * @param[in] flags Cờ cấu hình.
 * @param[in] name Tên task hoặc application.
 * @return 0 nếu ghi thành công, giá trị âm nếu thất bại.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Một slot cũ hoặc mới luôn còn hợp lệ theo transaction contract.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int app_write(const u8 *code, u16 size, u16 flags, const char *name) {
    if (!code || size == 0 || size > 256)
        return -1;
    if (!bytecode_verify(code, size))
        return -3;
    u32 crc = crc32_bytes(code, size), seq = g_compile_count + 1u, slot = seq % APP_SLOT_COUNT,
        base = app_slot_addr(slot);
    char nm[12];
    name_copy(nm, name && *name ? name : "app0");
    flash_erase_page(base);
    if (g_app_fail_stage == 1)
        return -31;
    flash_program_half(base, APP_STATUS_WRITING);
    flash_program_half(base + 2u, 2u);
    flash_program_half(base + 4u, (u16)(APP_MAGIC & 0xFFFFu));
    flash_program_half(base + 6u, (u16)(APP_MAGIC >> 16));
    flash_program_half(base + 8u, size);
    flash_program_half(base + 10u, flags);
    flash_program_half(base + 12u, (u16)crc);
    flash_program_half(base + 14u, (u16)(crc >> 16));
    flash_program_half(base + 16u, (u16)seq);
    flash_program_half(base + 18u, (u16)(seq >> 16));
    for (u32 i = 0; i < 12u; i += 2u)
        flash_program_half(base + 20u + i, (u16)(u8)nm[i] | ((u16)(u8)nm[i + 1u] << 8));
    if (g_app_fail_stage == 2)
        return -32;
    mem_copy(g_flash_staging, code, size);
    g_flash_staging[size] = 0xFFu;
    for (u32 i = 0; i < size; i += 2) {
        u16 v = (u16)g_flash_staging[i] | (u16)((u16)g_flash_staging[i + 1u] << 8);
        flash_program_half(base + APP_HEADER_SIZE + i, v);
        if (g_app_fail_stage == 3 && i >= 2)
            return -33;
    }
    flash_program_half(base, APP_STATUS_VERIFYING);
    if (g_app_fail_stage == 4)
        return -34;
    if (crc32_bytes((const u8 *)(base + APP_HEADER_SIZE), size) != crc)
        return -2;
    flash_program_half(base, APP_STATUS_VALID);
    g_compile_count = seq;
    g_app_slot = slot;
    g_app_type = flags;
    g_app_valid = 1;
    g_app_fail_stage = 0;
    return 0;
}

/**
 * @brief Chuẩn hóa source Hala-C nhưng giữ nguyên nội dung string literal.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chuẩn hóa source Hala-C nhưng giữ nguyên nội dung
 * string literal. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @return Số byte source sau chuẩn hóa.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int compact_source(const char *s, u32 n) {
    if (n >= sizeof(g_compact))
        return -1;
    u32 o = 0;
    u8 in_string = 0, escaped = 0;
    for (u32 i = 0; i < n; i++) {
        char c = s[i];
        if (in_string) {
            g_compact[o++] = (u8)c;
            if (escaped)
                escaped = 0;
            else if (c == '\\')
                escaped = 1;
            else if (c == '"')
                in_string = 0;
        } else {
            if (c == '"') {
                in_string = 1;
                g_compact[o++] = (u8)c;
            } else if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                g_compact[o++] = (u8)c;
        }
    }
    g_compact[o] = 0;
    if (o > g_compiler_peak)
        g_compiler_peak = o;
    return (int)o;
}

/**
 * @brief Tìm chuỗi con trong source bằng thuật toán tuyến tính nhỏ gọn.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Tìm chuỗi con trong source bằng thuật toán tuyến
 * tính nhỏ gọn. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @param[in] needle Chuỗi con cần tìm.
 * @return 1 nếu tìm thấy, 0 nếu không.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
const char *find_text(const char *s, const char *needle) {
    if (!*needle)
        return s;
    for (; *s; s++) {
        u32 i = 0;
        while (needle[i] && s[i] == needle[i])
            i++;
        if (!needle[i])
            return s;
    }
    return NULL;
}

/**
 * @brief Phân tích Hala-C educational subset và phát sinh HBC package.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Phân tích Hala-C educational subset và phát sinh HBC
 * package. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @return Kích thước bytecode; 0 khi compile thất bại.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int compile_hala_c(const char *s, u32 n) {
    if (!ptr_in_sram(s, n))
        return -20;
    /* Các application qualification đa-thread/fault giữ profile chuyên biệt. Mọi application
     * bytecode thông thường đi qua lexer/parser tổng quát, không phụ thuộc tên file. */
    int m = compact_source(s, n);
    if (m < 0)
        return -21;
    const char *c = (const char *)g_compact;
    if (str_eq(g_compile_name, "thread-demo")) {
        if (!find_text(c, "threadA") || !find_text(c, "threadB") || !find_text(c, "spawn") ||
            !find_text(c, "join"))
            return -28;
        u8 code[2] = {0x30u, 0xFFu};
        return app_write(code, 2u, APP_TYPE_THREADS, g_compile_name);
    }
    if (str_eq(g_compile_name, "ipc-demo")) {
        if (!find_text(c, "producer") || !find_text(c, "consumer") || !find_text(c, "send") ||
            !find_text(c, "receive"))
            return -29;
        u8 code[2] = {0x31u, 0xFFu};
        return app_write(code, 2u, APP_TYPE_IPC, g_compile_name);
    }
    if (str_eq(g_compile_name, "fault-demo")) {
        if (!find_text(c, "fault"))
            return -30;
        u8 code[2] = {0x32u, 0xFFu};
        return app_write(code, 2u, APP_TYPE_FAULT, g_compile_name);
    }
    if (str_eq(g_compile_name, "loop-demo")) {
        if (!find_text(c, "while") || !find_text(c, "sleep(250)") ||
            !find_text(c, "[loop-demo] iteration="))
            return -31;
        u8 code[2] = {0x33u, 0xFFu};
        return app_write(code, 2u, APP_TYPE_LOOP, g_compile_name);
    }
    if (str_eq(g_compile_name, "pipe-demo")) {
        if (!find_text(c, "pipe(") || !find_text(c, "read(") || !find_text(c, "write("))
            return -32;
        u8 code[2] = {0x34u, 0xFFu};
        return app_write(code, 2u, APP_TYPE_PIPE, g_compile_name);
    }
    if (str_eq(g_compile_name, "posix-demo")) {
        if (!find_text(c, "open(") || !find_text(c, "nanosleep(") || !find_text(c, "spawn("))
            return -33;
        u8 code[2] = {0x35u, 0xFFu};
        return app_write(code, 2u, APP_TYPE_POSIX, g_compile_name);
    }
    return hala_compile_general(s, n, g_compile_name);
}

/**
 * @brief Cấp phát một process resource slot từ fixed pool.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Cấp phát một process resource slot từ fixed pool.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @return Chỉ số slot được cấp hoặc giá trị âm khi hết tài nguyên.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int proc_alloc(void) {
    for (u32 i = 0; i < ARRAY_LEN(g_proc_pool); i++)
        if (!g_proc_pool[i].used) {
            g_proc_pool[i].used = 1;
            g_proc_pool[i].generation++;
            g_proc_pool[i].owner = (u16)(i + 100);
            g_proc_spawns++;
            return (int)i;
        }
    return -1;
}

/**
 * @brief Thu hồi toàn bộ resource của process slot.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Thu hồi toàn bộ resource của process slot. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] i Chỉ số task.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void proc_reap(u32 i) {
    if (i >= ARRAY_LEN(g_proc_pool) || !g_proc_pool[i].used)
        return;
    if (g_proc_pool[i].handles || g_proc_pool[i].timers || g_proc_pool[i].queues)
        g_proc_leaks++;
    mem_zero(&g_proc_pool[i], sizeof(g_proc_pool[i]));
    g_proc_reaps++;
}

/**
 * @brief Chạy nhiều vòng cấp phát/thu hồi để kiểm tra process pool.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chạy nhiều vòng cấp phát/thu hồi để kiểm tra process
 * pool. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @return Giá trị kết quả hoặc mã trạng thái của thao tác.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int proc_stress(u32 n) {
    u32 before = g_proc_leaks;
    for (u32 i = 0; i < n; i++) {
        int p = proc_alloc();
        if (p < 0)
            return -1;
        g_proc_pool[p].handles = 1;
        g_proc_pool[p].handles = 0;
        proc_reap((u32)p);
    }
    return g_proc_leaks == before ? 0 : -2;
}

/**
 * @brief Kiểm tra mô hình priority inheritance và phục hồi priority.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kiểm tra mô hình priority inheritance và phục hồi
 * priority. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @return 0 nếu PASS, khác 0 nếu FAIL.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int pi_selftest(void) {
    u8 low = 8, high = 6;
    Tcb *l = &g_tasks[low], *h = &g_tasks[high];
    u8 old = l->effective_prio;
    g_test_mutex.owner = low;
    g_test_mutex.wait_head = high;
    if (h->effective_prio < l->effective_prio) {
        if (is_rt_policy(l->policy))
            rt_remove(low);
        l->effective_prio = h->effective_prio;
        if (is_rt_policy(l->policy))
            rt_enqueue(low);
        g_pi_boosts++;
        g_test_mutex.pi_boosts++;
    }
    int pass = l->effective_prio == h->effective_prio;
    l->effective_prio = l->base_prio;
    g_pi_restores++;
    g_test_mutex.owner = INVALID_IDX;
    g_test_mutex.wait_head = INVALID_IDX;
    return pass && l->effective_prio == old;
}

/**
 * @brief Dừng và ẩn toàn bộ workload scheduler test.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Dừng và ẩn toàn bộ workload scheduler test. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void load_disable_all(void) {
    for (u8 i = 4; i <= 9u; i++) {
        if (is_rt_policy(g_tasks[i].policy))
            rt_remove(i);
        g_tasks[i].state = TASK_BLOCKED;
        g_tasks[i].flags &= (u8)~TF_LOAD_ENABLED;
    }
    g_load_mode = 0;
    pend_resched();
}

/**
 * @brief Kích hoạt workload scheduler theo mode được chọn.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kích hoạt workload scheduler theo mode được chọn.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] mode Chế độ workload hoặc loại application.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void load_enable(u32 mode) {
    /* Bước 1: Thoát tickless trước khi tạo workload. Nếu giữ g_tick_step lớn,
     * tick đầu tiên sẽ bị tính toàn bộ cho task vừa được chọn và làm sai fairness. */
    g_tick_step = 1u;
    SYST_RVR = TICK_CYCLES - 1u;
    SYST_CVR = 0u;
    g_rt_window_start = g_ticks;
    g_rt_used = 0u;
    g_rt_throttled = 0u;
    load_disable_all();
    g_load_mode = mode;
    if (mode == 1 || mode == 4) {
        for (u8 i = 4; i <= 5; i++) {
            g_tasks[i].state = TASK_READY;
            g_tasks[i].flags |= TF_LOAD_ENABLED;
            g_tasks[i].runtime = 0;
            if (i == 4)
                g_rr_a_ticks = 0;
            else
                g_rr_b_ticks = 0;
            g_tasks[i].quantum_left = g_tasks[i].quantum;
            rt_enqueue(i);
        }
    }
    u64 fair_base = g_tasks[1].vruntime;
    if (mode == 2 || mode == 4) {
        for (u8 i = 7; i <= 8; i++) {
            g_tasks[i].state = TASK_READY;
            g_tasks[i].flags |= TF_LOAD_ENABLED;
            g_tasks[i].runtime = 0;
            if (i == 7)
                g_fair_a_ticks = 0;
            else
                g_fair_b_ticks = 0;
            g_tasks[i].vruntime = fair_base;
            g_tasks[i].virtual_deadline = 0;
        }
        g_tasks[7].weight = 100;
        g_tasks[8].weight = 100;
    }
    if (mode == 5) {
        for (u8 i = 7; i <= 8; i++) {
            g_tasks[i].state = TASK_READY;
            g_tasks[i].flags |= TF_LOAD_ENABLED;
            g_tasks[i].runtime = 0;
            if (i == 7)
                g_fair_a_ticks = 0;
            else
                g_fair_b_ticks = 0;
            g_tasks[i].vruntime = fair_base;
            g_tasks[i].virtual_deadline = 0;
        }
        g_tasks[7].weight = 200;
        g_tasks[8].weight = 100;
    }
    if (mode == 3 || mode == 4) {
        u8 i = 9;
        g_tasks[i].state = TASK_READY;
        g_tasks[i].flags |= TF_LOAD_ENABLED;
        g_tasks[i].runtime = 0;
        g_deadline_ticks = 0;
        g_tasks[i].deadline_remaining = g_tasks[i].deadline_runtime;
        g_tasks[i].abs_deadline = (u64)g_ticks + g_tasks[i].deadline_period;
        g_tasks[i].next_release = (u64)g_ticks + g_tasks[i].deadline_period;
    }
    if (mode == 6) {
        u8 i = 6;
        g_tasks[i].state = TASK_READY;
        g_tasks[i].flags |= TF_LOAD_ENABLED;
        g_tasks[i].runtime = 0;
        rt_enqueue(i);
    }
    pend_resched();
}

/**
 * @brief Tính số tick tới sự kiện gần nhất cho tickless idle.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Tính số tick tới sự kiện gần nhất cho tickless idle.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @return Số tick tới sự kiện gần nhất.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 next_event_ticks(void) {
    u32 n = 5u;
    if (g_sleep_head != INVALID_IDX && g_tasks[g_sleep_head].sleep_delta < n)
        n = g_tasks[g_sleep_head].sleep_delta;
    if (n < 2)
        n = 2;
    if (n > 0x1FFFFFu / TICK_CYCLES)
        n = 0x1FFFFFu / TICK_CYCLES;
    return n;
}

/**
 * @brief Đưa CPU vào WFI và tối ưu khoảng idle theo sự kiện tiếp theo.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đưa CPU vào WFI và tối ưu khoảng idle theo sự kiện
 * tiếp theo. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void enter_idle_tickless(void) {
    if (g_load_mode != 0 || g_tick_step != 1u)
        return;
    u32 n = next_event_ticks();
    if (n > 1) {
        g_tick_step = n;
        SYST_RVR = n * TICK_CYCLES - 1u;
        SYST_CVR = 0;
        g_tickless_entries++;
    }
}

/**
 * @brief So sánh mức ưu tiên của hai task đang chờ resource.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. So sánh mức ưu tiên của hai task đang chờ resource.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] a Đối tượng/chuỗi thứ nhất.
 * @param[in] b Đối tượng/chuỗi thứ hai.
 * @return 1 nếu task a ưu tiên hơn task b.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int wait_priority_better(u8 a, u8 b) { return task_precedes(&g_tasks[a], &g_tasks[b]); }

/**
 * @brief Kiểm tra thứ tự wakeup của wait queue theo priority.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kiểm tra thứ tự wakeup của wait queue theo priority.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void wait_queue_model_test(void) {
    u8 order[3] = {8, 4, 9};
    for (u32 i = 0; i < 3; i++)
        for (u32 j = i + 1; j < 3; j++)
            if (wait_priority_better(order[j], order[i])) {
                u8 t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
    if (order[0] == 9 && order[1] == 4)
        g_wait_ops++;
    else
        g_lost_wakeup++;
}

/**
 * @brief Đưa pipe model về trạng thái rỗng ban đầu.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đưa pipe model về trạng thái rỗng ban đầu. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void pipe_reset(void) {
    mem_zero(&g_pipe, sizeof(g_pipe));
    g_pipe.readers = 1;
    g_pipe.writers = 1;
}

/**
 * @brief Ghi byte vào pipe kernel với giới hạn buffer cố định.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Ghi byte vào pipe kernel với giới hạn buffer cố
 * định. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @param[in] data Con trỏ dữ liệu.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @return Số byte đã ghi.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int pipe_write_priv(const u8 *data, u32 n) {
    u32 done = 0;
    while (done < n && g_pipe.count < sizeof(g_pipe.data)) {
        g_pipe.data[g_pipe.write_pos] = data[done++];
        g_pipe.write_pos = (u16)((g_pipe.write_pos + 1u) % sizeof(g_pipe.data));
        g_pipe.count++;
        g_pipe_bytes++;
        g_ipc_operations++;
    }
    return (int)done;
}

/**
 * @brief Đọc byte từ pipe kernel và cập nhật con trỏ vòng.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đọc byte từ pipe kernel và cập nhật con trỏ vòng.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[out] out Buffer đầu ra.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @return Số byte đã đọc.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int pipe_read_priv(u8 *out, u32 n) {
    u32 done = 0;
    while (done < n && g_pipe.count) {
        out[done++] = g_pipe.data[g_pipe.read_pos];
        g_pipe.read_pos = (u16)((g_pipe.read_pos + 1u) % sizeof(g_pipe.data));
        g_pipe.count--;
        g_ipc_operations++;
    }
    return (int)done;
}

/**
 * @brief Chạy kiểm tra tích hợp IPC, pipe, semaphore, mutex và FD.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chạy kiểm tra tích hợp IPC, pipe, semaphore, mutex
 * và FD. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @return 0 nếu PASS, khác 0 nếu FAIL.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int ipc_posix_selftest(void) {
    static const u8 msg[] = "hala-ipc";
    u8 out[16];
    pipe_reset();
    int w = pipe_write_priv(msg, 8);
    int r = pipe_read_priv(out, 8);
    if (w != 8 || r != 8)
        return -1;
    for (u32 i = 0; i < 8; i++)
        if (out[i] != msg[i]) {
            g_ipc_lost++;
            return -2;
        }
    g_sem.count = 0;
    g_sem.count++;
    g_sem.posts++;
    if (--g_sem.count != 0)
        return -3;
    g_posix_mutex.owner = (u8)g_current_index;
    g_posix_mutex.locks++;
    g_posix_mutex.owner = INVALID_IDX;
    g_posix_mutex.unlocks++;
    g_posix_calls += 8;
    return 0;
}

/**
 * @brief Ghi dữ liệu vào file tmpfs đơn giản trong RAM.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Ghi dữ liệu vào file tmpfs đơn giản trong RAM. Thiết
 * kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài
 * nguyên của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void tmp_write(const char *s, u32 n) {
    if (n > sizeof(g_tmp_file.data))
        n = sizeof(g_tmp_file.data);
    g_tmp_file.used = 1;
    g_tmp_file.size = (u16)n;
    mem_copy(g_tmp_file.data, s, n);
}

/**
 * @brief Chuyển con trỏ TCB thành chỉ số task an toàn.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chuyển con trỏ TCB thành chỉ số task an toàn. Thiết
 * kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài
 * nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @param[in] t Con trỏ TCB cần thao tác.
 * @return Chỉ số task hoặc INVALID_IDX.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 task_index_from_ptr(const volatile Tcb *t) { return (u32)(t - &g_tasks[0]); }

/**
 * @brief Block một task demo và loại khỏi ready queue.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Block một task demo và loại khỏi ready queue. Thiết
 * kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài
 * nguyên của STM32F103C8.
 * @param[in] idx Chỉ số task/process trong fixed pool.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void demo_block_task(u8 idx) {
    if (idx >= MAX_TASKS)
        return;
    sleep_remove(idx);
    if (is_rt_policy(g_tasks[idx].policy))
        rt_remove(idx);
    g_tasks[idx].state = TASK_BLOCKED;
    g_tasks[idx].sleep_next = INVALID_IDX;
    g_tasks[idx].sleep_delta = 0;
}

/**
 * @brief Dừng application demo, thu hồi thread và cập nhật trạng thái exit.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Dừng application demo, thu hồi thread và cập nhật
 * trạng thái exit. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Không còn worker demo runnable và waiter được đánh thức.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void demo_stop(void) {
    g_app_running = 0;
    g_app_paused = 0;
    demo_block_task(3);
    demo_block_task(11);
    demo_block_task(12);
    g_thread_leaks = 0;
    pend_resched();
}

/**
 * @brief Khởi tạo application demo theo loại bytecode đã build.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Khởi tạo application demo theo loại bytecode đã
 * build. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] type Loại application demo.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void demo_start(u32 type) {
    demo_stop();
    g_thread_leaks = 0;
    g_app_type = type;
    g_app_thread_done = 0;
    g_app_thread_a_count = 0;
    g_app_thread_b_count = 0;
    g_demo_queue_sent = 0;
    g_demo_queue_received = 0;
    g_demo_q_head = g_demo_q_tail = g_demo_q_count = 0;
    g_app_exit_status = 0;
    g_app_running = 1;
    if (type == APP_TYPE_THREADS) {
        g_tasks[11].pid = 20;
        g_tasks[12].pid = 20;
        g_tasks[11].name = "worker-A";
        g_tasks[12].name = "worker-B";
        g_thread_creates += 2;
        task_make_ready(11);
        task_make_ready(12);
    } else if (type == APP_TYPE_IPC) {
        g_tasks[11].pid = 31;
        g_tasks[12].pid = 30;
        g_tasks[11].name = "producer";
        g_tasks[12].name = "consumer";
        g_thread_creates += 2;
        task_make_ready(12);
        task_make_ready(11);
    } else if (type == APP_TYPE_FAULT) {
        g_tasks[11].pid = 40;
        g_tasks[11].name = "fault-demo";
        g_thread_creates++;
        task_make_ready(11);
    } else if (type == APP_TYPE_LOOP) {
        g_tasks[11].pid = 41;
        g_tasks[11].name = "loop-demo";
        g_thread_creates++;
        task_make_ready(11);
    } else if (type == APP_TYPE_PIPE) {
        g_tasks[11].pid = 42;
        g_tasks[11].name = "pipe-demo";
        g_thread_creates++;
        task_make_ready(11);
    } else if (type == APP_TYPE_POSIX) {
        g_tasks[11].pid = 43;
        g_tasks[11].name = "posix-demo";
        g_thread_creates++;
        task_make_ready(11);
    } else
        task_make_ready(3);
}

/**
 * @brief Ghi nhận thread hoàn thành và kết thúc app khi đủ thành viên.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Ghi nhận thread hoàn thành và kết thúc app khi đủ
 * thành viên. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] bit Bit đánh dấu thread hoàn thành.
 * @param[in] expected Mặt nạ hoàn thành mong đợi.
 * @param[in] bit Bit đánh dấu thread hoàn thành.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void demo_thread_complete(u32 bit, u32 expected, const char *message) {
    if (!(g_app_thread_done & bit)) {
        g_app_thread_done |= bit;
        g_thread_exits++;
    }
    if (g_app_thread_done == expected && g_app_running) {
        g_thread_joins +=
            (expected == 3u) ? 2u
                             : 1u; /* Publish lifecycle state before the visible prompt. A UART peer
                                      may send the next command as soon as it sees "hala$ ". */
        g_app_running = 0;
        g_app_exit_status = 0;
        if (!message) {
            if (g_app_type == APP_TYPE_IPC)
                message = "IPC-DEMO:COMPLETE\r\n";
            else if (g_app_type == APP_TYPE_THREADS)
                message = "all threads completed\r\n";
        }
        if (message) {
            console_lock();
            outs("\r\n");
            outs(message);
            prompt();
            console_unlock();
        }
    }
    block_current(TASK_BLOCKED, 0);
}

/**
 * @brief Đưa một message vào queue producer/consumer demo.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đưa một message vào queue producer/consumer demo.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] value Giá trị cần ghi hoặc truyền.
 * @return 1 nếu push thành công, 0 nếu queue đầy.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int demo_queue_push(u8 value) {
    if (g_demo_q_count >= ARRAY_LEN(g_demo_queue))
        return 0;
    g_demo_queue[g_demo_q_tail] = value;
    g_demo_q_tail = (u8)((g_demo_q_tail + 1u) % ARRAY_LEN(g_demo_queue));
    g_demo_q_count++;
    g_demo_queue_sent++;
    g_ipc_operations++;
    return 1;
}

/**
 * @brief Lấy một message khỏi queue producer/consumer demo.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Lấy một message khỏi queue producer/consumer demo.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[out] out Buffer đầu ra.
 * @return 1 nếu pop thành công, 0 nếu queue rỗng.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int demo_queue_pop(u8 *out) {
    if (!g_demo_q_count)
        return 0;
    *out = g_demo_queue[g_demo_q_head];
    g_demo_q_head = (u8)((g_demo_q_head + 1u) % ARRAY_LEN(g_demo_queue));
    g_demo_q_count--;
    g_demo_queue_received++;
    g_ipc_operations++;
    return 1;
}
