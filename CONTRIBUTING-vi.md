# Hướng dẫn đóng góp cho NexKey

Cảm ơn bạn đã quan tâm và muốn đóng góp cho dự án **NexKey**! Dù bạn tham gia báo cáo lỗi, đề xuất tính năng mới, cải thiện tài liệu hay trực tiếp gửi mã nguồn (Pull Request), sự đóng góp của bạn đều giúp trải nghiệm gõ tiếng Việt trên Linux trở nên nhanh hơn, mượt mà hơn và ổn định hơn.

Vui lòng dành chút thời gian đọc qua tài liệu hướng dẫn này trước khi bắt đầu.

---

## Mục lục

- [Quy tắc ứng xử (Code of Conduct)](#quy-tắc-ứng-xử-code-of-conduct)
- [Tôi có thể đóng góp bằng cách nào?](#tôi-có-thể-đóng-góp-bằng-cách-nào)
  - [Báo cáo lỗi (Bug Report)](#báo-cáo-lỗi-bug-report)
  - [Đề xuất tính năng mới](#đề-xuất-tính-năng-mới)
  - [Cải thiện tài liệu](#cải-thiện-tài-liệu)
  - [Đóng góp mã nguồn (Pull Request)](#đóng-góp-mã-nguồn-pull-request)
- [Thiết lập môi trường phát triển](#thiết-lập-môi-trường-phát-triển)
  - [Yêu cầu hệ thống & Thư viện](#yêu-cầu-hệ-thống--thư-viện)
  - [Biên dịch từ mã nguồn](#biên-dịch-từ-mã-nguồn)
  - [Chạy bộ kiểm thử (Unit Tests)](#chạy-bộ-kiểm-thử-unit-tests)
- [Kiến trúc & Tiêu chuẩn mã nguồn](#kiến-trúc--tiêu-chuẩn-mã-nguồn)
  - [Kiến trúc phân tầng của NexKey](#kiến-trúc-phân-tầng-của-nexkey)
  - [Chuẩn lập trình C++23](#chuẩn-lập-trình-c23)
  - [Quy chuẩn định dạng code (Formatting)](#quy-chuẩn-định-dạng-code-formatting)
  - [Cảnh báo biên dịch (Compiler Warnings)](#cảnh-báo-biên-dịch-compiler-warnings)
- [Quy trình gửi Pull Request](#quy-trình-gửi-pull-request)
  - [Quy tắc đặt tên nhánh](#quy-tắc-đặt-tên-nhánh)
  - [Quy chuẩn Commit Message](#quy-chuẩn-commit-message)
  - [Checklist trước khi tạo PR](#checklist-trước-khi-tạo-pr)
- [Giấy phép bản quyền](#giấy-phép-bản-quyền)

---

## Quy tắc ứng xử (Code of Conduct)

Dự án này và mọi thành viên tham gia đều tuân thủ [Quy tắc ứng xử](CODE_OF_CONDUCT.vi.md) (Bản tiếng Anh: [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)). Bằng việc tham gia, bạn được kỳ vọng sẽ duy trì tinh thần tôn trọng, cởi mở và xây dựng. Vui lòng báo cáo bất kỳ hành vi không phù hợp nào cho đội ngũ quản trị dự án.

---

## Tôi có thể đóng góp bằng cách nào?

### Báo cáo lỗi (Bug Report)

Trước khi mở một issue mới, vui lòng kiểm tra danh sách [GitHub Issues](https://github.com/Hyggshi-OS-Research-Technology/Nexkey/issues) xem vấn đề đó đã được báo cáo hay xử lý chưa.

Khi tạo báo cáo lỗi, vui lòng cung cấp đầy đủ:
1. **Mô tả rõ ràng**: Hiện tượng thực tế gặp phải so với kỳ vọng của bạn.
2. **Các bước tái hiện (Steps to Reproduce)**: Chuỗi phím bấm cụ thể, kiểu gõ (Telex, VNI, VIQR), bảng mã xuất, và phần mềm xảy ra lỗi.
3. **Môi trường hệ thống**:
   - Bản phân phối Linux (ví dụ: Ubuntu 24.04, Arch Linux, Fedora 40).
   - Display Server: Wayland hay X11.
   - Bộ khung gõ: Fcitx5 hay IBus (kèm phiên bản).
   - Ứng dụng gặp lỗi: (ví dụ: Chrome, Firefox, VS Code, GNOME Text Editor, Terminal).
4. **Nhật ký lỗi (Logs)**: Trích xuất log chạy từ terminal nếu có (`fcitx5 -d --replace` hoặc `journalctl --user -u fcitx5`).

### Đề xuất tính năng mới

Chúng tôi luôn chào đón mọi ý tưởng cải tiến trải nghiệm gõ phím, tối ưu hiệu năng hoặc thêm bảng mã/keymap mới!

Khi đề xuất:
- Trình bày rõ ràng **lợi ích** và bài toán thực tế mà tính năng này giải quyết.
- Đảm bảo đề xuất phù hợp với triết lý cốt lõi của NexKey: **hiệu năng cực cao**, **gõ trực tiếp không gạch chân (non-preedit)**, và **lõi engine hoàn toàn độc lập, nhẹ nhàng**.

### Cải thiện tài liệu

Tài liệu là một phần vô cùng quan trọng:
- Sửa lỗi chính tả, câu từ hoặc làm rõ các hướng dẫn kỹ thuật.
- Bổ sung tài liệu cấu hình ([CONFIGURATION.md](docs/CONFIGURATION.md)) hoặc kiến trúc ([ARCHITECTURE.md](docs/ARCHITECTURE.md)).
- Hoàn thiện và cập nhật các bản dịch song ngữ Anh - Việt.

### Đóng góp mã nguồn (Pull Request)

Nếu bạn muốn đóng góp code:
1. Fork repository về tài khoản của bạn và tạo nhánh mới từ `main`.
2. Tuân thủ ranh giới kiến trúc và chuẩn code của dự án.
3. Đảm bảo toàn bộ unit test cũ và mới đều vượt qua thành công.
4. Gửi Pull Request để được review và hợp nhất.

---

## Thiết lập môi trường phát triển

### Yêu cầu hệ thống & Thư viện

| Thành phần | Yêu cầu tối thiểu | Ghi chú |
| --- | --- | --- |
| **Trình biên dịch** | GCC 12+ hoặc Clang 15+ | Hỗ trợ đầy đủ C++23 |
| **Hệ thống build** | CMake 3.21+ | Khuyến nghị dùng cùng Ninja |
| **GUI (Tùy chọn)** | Qt 6.2+ | `QtCore`, `QtGui`, `QtWidgets`, `QtDBus` |
| **Fcitx5 Addon (Tùy chọn)** | Fcitx5 5.0+ dev packages | `Fcitx5Core`, `Fcitx5Config`, `Fcitx5Utils` |
| **IBus Engine (Tùy chọn)** | IBus 1.5+ dev packages | `ibus-1.0` |

Trên Debian / Ubuntu, bạn có thể cài đặt nhanh qua script:
```bash
./scripts/install-build-deps.sh
```

Hoặc cài đặt thủ công bằng `apt`:
```bash
sudo apt install build-essential cmake ninja-build \
    qt6-base-dev libqt6dbus6 \
    extra-cmake-modules fcitx5-modules-dev libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev \
    libibus-1.0-dev
```

### Biên dịch từ mã nguồn

```bash
# Clone repository
git clone https://github.com/Hyggshi-OS-Research-Technology/Nexkey.git
cd Nexkey

# Cấu hình dự án với CMake (bật cảnh báo nghiêm ngặt cho môi trường dev)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DNEXKEY_WARNINGS_AS_ERRORS=ON

# Biên dịch đa luồng
cmake --build build --parallel
```

### Chạy bộ kiểm thử (Unit Tests)

NexKey tích hợp sẵn bộ unit test toàn diện kiểm tra quy tắc đặt dấu, bóc tách âm tiết, phương thức gõ (Telex, VNI, VIQR), mã hóa Unicode và serialization cấu hình:

```bash
ctest --test-dir build --output-on-failure
```

Hoặc thực thi trực tiếp file test:
```bash
./build/tests/nexkey-tests
```

---

## Kiến trúc & Tiêu chuẩn mã nguồn

### Kiến trúc phân tầng của NexKey

Cấu trúc dự án được phân chia ranh giới rõ ràng:

```
Nexkey/
├── core/                 # Engine C++23 xử lý tiếng Việt, bảng mã, settings (HOÀN TOÀN KHÔNG phụ thuộc thư viện ngoài)
├── cli/                  # Tiện ích dòng lệnh (`nexkey`) phục vụ cấu hình, kiểm tra và chuyển đổi bảng mã
├── gui/                  # Giao diện Qt6: khay hệ thống, bảng cài đặt và bộ chọn emoji thông minh
├── integrations/
│   ├── fcitx5/           # Module tích hợp bộ gõ cho Fcitx5
│   └── ibus/             # Engine tích hợp bộ gõ cho IBus
├── tests/                # Bộ kiểm thử đơn vị độc lập
├── packaging/            # Cấu hình đóng gói Debian (.deb), AppImage, desktop icon
├── scripts/              # Các script hỗ trợ build, test và phát hành
└── docs/                 # Tài liệu hướng dẫn chi tiết về cấu hình, kiến trúc và tích hợp
```

**Nguyên tắc kiến trúc bắt buộc:**
- **Thư mục `core/` TUYỆT ĐỐI KHÔNG được phụ thuộc vào Qt, Fcitx5, IBus, X11 hay Wayland.** Lõi này phải luôn là một thư viện C++23 thuần túy tiêu chuẩn.
- Mọi logic ngôn ngữ về phân tích âm tiết, bỏ dấu, chuyển đổi bảng mã và chính tả tiếng Việt phải nằm hoàn toàn trong `core/`.
- Các tầng giao diện (`gui/`, `integrations/fcitx5/`, `integrations/ibus/`) chỉ đóng vai trò đón nhận sự kiện gõ phím từ hệ điều hành và chuyển tiếp cho `Engine` xử lý.

### Chuẩn lập trình C++23

- Sử dụng các tính năng C++ hiện đại: `std::string_view`, `std::span`, `constexpr`, structured bindings, và `<ranges>`.
- Đánh dấu `const` cho các hàm thành viên không thay đổi trạng thái đối tượng.
- Sử dụng thuộc tính `[[nodiscard]]` cho các hàm trả về trạng thái hoặc dữ liệu tính toán để tránh bỏ sót lỗi.
- Luôn kiểm tra giá trị trả về của các hàm `[[nodiscard]]`.
- Tránh sử dụng con trỏ thô (raw pointers) để quản lý tài nguyên; sử dụng con trỏ thông minh (`std::unique_ptr`, `std::shared_ptr`) hoặc kỹ thuật RAII.

### Quy chuẩn định dạng code (Formatting)

Dự án sử dụng tệp cấu hình `.clang-format` dựa trên chuẩn LLVM với độ thụt lề 4 dấu cách (4 spaces).

Trước khi commit mã nguồn, hãy chạy clang-format để tự động định dạng:
```bash
clang-format -i core/src/* core/include/nexkey/* cli/* gui/* integrations/fcitx5/* integrations/ibus/* tests/*
```

### Cảnh báo biên dịch (Compiler Warnings)

Hệ thống CI của NexKey cấu hình kiểm tra cảnh báo biên dịch ở mức nghiêm ngặt:
`-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-qual -Wconversion -Wsign-conversion -Wunused -Werror`.

Hãy chắc chắn rằng code của bạn biên dịch hoàn hảo, không có bất kỳ warning nào khi bật `-DNEXKEY_WARNINGS_AS_ERRORS=ON`.

---

## Quy trình gửi Pull Request

1. **Tập trung vào một mục tiêu**: Mỗi PR chỉ nên giải quyết một vấn đề cụ thể (fix một bug, refactor một module hoặc thêm một tính năng).
2. **Kèm theo Unit Test**: Nếu bạn sửa một lỗi về đặt dấu/âm tiết hoặc thêm tính năng mới, hãy bổ sung test tương ứng trong thư mục `tests/`.
3. **Kiểm tra kỹ tại local**:
   ```bash
   cmake --build build --parallel && ctest --test-dir build --output-on-failure
   ```
4. **Quy ước đặt tên nhánh**:
   - `feat/ten-tinh-nang`
   - `fix/ten-loi`
   - `docs/cap-nhat-tai-lieu`
5. **Quy ước Commit Message**:
   - `feat: thêm preset bàn phím kiểu Microsoft`
   - `fix: sửa lỗi lặp chữ trên thanh địa chỉ trình duyệt web`
   - `docs: cập nhật tài liệu cấu hình app rules`
   - `test: bổ sung test kiểm tra đặt dấu nguyên âm đôi`
6. **Mở Pull Request**: Mô tả rõ ràng nội dung thay đổi, lý do thực hiện và đính kèm issue liên quan (ví dụ: `Closes #12`).

---

## Giấy phép bản quyền

Khi đóng góp mã nguồn cho dự án NexKey, bạn đồng ý rằng các đóng góp của bạn sẽ được phát hành theo giấy phép mã nguồn mở [MIT License](LICENSE).
