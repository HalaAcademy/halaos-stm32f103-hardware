/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    halaos_dtb_driver.c
 * @brief   Parser compact DTB v2 và registry driver/device của HalaOS.
 * @details Module đọc trực tiếp các node/property dạng TLV từ DTB do dtsgen sinh ra.
 *          Shell, Loader và driver binding cùng sử dụng một API, vì vậy dữ liệu hiển thị
 *          không còn là chuỗi hard-code tách rời cấu hình board.
 */
#include "halaos/internal/halaos_internal.h"

#define DTB_HEADER_SIZE 24u
#define DTB_TYPE_STRING 1u
#define DTB_TYPE_U32 2u
#define DTB_TYPE_U32_PAIR 3u
#define DTB_TYPE_U32_TRIPLE 4u
#define DTB_TYPE_BOOL 5u

/** @brief Đọc số 16-bit little-endian không yêu cầu căn chỉnh. */
static u16 dtb_rd16(const u8 *p) { return (u16)p[0] | ((u16)p[1] << 8); }

/** @brief Kiểm tra một chuỗi NUL trong DTB có khớp chuỗi C hay không. */
static int dtb_name_eq(const u8 *p, u16 n, const char *s) {
    u32 i = 0;
    if (!p || !s || n == 0)
        return 0;
    while (i + 1u < n && s[i] && p[i] == (u8)s[i])
        i++;
    return i + 1u == n && s[i] == 0 && p[i] == 0;
}

/**
 * @brief Xác minh toàn bộ cấu trúc TLV của compact DTB v2.
 * @details Ngoài magic/version/CRC, hàm duyệt từng node/property và kiểm tra record size,
 *          độ dài tên, độ dài value và tổng số node/property. Điều này ngăn parser đọc
 *          ra ngoài blob khi DTB bị truncate hoặc cố tình làm sai length.
 */
int hala_dtb_validate_blob(void) {
#ifdef HALAOS_BAD_DTB
    return 0;
#endif
    const u8 *b = g_halaos_compact_dtb;
    u32 total = rd32le(b + 8), nodes = rd32le(b + 12), props = rd32le(b + 16), off = rd32le(b + 20);
    if (rd32le(b) != HALAOS_DTB_MAGIC || rd32le(b + 4) != HALAOS_DTB_VERSION)
        return 0;
    if (total != HALAOS_DTB_SIZE || total < DTB_HEADER_SIZE + 4u || off < DTB_HEADER_SIZE ||
        off >= total - 4u)
        return 0;
    if (crc32_bytes(b, total - 4u) != rd32le(b + total - 4u))
        return 0;
    u32 seen_nodes = 0, seen_props = 0, pos = off;
    while (pos < total - 4u) {
        if (pos + 4u > total - 4u)
            return 0;
        u8 path_len = b[pos], pc = b[pos + 1];
        u16 rec = dtb_rd16(b + pos + 2);
        if (path_len < 2u || rec < 4u + path_len || pos + rec > total - 4u)
            return 0;
        const u8 *path = b + pos + 4;
        if (path[path_len - 1u] != 0)
            return 0;
        u32 q = pos + 4u + path_len;
        for (u32 i = 0; i < pc; i++) {
            if (q + 4u > pos + rec)
                return 0;
            u8 name_len = b[q], type = b[q + 1];
            u16 value_len = dtb_rd16(b + q + 2);
            q += 4u;
            if (name_len < 2u || q + name_len + value_len > pos + rec)
                return 0;
            if (b[q + name_len - 1u] != 0 || type < DTB_TYPE_STRING || type > DTB_TYPE_BOOL)
                return 0;
            if (type == DTB_TYPE_STRING &&
                (value_len < 1u || b[q + name_len + value_len - 1u] != 0))
                return 0;
            if (type == DTB_TYPE_U32 && value_len != 4u)
                return 0;
            if (type == DTB_TYPE_U32_PAIR && value_len != 8u)
                return 0;
            if (type == DTB_TYPE_U32_TRIPLE && value_len != 12u)
                return 0;
            if (type == DTB_TYPE_BOOL && value_len != 1u)
                return 0;
            q += name_len + value_len;
            seen_props++;
        }
        if (q != pos + rec)
            return 0;
        pos += rec;
        seen_nodes++;
    }
    return pos == total - 4u && seen_nodes == nodes && seen_props == props;
}

