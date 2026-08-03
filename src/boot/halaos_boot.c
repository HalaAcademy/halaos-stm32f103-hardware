/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file halaos_boot.c
 * @brief Boot, startup, compact DTB và platform early services.
 * @details File được tách từ baseline đã qualification; comment tiếng Việt được giữ để phục vụ
 * review và đào tạo.
 */
#include "halaos/internal/halaos_internal.h"

/**
 * @brief Tính CRC-32 cho một vùng byte theo đa thức Ethernet chuẩn.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Tính CRC-32 cho một vùng byte theo đa thức Ethernet
 * chuẩn. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] d Con trỏ vùng đích.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @return Giá trị CRC-32 đã đảo bit cuối.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 crc32_bytes(const u8 *d, u32 n) {
    u32 c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *d++;
        for (u32 i = 0; i < 8; i++)
            c = (c >> 1) ^ ((0u - (c & 1u)) & 0xEDB88320u);
    }
    return ~c;
}

/**
 * @brief Tính CRC của phần dữ liệu được bảo vệ trong boot manifest.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Tính CRC của phần dữ liệu được bảo vệ trong boot
 * manifest. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @return CRC của boot manifest.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 manifest_crc(void) {
    return crc32_bytes((const u8 *)&g_boot_manifest, (u32)sizeof(BootManifest) - 4u);
}

/**
 * @brief Tính chiều dài chuỗi C không bao gồm ký tự kết thúc.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Tính chiều dài chuỗi C không bao gồm ký tự kết thúc.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @return Số ký tự trước NUL.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 str_len(const char *s) {
    u32 n = 0;
    while (s && s[n])
        n++;
    return n;
}

/**
 * @brief So sánh hai chuỗi C theo từng ký tự.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. So sánh hai chuỗi C theo từng ký tự. Thiết kế tránh
 * cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên của
 * STM32F103C8.
 * @param[in] a Đối tượng/chuỗi thứ nhất.
 * @param[in] b Đối tượng/chuỗi thứ hai.
 * @return 1 nếu bằng nhau, 0 nếu khác.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int str_eq(const char *a, const char *b) {
    u32 i = 0;
    for (;; i++) {
        if (a[i] != b[i])
            return 0;
        if (!a[i])
            return 1;
    }
}

/**
 * @brief Kiểm tra chuỗi nguồn có bắt đầu bằng tiền tố cho trước.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Kiểm tra chuỗi nguồn có bắt đầu bằng tiền tố cho
 * trước. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] a Đối tượng/chuỗi thứ nhất.
 * @param[in] b Đối tượng/chuỗi thứ hai.
 * @return 1 nếu đúng tiền tố, 0 nếu không.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int str_starts(const char *a, const char *b) {
    for (u32 i = 0; b[i]; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

/**
 * @brief Sao chép tuần tự một vùng bộ nhớ theo đơn vị byte.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Sao chép tuần tự một vùng bộ nhớ theo đơn vị byte.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[out] d Con trỏ vùng đích.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void mem_copy(void *d, const void *s, u32 n) {
    u8 *dd = d;
    const u8 *ss = s;
    while (n--)
        *dd++ = *ss++;
}

/**
 * @brief Đặt toàn bộ vùng bộ nhớ về giá trị zero.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đặt toàn bộ vùng bộ nhớ về giá trị zero. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[out] d Con trỏ vùng đích.
 * @param[in] n Số phần tử hoặc số byte cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void mem_zero(void *d, u32 n) {
    u8 *p = d;
    while (n--)
        *p++ = 0;
}

/* Clang có thể hạ phép gán struct thành helper EABI ở profile tối ưu kích thước.
 * HalaOS cung cấp implementation freestanding để không kéo libc vào firmware. */
