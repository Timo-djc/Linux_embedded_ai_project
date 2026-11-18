cmake_minimum_required(VERSION 3.16)          # 要求 CMake 最低版本为 3.16（支持 find_package CONFIG 模式）
project(MultiProjectSystem VERSION 2.0.0)     # 项目名称 MultiProjectSystem，版本号 2.0.0

set(CMAKE_CXX_STANDARD 17)                    # 指定 C++ 标准为 C++17（项目使用 std::filesystem / structured binding 等 C++17 特性）
set(CMAKE_CXX_STANDARD_REQUIRED ON)           # 若编译器不支持 C++17 则报错退出，不允许降级回退
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)         # 生成 compile_commands.json，供 clangd / VSCode IntelliSense 代码补全使用

# =========================
# 项目选择
# =========================
option(FIRE_DETECTION_PROJECT    "Build Fire Detection Project"    OFF)  # P1 火焰检测，默认关闭
option(BEARING_VIBRATION_PROJECT "Build Bearing Vibration Project"  ON)  # P2 轴承诊断，默认开启

# 兜底保护：若两个 option 均未通过 -D 参数指定，自动启用 P2，避免裸 cmake .. 构建失败
if(NOT FIRE_DETECTION_PROJECT AND NOT BEARING_VIBRATION_PROJECT)
    set(BEARING_VIBRATION_PROJECT ON)
endif()

# 向 C++ 编译器传递宏定义（二选一）
# 源码中所有 #ifdef FIRE_DETECTION_PROJECT / #ifdef BEARING_VIBRATION_PROJECT 块
# 在编译期由此宏决定是否参与编译，未选中项目的代码完全不被编译进可执行文件
if(FIRE_DETECTION_PROJECT)
    add_definitions(-DFIRE_DETECTION_PROJECT)       # P1：传入火焰检测宏
elseif(BEARING_VIBRATION_PROJECT)
    add_definitions(-DBEARING_VIBRATION_PROJECT)    # P2：传入轴承诊断宏
endif()

# =========================
# Python 集成（可选）
# =========================
# 默认 OFF，主流程不依赖 Python，开启后需确保树莓派已安装 Python3 开发头文件
option(USE_PYTHON_INTEGRATION "Enable Python integration for AI models" OFF)
if(USE_PYTHON_INTEGRATION)
    add_definitions(-DUSE_PYTHON_INTEGRATION)                              # 向源码传入 Python 集成宏
    find_package(Python3 COMPONENTS Development Interpreter REQUIRED)      # 查找 Python3 开发库与解释器
endif()

# =========================
# onnxruntime
# =========================
# 若未通过环境变量或命令行指定 onnxruntime_DIR，则自动设为安装时的默认路径
# 对应 4.3 节中 ONNX Runtime 安装后 sudo ldconfig 写入的路径
if(NOT DEFINED onnxruntime_DIR OR onnxruntime_DIR STREQUAL "")
    set(onnxruntime_DIR "/usr/local/lib/cmake/onnxruntime"
        CACHE PATH "Path to onnxruntime CMake config directory" FORCE)     # FORCE：覆盖缓存中的旧值
endif()
find_package(onnxruntime REQUIRED CONFIG)     # CONFIG 模式：使用 onnxruntimeConfig.cmake 查找，而非系统 FindXXX 模块

# =========================
# OpenCV
# =========================
find_package(OpenCV REQUIRED)                                 # 查找 OpenCV，找不到则报错（P1/P2 均依赖）
message(STATUS "OpenCV version: ${OpenCV_VERSION}")           # 打印已找到的 OpenCV 版本，便于确认
message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")      # 打印 OpenCV 头文件路径，便于排查路径问题
find_package(JPEG REQUIRED)                  # libjpeg：图像编解码，OpenCV 部分功能依赖
# =========================
# 其他依赖
# =========================

find_library(RT_LIB rt)                      # librt：POSIX 实时扩展（clock_gettime 等），某些 Linux 发行版需要显式链接

find_package(PkgConfig REQUIRED)             # 启用 pkg_check_modules，用于查找未提供 CMake 包的库
pkg_check_modules(FFTW REQUIRED fftw3)       # 通过 pkg-config 查找 FFTW3（P2 CWT 时频图计算核心库）

# =========================
# wiringPi
# =========================
# 查找 wiringPi 头文件路径（wiringPi.h）
find_path(WIRINGPI_INCLUDE_DIR wiringPi.h
    PATHS /usr/include /usr/local/include     # 依次搜索这两个标准头文件目录
)

