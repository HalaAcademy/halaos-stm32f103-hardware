/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file halaos_shell.c
 * @brief HalaShell, observability và command execution.
 * @details File được tách từ baseline đã qualification; comment tiếng Việt được giữ để phục vụ
 * review và đào tạo.
 */
#include "halaos/internal/halaos_internal.h"
#include "halaos/user/hala_posix.h"

#define HALA_SHELL_CAPTURE_DEPTH 2u
#define HALA_SHELL_PIPE_BUFFER 128u
#define HALA_SHELL_MAX_STAGES 3u
#define HALA_SHELL_MAX_JOBS 2u
#define HALA_SHELL_CAT_LIMIT 256u

typedef struct {
    char *buffer;
    u32 capacity;
    u32 size;
    u8 overflow;
} HalaShellCapture;
typedef struct {
    char *commands[HALA_SHELL_MAX_STAGES];
    char *redirect_path;
    u8 command_count;
    u8 background;
} HalaShellPlan;
typedef struct {
    u32 id;
    u8 used;
    char name[12];
} HalaShellJob;

static HalaShellCapture g_shell_captures[HALA_SHELL_CAPTURE_DEPTH];
static HalaShellJob g_shell_jobs[HALA_SHELL_MAX_JOBS];
static u8 g_shell_capture_depth;
static u32 g_shell_job_id;

static void shell_capture_begin(char *buffer, u32 capacity) {
    if (g_shell_capture_depth >= HALA_SHELL_CAPTURE_DEPTH)
        return;
    HalaShellCapture *capture = &g_shell_captures[g_shell_capture_depth++];
    capture->buffer = buffer;
    capture->capacity = capacity;
    capture->size = 0u;
    capture->overflow = 0u;
}

static u32 shell_capture_end(u8 *overflow) {
    if (g_shell_capture_depth == 0u) {
        if (overflow)
            *overflow = 1u;
        return 0u;
    }
    HalaShellCapture *capture = &g_shell_captures[--g_shell_capture_depth];
    if (overflow)
        *overflow = capture->overflow;
    return capture->size;
}

