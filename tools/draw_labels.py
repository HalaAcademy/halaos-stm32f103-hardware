#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
import os
from PIL import Image, ImageDraw, ImageFont

# Danh sách hình ảnh và mô tả bằng tiếng Việt
SCREENSHOTS_DATA = {
    "01-linux-uart-connected.png": {
        "title": "HalaOS - Bước 01: Kết nối UART",
        "desc": "Thiết lập và kết nối thành công cổng serial UART trên máy host bằng picocom với cấu hình 115200 8N1."
    },
    "02-userspace-proof.png": {
        "title": "HalaOS - Bước 02: Xác thực Chế độ User Space",
        "desc": "Xác định hệ thống đã sẵn sàng tương tác ở chế độ bảo vệ người dùng, cách ly an toàn với nhân."
    },
    "03-complete-boot.png": {
        "title": "HalaOS - Bước 03: Chu kỳ Khởi động Hoàn tất",
        "desc": "Ghi nhận toàn bộ chuỗi khởi động từ bootloader, giải nén cấu hình Device Tree (DTB) đến khi chạy nhân."
    },
    "04-boot-review.png": {
        "title": "HalaOS - Bước 04: Truy vấn Boot Log",
        "desc": "Sử dụng các lệnh dmesg hoặc bootlog để duyệt lịch sử khởi động và trạng thái các driver phần cứng."
    },
    "05-vfs-dtb.png": {
        "title": "HalaOS - Bước 05: Cây Thiết bị & VFS",
        "desc": "Duyệt cây thiết bị phần cứng Device Tree (DTB) và truy cập các phân vùng ảo /dev, /proc, /tmp."
    },
    "06-process-memory.png": {
        "title": "HalaOS - Bước 06: Tiến trình & Bộ nhớ RAM/Stack",
        "desc": "Truy vấn các bảng tiến trình (ps, top) và biểu đồ ngăn xếp (stack, mem) để quản lý tài nguyên tĩnh."
    },
    "07-hello-compiler.png": {
        "title": "HalaOS - Bước 07: Trình biên dịch tương tác Hala-C",
        "desc": "Viết và biên dịch mã nguồn C trực tiếp tại chỗ sang bytecode máy ảo (VM) trên bộ nhớ Flash."
    },
    "08-process-control.png": {
        "title": "HalaOS - Bước 08: Kiểm soát Tiến trình",
        "desc": "Vận hành các chức năng kiểm soát vòng đời ứng dụng máy ảo bao gồm run, stop, continue và kill."
    },
    "09-thread-app.png": {
        "title": "HalaOS - Bước 09: Ứng dụng Đa luồng song song",
        "desc": "Chạy đa luồng mẫu với stack riêng biệt và ghi nhận kết quả xuất ra UART xen kẽ nhau."
    },
    "10-ipc.png": {
        "title": "HalaOS - Bước 10: Đồng bộ & Giao tiếp IPC",
        "desc": "Thử nghiệm truyền dữ liệu Producer/Consumer qua Message Queue và bảo vệ tài nguyên bằng Mutex."
    },
    "11-pipe-posix.png": {
        "title": "HalaOS - Bước 11: Mô phỏng Pipe & POSIX API",
        "desc": "Chạy ứng dụng giáo dục sử dụng đường ống Pipe và giao tiếp file descriptor tiêu chuẩn POSIX."
    },
    "12-scheduler.png": {
        "title": "HalaOS - Bước 12: Đánh giá Bộ lập lịch",
        "desc": "Kiểm tra phản hồi của CPU dưới các giải thuật lập lịch: Round-Robin, Fair, Weighted, Deadline, FIFO."
    },
    "13-fault.png": {
        "title": "HalaOS - Bước 13: Chứa lỗi và Bảo vệ Nhân",
        "desc": "Khi xảy ra lỗi nghiêm trọng (HardFault) ở User Space, kernel ghi lại lỗi và tiếp tục hoạt động an toàn."
    },
    "14-persistence.png": {
        "title": "HalaOS - Bước 14: Lưu trữ Ứng dụng qua Reset",
        "desc": "Ứng dụng máy ảo lưu trên Flash slot A/B tồn tại nguyên vẹn sau khi thực hiện reset cứng/mềm hệ thống."
    },
    "15-final-report.png": {
        "title": "HalaOS - Bước 15: Báo cáo Nghiệm thu",
        "desc": "Lệnh acceptance-report sinh tự động báo cáo thống kê hoạt động của hệ điều hành ngay lúc chạy."
    },
    "16-session-closed.png": {
        "title": "HalaOS - Bước 16: Đóng Kết nối và Kết thúc",
        "desc": "Thực hiện ngắt kết nối an toàn picocom và quay trở lại màn hình shell mặc định của máy host."
    }
}

FONT_PATH = "/System/Library/Fonts/Supplemental/Arial.ttf"
IMAGES_DIR = "docs/images/evidence"

def process_images():
    print("Bắt đầu chèn thông tin mô tả vào các hình ảnh minh chứng...")
    
    # Kiểm tra font
    if not os.path.exists(FONT_PATH):
        raise FileNotFoundError(f"Không tìm thấy font tại {FONT_PATH}")
        
    title_font = ImageFont.truetype(FONT_PATH, 28)
    desc_font = ImageFont.truetype(FONT_PATH, 18)
    
    for filename, info in SCREENSHOTS_DATA.items():
        img_path = os.path.join(IMAGES_DIR, filename)
        if not os.path.exists(img_path):
            print(f"Bỏ qua {filename} (Không tồn tại file)")
            continue
            
        print(f"Đang xử lý: {filename}...")
        img = Image.open(img_path).convert("RGBA")
        width, height = img.size
        
        # Tạo ảnh overlay cho banner có độ trong suốt
        overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)
        
        # Vẽ banner nền tối màu ở góc dưới (cao 110px)
        banner_h = 110
        banner_y = height - banner_h
        # Màu nền RGBA: (24, 28, 36, 235) - Xám xanh tối, trong suốt nhẹ
        draw.rectangle(
            [(0, banner_y), (width, height)],
            fill=(24, 28, 36, 235)
        )
        
        # Vẽ thanh accent dọc màu xanh Cyan ở bên trái
        draw.rectangle(
            [(30, banner_y + 20), (40, height - 20)],
            fill=(0, 168, 204, 255)
        )
        
        # Vẽ text Tiêu đề và Mô tả
        draw.text((60, banner_y + 15), info["title"], font=title_font, fill=(255, 255, 255, 255))
        draw.text((60, banner_y + 58), info["desc"], font=desc_font, fill=(200, 200, 200, 255))
        
        # Trộn ảnh gốc với overlay
        final_img = Image.alpha_composite(img, overlay).convert("RGB")
        final_img.save(img_path, "PNG")
        print(f"Đã cập nhật: {img_path}")
        
    print("Hoàn tất chèn nội dung vào toàn bộ hình ảnh!")

if __name__ == "__main__":
    process_images()