# 查找 wiringPi 动态库（支持 aarch64 和 armv7 两种路径，兼容不同树莓派系统配置）
find_library(WIRINGPI_LIBRARY wiringPi
    PATHS
        /usr/lib
        /usr/local/lib
        /usr/lib/aarch64-linux-gnu            # 64-bit OS 标准路径
        /usr/lib/arm-linux-gnueabihf          # 32-bit OS 标准路径（兼容旧镜像）
)

# 任一未找到则以 FATAL_ERROR 终止，并给出安装命令，避免链接期才报错
if(NOT WIRINGPI_INCLUDE_DIR OR NOT WIRINGPI_LIBRARY)
    message(FATAL_ERROR
        "wiringPi not found.\n"
        "Try: sudo apt install wiringpi"
    )
endif()

message(STATUS "Found wiringPi include: ${WIRINGPI_INCLUDE_DIR}")   # 确认头文件路径
message(STATUS "Found wiringPi lib: ${WIRINGPI_LIBRARY}")           # 确认动态库路径

# =========================
# pigpio
# =========================
# 查找 pigpio 头文件（pigpio.h 提供 hardware PWM / GPIO 中断等接口）
find_path(PIGPIO_INCLUDE_DIR pigpio.h
    PATHS /usr/include /usr/local/include
)

# 查找 pigpio 动态库（同样兼容 aarch64 与 armv7 路径）
find_library(PIGPIO_LIBRARY pigpio
    PATHS
        /usr/lib
        /usr/local/lib
        /usr/lib/aarch64-linux-gnu
        /usr/lib/arm-linux-gnueabihf
)

# 未找到则终止，给出安装命令（pigpiod 守护进程也需同步安装）
if(NOT PIGPIO_INCLUDE_DIR OR NOT PIGPIO_LIBRARY)
    message(FATAL_ERROR
        "pigpio not found.\n"
        "Try: sudo apt install pigpio libpigpio-dev"
    )
endif()

message(STATUS "Found pigpio include: ${PIGPIO_INCLUDE_DIR}")       # 确认头文件路径
message(STATUS "Found pigpio lib: ${PIGPIO_LIBRARY}")               # 确认动态库路径

# =========================
# u8g2
# =========================
# 只在项目源码树内查找 libu8g2.a（NO_DEFAULT_PATH：不搜索系统路径）
# 防止意外链接系统中可能存在的同名库（版本不一致会导致 OLED 渲染异常）
# 使用前须先在 src/common/oled/u8g2/ 目录执行 cmake .. && make -j4 完成本地编译
find_library(U8G2_LIB u8g2
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src/common/oled/u8g2/build
    NO_DEFAULT_PATH
)

# 未找到则终止，提示先完成 u8g2 本地编译
if(NOT U8G2_LIB)
    message(FATAL_ERROR "libu8g2 not found in src/common/oled/u8g2/build. Build u8g2 first.")
endif()

message(STATUS "Found u8g2 lib: ${U8G2_LIB}")                       # 确认静态库路径

# =========================
# 包含目录
# =========================
include_directories(
    # ── 项目自身头文件 ──
    ${CMAKE_CURRENT_SOURCE_DIR}/include              # 项目顶层 include/
    ${CMAKE_CURRENT_SOURCE_DIR}/include/common       # 公共基础组件（ConcurrentQueue / FrameworkBase 等）
    ${CMAKE_CURRENT_SOURCE_DIR}/include/common/oled  # OLED 驱动接口头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/include/frameworks   # 五层框架纯虚接口头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/include/projects     # 业务项目层头文件

    # ── 第三方库头文件 ──
    ${OpenCV_INCLUDE_DIRS}                           # OpenCV 头文件路径（由 find_package 自动填充）
    ${JPEG_INCLUDE_DIRS}                             # libjpeg 头文件路径
    ${WIRINGPI_INCLUDE_DIR}                          # wiringPi.h 所在目录
    ${PIGPIO_INCLUDE_DIR}                            # pigpio.h 所在目录
    ${FFTW_INCLUDE_DIRS}                             # fftw3.h 所在目录（由 pkg_check_modules 填充）

    # ── u8g2 源码头文件（以源码方式集成，非安装方式）──
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/oled/u8g2/csrc                   # u8g2 核心 C 源码头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/oled/u8g2/sys/linux-i2c/common   # Linux I2C 平台适配头文件
)

# Python 集成启用时，追加 Python3 头文件目录
if(USE_PYTHON_INTEGRATION)
    include_directories(${Python3_INCLUDE_DIRS})
endif()