__attribute__((used)) void __aeabi_memcpy(void *d, const void *s, u32 n) { mem_copy(d, s, n); }
__attribute__((used)) void __aeabi_memcpy4(void *d, const void *s, u32 n) { mem_copy(d, s, n); }
__attribute__((used)) void __aeabi_memcpy8(void *d, const void *s, u32 n) { mem_copy(d, s, n); }
__attribute__((used)) void __aeabi_memset(void *d, u32 n, int value) {
    u8 *p = d;
    while (n--)
        *p++ = (u8)value;
}
__attribute__((used)) void __aeabi_memclr(void *d, u32 n) { mem_zero(d, n); }
__attribute__((used)) void __aeabi_memclr4(void *d, u32 n) { mem_zero(d, n); }
__attribute__((used)) void __aeabi_memclr8(void *d, u32 n) { mem_zero(d, n); }

/**
 * @brief Khởi tạo C runtime bằng cách copy .data và xóa .bss.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Khởi tạo C runtime bằng cách copy .data và xóa .bss.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post .data chứa giá trị khởi tạo và .bss bằng zero.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void copy_zero(void) {
    u32 *src = &_sidata;
    for (u32 *d = &_sdata; d < &_edata;)
        *d++ = *src++;
    for (u32 *d = &_sbss; d < &_ebss;)
        *d++ = 0;
}

/**
 * @brief Gửi một ký tự trực tiếp qua USART1 bằng polling ở privileged mode.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Gửi một ký tự trực tiếp qua USART1 bằng polling ở
 * privileged mode. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8.
 * @param[in] u Tham số u của hàm.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void uart_putc_priv(char c) {
    while ((USART1_SR & (1u << 7)) == 0u) {
    }
    USART1_DR = (u32)(u8)c;
}

/**
 * @brief Gửi chuỗi ký tự trực tiếp qua USART1 bằng polling.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Gửi chuỗi ký tự trực tiếp qua USART1 bằng polling.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] s Con trỏ chuỗi hoặc vùng nguồn.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void uart_puts_priv(const char *s) {
    while (*s)
        uart_putc_priv(*s++);
}

/**
 * @brief In số 32-bit ở dạng hexadecimal đủ tám chữ số.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In số 32-bit ở dạng hexadecimal đủ tám chữ số. Thiết
 * kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài
 * nguyên của STM32F103C8.
 * @param[in] v Giá trị số cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void uart_hex_priv(u32 v) {
    static const char h[] = "0123456789ABCDEF";
    for (i32 s = 28; s >= 0; s -= 4)
        uart_putc_priv(h[(v >> (u32)s) & 15u]);
}

/**
 * @brief In số 32-bit không dấu ở dạng thập phân.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In số 32-bit không dấu ở dạng thập phân. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @param[in] v Giá trị số cần xử lý.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void uart_dec_priv(u32 v) {
    char b[11];
    u32 n = 0;
    if (v == 0) {
        uart_putc_priv('0');
        return;
    }
    while (v && n < 10) {
        b[n++] = (char)('0' + v % 10u);
        v /= 10u;
    }
    while (n)
        uart_putc_priv(b[--n]);
}

/**
 * @brief Đọc một giá trị 32-bit little-endian từ vùng byte.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đọc một giá trị 32-bit little-endian từ vùng byte.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] p Con trỏ hoặc giá trị policy/priority tùy ngữ cảnh.
 * @return Giá trị 32-bit đã ghép theo little-endian.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Có, nếu các vùng dữ liệu đầu vào không bị thay đổi đồng thời.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 rd32le(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/**
 * @brief Ghi nhận một sự kiện boot vào bộ đệm và crash record.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Ghi nhận một sự kiện boot vào bộ đệm và crash
 * record. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp
 * giới hạn tài nguyên của STM32F103C8.
 * @param[in] event Mã sự kiện boot cần ghi nhận.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void boot_event(u16 event) {
    if (g_boot_event_count < ARRAY_LEN(g_boot_events))
        g_boot_events[g_boot_event_count++] = event;
    g_crash_record.last_event = event;
}

/**
 * @brief Xác minh cấu trúc, kích thước, CRC và thông số phần cứng của compact DTB.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Xác minh cấu trúc, kích thước, CRC và thông số phần
 * cứng của compact DTB. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định
 * để phù hợp giới hạn tài nguyên của STM32F103C8.
 * @return 1 nếu DTB hợp lệ, 0 nếu phát hiện lỗi.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
int compact_dtb_validate(void) {
    u32 base = 0, size = 0, uart = 0, baud = 0;
    if (!hala_dtb_validate_blob())
        return 0;
    if (!hala_dtb_get_pair("/memory", "reg", &base, &size))
        return 0;
    if (!hala_dtb_get_pair("/soc/usart1", "reg", &uart, &baud))
        return 0;
    if (!hala_dtb_get_u32("/soc/usart1", "current-speed", &baud))
        return 0;
    if (base != HALAOS_RAM_BASE || size != HALAOS_RAM_SIZE || uart != HALAOS_USART1_BASE ||
        baud != HALAOS_USART1_BAUD)
        return 0;
    return 1;
}

/**
 * @brief In marker của một boot stage và ghi event tương ứng.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. In marker của một boot stage và ghi event tương ứng.
 * Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn
 * tài nguyên của STM32F103C8.
 * @param[in] tag Nhãn stage dùng trong boot log.
 * @param[in] message Thông điệp cần xuất ra UART.
 * @param[in] event Mã sự kiện boot cần ghi nhận.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void early_stage(const char *tag, const char *message, u16 event) {
    uart_puts_priv("[");
    uart_puts_priv(tag);
    uart_puts_priv("] ");
    uart_puts_priv(message);
    uart_puts_priv("\r\n");
    boot_event(event);
}

/**
 * @brief Khởi tạo tối thiểu clock ngoại vi, GPIO, LED và USART1 trước kernel.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Khởi tạo tối thiểu clock ngoại vi, GPIO, LED và
 * USART1 trước kernel. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định
 * để phù hợp giới hạn tài nguyên của STM32F103C8.
 * @pre Clock reset mặc định của STM32F103 phải còn khả dụng.
 * @post USART1 polling hoạt động ở 115200 8N1 và LED ở trạng thái mặc định.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void board_early_init(void) {
    RCC_APB2RSTR |= (1u << 2) | (1u << 4) | (1u << 14);
    RCC_APB2RSTR &= ~((1u << 2) | (1u << 4) | (1u << 14));
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 4) | (1u << 14);
    GPIOA_CRH = (GPIOA_CRH & ~((0xFu << 4) | (0xFu << 8))) | (0xBu << 4) | (0x4u << 8);
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20)) | (0x2u << 20);
    GPIOC_ODR |= (1u << 13);
    USART1_CR1 = 0u;
    (void)USART1_SR;
    (void)USART1_DR;
    USART1_BRR = 0x45u;
    USART1_CR1 = (1u << 13) | (1u << 3) | (1u << 2);
    NVIC_ICER1 = (1u << 5);
    NVIC_ICPR1 = (1u << 5);
}

/**
 * @brief Đảo trạng thái LED PC13 bằng truy cập thanh ghi trực tiếp.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đảo trạng thái LED PC13 bằng truy cập thanh ghi trực
 * tiếp. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void led_toggle_priv(void) { GPIOC_ODR ^= (1u << 13); }

/**
 * @brief Yêu cầu reset hệ thống theo cơ chế phù hợp backend hoặc phần cứng.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Yêu cầu reset hệ thống theo cơ chế phù hợp backend
 * hoặc phần cứng. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để
 * phù hợp giới hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void reboot_priv(void) {
    /* Ghi khóa VECTKEY và đặt SYSRESETREQ để Cortex-M3 yêu cầu reset hệ thống. */
    SCB_AIRCR = (0x5FAu << 16) | (1u << 2);
    for (;;) {
    }
}