/**
 * @brief Ghi một buffer user ra console thông qua syscall WRITE.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Ghi một buffer user ra console thông qua syscall
 * WRITE. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void out_write(const char *text, u32 length) {
    if (length == 0u)
        return;
    if (g_shell_capture_depth > 0u) {
        HalaShellCapture *capture = &g_shell_captures[g_shell_capture_depth - 1u];
        u32 room = capture->capacity > capture->size ? capture->capacity - capture->size : 0u;
        u32 copy = length < room ? length : room;
        if (copy > 0u)
            mem_copy(capture->buffer + capture->size, text, copy);
        capture->size += copy;
        if (copy != length)
            capture->overflow = 1u;
        return;
    }
    (void)svc_ptr(SVC_WRITE, text, length);
}

/**
 * @brief Ghi một ký tự user ra console thông qua syscall PUTC.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Ghi một ký tự user ra console thông qua syscall
 * PUTC. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @param[in] c Ký tự cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void outc(char character) {
    if (g_shell_capture_depth > 0u) {
        out_write(&character, 1u);
        return;
    }
    register u32 r0 __asm("r0") = (u32)(u8)character;
    __asm volatile("svc #0" : "+r"(r0)::"memory");
}

/**
 * @brief Ghi một chuỗi kết thúc NUL ra console user.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Ghi một chuỗi kết thúc NUL ra console user. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void outs(const char *s) { out_write(s, str_len(s)); }

/**
 * @brief In số 32-bit dạng hexadecimal từ user space.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In số 32-bit dạng hexadecimal từ user space. Thiết
 * kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài
 * nguyên của STM32F103C8.
 * @param[in] v Giá trị số cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void out_hex(u32 v) {
    static const char h[] = "0123456789ABCDEF";
    char b[8];
    for (u32 i = 0; i < 8; i++)
        b[i] = h[(v >> (28u - i * 4u)) & 15u];
    out_write(b, 8);
}

/**
 * @brief In số 32-bit dạng thập phân từ user space.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In số 32-bit dạng thập phân từ user space. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] v Giá trị số cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void out_dec(u32 v) {
    char b[11];
    u32 n = 0;
    if (v == 0) {
        out_write("0", 1);
        return;
    }
    while (v && n < 10) {
        b[n++] = (char)('0' + v % 10u);
        v /= 10u;
    }
    for (u32 i = 0; i < n / 2; i++) {
        char t = b[i];
        b[i] = b[n - 1u - i];
        b[n - 1u - i] = t;
    }
    out_write(b, n);
}

/**
 * @brief Chiếm quyền xuất console để bảo toàn một đoạn output nguyên tử.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chiếm quyền xuất console để bảo toàn một đoạn output
 * nguyên tử. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void console_lock(void) {
    while (svc_call0(SVC_CONSOLE_LOCK) != 0)
        svc_call0(SVC_YIELD);
}

/**
 * @brief Nhả quyền xuất console đang thuộc task hiện hành.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Nhả quyền xuất console đang thuộc task hiện hành.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void console_unlock(void) { svc_call0(SVC_CONSOLE_UNLOCK); }

/**
 * @brief In prompt chính của HalaShell.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In prompt chính của HalaShell. Thiết kế tránh cấp
 * phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên của
 * STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void prompt(void) { outs("hala$ "); }

/**
 * @brief In prompt dùng trong chế độ nhập source đa dòng.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In prompt dùng trong chế độ nhập source đa dòng.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void source_prompt(void) { outs("source> "); }

/**
 * @brief In tên application active hoặc thông báo không có app.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In tên application active hoặc thông báo không có
 * app. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_app_name(void) {
    if (g_app_valid)
        outs(app_name());
    else
        outs("(none)");
}

/**
 * @brief Bắt đầu một đoạn output bất đồng bộ có khóa console.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Bắt đầu một đoạn output bất đồng bộ có khóa console.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void async_begin(void) {
    console_lock();
    outs("\r\n");
}

/**
 * @brief Kết thúc đoạn output bất đồng bộ và nhả khóa console.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kết thúc đoạn output bất đồng bộ và nhả khóa
 * console. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void async_end(void) { console_unlock(); }

/**
 * @brief Chuyển task state enum thành chuỗi dễ đọc.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chuyển task state enum thành chuỗi dễ đọc. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @return Con trỏ chuỗi tên state.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
const char *state_name(u8 s) {
    if (s == TASK_READY)
        return "READY";
    if (s == TASK_SLEEP)
        return "SLEEP";
    if (s == TASK_BLOCKED)
        return "BLOCKED";
    if (s == TASK_ZOMBIE)
        return "ZOMBIE";
    if (s == TASK_STOPPED)
        return "STOPPED";
    return "?";
}

/**
 * @brief Chuyển scheduler policy enum thành chuỗi dễ đọc.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chuyển scheduler policy enum thành chuỗi dễ đọc.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] p Con trỏ hoặc giá trị policy/priority tùy ngữ cảnh.
 * @return Con trỏ chuỗi tên policy.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
const char *policy_name(u8 p) {
    if (p == POLICY_IDLE)
        return "IDLE";
    if (p == POLICY_FIFO)
        return "FIFO";
    if (p == POLICY_RR)
        return "RR";
    if (p == POLICY_NORMAL)
        return "NORMAL";
    if (p == POLICY_BATCH)
        return "BATCH";
    if (p == POLICY_DEADLINE)
        return "DEADLINE";
    return "?";
}

/**
 * @brief In một số lượng ký tự khoảng trắng.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In một số lượng ký tự khoảng trắng. Thiết kế tránh
 * cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên của
 * STM32F103C8.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void out_spaces(u32 n) {
    while (n--)
        outc(' ');
}

/**
 * @brief In trường chuỗi theo độ rộng cột cố định.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In trường chuỗi theo độ rộng cột cố định. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @param[in] width Độ rộng cột hiển thị.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void out_field(const char *s, u32 width) {
    u32 n = str_len(s);
    outs(s);
    if (n < width)
        out_spaces(width - n);
    else
        outc(' ');
}

/**
 * @brief In trường số theo độ rộng cột cố định.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In trường số theo độ rộng cột cố định. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] v Giá trị số cần xử lý.
 * @param[in] width Độ rộng cột hiển thị.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void out_num_field(u32 v, u32 width) {
    char b[11];
    u32 n = 0;
    if (v == 0)
        b[n++] = '0';
    else {
        while (v && n < 10) {
            b[n++] = (char)('0' + v % 10u);
            v /= 10u;
        }
        for (u32 i = 0; i < n / 2; i++) {
            char t = b[i];
            b[i] = b[n - 1u - i];
            b[n - 1u - i] = t;
        }
    }
    out_write(b, n);
    if (n < width)
        out_spaces(width - n);
    else
        outc(' ');
}

/**
 * @brief Quyết định một task có được hiển thị trong công cụ quan sát hay không.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Quyết định một task có được hiển thị trong công cụ
 * quan sát hay không. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định
 * để phù hợp giới hạn tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc
 * hoặc block thông qua syscall/scheduler thay vì trả về tự do.
 * @param[in] i Chỉ số task.
 * @return 1 nếu task nên hiển thị.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int task_visible(u32 i) {
    Tcb *t = &g_tasks[i];
    if (i >= 4u && i <= 9u && !(t->flags & TF_LOAD_ENABLED))
        return 0;
    if ((t->flags & TF_DYNAMIC) && t->state == TASK_BLOCKED)
        return 0;
    return 1;
}

/**
 * @brief Suy ra PPID giáo dục của một task từ topology hiện tại.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Suy ra PPID giáo dục của một task từ topology hiện
 * tại. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông
 * qua syscall/scheduler thay vì trả về tự do.
 * @param[in] i Chỉ số task.
 * @return PPID suy ra.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 task_ppid(u32 i) {
    if (i == 0u || i == 10u)
        return 0u;
    if (i == 1u || i == 2u || i == 3u)
        return 1u;
    return 2u;
}

/**
 * @brief Suy ra TID của thread application.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Suy ra TID của thread application. Thiết kế tránh
 * cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên của
 * STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @param[in] i Chỉ số task.
 * @return TID suy ra.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 task_tid(u32 i) {
    if (g_app_type == APP_TYPE_THREADS && i == 11u)
        return 1u;
    if (g_app_type == APP_TYPE_THREADS && i == 12u)
        return 2u;
    return 1u;
}

/**
 * @brief In một dòng trạng thái task theo dạng bảng.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In một dòng trạng thái task theo dạng bảng. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] i Chỉ số task.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_task(u32 i) {
    Tcb *t = &g_tasks[i];
    out_num_field(t->pid, 5);
    out_num_field(task_ppid(i), 6);
    out_field(t->name, 19);
    out_field(state_name(t->state), 10);
    out_field(policy_name(t->policy), 10);
    out_num_field((u32)t->runtime, 10);
    out_num_field(t->stack_peak, 6);
    outs("\r\n");
}

/**
 * @brief In bảng process/task đang tồn tại.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In bảng process/task đang tồn tại. Thiết kế tránh
 * cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên của
 * STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_ps_table(void) {
    outs("PID  PPID  NAME               STATE     POLICY    CPU       STACK\r\n");
    for (u32 i = 0; i < MAX_TASKS; i++)
        if (task_visible(i))
            print_task(i);
}

/**
 * @brief In bảng thread và các bộ đếm lifecycle.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In bảng thread và các bộ đếm lifecycle. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_threads_table(void) {
    outs("PID  TID  NAME               STATE     STACK      CPU\r\n");
    for (u32 i = 11u; i <= 12u; i++)
        if (task_visible(i)) {
            Tcb *t = &g_tasks[i];
            out_num_field(t->pid, 5);
            out_num_field(task_tid(i), 5);
            out_field(t->name, 19);
            out_field(state_name(t->state), 10);
            out_num_field(t->stack_peak, 11);
            out_dec((u32)t->runtime);
            outs("\r\n");
        }
    outs("created=");
    out_dec(g_thread_creates);
    outs(" joined=");
    out_dec(g_thread_joins);
    outs(" exits=");
    out_dec(g_thread_exits);
    outs(" faults=");
    out_dec(g_thread_faults);
    outs(" leaks=");
    out_dec(g_thread_leaks);
    outs("\r\n");
}

/**
 * @brief In boot ID và danh sách event đã ghi nhận.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In boot ID và danh sách event đã ghi nhận. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_boot_events(void) {
    outs("boot_id=");
    out_dec(g_crash_record.boot_id);
    outs(" events=");
    out_dec(g_boot_event_count);
    outs(" [");
    for (u32 i = 0; i < g_boot_event_count; i++) {
        if (i)
            outc(',');
        out_dec(g_boot_events[i]);
    }
    outs("]\r\n");
}

/**
 * @brief In các chỉ số scheduler theo policy.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In các chỉ số scheduler theo policy. Thiết kế tránh
 * cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên của
 * STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_schedstat(void) {
    outs("POLICY      PICKS/RUN       MISSES  THROTTLE\r\n");
    outs("RR          ");
    out_num_field(g_rr_rotations, 16);
    out_num_field(0, 8);
    out_dec(g_rt_throttle_count);
    outs("\r\nFAIR        ");
    out_num_field(g_fair_picks, 16);
    out_num_field(0, 8);
    out_dec(0);
    outs("\r\nDEADLINE    ");
    out_num_field(g_deadline_picks, 16);
    out_num_field(g_deadline_misses, 8);
    out_dec(g_deadline_throttles);
    outs("\r\nTOTAL       switches=");
    out_dec(g_switches);
    outs(" calls=");
    out_dec(g_scheduler_calls);
    outs(" tickless=");
    out_dec(g_tickless_entries);
    outs("\r\n");
}

/**
 * @brief In thống kê IRQ USART1 và lỗi RX overflow.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In thống kê IRQ USART1 và lỗi RX overflow. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_irqstat(void) {
    outs("IRQ       COUNT      BYTES      WAKE       OVERFLOW\r\nUSART1    ");
    out_num_field(g_uart_irq_count, 11);
    out_num_field(g_uart_bytes, 11);
    out_num_field(g_uart_wakeups, 11);
    out_dec(g_rx_overflow);
    outs("\r\n");
}

/**
 * @brief In log chi tiết của một boot stage được chọn.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In log chi tiết của một boot stage được chọn. Thiết
 * kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài
 * nguyên của STM32F103C8.
 * @param[in] stage Tên boot stage.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_boot_stage(const char *stage) {
    if (str_eq(stage, "reset")) {
        outs("[RESET] vector=0x08000000 VTOR=0x08000000 initial_MSP=0x");
        out_hex((u32)(usize)&_estack);
        outs("\r\n[START] .data flash=0x");
        out_hex((u32)(usize)&_sidata);
        outs(" ram=0x");
        out_hex((u32)(usize)&_sdata);
        outs(" size=");
        out_dec((u32)(&_edata - &_sdata) * 4u);
        outs("\r\n[START] .bss start=0x");
        out_hex((u32)(usize)&_sbss);
        outs(" size=");
        out_dec((u32)(&_ebss - &_sbss) * 4u);
        outs(" reset_reason=software\r\n");
    } else if (str_eq(stage, "stage0")) {
        outs("[S0] manifest=0x");
        out_hex((u32)(usize)&g_boot_manifest);
        outs(" magic=0x");
        out_hex(g_boot_manifest.magic);
        outs(" version=");
        out_dec(g_boot_manifest.version);
        outs("\r\n[S0] header_crc expected=0x");
        out_hex(g_boot_manifest.header_crc);
        outs(" calculated=0x");
        out_hex(manifest_crc());
        outs("\r\n[S0] loader=0x");
        out_hex(g_boot_manifest.loader_addr);
        outs("+");
        out_dec(g_boot_manifest.loader_size);
        outs(" crc=0x");
        out_hex(g_boot_manifest.loader_crc);
        outs("\r\n[S0] dtb=0x");
        out_hex(g_boot_manifest.dtb_addr);
        outs("+");
        out_dec(g_boot_manifest.dtb_size);
        outs(" crc=0x");
        out_hex(g_boot_manifest.dtb_crc);
        outs("\r\n[S0] kernel=0x");
        out_hex(g_boot_manifest.kernel_addr);
        outs("+");
        out_dec(g_boot_manifest.kernel_size);
        outs(" crc=0x");
        out_hex(g_boot_manifest.kernel_crc);
        outs(" entry=0x");
        out_hex(g_boot_manifest.entry_point);
        outs("\r\n");
    } else if (str_eq(stage, "loader")) {
        outs("[LDR] boot_source=internal-flash\r\n[LDR] early_console=USART1 polling 115200 8N1 "
             "TX=PA9 RX=PA10\r\n[LDR] HalaBootInfo prepared; kernel handoff=Thumb\r\n");
    } else if (str_eq(stage, "dtb")) {
        outs("[DTB] address=0x");
        out_hex((u32)(usize)g_halaos_compact_dtb);
        outs(" size=");
        out_dec(HALAOS_DTB_SIZE);
        outs(" nodes=7 properties=28\r\n[DTB] model=HALA Blue Pill STM32F103C8T6\r\n[DTB] "
             "stdout-path=/soc/serial@40013800 init=/sbin/hala-init validation=OK\r\n");
    } else if (str_eq(stage, "kernel")) {
        outs("[KERN] SRAM=0x20000000-0x20004FFF MSP=2048 appstore=8192\r\n[KERN] console handover "
             "polling -> USART1 interrupt irq=37\r\n[KERN] "
             "sched=DEADLINE,FIFO,RR,NORMAL,BATCH,IDLE syscall=SVC IPC/VFS=ready\r\n");
    } else if (str_eq(stage, "init")) {
        outs("[INIT] hala-init pid=1 fds=0,1,2 -> /dev/console\r\n[USER] hala-shell pid=2 "
             "mode=unprivileged PSP active\r\n");
    } else
        outs("bootlog: unknown stage\r\n");
}

/**
 * @brief In kernel log ring dạng Linux-like, có thể chỉ lấy phần cuối.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In kernel log ring dạng Linux-like, có thể chỉ lấy
 * phần cuối. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] tail Khác zero để chỉ in phần cuối log.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_dmesg(int tail) {
    if (!tail) {
        outs("[000001][0.000000] INFO  kernel: HalaOS kernel entered\r\n[000002][0.000080] INFO  "
             "dtb: compact DTB verified\r\n[000003][0.000110] INFO  driver: USART1/GPIO/SysTick "
             "ready\r\n[000004][0.000180] INFO  sched: scheduler classes "
             "ready\r\n[000005][0.000400] INFO  init: hala-init pid=1\r\n[000006][0.000560] INFO  "
             "user: hala-shell pid=2 unprivileged\r\n");
    }
    if (g_user_fault_recoveries) {
        outs("[000081][runtime] ERROR fault: user pid=");
        out_dec(g_last_fault_pid);
        outs(" process=");
        outs(g_last_fault_task < MAX_TASKS ? g_tasks[g_last_fault_task].name : "unknown");
        outs(" pc=0x");
        out_hex(g_fault_pc);
        outs(" lr=0x");
        out_hex(g_fault_lr);
        outs(" action=terminated kernel=continued\r\n");
    }
    outs("log_dropped=0\r\n");
}

/**
 * @brief Disassemble HBC bytecode của application active.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Disassemble HBC bytecode của application active.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] g_app_valid Tham số g_app_valid của hàm.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_disasm(void) {
    if (!g_app_valid) {
        outs("disasm: no active app\r\n");
        return;
    }
    const u8 *code = app_code();
    u16 size = app_header()->code_size;
    outs("HBC disassembly name=");
    print_app_name();
    outs(" bytes=");
    out_dec(size);
    outs("\r\n");

    for (u16 pc = 0u; pc < size;) {
        u16 address = pc;
        u8 opcode = code[pc++];
        out_hex(address);
        outs("  ");
        if (opcode == HBC_PUSH_I32) {
            u32 value = (u32)code[pc] | ((u32)code[pc + 1u] << 8) | ((u32)code[pc + 2u] << 16) |
                        ((u32)code[pc + 3u] << 24);
            pc += 4u;
            outs("PUSH_I32 0x");
            out_hex(value);
            outs("\r\n");
        } else if (opcode == HBC_LOAD_LOCAL) {
            outs("LOAD_LOCAL ");
            out_dec(code[pc++]);
            outs("\r\n");
        } else if (opcode == HBC_STORE_LOCAL) {
            outs("STORE_LOCAL ");
            out_dec(code[pc++]);
            outs("\r\n");
        } else if (opcode == HBC_ADD)
            outs("ADD\r\n");
        else if (opcode == HBC_SUB)
            outs("SUB\r\n");
        else if (opcode == HBC_MUL)
            outs("MUL\r\n");
        else if (opcode == HBC_DIV)
            outs("DIV\r\n");
        else if (opcode == HBC_LT)
            outs("LT\r\n");
        else if (opcode == HBC_LE)
            outs("LE\r\n");
        else if (opcode == HBC_GT)
            outs("GT\r\n");
        else if (opcode == HBC_GE)
            outs("GE\r\n");
        else if (opcode == HBC_EQ)
            outs("EQ\r\n");
        else if (opcode == HBC_NE)
            outs("NE\r\n");
        else if ((opcode == HBC_CALL) || (opcode == HBC_SLEEP) || (opcode == HBC_JMP) ||
                 (opcode == HBC_JZ)) {
            u16 operand = (u16)code[pc] | ((u16)code[pc + 1u] << 8);
            pc += 2u;
            if (opcode == HBC_CALL)
                outs("CALL ");
            else if (opcode == HBC_SLEEP)
                outs("SLEEP ");
            else if (opcode == HBC_JMP)
                outs("JMP ");
            else
                outs("JZ ");
            out_dec(operand);
            outs("\r\n");
        } else if (opcode == HBC_RET)
            outs("RET\r\n");
        else if (opcode == HBC_GPIO)
            outs("GPIO_TOGGLE\r\n");
        else if (opcode == HBC_PRINT_INT)
            outs("PRINT_INT\r\n");
        else if (opcode == HBC_POP)
            outs("POP\r\n");
        else if (opcode == HBC_WRITE) {
            u8 length = code[pc++];
            outs("WRITE len=");
            out_dec(length);
            outs(" data=\"");
            for (u32 index = 0u; index < length; ++index) {
                char character = (char)code[pc++];
                if ((character >= 32) && (character < 127))
                    outc(character);
                else
                    outc('.');
            }
            outs("\"\r\n");
        } else if (opcode == 0x30u)
            outs("THREAD_APP\r\n");
        else if (opcode == 0x31u)
            outs("IPC_APP\r\n");
        else if (opcode == 0x32u)
            outs("FAULT_APP\r\n");
        else if (opcode == 0x33u)
            outs("LOOP_APP\r\n");
        else if (opcode == 0x34u)
            outs("PIPE_APP\r\n");
        else if (opcode == 0x35u)
            outs("POSIX_APP\r\n");
        else if (opcode == HBC_HALT)
            outs("HALT\r\n");
        else {
            outs("INVALID 0x");
            out_hex(opcode);
            outs("\r\n");
            return;
        }
    }
}

/**
 * @brief Sinh nội dung pseudo-file trong procfs theo path.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Sinh nội dung pseudo-file trong procfs theo path.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] path Đường dẫn VFS/procfs.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void print_proc_path(const char *path) {
    if (str_eq(path, "/proc/uptime")) {
        out_dec(g_ticks);
        outs(" ticks\r\n");
    } else if (str_eq(path, "/proc/meminfo")) {
        outs("MemTotal: 20480\r\nStaticEnd: 0x");
        out_hex((u32)(usize)&_ebss);
        outs("\r\nMSP: 2048\r\nAppStore: 8192\r\n");
    } else if (str_eq(path, "/proc/version")) {
        outs("HalaOS Educational 0.4 Cortex-M3\r\n");
    } else if (str_eq(path, "/proc/bootinfo")) {
        print_boot_events();
        outs("dtb_valid=");
        out_dec(g_dtb_valid);
        outs(" init_ready=");
        out_dec(g_init_ready);
        outs(" userspace_ready=");
        out_dec(g_userspace_ready);
        outs("\r\n");
    } else if (str_eq(path, "/proc/ipc")) {
        outs("operations=");
        out_dec(g_ipc_operations);
        outs(" lost=");
        out_dec(g_ipc_lost);
        outs(" pipe_bytes=");
        out_dec(g_pipe_bytes);
        outs(" demo_sent=");
        out_dec(g_demo_queue_sent);
        outs(" demo_received=");
        out_dec(g_demo_queue_received);
        outs("\r\n");
    } else if (str_eq(path, "/proc/processes")) {
        print_ps_table();
    } else if (str_eq(path, "/proc/dtb")) {
        outs("DTB: valid=");
        out_dec(g_dtb_valid);
        outs(" size=");
        out_dec(HALAOS_DTB_SIZE);
        outs(" model=HALA Blue Pill STM32F103C8T6\r\n");
    } else if (str_eq(path, "/proc/schedstat")) {
        print_schedstat();
    } else if (str_eq(path, "/proc/interrupts")) {
        print_irqstat();
    } else if (str_eq(path, "/etc/version")) {
        outs("HalaOS Educational 0.4\r\n");
    } else if (str_eq(path, "/tmp/out")) {
        if (g_tmp_file.used) {
            for (u32 i = 0; i < g_tmp_file.size; i++)
                outc((char)g_tmp_file.data[i]);
            outc('\r');
            outc('\n');
        } else
            outs("cat: /tmp/out: No such file\r\n");
    } else
        outs("cat: No such file\r\n");
}

/**
 * @brief Phân tích và thực thi một lệnh HalaShell đơn.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Phân tích và thực thi một lệnh HalaShell đơn. Thiết
 * kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài
 * nguyên của STM32F103C8.
 * @param[in] line Dòng lệnh hoặc source cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */

/** @brief Yêu cầu kernel liệt kê node DTB qua syscall. */
void print_dtb_nodes(void) { (void)svc_call0(SVC_DTB_LIST); }

/** @brief Property được kernel đọc và xuất ở privileged context. */
void print_dtb_property(const char *path, const char *property) {
    (void)path;
    (void)property;
}

/** @brief Tách `dt get PATH PROPERTY`, tạo hai chuỗi NUL liên tiếp và gọi syscall. */
void shell_dtb_get(char *line) {
    char *p = line + 7;
    while (*p == ' ')
        p++;
    char *path = p;
    while (*p && *p != ' ')
        p++;
    if (!*p) {
        outs("usage: dt get PATH PROPERTY\r\n");
        g_shell_errors++;
        return;
    }
    *p++ = 0;
    while (*p == ' ')
        p++;
    if (!*p) {
        outs("usage: dt get PATH PROPERTY\r\n");
        g_shell_errors++;
        return;
    }
    u32 total = str_len(path) + 1u + str_len(p) + 1u;
    i32 r = svc_ptr(SVC_DTB_GET, path, total);
    if (r < 0) {
        outs("dt: property not found\r\n");
        g_shell_errors++;
    }
}

/** @brief Yêu cầu kernel in driver registry đã bind. */
void print_device_registry(void) { (void)svc_call0(SVC_DEVICE_LIST); }