# =========================
# 源文件
# =========================
# 递归扫描 src/common/ 下全部 .cpp（包含 oled.cpp、各工具类等）
file(GLOB_RECURSE COMMON_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp")
# 排除 u8g2 源码目录（u8g2 以静态库 libu8g2.a 形式链接，不参与本项目编译）
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/src/common/oled/u8g2/.*")
# 显式追加 oled.cpp（OLED 驱动封装实现，上一行 FILTER 不会把它排除，此行为保险起见显式保留）
list(APPEND COMMON_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/common/oled/oled.cpp")

# 递归扫描 src/frameworks/ 下全部 .cpp（五层框架具体实现）
file(GLOB_RECURSE FRAMEWORK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/frameworks/*.cpp")

# 业务层：只编译当前项目对应的 .cpp，另一个项目的业务文件完全不参与链接
if(FIRE_DETECTION_PROJECT)
    set(PROJECT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/projects/FireDetectionProject.cpp)     # P1
elseif(BEARING_VIBRATION_PROJECT)
    set(PROJECT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/projects/BearingVibrationProject.cpp)  # P2
endif()

# 汇总所有参与编译的源文件
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp     # 系统入口：命令行解析 → 初始化 → 主循环
    ${COMMON_SOURCES}                            # 公共基础组件
    ${FRAMEWORK_SOURCES}                         # 五层框架实现
    ${PROJECT_SOURCES}                           # 当前项目业务层
)

add_executable(multi_project_system ${SOURCES})  # 生成可执行文件 multi_project_system

# =========================
# 链接库
# =========================
target_link_libraries(multi_project_system
    ${OpenCV_LIBS}                  # OpenCV（图像采集 / NCC 融合 / CWT resize 等）
    onnxruntime::onnxruntime        # ONNX Runtime（INT8 模型推理）
    ${WIRINGPI_LIBRARY}             # wiringPi（SPI 总线 / I2C 总线驱动）
    ${PIGPIO_LIBRARY}               # pigpio（hardware PWM → 4-20mA 输出 / GPIO 中断）
    ${U8G2_LIB}                     # u8g2 静态库（SSD1309 OLED 驱动）
    i2c                             # libi2c（I2C 总线底层读写）
    pthread                         # POSIX 线程库（三线程并发运行依赖）
    ${JPEG_LIBRARIES}               # libjpeg（图像编解码）
    ${RT_LIB}                       # librt（POSIX 实时时钟，clock_gettime 等）
    ${FFTW_LIBRARIES}               # FFTW3（CWT 时频图 FFT 计算核心）
)

# Python 集成启用时，追加 Python3 运行时库
if(USE_PYTHON_INTEGRATION)
    target_link_libraries(multi_project_system ${Python3_LIBRARIES})
endif()

# =========================
# 编译选项
# =========================
target_compile_options(multi_project_system PRIVATE
    -Wall           # 开启全部常规编译警告
    -Wextra         # 开启额外编译警告（未使用参数、符号比较等）
    -O2             # 二级优化（始终生效，Debug 构建类型下 CMake 额外附加 -g 调试符号）
    -march=native   # 针对当前 CPU 自动启用最优指令集（树莓派 4B：ARMv8-A + NEON SIMD）
)

# =========================
# 安装
# =========================
install(TARGETS multi_project_system DESTINATION bin)                                  # 可执行文件 → /usr/local/bin/
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config  DESTINATION etc/multi_project)  # 配置头文件 → /etc/multi_project/
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/models  DESTINATION var/lib/multi_project)  # ONNX 模型 → /var/lib/multi_project/
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts DESTINATION usr/share/multi_project) # 脚本文件 → /usr/share/multi_project/

# 将 service 模板渲染为正式 systemd service 文件（@ONLY：只替换 @变量@ 占位符）
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/multi_project.service.in  # 输入：模板文件
    ${CMAKE_CURRENT_BINARY_DIR}/multi_project.service             # 输出：渲染后的 service 文件
    @ONLY
)

# 将渲染后的 service 文件安装到 systemd 目录，执行 sudo systemctl enable 后即可开机自启
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/multi_project.service
        DESTINATION /lib/systemd/system)

# =========================
# tests（可选）
# =========================
# 若 tests/ 目录存在独立的 CMakeLists.txt，则启用 CTest 并将其纳入构建
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "No tests directory found, skipping tests.")    # 无 tests 目录时静默跳过
endif()

# 设置运行时动态库搜索路径（RPATH），确保程序在编译环境和安装环境均能找到 GCC 11 运行时库
set_target_properties(multi_project_system PROPERTIES
    BUILD_RPATH   "/usr/local/gcc-11/lib64"    # 编译阶段（build tree）使用的 RPATH
    INSTALL_RPATH "/usr/local/gcc-11/lib64"    # 安装后（install tree）使用的 RPATH
)