/**
 * @brief Đọc giá trị Main Stack Pointer hiện tại.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Đọc giá trị Main Stack Pointer hiện tại. Thiết kế
 * tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài nguyên
 * của STM32F103C8.
 * @return Giá trị MSP hiện tại.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
u32 read_msp(void) {
    u32 v;
    __asm volatile("mrs %0,msp" : "=r"(v));
    return v;
}

/**
 * @brief Entry point đầu tiên sau reset của Cortex-M3.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Entry point đầu tiên sau reset của Cortex-M3. Thiết
 * kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới hạn tài
 * nguyên của STM32F103C8. Hàm chạy trong exception/interrupt context; không được gọi trực tiếp từ
 * application.
 * @pre Vector table và MSP ban đầu phải trỏ tới vùng RAM hợp lệ.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
__attribute__((naked)) void Reset_Handler(void) {
    __asm volatile("cpsid i\n ldr sp,=_estack\n b Reset_Handler_C\n");
}

/**
 * @brief Điền mẫu canary vào vùng MSP chưa sử dụng để đo high-water.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Điền mẫu canary vào vùng MSP chưa sử dụng để đo
 * high-water. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù
 * hợp giới hạn tài nguyên của STM32F103C8.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void fill_msp_pattern(void) {
    u32 *p = &__msp_stack_bottom;
    u32 limit = read_msp();
    if (limit > 128u)
        limit -= 128u;
    while ((u32)(usize)p < limit)
        *p++ = STACK_PATTERN;
}

/**
 * @brief Thực hiện startup, Stage-0, Loader, DTB validation và chuyển vào kernel.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Thực hiện startup, Stage-0, Loader, DTB validation
 * và chuyển vào kernel. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định
 * để phù hợp giới hạn tài nguyên của STM32F103C8.
 * @pre CPU đang ở privileged Handler mode, interrupt đã bị khóa.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
#ifndef HALAOS_BOOT_CRC_GENERATION
static int flash_range_valid(u32 address, u32 size) {
    if (size == 0u || address < 0x08000000u || address >= 0x0800E000u)
        return 0;
    if (size > 0x0000E000u)
        return 0;
    return address + size >= address && address + size <= 0x0800E000u;
}

static void stage0_fail(const char *reason) {
    uart_puts_priv("[S0] ERROR: ");
    uart_puts_priv(reason);
    uart_puts_priv("\r\n[S0] Entering recovery\r\n");
    for (;;) {
    }
}
#endif

/**
 * @brief Loader entry được Stage-0 gọi sau khi mọi image range/CRC hợp lệ.
 * @details Loader xác minh compact DTB, đọc /chosen và memory node, tạo
 *          HalaBootInfo thật rồi truyền pointer sang kernel entry.
 */