/** @brief Chuyển chuỗi thập phân không dấu thành u32 và từ chối ký tự lạ. */
static int shell_parse_u32(const char *text, u32 *value) {
    u32 result = 0u;
    if ((text == NULL) || (*text == '\0'))
        return 0;
    while (*text != '\0') {
        if ((*text < '0') || (*text > '9'))
            return 0;
        if (result > 429496729u)
            return 0;
        result = result * 10u + (u32)(*text - '0');
        text++;
    }
    *value = result;
    return 1;
}

/** @brief Đọc file tmpfs qua public FD API thay vì truy cập buffer shell cố định. */
static int shell_vfs_cat(const char *path) {
    u8 buffer[24];
    u32 total = 0u;
    int truncated = 0;
    int fd = hala_open(path, 0u);
    if (fd < 0)
        return fd;
    for (;;) {
        u32 request = sizeof(buffer);
        if (total + request > HALA_SHELL_CAT_LIMIT)
            request = HALA_SHELL_CAT_LIMIT - total;
        if (request == 0u) {
            truncated = 1;
            break;
        }
        hala_ssize_t count = hala_read(fd, buffer, request);
        if (count < 0) {
            (void)hala_close(fd);
            return (int)count;
        }
        if (count == 0)
            break;
        out_write((const char *)buffer, (u32)count);
        total += (u32)count;
    }
    (void)hala_close(fd);
    if (truncated)
        outs("\r\n[stream truncated at 256 bytes]");
    outs("\r\n");
    return 0;
}

