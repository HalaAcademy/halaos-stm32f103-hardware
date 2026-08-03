/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file halaos_kernel.c
 * @brief Kernel core, scheduler, exception, UART IRQ và memory boundary.
 * @details File được tách từ baseline đã qualification; comment tiếng Việt được giữ để phục vụ
 * review và đào tạo.
 */
#include "halaos/internal/halaos_internal.h"

/**
 * @brief Xác định policy có thuộc nhóm real-time FIFO/RR hay không.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Xác định policy có thuộc nhóm real-time FIFO/RR hay
 * không. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] p Con trỏ hoặc giá trị policy/priority tùy ngữ cảnh.
 * @return 1 nếu là FIFO/RR, ngược lại 0.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int is_rt_policy(u8 p) { return p == POLICY_FIFO || p == POLICY_RR; }

/**
 * @brief Xác định policy có thuộc nhóm fair NORMAL/BATCH hay không.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Xác định policy có thuộc nhóm fair NORMAL/BATCH hay
 * không. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] p Con trỏ hoặc giá trị policy/priority tùy ngữ cảnh.
 * @return 1 nếu là NORMAL/BATCH, ngược lại 0.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int is_fair_policy(u8 p) { return p == POLICY_NORMAL || p == POLICY_BATCH; }

/**
 * @brief Tính địa chỉ đầu của một slot app store.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Tính địa chỉ đầu của một slot app store. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] slot Chỉ số app slot A/B.
 * @return Địa chỉ đầu slot trong Flash.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 app_slot_addr(u32 slot) { return APP_STORE_BASE + (slot % APP_SLOT_COUNT) * APP_SLOT_SIZE; }

/**
 * @brief Lấy con trỏ header của một slot app store.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Lấy con trỏ header của một slot app store. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] slot Chỉ số app slot A/B.
 * @return Con trỏ read-only tới header slot.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
const volatile AppHeader *app_header_slot(u32 slot) {
    return (const volatile AppHeader *)app_slot_addr(slot);
}

/**
 * @brief Lấy header của slot application đang active.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Lấy header của slot application đang active. Thiết
 * kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài
 * nguyên của STM32F103C8.
 * @return Con trỏ header active.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
const volatile AppHeader *app_header(void) { return app_header_slot(g_app_slot); }

/**
 * @brief Lấy địa chỉ bytecode của application đang active.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Lấy địa chỉ bytecode của application đang active.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @return Con trỏ bytecode active.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
const u8 *app_code(void) { return (const u8 *)(app_slot_addr(g_app_slot) + APP_HEADER_SIZE); }

/**
 * @brief Lấy địa chỉ tên application trong header Flash.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Lấy địa chỉ tên application trong header Flash.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @return Con trỏ tên application active.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
const char *app_name(void) { return (const char *)(app_slot_addr(g_app_slot) + 20u); }

/**
 * @brief Sao chép tên application có giới hạn và luôn kết thúc bằng NUL.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Sao chép tên application có giới hạn và luôn kết
 * thúc bằng NUL. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8.
 * @param[out] d Con trỏ vùng đích.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void name_copy(char *d, const char *s) {
    u32 i = 0;
    for (; i < 11u && s && s[i]; i++)
        d[i] = s[i];
    for (; i < 12u; i++)
        d[i] = 0;
}

/**
 * @brief Kiểm tra tên application theo tập ký tự và độ dài cho phép.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kiểm tra tên application theo tập ký tự và độ dài
 * cho phép. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @return 1 nếu hợp lệ, 0 nếu không.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int name_valid(const char *s) {
    u32 n = str_len(s);
    if (n == 0 || n > 11u)
        return 0;
    for (u32 i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '_'))
            return 0;
    }
    return 1;
}

/**
 * @brief So sánh tên chỉ định với tên application active trong Flash.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. So sánh tên chỉ định với tên application active
 * trong Flash. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @return 1 nếu tên trùng application active.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int app_name_eq(const char *s) {
    const char *n = app_name();
    u32 i = 0;
    for (; i < 11u; i++) {
        if (n[i] != s[i])
            return 0;
        if (!n[i])
            return 1;
    }
    return s[11] == 0;
}

/**
 * @brief Khởi tạo toàn bộ hàng đợi ready của scheduler real-time.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Khởi tạo toàn bộ hàng đợi ready của scheduler real-
 * time. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void rt_queue_init(void) {
    for (u32 i = 0; i < RT_LEVELS; i++) {
        g_rt_head[i] = INVALID_IDX;
        g_rt_tail[i] = INVALID_IDX;
    }
    g_rt_bitmap = 0;
}

/**
 * @brief Đưa task real-time vào cuối hàng đợi theo effective priority.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đưa task real-time vào cuối hàng đợi theo effective
 * priority. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] idx Chỉ số task/process trong fixed pool.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void rt_enqueue(u8 idx) {
    Tcb *t = &g_tasks[idx];
    u8 p = t->effective_prio;
    if (p >= RT_LEVELS)
        p = RT_LEVELS - 1;
    t->ready_next = INVALID_IDX;
    if (g_rt_tail[p] == INVALID_IDX)
        g_rt_head[p] = g_rt_tail[p] = idx;
    else {
        g_tasks[g_rt_tail[p]].ready_next = idx;
        g_rt_tail[p] = idx;
    }
    g_rt_bitmap |= (u16)(1u << p);
}

/**
 * @brief Loại task khỏi hàng đợi real-time đang chứa task đó.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Loại task khỏi hàng đợi real-time đang chứa task đó.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] idx Chỉ số task/process trong fixed pool.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void rt_remove(u8 idx) {
    Tcb *t = &g_tasks[idx];
    u8 p = t->effective_prio;
    if (p >= RT_LEVELS)
        p = RT_LEVELS - 1;
    u8 prev = INVALID_IDX, cur = g_rt_head[p];
    while (cur != INVALID_IDX) {
        if (cur == idx) {
            u8 next = g_tasks[cur].ready_next;
            if (prev == INVALID_IDX)
                g_rt_head[p] = next;
            else
                g_tasks[prev].ready_next = next;
            if (g_rt_tail[p] == cur)
                g_rt_tail[p] = prev;
            g_tasks[cur].ready_next = INVALID_IDX;
            if (g_rt_head[p] == INVALID_IDX)
                g_rt_bitmap &= (u16) ~(1u << p);
            return;
        }
        prev = cur;
        cur = g_tasks[cur].ready_next;
    }
}

/**
 * @brief Thực hiện xoay vòng Round Robin tại một mức ưu tiên.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Thực hiện xoay vòng Round Robin tại một mức ưu tiên.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] p Con trỏ hoặc giá trị policy/priority tùy ngữ cảnh.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void rt_rotate(u8 p) {
    if (p >= RT_LEVELS)
        return;
    u8 h = g_rt_head[p];
    if (h == INVALID_IDX || g_tasks[h].ready_next == INVALID_IDX)
        return;
    g_rt_head[p] = g_tasks[h].ready_next;
    g_tasks[h].ready_next = INVALID_IDX;
    g_tasks[g_rt_tail[p]].ready_next = h;
    g_rt_tail[p] = h;
    g_rr_rotations++;
}

/**
 * @brief Tìm task real-time READY có mức ưu tiên cao nhất.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Tìm task real-time READY có mức ưu tiên cao nhất.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @return Chỉ số task RT hoặc INVALID_IDX.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u8 highest_rt(void) {
    for (u8 p = 0; p < RT_LEVELS; p++)
        if (g_rt_bitmap & (1u << p))
            return g_rt_head[p];
    return INVALID_IDX;
}

/**
 * @brief Loại task khỏi delta sleep queue.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Loại task khỏi delta sleep queue. Thiết kế tránh cấp
 * phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên của
 * STM32F103C8.
 * @param[in] idx Chỉ số task/process trong fixed pool.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void sleep_remove(u8 idx) {
    u8 prev = INVALID_IDX, cur = g_sleep_head;
    while (cur != INVALID_IDX) {
        if (cur == idx) {
            u8 next = g_tasks[cur].sleep_next;
            if (next != INVALID_IDX)
                g_tasks[next].sleep_delta += g_tasks[cur].sleep_delta;
            if (prev == INVALID_IDX)
                g_sleep_head = next;
            else
                g_tasks[prev].sleep_next = next;
            g_tasks[cur].sleep_next = INVALID_IDX;
            g_tasks[cur].sleep_delta = 0;
            return;
        }
        prev = cur;
        cur = g_tasks[cur].sleep_next;
    }
}

/**
 * @brief Đưa task vào delta sleep queue với thời gian chờ tương đối.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đưa task vào delta sleep queue với thời gian chờ
 * tương đối. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] idx Chỉ số task/process trong fixed pool.
 * @param[in] ticks Số tick cần ngủ hoặc thời gian tương đối.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void sleep_insert(u8 idx, u32 ticks) {
    if (ticks == 0)
        ticks = 1;
    sleep_remove(idx);
    u8 prev = INVALID_IDX, cur = g_sleep_head;
    u32 remain = ticks;
    while (cur != INVALID_IDX && remain >= g_tasks[cur].sleep_delta) {
        remain -= g_tasks[cur].sleep_delta;
        prev = cur;
        cur = g_tasks[cur].sleep_next;
    }
    g_tasks[idx].sleep_delta = remain;
    g_tasks[idx].sleep_next = cur;
    if (cur != INVALID_IDX)
        g_tasks[cur].sleep_delta -= remain;
    if (prev == INVALID_IDX)
        g_sleep_head = idx;
    else
        g_tasks[prev].sleep_next = idx;
}

/**
 * @brief Chuyển task sang READY và cập nhật hàng đợi tương ứng.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chuyển task sang READY và cập nhật hàng đợi tương
 * ứng. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông
 * qua syscall/scheduler thay vì trả về tự do.
 * @param[in] idx Chỉ số task/process trong fixed pool.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_make_ready(u8 idx) {
    Tcb *t = &g_tasks[idx];
    if (t->state == TASK_READY)
        return;
    t->state = TASK_READY;
    t->sleep_next = INVALID_IDX;
    t->sleep_delta = 0;
    if (is_rt_policy(t->policy))
        rt_enqueue(idx);
    g_wakeup_count++;
    g_need_resched = 1;
}

/**
 * @brief Tiến thời gian của delta sleep queue và đánh thức task đến hạn.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Tiến thời gian của delta sleep queue và đánh thức
 * task đến hạn. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] INVALID_IDX Tham số INVALID_IDX của hàm.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void sleep_advance(u32 elapsed) {
    while (elapsed && g_sleep_head != INVALID_IDX) {
        u8 h = g_sleep_head;
        if (g_tasks[h].sleep_delta > elapsed) {
            g_tasks[h].sleep_delta -= elapsed;
            elapsed = 0;
            break;
        }
        elapsed -= g_tasks[h].sleep_delta;
        g_tasks[h].sleep_delta = 0;
        g_sleep_head = g_tasks[h].sleep_next;
        g_tasks[h].sleep_next = INVALID_IDX;
        if (g_tasks[h].policy == POLICY_DEADLINE) {
            g_tasks[h].deadline_remaining = g_tasks[h].deadline_runtime;
            g_tasks[h].abs_deadline = (u64)g_ticks + g_tasks[h].deadline_period;
            g_tasks[h].next_release = (u64)g_ticks + g_tasks[h].deadline_period;
        }
        task_make_ready(h);
    }
}

/**
 * @brief Đo số word stack đã sử dụng dựa trên mẫu canary.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đo số word stack đã sử dụng dựa trên mẫu canary.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] t Con trỏ TCB cần thao tác.
 * @return Số word stack đã sử dụng.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 stack_used_words(const Tcb *t) {
    u32 total = (u32)(t->stack_high - t->stack_low);
    u32 free = 0;
    while (free < total && t->stack_low[free] == STACK_PATTERN)
        free++;
    return total - free;
}

/**
 * @brief Đo số byte MSP đã sử dụng dựa trên vùng canary.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đo số byte MSP đã sử dụng dựa trên vùng canary.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @return Số byte MSP đã sử dụng.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 msp_used_bytes(void) {
    u32 *p = &__msp_stack_bottom;
    u32 total = (u32)(&__msp_stack_top - &__msp_stack_bottom), free = 0;
    while (free < total && p[free] == STACK_PATTERN)
        free++;
    return (total - free) * 4u;
}

/**
 * @brief Cập nhật giá trị high-water stack của một task.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Cập nhật giá trị high-water stack của một task.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] t Con trỏ TCB cần thao tác.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void update_stack_peak(Tcb *t) {
    u32 used = stack_used_words(t) * 4u;
    if (used > t->stack_peak)
        t->stack_peak = (u16)used;
}

/**
 * @brief Kiểm tra canary ở biên stack để phát hiện tràn stack.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kiểm tra canary ở biên stack để phát hiện tràn
 * stack. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] t Con trỏ TCB cần thao tác.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void stack_guard_check(Tcb *t) {
    if (t && t->stack_low && t->stack_low[0] != STACK_PATTERN)
        g_stack_corruption++;
}

/**
 * @brief Điểm chặn khi task function trả về ngoài dự kiến.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Điểm chặn khi task function trả về ngoài dự kiến.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông qua
 * syscall/scheduler thay vì trả về tự do.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_exit_trap(void) {
    for (;;)
        __asm volatile("svc #10");
}

/**
 * @brief Tạo exception frame và context ban đầu cho một task mới.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Tạo exception frame và context ban đầu cho một task
 * mới. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @param[inout] base Địa chỉ đầu vùng stack.
 * @param[in] words Kích thước stack theo word 32-bit.
 * @param[inout] entry Hàm entry của task.
 * @param[inout] arg Tham số truyền cho task hoặc syscall.
 * @return Stack pointer ban đầu dùng cho context restore.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 *init_stack(u32 *base, u32 words, void (*entry)(void *), void *arg) {
    for (u32 i = 0; i < words; i++)
        base[i] = STACK_PATTERN;
    u32 *sp = base + words;
    sp = (u32 *)((usize)sp & ~7u);
    *(--sp) = 0x01000000u;
    *(--sp) = ((u32)(usize)entry) | 1u;
    *(--sp) = ((u32)(usize)task_exit_trap) | 1u;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = (u32)(usize)arg;
    for (u32 i = 0; i < 8; i++)
        *(--sp) = 0;
    return sp;
}

/**
 * @brief Khởi tạo đầy đủ TCB, stack, policy, capability và metadata task.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Khởi tạo đầy đủ TCB, stack, policy, capability và
 * metadata task. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc
 * block thông qua syscall/scheduler thay vì trả về tự do.
 * @param[in] i Chỉ số task.
 * @param[inout] stack Con trỏ vùng stack của task.
 * @param[in] words Kích thước stack theo word 32-bit.
 * @param[inout] entry Hàm entry của task.
 * @param[in] name Tên task hoặc application.
 * @param[in] pid Process ID.
 * @param[in] policy Scheduler policy.
 * @param[in] prio Độ ưu tiên cơ sở.
 * @param[in] weight Trọng số fair scheduling.
 * @param[in] caps Bitmap capability.
 * @param[in] flags Cờ cấu hình.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_init(u8 i, u32 *stack, u32 words, void (*entry)(void *), const char *name, u16 pid,
               u8 policy, u8 prio, u16 weight, u32 caps, u8 flags) {
    Tcb *t = &g_tasks[i];
    mem_zero(t, sizeof(*t));
    t->sp = init_stack(stack, words, entry, NULL);
    t->stack_low = stack;
    t->stack_high = stack + words;
    t->caps = caps;
    t->pid = pid;
    t->state = TASK_READY;
    t->policy = policy;
    t->base_prio = prio;
    t->effective_prio = prio;
    t->weight = weight ? weight : 100;
    t->quantum = 2;
    t->quantum_left = 2;
    t->ready_next = INVALID_IDX;
    t->sleep_next = INVALID_IDX;
    t->flags = flags;
    t->name = name;
    if (is_rt_policy(policy))
        rt_enqueue(i);
}

/**
 * @brief So sánh thứ tự ưu tiên của hai scheduling entity fair.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. So sánh thứ tự ưu tiên của hai scheduling entity
 * fair. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc block thông
 * qua syscall/scheduler thay vì trả về tự do.
 * @param[in] a Đối tượng/chuỗi thứ nhất.
 * @param[in] b Đối tượng/chuỗi thứ hai.
 * @return 1 nếu task thứ nhất nên được chọn trước.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((noinline, optnone)) int task_precedes(const Tcb *a, const Tcb *b) {
    if (a->policy == POLICY_DEADLINE && b->policy != POLICY_DEADLINE)
        return 1;
    if (b->policy == POLICY_DEADLINE && a->policy != POLICY_DEADLINE)
        return 0;
    if (is_rt_policy(a->policy) && !is_rt_policy(b->policy) && !is_rt_policy(b->policy))
        return 1;
    if (is_rt_policy(b->policy) && !is_rt_policy(a->policy))
        return 0;
    if (is_rt_policy(a->policy) && is_rt_policy(b->policy))
        return a->effective_prio < b->effective_prio;
    if (is_fair_policy(a->policy) && is_fair_policy(b->policy))
        return a->virtual_deadline < b->virtual_deadline;
    return 0;
}

/**
 * @brief Chọn task DEADLINE đủ điều kiện có absolute deadline sớm nhất.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chọn task DEADLINE đủ điều kiện có absolute deadline
 * sớm nhất. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @return Chỉ số task DEADLINE hoặc INVALID_IDX.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((noinline, optnone)) u8 pick_deadline(void) {
    u8 best = INVALID_IDX;
    u64 d = ~(u64)0;
    u32 scan = 0;
    for (u8 i = 0; i < MAX_TASKS; i++) {
        scan++;
        Tcb *t = &g_tasks[i];
        if (t->state == TASK_READY && t->policy == POLICY_DEADLINE && t->deadline_remaining &&
            t->abs_deadline < d) {
            best = i;
            d = t->abs_deadline;
        }
    }
    if (scan > g_scheduler_max_scan)
        g_scheduler_max_scan = scan;
    return best;
}

/**
 * @brief Chọn task NORMAL/BATCH có virtual deadline nhỏ nhất.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chọn task NORMAL/BATCH có virtual deadline nhỏ nhất.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @return Chỉ số task FAIR hoặc INVALID_IDX.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((noinline, optnone)) u8 pick_fair(void) {
    u8 best = INVALID_IDX;
    u64 vd = ~(u64)0;
    u32 scan = 0;
    for (u8 i = 0; i < MAX_TASKS; i++) {
        scan++;
        Tcb *t = &g_tasks[i];
        if (t->state == TASK_READY && is_fair_policy(t->policy)) {
            if (t->virtual_deadline == 0)
                t->virtual_deadline = t->vruntime + (u64)(4u * 1024u / t->weight);
            if (t->virtual_deadline < vd) {
                vd = t->virtual_deadline;
                best = i;
            }
        }
    }
    if (scan > g_scheduler_max_scan)
        g_scheduler_max_scan = scan;
    if (best != INVALID_IDX)
        g_fair_picks++;
    return best;
}

/**
 * @brief Chọn task tiếp theo theo thứ tự DEADLINE, RT, FAIR và IDLE.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Chọn task tiếp theo theo thứ tự DEADLINE, RT, FAIR
 * và IDLE. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @return Chỉ số task kế tiếp.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((noinline, optnone)) u8 sched_pick_next(void) {
    g_scheduler_calls++;
    u8 idx = pick_deadline();
    if (idx != INVALID_IDX) {
        g_deadline_picks++;
        return idx;
    }
    if (!g_rt_throttled) {
        idx = highest_rt();
        if (idx != INVALID_IDX)
            return idx;
    }
    idx = pick_fair();
    if (idx != INVALID_IDX)
        return idx;
    return 0;
}

/**
 * @brief Cập nhật accounting và trả về stack pointer của task kế tiếp.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Cập nhật accounting và trả về stack pointer của task
 * kế tiếp. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((used, noinline, optnone)) void hala_schedule_next(void) {
    u8 old = (u8)g_current_index;
    stack_guard_check(&g_tasks[old]);
    u8 next = sched_pick_next();
    if (next != old) {
        g_tasks[old].involuntary++;
        g_tasks[next].switches++;
        g_switches++;
    }
    g_current_index = next;
    g_current = &g_tasks[next];
    g_need_resched = 0;
}

/**
 * @brief Yêu cầu PendSV để thực hiện context switch trì hoãn.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Yêu cầu PendSV để thực hiện context switch trì hoãn.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void pend_resched(void) {
    g_need_resched = 1;
    if (g_preempt_count == 0)
        SCB_ICSR = (1u << 28);
}

/**
 * @brief Block task hiện tại theo state và thời gian yêu cầu.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Block task hiện tại theo state và thời gian yêu cầu.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] state Trạng thái task cần thiết lập.
 * @param[in] ticks Số tick cần ngủ hoặc thời gian tương đối.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void block_current(u8 state, u32 ticks) {
    u8 idx = (u8)g_current_index;
    Tcb *t = &g_tasks[idx];
    if (is_rt_policy(t->policy))
        rt_remove(idx);
    t->state = state;
    t->voluntary++;
    if (state == TASK_SLEEP)
        sleep_insert(idx, ticks);
    pend_resched();
}

/**
 * @brief Ghi nhận PSP không hợp lệ và chọn biện pháp phục hồi an toàn.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Ghi nhận PSP không hợp lệ và chọn biện pháp phục hồi
 * an toàn. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] psp Giá trị Process Stack Pointer.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((used, noinline)) void hala_bad_psp(u32 psp) {
    g_bad_psp = psp;
    g_bad_task = g_current_index;
    uart_puts_priv("KERNEL:BAD_PSP psp=");
    uart_hex_priv(psp);
    uart_puts_priv("\r\n");
    for (;;) {
    }
}

/**
 * @brief Lưu/khôi phục context R4-R11 và chuyển PSP giữa các task.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Lưu/khôi phục context R4-R11 và chuyển PSP giữa các
 * task. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8. Hàm chạy trong exception/interrupt context; không được gọi trực
 * tiếp từ application.
 * @pre g_current trỏ tới TCB hợp lệ và PSP nằm trong stack task.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile(
        "cpsid i\n mrs r0,psp\n ldr r1,=0x20000020\n cmp r0,r1\n blo 2f\n ldr r1,=0x20005000\n cmp "
        "r0,r1\n bhi 2f\n ldr r3,=g_current\n ldr r2,[r3]\n stmdb r0!,{r4-r11}\n str r0,[r2]\n "
        "push {r3,lr}\n bl hala_schedule_next\n pop {r3,lr}\n ldr r1,[r3]\n ldr r0,[r1]\n ldmia "
        "r0!,{r4-r11}\n msr psp,r0\n cpsie i\n bx lr\n 2:\n b hala_bad_psp\n");
}

/**
 * @brief Kết thúc task user sau khi HardFault được chuyển thành lỗi process.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kết thúc task user sau khi HardFault được chuyển
 * thành lỗi process. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8. Đây là entry function của task; task kết thúc hoặc
 * block thông qua syscall/scheduler thay vì trả về tự do.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void task_fault_trampoline(void) {
    __asm volatile("svc #10");
    for (;;) {
    }
}

/**
 * @brief Phân tích exception frame, phân loại user/kernel fault và phục hồi.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Phân tích exception frame, phân loại user/kernel
 * fault và phục hồi. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] frame Con trỏ exception/SVC stack frame.
 * @param[in] exc_return Giá trị EXC_RETURN do CPU cung cấp.
 * @pre frame trỏ tới exception frame do Cortex-M3 tạo.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((used, noinline)) void HardFault_C(u32 *frame, u32 exc_return) {
    g_fault_count++;
    g_fault_pc = frame[6];
    g_fault_lr = frame[5];
    u32 cfsr = SCB_CFSR;
    u32 hfsr = SCB_HFSR;
    (void)cfsr;
    (void)hfsr;
    if ((exc_return & 4u) && g_current_index != 0u && g_current_index < MAX_TASKS) {
        g_last_fault_task = (u8)g_current_index;
        g_last_fault_pid = g_tasks[g_current_index].pid;
        uart_puts_priv("\r\nUser HardFault\r\n  PID: ");
        uart_dec_priv(g_tasks[g_current_index].pid);
        uart_puts_priv("\r\n  Process: ");
        uart_puts_priv(g_tasks[g_current_index].name);
        uart_puts_priv("\r\n  PC: 0x");
        uart_hex_priv(g_fault_pc);
        uart_puts_priv("\r\n  LR: 0x");
        uart_hex_priv(g_fault_lr);
        uart_puts_priv("\r\n  CFSR: 0x");
        uart_hex_priv(cfsr);
        uart_puts_priv("\r\nAction: terminate user process\r\nKernel status: continuing\r\nhala$ ");
        g_tasks[g_current_index].state = TASK_ZOMBIE;
        if (g_current_index == 11u || g_current_index == 12u) {
            g_thread_faults++;
            g_thread_exits++;
            g_app_running = 0;
            demo_block_task(g_current_index == 11u ? 12u : 11u);
        }
        g_user_fault_recoveries++;
        g_crash_record.user_fault_recoveries_total = g_user_fault_recoveries;
        g_app_exit_status = 139;
        frame[6] = ((u32)(usize)task_fault_trampoline) | 1u;
        SCB_CFSR = 0xFFFFFFFFu;
        SCB_HFSR = 0xFFFFFFFFu;
        return;
    }
    g_kernel_faults++;
    g_crash_record.magic = 0x48435253u;
    g_crash_record.fault_pc = g_fault_pc;
    g_crash_record.fault_lr = g_fault_lr;
    g_crash_record.last_stage = 5;
    uart_puts_priv("FAULT:KERNEL PC=");
    uart_hex_priv(g_fault_pc);
    uart_puts_priv("\r\n");
    for (;;) {
    }
}

/**
 * @brief Assembly wrapper chọn MSP/PSP rồi gọi bộ xử lý HardFault bằng C.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Assembly wrapper chọn MSP/PSP rồi gọi bộ xử lý
 * HardFault bằng C. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8. Hàm chạy trong exception/interrupt context; không
 * được gọi trực tiếp từ application.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((naked)) void HardFault_Handler(void) {
    __asm volatile(
        "mov r1,lr\n tst lr,#4\n ite eq\n mrseq r0,msp\n mrsne r0,psp\n b HardFault_C\n");
}

/**
 * @brief Theo dõi ngân sách real-time theo cửa sổ và kích hoạt throttling.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Theo dõi ngân sách real-time theo cửa sổ và kích
 * hoạt throttling. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] RT_WINDOW_TICKS Tham số RT_WINDOW_TICKS của hàm.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void rt_account(u32 step) {
    if ((u32)(g_ticks - g_rt_window_start) >= RT_WINDOW_TICKS) {
        g_rt_window_start = g_ticks;
        g_rt_used = 0;
        g_rt_throttled = 0;
    }
    if (g_current && is_rt_policy(g_current->policy)) {
        g_rt_used += step;
        if (g_rt_used >= RT_BUDGET_TICKS && !g_rt_throttled) {
            g_rt_throttled = 1;
            g_rt_throttle_count++;
            pend_resched();
        }
    }
}

/**
 * @brief Trừ runtime budget, phát hiện miss và tái phát hành task DEADLINE.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Trừ runtime budget, phát hiện miss và tái phát hành
 * task DEADLINE. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] t Con trỏ TCB cần thao tác.
 * @param[in] step Số tick/runtime cần accounting.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void deadline_account(Tcb *t, u32 step) {
    if (step >= t->deadline_remaining)
        t->deadline_remaining = 0;
    else
        t->deadline_remaining -= step;
    if ((u64)g_ticks > t->abs_deadline) {
        t->deadline_misses++;
        g_deadline_misses++;
    }
    if (t->deadline_remaining == 0) {
        t->throttle_count++;
        g_deadline_throttles++;
        t->state = TASK_SLEEP;
        u32 wait = (t->next_release > (u64)g_ticks) ? (u32)(t->next_release - g_ticks)
                                                    : t->deadline_period;
        sleep_insert((u8)g_current_index, wait);
        pend_resched();
    }
}

/**
 * @brief Cập nhật system tick, accounting, wakeup và yêu cầu reschedule.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Cập nhật system tick, accounting, wakeup và yêu cầu
 * reschedule. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8. Hàm chạy trong exception/interrupt context; không được
 * gọi trực tiếp từ application.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
/**
 * @brief Xử lý kernel tick và yêu cầu lập lịch lại khi cần.
 * @details Tạm vô hiệu hóa tối ưu riêng hàm này. Với Clang/LTO -O1 trên
 *          Cortex-M3, phiên bản tối ưu từng phát sinh đường khôi phục exception
 *          không ổn định khi UART xuất bảng process dài đồng thời có SysTick.
 *          Các test regression `ps` + preemption và stress tick bảo vệ workaround.
 * @warning Chỉ gỡ `optnone` sau khi assembly mới vượt toàn bộ regression trên Blue Pill
 *          và kiểm thử context-switch trên phần cứng thật.
 * @trace HOS-KER-ERR-001
 */