__attribute__((used, noinline, section(".hala_loader.text"))) void
hala_loader_entry(const BootManifest *manifest) {
    uart_puts_priv("\r\nHala Loader 0.5.0\r\n");
    g_crash_record.last_stage = 3;
    boot_event(HALA_BOOT_LOADER_ENTER);
    uart_puts_priv("[LDR] Boot source: internal flash\r\n[LDR] Console: USART1 115200 8N1\r\n[LDR] "
                   "Loading compact DTB, size=");
    uart_dec_priv(manifest->dtb_size);
    uart_puts_priv(" bytes\r\n");
    g_dtb_valid = (u32)compact_dtb_validate();
    if (!g_dtb_valid) {
        uart_puts_priv(
            "[DTB] ERROR: compact DTB validation failed\r\n[LDR] Cannot boot kernel\r\n");
        for (;;) {
        }
    }
    const char *model = hala_dtb_get_string("/", "model");
    const char *compatible = hala_dtb_get_string("/", "compatible");
    const char *init_path = hala_dtb_get_string("/chosen", "hala,init");
    const char *stdout_path = hala_dtb_get_string("/chosen", "stdout-path");
    u32 memory_base = 0u, memory_size = 0u, console_base = 0u, console_irq = 0u;
    if (!model || !compatible || !init_path || !stdout_path ||
        !hala_dtb_get_pair("/memory", "reg", &memory_base, &memory_size) ||
        !hala_dtb_get_pair(stdout_path, "reg", &console_base, &g_boot_info.console_base) ||
        !hala_dtb_get_u32(stdout_path, "interrupts", &console_irq)) {
        uart_puts_priv("[LDR] ERROR: required DTB property missing\r\n");
        for (;;) {
        }
    }
    /* reg là cặp base,size; biến tạm thứ hai được tái sử dụng rồi chuẩn hóa. */
    u32 console_size = g_boot_info.console_base;
    (void)console_size;
    g_boot_info.magic = 0x4842494Fu;
    g_boot_info.dtb = g_halaos_compact_dtb;
    g_boot_info.dtb_size = manifest->dtb_size;
    g_boot_info.memory_base = memory_base;
    g_boot_info.memory_size = memory_size;
    g_boot_info.console_base = console_base;
    g_boot_info.console_irq = console_irq;
    g_boot_info.stdout_path = stdout_path;
    g_boot_info.init_path = init_path;
    g_boot_info.manifest_flags = manifest->flags;
    uart_puts_priv("[DTB] Magic/version/CRC: OK\r\n[DTB] Model: ");
    uart_puts_priv(model);
    uart_puts_priv("\r\n[DTB] Compatible: ");
    uart_puts_priv(compatible);
    uart_puts_priv("\r\n[DTB] /chosen/init: ");
    uart_puts_priv(init_path);
    uart_puts_priv("\r\n");
    boot_event(HALA_BOOT_DTB_VALID);
    uart_puts_priv("[LDR] HalaBootInfo ready memory=0x");
    uart_hex_priv(memory_base);
    uart_puts_priv("+");
    uart_dec_priv(memory_size);
    uart_puts_priv(" console=0x");
    uart_hex_priv(console_base);
    uart_puts_priv(" irq=");
    uart_dec_priv(console_irq);
    uart_puts_priv("\r\n");
    uart_puts_priv(
        "[LDR] Console handoff pending: polling -> interrupt\r\n[LDR] Jumping to kernel\r\n");
    kernel_main(&g_boot_info);
    for (;;) {
    }
}