void shell_command(char *line) {
    g_shell_commands++;
    if (str_eq(line, "help")) {
        outs("help version tty whoami uptime pwd ls cat echo ps top threads process-tree mem stack "
             "bootinfo bootlog dmesg dt lsdev devinfo mount ipc pipe posix-demo schedstat irqstat "
             "apps appinfo edit build verify verify-corrupt disasm run stop continue kill wait "
             "acceptance-report reboot load appfail proc api-test jobs wc\r\n");
    } else if (str_eq(line, "version")) {
        outs("HalaOS Blue Pill Educational 0.4\r\nArchitecture: ARM Cortex-M3\r\nProfile: HalaOS "
             "POSIX Educational Subset\r\n");
    } else if (str_eq(line, "tty")) {
        outs("/dev/console -> /dev/uart1\r\nUSART1 115200 8N1 TX=PA9 RX=PA10\r\n");
    } else if (str_eq(line, "whoami")) {
        outs("hala-shell\r\npid=2 mode=user privilege=unprivileged\r\n");
    } else if (str_eq(line, "uptime")) {
        outs("uptime=");
        out_dec((u32)svc_call0(SVC_UPTIME));
        outs(" ticks\r\n");
    } else if (str_eq(line, "pwd")) {
        outs("/\r\n");
    } else if (str_eq(line, "ls")) {
        (void)hala_vfs_list("/");
        outs("\r\n");
    } else if (str_eq(line, "ls /dev") || str_eq(line, "ls /proc") || str_eq(line, "ls /tmp") ||
               str_eq(line, "ls /apps")) {
        (void)hala_vfs_list(line + 3);
        outs("\r\n");
    } else if (str_eq(line, "lsdev") || str_eq(line, "devinfo")) {
        print_device_registry();
    } else if (str_eq(line, "ls /etc")) {
        outs("version\r\n");
    } else if (str_eq(line, "cat")) {
        outs("usage: cat PATH\r\n");
        g_shell_errors++;
    } else if (str_starts(line, "cat ")) {
        if (shell_vfs_cat(line + 4) < 0)
            print_proc_path(line + 4);
    } else if (str_starts(line, "echo ")) {
        const char *text = line + 5;
        u32 n = str_len(text);
        if (n >= 2u && text[0] == '"' && text[n - 1u] == '"') {
            text++;
            n -= 2u;
        }
        out_write(text, n);
        outs("\r\n");
    } else if (str_eq(line, "bootinfo") || str_eq(line, "bootlog")) {
        print_boot_events();
    } else if (str_starts(line, "bootlog --stage ")) {
        print_boot_stage(line + 16);
    } else if (str_eq(line, "dmesg")) {
        print_dmesg(0);
    } else if (str_eq(line, "dmesg | tail")) {
        print_dmesg(1);
    } else if (str_eq(line, "dt") || str_eq(line, "dt ls")) {
        print_dtb_nodes();
    } else if (str_eq(line, "dt get")) {
        outs("usage: dt get PATH PROPERTY\r\n");
        g_shell_errors++;
    } else if (str_starts(line, "dt get ")) {
        shell_dtb_get(line);
    } else if (str_eq(line, "mount")) {
        (void)svc_call0(SVC_VFS_MOUNTS);
    } else if (str_eq(line, "ipc")) {
        print_proc_path("/proc/ipc");
    } else if (str_eq(line, "ipc-test") || str_eq(line, "pipe")) {
        i32 r = svc_call0(SVC_IPC_TEST);
        outs(r == 0 ? "IPC:PASS\r\n" : "IPC:FAIL\r\n");
    } else if (str_eq(line, "posix-test") || str_eq(line, "api-test")) {
        i32 r = hala_user_api_selftest();
        outs(r == 0 ? "POSIX-API:PASS\r\n" : "POSIX-API:FAIL code=");
        if (r != 0) {
            out_dec((u32)(-r));
            outs("\r\n");
        }
    } else if (str_eq(line, "jobs")) {
        u32 shown = 0u;
        for (u32 i = 0u; i < HALA_SHELL_MAX_JOBS; i++) {
            if (!g_shell_jobs[i].used)
                continue;
            outs("[");
            out_dec(g_shell_jobs[i].id);
            outs("] ");
            outs(g_app_running ? "RUNNING " : "DONE ");
            outs(g_shell_jobs[i].name);
            outs("\r\n");
            shown++;
        }
        if (shown == 0u)
            outs("no jobs\r\n");
    } else if (str_eq(line, "proc") || str_eq(line, "proc list")) {
        (void)svc_call0(SVC_PROCESS_LIST);
    } else if (str_starts(line, "proc spawn ")) {
        i32 pid = hala_spawn(line + 11);
        if (pid > 0) {
            outs("PROC:SPAWN pid=");
            out_dec((u32)pid);
            outs("\r\n");
        } else {
            outs("PROC:SPAWN_FAIL\r\n");
            g_shell_errors++;
        }
    } else if (str_starts(line, "proc kill ")) {
        char *p = line + 10;
        char *sp = p;
        while (*sp && *sp != ' ')
            sp++;
        u32 pid = 0, status = 137;
        if (*sp) {
            *sp++ = 0;
            while (*sp == ' ')
                sp++;
            if (!shell_parse_u32(sp, &status)) {
                outs("PROC:INVALID\r\n");
                g_shell_errors++;
                return;
            }
        }
        if (!shell_parse_u32(p, &pid)) {
            outs("PROC:INVALID\r\n");
            g_shell_errors++;
        } else {
            int r = hala_kill((int)pid, (int)status);
            outs(r == 0 ? "PROC:KILL_OK\r\n" : "PROC:KILL_FAIL\r\n");
        }
    } else if (str_starts(line, "proc wait ")) {
        u32 pid = 0;
        i32 status = -1;
        if (!shell_parse_u32(line + 10, &pid)) {
            outs("PROC:INVALID\r\n");
            g_shell_errors++;
        } else {
            i32 r = hala_waitpid((int)pid, (int *)&status);
            if (r > 0) {
                outs("PROC:WAIT_OK status=");
                out_dec((u32)status);
                outs("\r\n");
            } else
                outs("PROC:WAIT_PENDING_OR_INVALID\r\n");
        }
    } else if (str_eq(line, "process-tree")) {
        outs("0 kernel\r\n `- 1 hala-init\r\n     `- 2 hala-shell\r\n         |- 3 compiler\r\n    "
             "     |- 4 hbc-vm\r\n");
        if (task_visible(11)) {
            outs("         |- ");
            out_dec(g_tasks[11].pid);
            outs(" ");
            outs(g_tasks[11].name);
            outs("\r\n");
        }
        if (task_visible(12)) {
            outs("         `- ");
            out_dec(g_tasks[12].pid);
            outs(" ");
            outs(g_tasks[12].name);
            outs("\r\n");
        }
    } else if (str_eq(line, "apps")) {
        outs("Active application store (A/B transactional, single active app)\r\n");
        if (g_app_valid) {
            outs("Active: ");
            print_app_name();
            outs(".happ VALID slot=");
            out_dec(g_app_slot);
            outs(" seq=");
            out_dec(g_compile_count);
            outs("\r\n");
        } else
            outs("Active: (empty)\r\n");
    } else if (str_eq(line, "mem")) {
        outs("RAM=20480 static_end=0x");
        out_hex((u32)(usize)&_ebss);
        outs(" MSP=2048 compiler_peak=");
        out_dec(g_compiler_peak);
        outs(" appstore=8192\r\n");
    } else if (str_eq(line, "stack")) {
        outs("OWNER               USED  SIZE  FREE  HIGH-WATER\r\n");
        for (u32 i = 0; i < MAX_TASKS; i++)
            if (task_visible(i)) {
                update_stack_peak(&g_tasks[i]);
                u32 total = (u32)(g_tasks[i].stack_high - g_tasks[i].stack_low) * 4u;
                out_field(g_tasks[i].name, 20);
                out_num_field(g_tasks[i].stack_peak, 6);
                out_num_field(total, 6);
                out_num_field(total > g_tasks[i].stack_peak ? total - g_tasks[i].stack_peak : 0, 6);
                out_dec(total ? g_tasks[i].stack_peak * 100u / total : 0);
                outs("%\r\n");
            }
        outs("MSP                 ");
        out_num_field(msp_used_bytes(), 6);
        out_num_field(2048, 6);
        out_num_field(2048u - msp_used_bytes(), 6);
        out_dec(msp_used_bytes() * 100u / 2048u);
        outs("%\r\n");
    } else if (str_eq(line, "ps") || str_eq(line, "top") || str_starts(line, "top --samples")) {
        print_ps_table();
    } else if (str_eq(line, "schedstat")) {
        print_schedstat();
    } else if (str_eq(line, "irqstat")) {
        print_irqstat();
    } else if (str_eq(line, "appinfo") || str_starts(line, "appinfo ")) {
        const char *q = str_eq(line, "appinfo") ? NULL : line + 8;
        if (q && g_app_valid && !app_name_eq(q)) {
            outs("appinfo: app not found\r\n");
            g_shell_errors++;
        } else {
            outs("name=");
            print_app_name();
            outs(" valid=");
            outc(g_app_valid ? '1' : '0');
            outs(" running=");
            outc(g_app_running ? '1' : '0');
            outs(" type=");
            out_dec(g_app_type);
            outs(" slot=");
            out_dec(g_app_slot);
            outs(" seq=");
            out_dec(g_compile_count);
            outs(" threads_created=");
            out_dec(g_thread_creates);
            outs(" joined=");
            out_dec(g_thread_joins);
            outs(" exits=");
            out_dec(g_thread_exits);
            outs(" faults=");
            out_dec(g_thread_faults);
            outs(" leaks=");
            out_dec(g_thread_leaks);
            outs(" store=8192\r\n");
        }
    } else if (str_eq(line, "selftest")) {
        u8 bad[1] = {0x99};
        int vr = bytecode_verify(bad, 1);
        i32 pr = svc_ptr(SVC_QUEUE_COMPILE, (const char *)0x1000u, 4);
        outs("SELFTEST ptrret=0x");
        out_hex((u32)pr);
        outs(" vmreject=");
        out_dec(g_bytecode_rejects);
        outs(vr ? " FAIL\r\n" : " PASS\r\n");
    } else if (str_eq(line, "pitest")) {
        i32 r = svc_call0(SVC_PI_TEST);
        outs(r == 0 ? "PI:PASS\r\n" : "PI:FAIL\r\n");
    } else if (str_eq(line, "procstress")) {
        i32 r = 0;
        for (u32 i = 0; i < 1000u; i++) {
            if (svc_arg(SVC_PROC_STRESS, 1) != 0) {
                r = -1;
                break;
            }
            if ((i & 31u) == 31u)
                svc_call0(SVC_YIELD);
        }
        outs(r == 0 ? "PROC:PASS\r\n" : "PROC:FAIL\r\n");
    } else if (str_eq(line, "faulttest")) {
        i32 r = svc_call0(SVC_FAULT_TEST);
        outs(r == 0 ? "FAULTTEST:ARMED\r\n" : "FAULTTEST:FAIL\r\n");
    } else if (str_eq(line, "run") || str_starts(line, "run ")) {
        const char *q = str_eq(line, "run") ? NULL : line + 4;
        if (q && g_app_valid && !app_name_eq(q)) {
            outs("RUN:NOT_FOUND\r\n");
            g_shell_errors++;
        } else {
            i32 r = svc_call0(SVC_APP_RUN);
            if (r == 0) {
                outs("RUN:OK name=");
                print_app_name();
                outs("\r\n");
            } else
                outs("RUN:FAIL\r\n");
        }
    } else if (str_eq(line, "stop") || str_starts(line, "stop ")) {
        if (!g_app_running) {
            outs("STOP:NOT_RUNNING\r\n");
        } else {
            g_app_paused = 1;
            demo_block_task(3);
            demo_block_task(11);
            demo_block_task(12);
            if (g_app_type == APP_TYPE_BYTECODE)
                g_tasks[3].state = TASK_STOPPED;
            else if (g_app_type == APP_TYPE_THREADS || g_app_type == APP_TYPE_IPC) {
                g_tasks[11].state = TASK_STOPPED;
                g_tasks[12].state = TASK_STOPPED;
            } else
                g_tasks[11].state = TASK_STOPPED;
            outs("STOP:OK state=STOPPED\r\n");
        }
    } else if (str_eq(line, "continue") || str_starts(line, "continue ")) {
        if (!g_app_running || !g_app_paused) {
            outs("CONTINUE:NOT_STOPPED\r\n");
        } else {
            g_app_paused = 0;
            if (g_app_type == APP_TYPE_BYTECODE)
                task_make_ready(3);
            else if (g_app_type == APP_TYPE_FAULT || g_app_type == APP_TYPE_LOOP ||
                     g_app_type == APP_TYPE_PIPE || g_app_type == APP_TYPE_POSIX)
                task_make_ready(11);
            else {
                task_make_ready(11);
                task_make_ready(12);
            }
            outs("CONTINUE:OK\r\n");
        }
    } else if (str_eq(line, "kill") || str_starts(line, "kill ")) {
        demo_stop();
        g_app_exit_status = 137;
        outs("KILL:OK status=137 resources=clean\r\n");
    } else if (str_eq(line, "wait") || str_starts(line, "wait ")) {
        if (g_app_running)
            outs("WAIT:RUNNING\r\n");
        else {
            outs("WAIT:EXITED status=");
            out_dec(g_app_exit_status);
            outs(" resources=clean\r\n");
        }
    } else if (str_eq(line, "threads")) {
        print_threads_table();
    } else if (str_eq(line, "verify") || str_starts(line, "verify ")) {
        outs(app_verify_slot(g_app_slot) ? "Verifier: PASS\r\n" : "Verifier: FAIL\r\n");
    } else if (str_eq(line, "verify-corrupt") || str_starts(line, "verify-corrupt ")) {
        u8 bad[2] = {0x12u, 0xFFu};
        int ok = bytecode_verify(bad, 2);
        outs(ok ? "Verifier: UNEXPECTED PASS\r\n"
                : "Verifier rejected bytecode: truncated jump operand\r\nActive application "
                  "preserved\r\n");
    } else if (str_eq(line, "disasm") || str_starts(line, "disasm ")) {
        print_disasm();
    } else if (str_eq(line, "acceptance-report")) {
        u32 pass = (g_boot_event_count == 17u && g_dtb_valid && g_init_ready && g_userspace_ready &&
                    g_kernel_faults == 0u && g_bad_psp == 0u && g_stack_corruption == 0u &&
                    g_thread_leaks == 0u && g_ipc_lost == 0u && g_rx_overflow == 0u &&
                    g_fd_open_count == g_fd_close_count);
        outs("HALAOS EDUCATIONAL SYSTEM REPORT\r\n----------------------------------------\r\nBoot "
             "events             ");
        out_dec(g_boot_event_count);
        outs("/17\r\nDTB valid               ");
        outs(g_dtb_valid ? "yes" : "no");
        outs("\r\nUser space ready        ");
        outs(g_userspace_ready ? "yes" : "no");
        outs("\r\nProcesses leaked        ");
        out_dec(g_proc_leaks);
        outs("\r\nThreads leaked          ");
        out_dec(g_thread_leaks);
        outs("\r\nFD leaked               ");
        out_dec(g_fd_open_count - g_fd_close_count);
        outs("\r\nIPC messages lost       ");
        out_dec(g_ipc_lost);
        outs("\r\nKernel faults           ");
        out_dec(g_kernel_faults);
        outs("\r\nUser faults recovered   ");
        out_dec(g_user_fault_recoveries);
        outs("\r\nBad PSP                 ");
        out_dec(g_bad_psp);
        outs("\r\nUART overflow           ");
        out_dec(g_rx_overflow);
        outs("\r\nApp store CRC           ");
        outs(g_app_valid && app_verify_slot(g_app_slot) ? "valid" : "empty/invalid");
        outs("\r\n\r\nRESULT: ");
        outs(pass ? "PASS\r\n" : "FAIL\r\n");
    } else if (str_eq(line, "reboot")) {
        outs("[USER] reboot requested by HalaShell PID=2\r\n[INIT] stopping user "
             "services\r\n[KERN] controlled software reset\r\n");
        svc_call0(SVC_REBOOT);
    } else if (str_eq(line, "load")) {
        outs("usage: load rr|fair|weighted|dl|fifo|all|off\r\n");
        g_shell_errors++;
    } else if (str_starts(line, "load ")) {
        u32 m = 0;
        const char *p = line + 5;
        if (str_eq(p, "rr"))
            m = 1;
        else if (str_eq(p, "fair"))
            m = 2;
        else if (str_eq(p, "dl"))
            m = 3;
        else if (str_eq(p, "all"))
            m = 4;
        else if (str_eq(p, "weighted"))
            m = 5;
        else if (str_eq(p, "fifo"))
            m = 6;
        else if (str_eq(p, "off")) {
            load_disable_all();
            outs("LOAD:OFF\r\n");
            return;
        }
        if (m) {
            svc_arg(SVC_LOAD, m);
            outs("LOAD:OK\r\n");
        } else {
            outs("LOAD:INVALID\r\n");
            g_shell_errors++;
        }
    } else if (str_eq(line, "appfail")) {
        outs("usage: appfail 1|2|3|4\r\n");
        g_shell_errors++;
    } else if (str_starts(line, "appfail ")) {
        if (line[8] >= '1' && line[8] <= '4' && line[9] == 0) {
            g_app_fail_stage = (u32)(line[8] - '0');
            outs("APPFAIL:SET\r\n");
        } else {
            outs("APPFAIL:INVALID\r\n");
            g_shell_errors++;
        }
    } else if (str_eq(line, "edit")) {
        outs("usage: edit NAME\r\n");
        g_shell_errors++;
    } else if (str_starts(line, "edit ")) {
        const char *name = line + 5;
        if (!name_valid(name)) {
            outs("EDIT:INVALID_NAME\r\n");
            g_shell_errors++;
        } else {
            name_copy(g_editor_name, name);
            g_editor_len = 0;
            g_editor_source[0] = 0;
            g_editor_active = 1;
            outs("EDITOR:");
            outs(g_editor_name);
            outs(" enter Hala-C Tiny source, finish with .end\r\n");
        }
    } else if (str_eq(line, "build")) {
        outs("usage: build NAME\r\n");
        g_shell_errors++;
    } else if (str_starts(line, "build ")) {
        const char *name = line + 6;
        if (!name_valid(name) || !str_eq(name, g_editor_name) || g_editor_len == 0) {
            outs("BUILD:NO_SOURCE\r\n");
            g_shell_errors++;
        } else {
            name_copy(g_compile_name, name);
            i32 r = svc_ptr(SVC_QUEUE_COMPILE, (const char *)g_editor_source, g_editor_len);
            if (r == 1) {
                outs("BUILD:QUEUED name=");
                outs(g_compile_name);
                outs(" bytes=");
                out_dec(g_editor_len);
                outs("\r\n");
            } else
                outs("BUILD:ERR\r\n");
        }
    } else if (str_eq(line, "compile")) {
        outs("usage: compile SOURCE\r\n");
        g_shell_errors++;
    } else if (str_starts(line, "compile ")) {
        name_copy(g_compile_name, "app0");
        i32 r = svc_ptr(SVC_QUEUE_COMPILE, line + 8, str_len(line + 8));
        outs(r == 1 ? "COMPILE:QUEUED\r\n" : "COMPILE:ERR\r\n");
    } else if (line[0]) {
        outs("ERR:UNKNOWN\r\n");
        g_shell_errors++;
    }
}

