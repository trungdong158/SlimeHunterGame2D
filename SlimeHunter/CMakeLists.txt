cmake_minimum_required(VERSION 3.16)
project(SlimeHunter VERSION 0.1.0 LANGUAGES CXX)

# ---- Chuẩn ngôn ngữ ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Kieu build" FORCE)
endif()

message(STATUS "Trinh bien dich: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER})")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    message(FATAL_ERROR "Can GCC 9 tro len (de dung <filesystem> cua C++17). Ban dang dung GCC ${CMAKE_CXX_COMPILER_VERSION}.")
endif()

# ============================================================================
#  TÌM SFML 3.1
#  Thứ tự ưu tiên:
#   1) Thư mục "SFML-3.1.0" nằm CẠNH hoặc BÊN TRONG dự án (ví dụ cấu trúc workspace:
#          game/SFML-3.1.0   và   game/SlimeHunter ), gồm 2 loại:
#          - bản DỰNG SẴN (có lib/cmake/SFML/SFMLConfig.cmake): dùng find_package
#          - bản MÃ NGUỒN (có CMakeLists.txt + src/SFML)      : biên dịch luôn cùng dự án
#   2) SFML đã cài trong hệ thống / -DCMAKE_PREFIX_PATH=... / -DSFML_DIR=...
#  Tắt cơ chế (1): -DSLIMEHUNTER_USE_LOCAL_SFML=OFF
#  Lưu ý: đổi SFML thì xóa thư mục build rồi cấu hình lại (CMake nhớ đường dẫn cũ trong cache).
# ============================================================================
option(SLIMEHUNTER_USE_LOCAL_SFML "Uu tien thu muc SFML-3.1.0 nam canh/trong du an" ON)