__attribute__((optnone)) void SysTick_Handler(void) {
    g_in_systick = 1;
    u32 step = g_tick_step;
    if (step == 0)
        step = 1;
    g_ticks += step;
    if (step > 1) {
        g_tickless_skipped += step - 1;
        g_tick_step = 1;
        SYST_RVR = TICK_CYCLES - 1u;
    }
    sleep_advance(step);
    kobj_ipc_tick(g_ticks);
    if (g_current) {
        Tcb *t = (Tcb *)g_current;
        t->runtime += step;
        if (g_current_index == 1)
            g_shell_ticks += step;
        else if (g_current_index == 2)
            g_compiler_ticks += step;
        else if (g_current_index == 3)
            g_vm_ticks += step;
        else if (g_current_index == 4)
            g_rr_a_ticks += step;
        else if (g_current_index == 5)
            g_rr_b_ticks += step;
        else if (g_current_index == 7)
            g_fair_a_ticks += step;
        else if (g_current_index == 8)
            g_fair_b_ticks += step;
        else if (g_current_index == 9)
            g_deadline_ticks += step;
        stack_guard_check(t);
        if (t->policy == POLICY_RR) {
            if (step >= t->quantum_left) {
                t->quantum_left = t->quantum;
                rt_rotate(t->effective_prio);
                pend_resched();
            } else
                t->quantum_left = (u8)(t->quantum_left - step);
        } else if (t->policy == POLICY_FIFO) {
            g_fifo_ticks += step;
        } else if (is_fair_policy(t->policy)) {
            u32 inc = (step * 1024u) / t->weight;
            if (inc == 0)
                inc = 1;
            t->vruntime += inc;
            t->slice_used += step;
            if (t->slice_used >= ((t->policy == POLICY_BATCH) ? 8u : 4u)) {
                t->slice_used = 0;
                t->virtual_deadline = t->vruntime + (u64)(((t->policy == POLICY_BATCH) ? 8u : 4u) *
                                                          1024u / t->weight);
                pend_resched();
            }
        } else if (t->policy == POLICY_DEADLINE)
            deadline_account(t, step);
    }
    rt_account(step);
    if (g_need_resched && g_preempt_count == 0)
        SCB_ICSR = (1u << 28);
    g_in_systick = 0;
}