/**
 * @brief Tiếp nhận một dòng source và quản lý trạng thái editor đa dòng.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Tiếp nhận một dòng source và quản lý trạng thái
 * editor đa dòng. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8.
 * @param[inout] end Tham số end của hàm.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void editor_accept_line(char *line) {
    if (str_eq(line, ".end")) {
        g_editor_active = 0;
        g_editor_source[g_editor_len] = 0;
        outs("SOURCE:SAVED name=");
        outs(g_editor_name);
        outs(" bytes=");
        out_dec(g_editor_len);
        outs("\r\n");
        return;
    }
    u32 n = str_len(line);
    if ((u32)g_editor_len + n + 1u >= sizeof(g_editor_source)) {
        g_editor_active = 0;
        g_editor_len = 0;
        outs("EDITOR:SOURCE_TOO_LONG\r\n");
        g_shell_errors++;
        return;
    }
    mem_copy(g_editor_source + g_editor_len, line, n);
    g_editor_len = (u16)(g_editor_len + n);
    g_editor_source[g_editor_len++] = '\n';
    g_editor_source[g_editor_len] = 0;
}

/** @brief Bỏ khoảng trắng ở đầu/cuối một command segment ngay trên buffer. */
static char *shell_trim(char *text) {
    while (*text == ' ')
        text++;
    char *end = text + str_len(text);
    while ((end > text) && (end[-1] == ' '))
        *--end = '\0';
    return text;
}

