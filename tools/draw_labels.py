#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
import os
from PIL import Image, ImageDraw, ImageFont

# Danh sách hình ảnh và mô tả bằng tiếng Việt
SCREENSHOTS_DATA = {
    "01-linux-uart-connected.png": {
        "title": "Bước 01: Kết nối UART",
        "desc": "Thiết lập và kết nối thành công cổng serial UART trên máy host bằng picocom với cấu hình 115200 8N1."
    },
    "02-userspace-proof.png": {
        "title": "Bước 02: Xác thực User Space",
        "desc": "Xác định hệ thống đã sẵn sàng tương tác ở chế độ bảo vệ người dùng, cách ly an toàn với nhân."
    },
    "03-complete-boot.png": {
        "title": "Bước 03: Khởi động Hoàn tất",
        "desc": "Ghi nhận toàn bộ chuỗi khởi động từ bootloader, giải nén cấu hình Device Tree (DTB) đến khi chạy nhân."
    },
    "04-boot-review.png": {
        "title": "Bước 04: Truy vấn Boot Log",
        "desc": "Sử dụng các lệnh dmesg hoặc bootlog để duyệt lịch sử khởi động và trạng thái các driver phần cứng."
    },
    "05-vfs-dtb.png": {
        "title": "Bước 05: Cây Thiết bị & VFS",
        "desc": "Duyệt cây thiết bị phần cứng Device Tree (DTB) và truy cập các phân vùng ảo /dev, /proc, /tmp."
    },
    "06-process-memory.png": {
        "title": "Bước 06: Tiến trình & Bộ nhớ",
        "desc": "Truy vấn các bảng tiến trình (ps, top) và biểu đồ ngăn xếp (stack, mem) để quản lý tài nguyên tĩnh."
    },
    "07-hello-compiler.png": {
        "title": "Bước 07: Trình biên dịch Hala-C",
        "desc": "Viết và biên dịch mã nguồn C trực tiếp tại chỗ sang bytecode máy ảo (VM) trên bộ nhớ Flash."
    },
    "08-process-control.png": {
        "title": "Bước 08: Kiểm soát Tiến trình",
        "desc": "Vận hành các chức năng kiểm soát vòng đời ứng dụng máy ảo bao gồm run, stop, continue và kill."
    },
    "09-thread-app.png": {
        "title": "Bước 09: Ứng dụng Đa luồng",
        "desc": "Chạy đa luồng mẫu với stack riêng biệt và ghi nhận kết quả xuất ra UART xen kẽ nhau."
    },
    "10-ipc.png": {
        "title": "Bước 10: Đồng bộ & IPC",
        "desc": "Thử nghiệm truyền dữ liệu Producer/Consumer qua Message Queue và bảo vệ tài nguyên bằng Mutex."
    },
    "11-pipe-posix.png": {
        "title": "Bước 11: Pipe & POSIX API",
        "desc": "Chạy ứng dụng giáo dục sử dụng đường ống Pipe và giao tiếp file descriptor tiêu chuẩn POSIX."
    },
    "12-scheduler.png": {
        "title": "Bước 12: Đánh giá Bộ lập lịch",
        "desc": "Kiểm tra phản hồi của CPU dưới các giải thuật lập lịch: Round-Robin, Fair, Weighted, Deadline, FIFO."
    },
    "13-fault.png": {
        "title": "Bước 13: Bảo vệ Nhân (Fault)",
        "desc": "Khi xảy ra lỗi nghiêm trọng (HardFault) ở User Space, kernel ghi lại lỗi và tiếp tục hoạt động an toàn."
    },
    "14-persistence.png": {
        "title": "Bước 14: Lưu trữ ứng dụng",
        "desc": "Ứng dụng máy ảo lưu trên Flash slot A/B tồn tại nguyên vẹn sau khi thực hiện reset cứng/mềm hệ thống."
    },
    "15-final-report.png": {
        "title": "Bước 15: Báo cáo Nghiệm thu",
        "desc": "Lệnh acceptance-report sinh tự động báo cáo thống kê hoạt động của hệ điều hành ngay lúc chạy."
    },
    "16-session-closed.png": {
        "title": "Bước 16: Đóng Kết nối UART",
        "desc": "Thực hiện ngắt kết nối an toàn picocom và quay trở lại màn hình shell mặc định của máy host."
    }
}

FONT_PATH = "/System/Library/Fonts/Supplemental/Arial.ttf"
IMAGES_DIR = "docs/images/evidence-v2"

def wrap_text(text, font, max_width):
    """Tự động xuống dòng cho văn bản dựa trên kích thước font và chiều rộng tối đa."""
    words = text.split()
    lines = []
    current_line = []
    
    for word in words:
        test_line = ' '.join(current_line + [word])
        # Lấy kích thước của dòng test
        bbox = font.getbbox(test_line)
        width = bbox[2] - bbox[0]
        if width <= max_width:
            current_line.append(word)
        else:
            if current_line:
                lines.append(' '.join(current_line))
                current_line = [word]
            else:
                lines.append(word)
                current_line = []
                
    if current_line:
        lines.append(' '.join(current_line))
    return lines

def process_images():
    print("Bắt đầu chèn thông tin mô tả vào góc trên bên phải các hình ảnh...")
    
    # Kiểm tra font
    if not os.path.exists(FONT_PATH):
        raise FileNotFoundError(f"Không tìm thấy font tại {FONT_PATH}")
        
    title_font = ImageFont.truetype(FONT_PATH, 20)
    desc_font = ImageFont.truetype(FONT_PATH, 14)
    
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
        
        # Cấu hình kích thước và vị trí hộp thông tin ở góc phải trên
        box_width = 460
        max_text_width = box_width - 40 # Padding 20px mỗi bên
        
        # Xuống dòng văn bản mô tả để vừa với chiều rộng hộp
        wrapped_desc_lines = wrap_text(info["desc"], desc_font, max_text_width)
        
        # Tính toán chiều cao động cho hộp dựa trên số dòng mô tả
        # Tiêu đề (25px) + Khoảng cách (10px) + Các dòng mô tả (mỗi dòng 20px) + Padding dọc (30px)
        box_height = 25 + 10 + (len(wrapped_desc_lines) * 20) + 30
        
        # Tọa độ hộp góc phải trên
        x2 = width - 40
        x1 = x2 - box_width
        y1 = 40
        y2 = y1 + box_height
        
        # Vẽ nền hộp: Đen mờ RGBA(15, 15, 15, 220) để đảm bảo chữ đỏ luôn nổi bật dễ đọc
        draw.rectangle(
            [(x1, y1), (x2, y2)],
            fill=(15, 15, 15, 220),
            outline=(255, 0, 0, 255), # Viền đỏ
            width=3 # Độ dày viền 3px
        )
        
        # Viết Tiêu đề và Mô tả bằng chữ đỏ RGB(255, 0, 0)
        draw.text((x1 + 20, y1 + 15), info["title"], font=title_font, fill=(255, 0, 0, 255))
        
        for i, line in enumerate(wrapped_desc_lines):
            draw.text((x1 + 20, y1 + 50 + i * 20), line, font=desc_font, fill=(255, 0, 0, 255))
            
        # Trộn ảnh gốc với overlay
        final_img = Image.alpha_composite(img, overlay).convert("RGB")
        final_img.save(img_path, "PNG")
        print(f"Đã cập nhật: {img_path}")
        
    print("Hoàn tất chèn nội dung vào góc phải trên của toàn bộ hình ảnh!")

if __name__ == "__main__":
    process_images()