/** @brief Trả về số node được khai báo trong DTB hợp lệ. */
u32 hala_dtb_node_count(void) {
    return hala_dtb_validate_blob() ? rd32le(g_halaos_compact_dtb + 12) : 0u;
}

/** @brief Tìm node theo chỉ số và trả về path nằm trực tiếp trong DTB. */
const char *hala_dtb_node_path(u32 index) {
    if (!hala_dtb_validate_blob())
        return NULL;
    const u8 *b = g_halaos_compact_dtb;
    u32 total = rd32le(b + 8), pos = rd32le(b + 20), n = 0;
    while (pos < total - 4u) {
        u16 rec = dtb_rd16(b + pos + 2);
        if (n++ == index)
            return (const char *)(b + pos + 4);
        pos += rec;
    }
    return NULL;
}

/**
 * @brief Tra cứu property theo path và tên property.
 * @param[in] path Đường dẫn node tuyệt đối.
 * @param[in] property Tên property.
 * @param[out] value Mô tả type, length và con trỏ value trong DTB.
 * @retval 1 Tìm thấy property.
 * @retval 0 Không tìm thấy hoặc DTB không hợp lệ.
 */
int hala_dtb_get(const char *path, const char *property, HalaDtbValue *value) {
    if (!path || !property || !value || !hala_dtb_validate_blob())
        return 0;
    const u8 *b = g_halaos_compact_dtb;
    u32 total = rd32le(b + 8), pos = rd32le(b + 20);
    while (pos < total - 4u) {
        u8 path_len = b[pos], pc = b[pos + 1];
        u16 rec = dtb_rd16(b + pos + 2);
        const u8 *np = b + pos + 4;
        u32 q = pos + 4u + path_len;
        if (dtb_name_eq(np, path_len, path))
            for (u32 i = 0; i < pc; i++) {
                u8 nl = b[q], type = b[q + 1];
                u16 vl = dtb_rd16(b + q + 2);
                const u8 *name = b + q + 4;
                const u8 *data = name + nl;
                if (dtb_name_eq(name, nl, property)) {
                    value->type = type;
                    value->length = vl;
                    value->data = data;
                    return 1;
                }
                q += 4u + nl + vl;
            }
        pos += rec;
    }
    return 0;
}

/** @brief Đọc property string và trả về con trỏ NUL-terminated nằm trong DTB. */
const char *hala_dtb_get_string(const char *path, const char *property) {
    HalaDtbValue v;
    if (!hala_dtb_get(path, property, &v) || v.type != DTB_TYPE_STRING)
        return NULL;
    return (const char *)v.data;
}

/** @brief Đọc property U32. */
int hala_dtb_get_u32(const char *path, const char *property, u32 *out) {
    HalaDtbValue v;
    if (!out || !hala_dtb_get(path, property, &v) || v.type != DTB_TYPE_U32)
        return 0;
    *out = rd32le(v.data);
    return 1;
}

/** @brief Đọc property gồm hai U32. */
int hala_dtb_get_pair(const char *path, const char *property, u32 *a, u32 *b) {
    HalaDtbValue v;
    if (!a || !b || !hala_dtb_get(path, property, &v) || v.type != DTB_TYPE_U32_PAIR)
        return 0;
    *a = rd32le(v.data);
    *b = rd32le(v.data + 4);
    return 1;
}