__attribute__((used, noinline)) void Reset_Handler_C(void) {
    SYST_CSR = 0u;
    SCB_ICSR = (1u << 25) | (1u << 27);
    NVIC_ICER0 = 0xFFFFFFFFu;
    NVIC_ICER1 = 0xFFFFFFFFu;
    NVIC_ICPR0 = 0xFFFFFFFFu;
    NVIC_ICPR1 = 0xFFFFFFFFu;
    u32 crash_valid = (g_crash_record.magic == 0x48424C47u || g_crash_record.magic == 0x48435253u);
    u32 prior_boot = crash_valid ? g_crash_record.boot_id : 0u;
    u32 prior_user_faults = crash_valid ? g_crash_record.user_fault_recoveries_total : 0u;
    u32 prior_startup = crash_valid ? g_startup_noinit_boots : 0u;
    board_early_init();
    uart_puts_priv("\r\nHalaOS Educational Boot Console\r\n");
    uart_puts_priv("[RESET] Cortex-M3 reset vector entered\r\n");
    uart_puts_priv("[RESET] Initial MSP: 0x");
    uart_hex_priv(read_msp());
    uart_puts_priv("\r\n");
    if (g_crash_record.magic == 0x48435253u) {
        uart_puts_priv("[RESET] Previous boot ended unexpectedly PC=0x");
        uart_hex_priv(g_crash_record.fault_pc);
        uart_puts_priv(" LR=0x");
        uart_hex_priv(g_crash_record.fault_lr);
        uart_puts_priv("\r\n");
    }
    copy_zero();
    g_startup_data_ok = (g_startup_data_probe == 0x13579BDFu);
    g_startup_bss_ok = (g_startup_bss_probe == 0u);
    g_startup_noinit_boots = prior_startup + 1u;
    fill_msp_pattern();
    g_crash_record.magic = 0x48424C47u;
    g_crash_record.user_fault_recoveries_total = prior_user_faults;
    g_user_fault_recoveries = prior_user_faults;
    g_boot_event_count = 0;
    g_crash_record.boot_id = prior_boot + 1u;
    g_crash_record.last_stage = 1;
    boot_event(HALA_BOOT_RESET_ENTER);
    uart_puts_priv("[START] .data=");
    uart_puts_priv(g_startup_data_ok ? "OK" : "FAIL");
    uart_puts_priv(" .bss=");
    uart_puts_priv(g_startup_bss_ok ? "OK" : "FAIL");
    uart_puts_priv(" .noinit_boots=");
    uart_dec_priv(g_startup_noinit_boots);
    uart_puts_priv("\r\n");
    boot_event(HALA_BOOT_STARTUP_COMPLETE);
    uart_puts_priv("\r\nHalaOS Stage-0 0.5.0\r\nCPU: STM32F103C8 Cortex-M3\r\nFlash: 64 "
                   "KiB\r\nSRAM: 20 KiB\r\n");
    g_crash_record.last_stage = 2;
    boot_event(HALA_BOOT_STAGE0_ENTER);
    uart_puts_priv("[S0] Reading boot manifest\r\n");
#ifndef HALAOS_BOOT_CRC_GENERATION
    if (g_boot_manifest.magic != 0x48414C41u)
        stage0_fail("manifest magic mismatch");
    if (g_boot_manifest.version != 4u || g_boot_manifest.header_size != sizeof(BootManifest))
        stage0_fail("manifest version/size mismatch");
    if (manifest_crc() != g_boot_manifest.header_crc)
        stage0_fail("manifest header CRC mismatch");
    if (!flash_range_valid(g_boot_manifest.loader_addr, g_boot_manifest.loader_size) ||
        !flash_range_valid(g_boot_manifest.dtb_addr, g_boot_manifest.dtb_size) ||
        !flash_range_valid(g_boot_manifest.kernel_addr, g_boot_manifest.kernel_size))
        stage0_fail("image address/size invalid");
    if ((g_boot_manifest.entry_point & 1u) == 0u ||
        (g_boot_manifest.entry_point & ~1u) < g_boot_manifest.kernel_addr ||
        (g_boot_manifest.entry_point & ~1u) >=
            g_boot_manifest.kernel_addr + g_boot_manifest.kernel_size)
        stage0_fail("kernel entry invalid");
    if (crc32_bytes((const u8 *)(usize)g_boot_manifest.loader_addr, g_boot_manifest.loader_size) !=
        g_boot_manifest.loader_crc)
        stage0_fail("loader CRC mismatch");
    if (crc32_bytes((const u8 *)(usize)g_boot_manifest.dtb_addr, g_boot_manifest.dtb_size) !=
        g_boot_manifest.dtb_crc)
        stage0_fail("DTB image CRC mismatch");
    if (crc32_bytes((const u8 *)(usize)g_boot_manifest.kernel_addr, g_boot_manifest.kernel_size) !=
        g_boot_manifest.kernel_crc)
        stage0_fail("kernel CRC mismatch");
#endif
    uart_puts_priv("[S0] Manifest/header/image CRC: OK header=0x");
    uart_hex_priv(g_boot_manifest.header_crc);
    uart_puts_priv(" loader=0x");
    uart_hex_priv(g_boot_manifest.loader_crc);
    uart_puts_priv(" dtb=0x");
    uart_hex_priv(g_boot_manifest.dtb_crc);
    uart_puts_priv(" kernel=0x");
    uart_hex_priv(g_boot_manifest.kernel_crc);
    uart_puts_priv("\r\n");
    boot_event(HALA_BOOT_MANIFEST_VALID);
    uart_puts_priv("[S0] Jumping to Hala Loader entry=0x");
    uart_hex_priv(g_boot_manifest.loader_addr | 1u);
    uart_puts_priv("\r\n");
    hala_loader_entry(&g_boot_manifest);
    for (;;) {
    }
}

/**
 * @brief Xử lý mặc định cho exception hoặc IRQ chưa được cài đặt.
 * @details
 * Hàm thuộc implementation nội bộ của HalaOS. Xử lý mặc định cho exception hoặc IRQ chưa được cài
 * đặt. Thiết kế tránh cấp phát heap động, ưu tiên fixed pool và thao tác xác định để phù hợp giới
 * hạn tài nguyên của STM32F103C8. Hàm chạy trong exception/interrupt context; không được gọi trực
 * tiếp từ application.
 * @pre Các subsystem và dữ liệu được hàm sử dụng đã được khởi tạo theo đúng boot phase.
 * @post Trạng thái liên quan được cập nhật nhất quán hoặc mã lỗi được trả về cho caller.
 * @reentrant Không; hàm sử dụng trạng thái kernel hoặc thanh ghi dùng chung.
 * @synchronous Có; hàm hoàn tất trước khi trả về, trừ task/handler có vòng đời riêng.
 */
void Default_Handler(void) {
    for (;;) {
    }
}