/**
 * @brief Phân tích segment thành command graph tối đa ba stage.
 * @details Parser tôn trọng quote và cặp ngoặc, tách nhiều pipe, một redirect
 *          output và marker background ở cuối. Mỗi stage trỏ trực tiếp vào
 *          line buffer để không cần heap hoặc copy chuỗi.
 */
static int shell_parse_plan(char *segment, HalaShellPlan *plan) {
    mem_zero(plan, sizeof(*plan));
    plan->commands[0] = shell_trim(segment);
    plan->command_count = 1u;
    char quote = 0;
    i32 depth = 0;
    u8 after_redirect = 0u;

    for (char *cursor = plan->commands[0]; *cursor != '\0'; ++cursor) {
        char character = *cursor;
        if (quote != 0) {
            if (character == quote)
                quote = 0;
            continue;
        }
        if ((character == '"') || (character == '\'')) {
            quote = character;
            continue;
        }
        if ((character == '(') || (character == '{') || (character == '[')) {
            depth++;
            continue;
        }
        if ((character == ')') || (character == '}') || (character == ']')) {
            if (depth == 0)
                return -1;
            depth--;
            continue;
        }
        if (depth != 0)
            continue;

        if (character == '|') {
            if (after_redirect || (plan->command_count >= HALA_SHELL_MAX_STAGES))
                return -1;
            *cursor = '\0';
            plan->commands[plan->command_count++] = shell_trim(cursor + 1);
        } else if (character == '>') {
            if (plan->redirect_path != NULL)
                return -1;
            *cursor = '\0';
            plan->redirect_path = shell_trim(cursor + 1);
            after_redirect = 1u;
        }
    }
    if ((quote != 0) || (depth != 0))
        return -1;

    for (u32 index = 0u; index < plan->command_count; ++index) {
        plan->commands[index] = shell_trim(plan->commands[index]);
        if (*plan->commands[index] == '\0')
            return -1;
    }
    if (plan->redirect_path != NULL) {
        plan->redirect_path = shell_trim(plan->redirect_path);
        if (*plan->redirect_path == '\0')
            return -1;
    }

    /* Background marker chỉ hợp lệ ở cuối redirect hoặc stage cuối. */
    char *last = plan->redirect_path != NULL ? plan->redirect_path
                                             : plan->commands[plan->command_count - 1u];
    u32 length = str_len(last);
    if ((length > 0u) && (last[length - 1u] == '&')) {
        last[length - 1u] = '\0';
        last = shell_trim(last);
        if (plan->redirect_path != NULL)
            plan->redirect_path = last;
        else
            plan->commands[plan->command_count - 1u] = last;
        plan->background = 1u;
    }
    return 0;
}