/** @brief Danh sách driver/device tối giản; bốn device được bind từ compatible trong DTB. */
static HalaDeviceInfo g_devices[] = {{"rcc", "/platform/rcc", "platform", 1, 0},
                                     {"systick", "/kernel/systick", "platform", 1, 0},
                                     {"flash", "/platform/flash", "platform", 1, 0},
                                     {"gpioa", "/soc/gpioa", "st,stm32f1-gpio", 0, 1},
                                     {"gpioc", "/soc/gpioc", "st,stm32f1-gpio", 0, 1},
                                     {"usart1", "/soc/usart1", "st,stm32f1-usart", 0, 1},
                                     {"status-led", "/leds/status", "hala,gpio-output", 0, 1}};

/**
 * @brief Bind toàn bộ driver bằng compatible đọc từ DTB.
 * @return Số device đã bind thành công, gồm platform device và DTB device.
 */
u32 hala_driver_bind_all(void) {
    u32 count = 0;
    for (u32 i = 0; i < ARRAY_LEN(g_devices); i++) {
        HalaDeviceInfo *d = &g_devices[i];
        if (!d->from_dtb)
            d->bound = 1;
        else {
            const char *c = hala_dtb_get_string(d->path, "compatible");
            d->bound = (u8)(c && str_eq(c, d->compatible));
        }
        if (d->bound)
            count++;
    }
    g_driver_count = count;
    return count;
}

u32 hala_device_count(void) { return ARRAY_LEN(g_devices); }
const HalaDeviceInfo *hala_device_at(u32 index) {
    return index < ARRAY_LEN(g_devices) ? &g_devices[index] : NULL;
}

/** @brief Xuất danh sách node qua boot/kernel console ở privileged context. */
void hala_dtb_console_list(void) {
    u32 n = hala_dtb_node_count();
    for (u32 i = 0; i < n; i++) {
        const char *p = hala_dtb_node_path(i);
        if (p) {
            if (i)
                uart_putc_priv(' ');
            uart_puts_priv(p);
        }
    }
    uart_puts_priv("\r\n");
}

/** @brief Xuất một property DTB ở privileged context. */
int hala_dtb_console_get(const char *path, const char *property) {
    HalaDtbValue v;
    if (!hala_dtb_get(path, property, &v))
        return -2;
    uart_puts_priv(property);
    uart_putc_priv('=');
    if (v.type == DTB_TYPE_STRING)
        uart_puts_priv((const char *)v.data);
    else if (v.type == DTB_TYPE_U32)
        uart_dec_priv(rd32le(v.data));
    else if (v.type == DTB_TYPE_U32_PAIR) {
        uart_puts_priv("<0x");
        uart_hex_priv(rd32le(v.data));
        uart_puts_priv(" 0x");
        uart_hex_priv(rd32le(v.data + 4));
        uart_putc_priv('>');
    } else if (v.type == DTB_TYPE_U32_TRIPLE) {
        uart_putc_priv('<');
        uart_dec_priv(rd32le(v.data));
        uart_putc_priv(' ');
        uart_dec_priv(rd32le(v.data + 4));
        uart_putc_priv(' ');
        uart_dec_priv(rd32le(v.data + 8));
        uart_putc_priv('>');
    } else if (v.type == DTB_TYPE_BOOL)
        uart_puts_priv(v.data[0] ? "true" : "false");
    else
        return -3;
    uart_puts_priv("\r\n");
    return 0;
}

/** @brief Xuất registry driver/device ở privileged context. */
void hala_driver_console_list(void) {
    for (u32 i = 0; i < ARRAY_LEN(g_devices); i++) {
        HalaDeviceInfo *d = &g_devices[i];
        uart_puts_priv(d->name);
        uart_puts_priv(" path=");
        uart_puts_priv(d->path);
        uart_puts_priv(" source=");
        uart_puts_priv(d->from_dtb ? "dtb" : "platform");
        uart_puts_priv(" compatible=");
        uart_puts_priv(d->compatible);
        uart_puts_priv(" state=");
        uart_puts_priv(d->bound ? "BOUND" : "UNBOUND");
        uart_puts_priv("\r\n");
    }
}
