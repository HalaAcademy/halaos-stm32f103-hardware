# CHUẨN COMMENT TIẾNG VIỆT CHI TIẾT CHO HALAOS — V2

## 1. Vai trò của mẫu Dio.c và Dio.h

`Dio.c` và `Dio.h` là ví dụ tham khảo về:

- File header.
- Phân vùng nội dung.
- Doxygen cho type, macro và function.
- Comment tiếng Việt chi tiết.
- Mô tả từng bước xử lý trong `.c`.

Hai file này không phải requirement tạo DIO Driver cho HalaOS và không được thêm vào target build.

## 2. Quy tắc cho file .h

File `.h` phải mô tả đầy đủ public contract:

- Module làm gì.
- Module không làm gì.
- Kiểu dữ liệu.
- Macro.
- Enum.
- Struct và từng field.
- Public API.
- Tham số input/output.
- Giá trị trả về.
- Điều kiện trước.
- Điều kiện sau.
- Side effect.
- Reentrancy.
- Thread/interrupt context.
- Error behavior.
- Requirement ID.

Ví dụ:

```c
/**
 * @brief Đưa task hiện hành vào trạng thái ngủ.
 * @details Hàm chuyển task từ RUNNING sang SLEEPING, tính thời điểm đánh thức
 *          theo kernel tick và chèn task vào delta sleep queue.
 *
 * @param[in] ticks Số tick cần ngủ. Giá trị 0 được xử lý như yield.
 *
 * @retval HOS_OK       Task đã được đưa vào sleep queue.
 * @retval HOS_E_STATE  Hàm được gọi khi scheduler chưa hoạt động.
 * @retval HOS_E_ACCESS Hàm được gọi từ context không cho phép.
 *
 * @pre Scheduler đã được khởi tạo.
 * @post Task hiện hành không còn ở ready queue cho đến khi hết thời gian ngủ.
 *
 * @note Hàm chỉ được gọi từ Thread mode.
 * @warning Không được gọi khi đang giữ spinlock của scheduler.
 * @trace HOS-KERNEL-SWS-042
 */
Hos_StatusType HalaTask_Sleep(uint32 ticks);
```

## 3. Quy tắc cho file .c

File `.c` phải giải thích implementation theo trình tự học được:

- Biến dùng để làm gì.
- Vì sao cần kiểm tra.
- Các bước thuật toán.
- Vì sao lock/disable interrupt.
- Register hoặc trạng thái nào thay đổi.
- Điều kiện rollback.
- Vì sao chọn nhánh xử lý.
- Giới hạn và workaround.

Có thể dùng:

```c
/* Bước 1: Kiểm tra scheduler đã hoạt động để tránh truy cập ready queue
 * trước khi các danh sách nội bộ được khởi tạo. */
```

Không được dùng comment sai hoặc chỉ để làm file dài.

## 4. Comment file header

```c
/**
 * @file    hala_scheduler.c
 * @brief   Hiện thực bộ lập lịch của HalaOS.
 * @details File chứa ready queue, sleep queue, lựa chọn task tiếp theo và
 *          runtime accounting cho các policy được bật trong build profile.
 *
 * @version 1.0.0
 * @author  HALA Academy
 *
 * @note    Đây là module nội bộ của kernel.
 * @see     hala_scheduler.h
 */
```

## 5. Phân vùng file

```c
/* ==========================================================================
 *                                 INCLUDES
 * ========================================================================== */

/* ==========================================================================
 *                            PRIVATE DEFINITIONS
 * ========================================================================== */

/* ==========================================================================
 *                             PRIVATE TYPES
 * ========================================================================== */

/* ==========================================================================
 *                         PRIVATE DATA DECLARATIONS
 * ========================================================================== */

/* ==========================================================================
 *                        PRIVATE FUNCTION PROTOTYPES
 * ========================================================================== */

/* ==========================================================================
 *                          PUBLIC FUNCTIONS
 * ========================================================================== */

/* ==========================================================================
 *                          PRIVATE FUNCTIONS
 * ========================================================================== */
```

## 6. Quy tắc đồng bộ giữa .h và .c

- `.h` mô tả contract.
- `.c` có thể lặp lại `@brief` ngắn và bổ sung `@details` theo implementation.
- Không copy nguyên khối dài rồi để hai nơi lệch nhau.
- Với mục tiêu đào tạo, function definition trong `.c` vẫn được comment chi tiết.
- Khi behavior thay đổi, phải cập nhật cả contract và implementation explanation.

## 7. Kiểm tra tự động

- Doxygen warning bằng 0.
- `@param` phải khớp tên.
- Không thiếu tài liệu public API.
- Mỗi file `.c/.h` phải có file header.
- Comment UTF-8 tiếng Việt có dấu.
- Requirement ID hợp lệ.
- Không claim vượt evidence.
