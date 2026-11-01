// main.cpp - Điểm vào chương trình. Toàn bộ logic nằm trong lớp Game.
//
// Tham số dòng lệnh (tùy chọn):
//   --data-dir <thư_mục>          dùng thư mục dữ liệu CSV khác (mặc định: tự tìm data/ của dự án)
//   --smoke-test [tiền_tố_ảnh]    tự kiểm thử không cần người chơi (dùng thư mục dữ liệu TẠM, không đụng dữ liệu thật)

#include "Game.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <imm.h>

// Vô hiệu hóa Windows IME trên toàn tiến trình để phím 1,2,3,W,A,S,D,B,S không bị Telex/VNI bắt giữ
static void disableWindowsImeProcessWide() {
    ImmDisableIME(static_cast<DWORD>(-1));
}
#endif

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
#ifdef _WIN32
    disableWindowsImeProcessWide();
#endif
    try {
        std::string dataDir;
        bool smokeTest = false;
        std::string screenshotPrefix;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--data-dir" && i + 1 < argc) {
                dataDir = argv[++i];
            } else if (arg == "--smoke-test") {
                smokeTest = true;
                if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) screenshotPrefix = argv[++i];
            }
        }

        if (smokeTest) {
            // Kiểm thử luôn chạy trên thư mục tạm để không làm hỏng tài khoản thật của bạn.
            const auto stamp = std::to_string(std::filesystem::file_time_type::clock::now().time_since_epoch().count());
            dataDir = (std::filesystem::temp_directory_path() / ("slimehunter_smoke_" + stamp)).string();
            Game game(dataDir);
            const int result = game.runSmokeTest(screenshotPrefix);
            std::error_code ec;
            std::filesystem::remove_all(dataDir, ec);
            return result;
        }

        Game game(dataDir);
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Loi nghiem trong: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