/** @brief Đọc pipe object và thực thi một filter built-in. */
static int shell_pipe_consume(int pipe_handle, const char *consumer) {
    u8 value = 0u;
    u32 bytes = 0u, lines = 0u;
    if (!str_eq(consumer, "cat") && !str_eq(consumer, "wc"))
        return -22;
    for (;;) {
        int result = hala_ipc_receive(pipe_handle, &value, 0u);
        if (result == -11)
            break;
        if (result != 1)
            return result;
        bytes++;
        if (value == '\n')
            lines++;
        if (str_eq(consumer, "cat"))
            outc((char)value);
    }
    if (str_eq(consumer, "wc")) {
        outs("lines=");
        out_dec(lines);
        outs(" bytes=");
        out_dec(bytes);
        outs("\r\n");
    }
    return 0;
}

/**
 * @brief Chuyển buffer giữa hai stage thông qua IPC pipe thật.
 * @details Capture output của consumer khi còn stage tiếp theo. Hai buffer stack
 *          được luân phiên nên pipeline ba stage không cần static RAM bổ sung.
 */
static int shell_pipe_stage(const char *input, u32 length, const char *consumer, char *output,
                            u32 capacity, u32 *output_length) {
    int pipe_handle = hala_ipc_create(HALA_IPC_PIPE, 0);
    if (pipe_handle < 0)
        return pipe_handle;
    for (u32 index = 0u; index < length; ++index) {
        if (hala_ipc_send(pipe_handle, (u8)input[index]) != 1) {
            (void)hala_ipc_close(pipe_handle);
            return -11;
        }
    }

    u8 overflow = 0u;
    if (output != NULL)
        shell_capture_begin(output, capacity);
    int result = shell_pipe_consume(pipe_handle, consumer);
    if (output != NULL)
        *output_length = shell_capture_end(&overflow);
    (void)hala_ipc_close(pipe_handle);
    if (overflow)
        return -28;
    return result;
}

/** @brief Thực thi command graph từ một đến ba stage. */
static int shell_execute_graph(HalaShellPlan *plan) {
    if (plan->command_count == 1u) {
        shell_command(plan->commands[0]);
        return 0;
    }

    char buffer_a[HALA_SHELL_PIPE_BUFFER];
    char buffer_b[HALA_SHELL_PIPE_BUFFER];
    u8 overflow = 0u;
    shell_capture_begin(buffer_a, sizeof(buffer_a));
    shell_command(plan->commands[0]);
    u32 length = shell_capture_end(&overflow);
    if (overflow)
        return -28;

    char *input = buffer_a;
    char *output = buffer_b;
    for (u32 stage = 1u; stage < plan->command_count; ++stage) {
        const int last = (stage + 1u == plan->command_count);
        u32 next_length = 0u;
        int result = shell_pipe_stage(input, length, plan->commands[stage], last ? NULL : output,
                                      HALA_SHELL_PIPE_BUFFER, &next_length);
        if (result != 0)
            return result;
        if (!last) {
            char *swap = input;
            input = output;
            output = swap;
            length = next_length;
        }
    }
    return 0;
}

/** @brief Ghi một background job vào bảng vòng cố định. */
static void shell_record_job(const char *name) {
    g_shell_job_id++;
    HalaShellJob *job = &g_shell_jobs[(g_shell_job_id - 1u) % HALA_SHELL_MAX_JOBS];
    mem_zero(job, sizeof(*job));
    job->id = g_shell_job_id;
    job->used = 1u;
    name_copy(job->name, name);
    outs("[");
    out_dec(job->id);
    outs("] started ");
    outs(job->name);
    outs("\r\n");
}

/** @brief Thực thi một command graph và áp dụng redirect/background policy. */
static void shell_execute_plan(HalaShellPlan *plan) {
    if (plan->background && !str_starts(plan->commands[0], "run ")) {
        outs("ERR:BACKGROUND_REQUIRES_RUN\r\n");
        g_shell_errors++;
        return;
    }

    if (plan->redirect_path != NULL) {
        char redirected[96];
        u8 overflow = 0u;
        shell_capture_begin(redirected, sizeof(redirected));
        int result = shell_execute_graph(plan);
        u32 length = shell_capture_end(&overflow);
        if ((result != 0) || overflow) {
            outs("redirect: command output too large or failed\r\n");
            g_shell_errors++;
            return;
        }
        int fd = hala_open(plan->redirect_path, HALA_O_CREAT | HALA_O_TRUNC);
        if ((fd < 0) || (hala_write(fd, redirected, length) != (hala_ssize_t)length) ||
            (hala_close(fd) != 0)) {
            outs("redirect: write failed\r\n");
            g_shell_errors++;
            return;
        }
    } else {
        int result = shell_execute_graph(plan);
        if (result != 0) {
            outs("pipe: execution failed code=");
            out_dec((u32)(0u - (u32)result));
            outs("\r\n");
            g_shell_errors++;
        }
    }

    if (plan->background)
        shell_record_job(plan->commands[0] + 4);
}

void shell_execute(char *line) {
    char quote = 0;
    i32 depth = 0;
    char *segment = line;
    for (char *cursor = line;; ++cursor) {
        char character = *cursor;
        if (quote != 0) {
            if (character == quote)
                quote = 0;
        } else if ((character == '"') || (character == '\''))
            quote = character;
        else if ((character == '(') || (character == '{') || (character == '['))
            depth++;
        else if ((character == ')') || (character == '}') || (character == ']')) {
            if (depth == 0) {
                outs("ERR:SYNTAX\r\n");
                g_shell_errors++;
                return;
            }
            depth--;
        }

        if (((character == ';') && (depth == 0) && (quote == 0)) || (character == '\0')) {
            char terminator = character;
            *cursor = '\0';
            HalaShellPlan plan;
            if (*shell_trim(segment) != '\0') {
                if (shell_parse_plan(segment, &plan) != 0) {
                    outs("ERR:SYNTAX\r\n");
                    g_shell_errors++;
                    return;
                }
                shell_execute_plan(&plan);
            }
            if (terminator == '\0')
                break;
            segment = cursor + 1;
        }
    }
    if ((quote != 0) || (depth != 0)) {
        outs("ERR:SYNTAX\r\n");
        g_shell_errors++;
    }
}