/**
 * @brief Đánh thức HalaShell khi USART1 nhận được dữ liệu mới.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đánh thức HalaShell khi USART1 nhận được dữ liệu
 * mới. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void uart_wake_shell(void) {
    Tcb *t = &g_tasks[1];
    if (t->state == TASK_BLOCKED) {
        t->state = TASK_READY;
        g_uart_wakeups++;
        g_wakeup_count++;
        g_need_resched = 1;
        if (!g_in_systick)
            SCB_ICSR = (1u << 28);
    }
}

/**
 * @brief Nhận byte UART vào ring buffer và đánh thức shell.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Nhận byte UART vào ring buffer và đánh thức shell.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8. Hàm chạy trong exception/interrupt context; không được gọi trực tiếp
 * từ application.
 * @pre USART1 IRQ đã được enable sau console handover.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void USART1_IRQHandler(void) {
    g_uart_irq_count++;
    u32 sr = USART1_SR;
    if (sr & (1u << 5)) {
        u8 v = (u8)USART1_DR;
        g_uart_bytes++;
        u16 next = (u16)((rx_head + 1u) & 127u);
        if (next == rx_tail)
            g_rx_overflow++;
        else {
            rx_ring[rx_head] = v;
            rx_head = next;
            uart_wake_shell();
        }
    } else {
        (void)USART1_DR;
    }
}

/**
 * @brief Kiểm tra toàn bộ khoảng địa chỉ nằm trong SRAM hợp lệ.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kiểm tra toàn bộ khoảng địa chỉ nằm trong SRAM hợp
 * lệ. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @param[in] p Con trỏ hoặc giá trị policy/priority tùy ngữ cảnh.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @return 1 nếu toàn bộ vùng nằm trong SRAM.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int ptr_in_sram(const void *p, u32 n) {
    u32 a = (u32)(usize)p;
    if (a < HalaOS_RAM_BASE || a > HalaOS_RAM_BASE + HalaOS_RAM_SIZE)
        return 0;
    if (n > HalaOS_RAM_SIZE || a + n < a || a + n > HalaOS_RAM_BASE + HalaOS_RAM_SIZE)
        return 0;
    return 1;
}

/**
 * @brief Kiểm tra con trỏ user có thể được kernel đọc an toàn.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kiểm tra con trỏ user có thể được kernel đọc an
 * toàn. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @param[in] p Con trỏ hoặc giá trị policy/priority tùy ngữ cảnh.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @return 1 nếu vùng có thể đọc an toàn.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int ptr_readable(const void *p, u32 n) {
    u32 a = (u32)(usize)p;
    if (n == 0)
        return 1;
    if (a + n < a)
        return 0;
    if (a >= HalaOS_RAM_BASE && a + n <= HalaOS_RAM_BASE + HalaOS_RAM_SIZE)
        return 1;
    if (a >= 0x08000000u && a + n <= APP_STORE_BASE + APP_STORE_SIZE)
        return 1;
    return 0;
}

/**
 * @brief Kiểm tra capability của task hiện hành.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kiểm tra capability của task hiện hành. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] cap Tham số cap của hàm.
 * @return 1 nếu task hiện hành có capability.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int current_has(u32 cap) { return (g_current->caps & cap) == cap; }

/**
 * @brief Lấy một byte từ ring buffer UART RX.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Lấy một byte từ ring buffer UART RX. Thiết kế tránh
 * cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên của
 * STM32F103C8.
 * @return Byte nhận được (0..255), hoặc giá trị âm nếu buffer rỗng.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int rx_pop(void) {
    if (rx_tail == rx_head)
        return -1;
    u8 v = rx_ring[rx_tail];
    rx_tail = (u16)((rx_tail + 1u) & 127u);
    return v;
}