set(_sfml_roots "")
foreach(_dir
        "${CMAKE_CURRENT_SOURCE_DIR}/../SFML-3.1.0"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../SFML-3.1.0"
        "${CMAKE_CURRENT_SOURCE_DIR}/SFML-3.1.0"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/SFML-3.1.0"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/SFML-3.1.0"
        "${CMAKE_CURRENT_SOURCE_DIR}/../SFML"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../SFML")
    get_filename_component(_dir "${_dir}" ABSOLUTE)
    list(APPEND _sfml_roots "${_dir}" "${_dir}/SFML-3.1.0")   # thêm trường hợp giải nén bị lồng 2 lớp
endforeach()

set(_sfml_local_root "")
set(_sfml_local_kind "")
if(SLIMEHUNTER_USE_LOCAL_SFML)
    foreach(_root IN LISTS _sfml_roots)
        if(EXISTS "${_root}/lib/cmake/SFML/SFMLConfig.cmake")
            set(_sfml_local_root "${_root}")
            set(_sfml_local_kind "prebuilt")
            break()
        elseif(EXISTS "${_root}/CMakeLists.txt" AND EXISTS "${_root}/src/SFML/Graphics/CMakeLists.txt")
            set(_sfml_local_root "${_root}")
            set(_sfml_local_kind "source")
            break()
        endif()
    endforeach()
endif()

if(_sfml_local_kind STREQUAL "source")
    # --- Mã nguồn SFML: biên dịch cùng dự án => cùng một trình biên dịch, không lo lệch phiên bản MinGW ---
    message(STATUS "Tim thay MA NGUON SFML tai: ${_sfml_local_root}")
    message(STATUS "  -> se bien dich SFML cung du an (lan dau mat vai phut). Can CMake >= 3.28.")
    set(SFML_BUILD_AUDIO ON CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    add_subdirectory("${_sfml_local_root}" "${CMAKE_BINARY_DIR}/_sfml" EXCLUDE_FROM_ALL)
else()
    set(_sfml_hint "")
    if(_sfml_local_kind STREQUAL "prebuilt")
        message(STATUS "Tim thay SFML DUNG SAN tai: ${_sfml_local_root}")
        set(_sfml_hint HINTS "${_sfml_local_root}/lib/cmake/SFML")
    endif()
    find_package(SFML 3.1 COMPONENTS Graphics Audio QUIET ${_sfml_hint})
    if(NOT SFML_FOUND)
        # Gói pacman của MSYS2 hiện là SFML 3.0.x. Code chỉ dùng API có từ 3.0 nên vẫn biên dịch được.
        find_package(SFML 3.0 COMPONENTS Graphics Audio QUIET ${_sfml_hint})
        if(SFML_FOUND)
            message(WARNING "Khong tim thay SFML 3.1, dang dung SFML ${SFML_VERSION}.")
        else()
            message(FATAL_ERROR
                "Khong tim thay SFML 3.x.\n"
                "Cach sua (chon 1):\n"
                "  1) Dat thu muc SFML-3.1.0 canh thu muc du an (cung cap voi 'SlimeHunter').\n"
                "  2) Hoac truyen:  -DCMAKE_PREFIX_PATH=<thu_muc_SFML>   (thu muc co lib/cmake/SFML)\n"
                "  3) Hoac:         -DSFML_DIR=<thu_muc_SFML>/lib/cmake/SFML\n"
                "Da tim o: ${_sfml_roots}")
        endif()
    endif()
    message(STATUS "Dung SFML ${SFML_VERSION} tai ${SFML_DIR}")
    if(MINGW AND _sfml_local_kind STREQUAL "prebuilt")
        message(STATUS "LUU Y: SFML dung san chi chay dung voi dung ban MinGW da dung de dung no")
        message(STATUS "       (goi chinh thuc: GCC 14.2.0 MinGW SEH UCRT). Ban dang dung GCC ${CMAKE_CXX_COMPILER_VERSION}.")
        message(STATUS "       Neu loi link/chay, hay dat MA NGUON SFML-3.1.0 vao day de CMake tu bien dich.")
    endif()
endif()

# ============================================================================
#  CHƯƠNG TRÌNH
# ============================================================================
# Module CSV (không phụ thuộc SFML): dùng chung cho game và chương trình kiểm thử tests/csv_test.cpp
add_library(SlimeHunterData STATIC src/UserRepository.cpp)
target_include_directories(SlimeHunterData PUBLIC include)
target_compile_definitions(SlimeHunterData PRIVATE SLIMEHUNTER_SOURCE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
if(MSVC)
    target_compile_options(SlimeHunterData PRIVATE /W4 /utf-8)
else()
    target_compile_options(SlimeHunterData PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(SlimeHunter
    src/main.cpp
    src/Game.cpp
    src/GameUi.cpp
    src/GameSmokeTest.cpp
    src/Assets.cpp
    src/Animation.cpp
    src/GameMap.cpp
    src/PathField.cpp
    src/Player.cpp
    src/Monster.cpp
    src/Slime.cpp
    src/Skeleton.cpp
    src/Bat.cpp
    src/Levels.cpp
    src/ExitGate.cpp
    src/Effects.cpp
    src/Projectile.cpp
    src/Item.cpp
    src/Weapon.cpp
    src/MonsterFactory.cpp
    src/AudioManager.cpp
    src/PowerUp.cpp
    src/Shop.cpp
)

target_include_directories(SlimeHunter PRIVATE include)
target_link_libraries(SlimeHunter PRIVATE SFML::Graphics SFML::Audio SlimeHunterData)
if(WIN32)
    target_link_libraries(SlimeHunter PRIVATE imm32)
endif()

# Đường dẫn assets trong thư mục nguồn: chỉ dùng làm phương án dự phòng cuối cùng khi chạy từ vị trí lạ.
target_compile_definitions(SlimeHunter PRIVATE SLIMEHUNTER_SOURCE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(MSVC)
    target_compile_options(SlimeHunter PRIVATE /W4 /utf-8)
else()
    target_compile_options(SlimeHunter PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- Sao chép tài nguyên cạnh file chạy, để chạy trực tiếp từ thư mục build ----
add_custom_command(TARGET SlimeHunter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:SlimeHunter>/assets"
    COMMENT "Sao chep assets/ vao thu muc chua file chay")

# ---- Windows: sao chép DLL cạnh file .exe để bấm đúp chạy được ----
if(WIN32)
    # (a) DLL của SFML (khi SFML là thư viện động). Cần CMake >= 3.21.
    get_target_property(_sfml_graphics_type SFML::Graphics TYPE)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21 AND _sfml_graphics_type STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET SlimeHunter POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:SlimeHunter> $<TARGET_FILE_DIR:SlimeHunter>
            COMMAND_EXPAND_LISTS
            COMMENT "Sao chep DLL cua SFML")
    endif()

    # (b) DLL runtime của MinGW (nằm cùng thư mục với g++.exe). Chỉ chép những file tồn tại.
    if(MINGW)
        get_filename_component(_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(_dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
            if(EXISTS "${_mingw_bin}/${_dll}")
                add_custom_command(TARGET SlimeHunter POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_mingw_bin}/${_dll}" "$<TARGET_FILE_DIR:SlimeHunter>"
                    COMMENT "Sao chep ${_dll}")
            endif()
        endforeach()
    endif()
endif()

# ---- Kiểm thử module CSV (không cần SFML, không cần màn hình): chạy bằng `ctest` hoặc ./csv_test ----
option(SLIMEHUNTER_BUILD_TESTS "Build chuong trinh kiem thu CSV" ON)
if(SLIMEHUNTER_BUILD_TESTS)
    enable_testing()
    add_executable(csv_test tests/csv_test.cpp)
    target_link_libraries(csv_test PRIVATE SlimeHunterData)
    add_test(NAME csv_test COMMAND csv_test)
endif()
