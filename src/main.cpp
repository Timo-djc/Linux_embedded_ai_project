/**
  ******************************************************************************
  * @file    main.cpp
  * @author  工科女的日常:The Daily Life of An Engineering Girl - RedBook - NewCoder - CSDN_ WeChat Official Account
  * @details link——RedBook: http://xhslink.com/a/ipmoHKmrFZWfb     NewCoder:https://www.nowcoder.com/share/jump/7241315122413047270
  * @version V1.0.0
  * @date    8-April-2026
  * @brief   Source Code File
  ******************************************************************************
  * @attention
  * Copyright (c) 2026-2036 The Daily Life of An Engineering Girl(工科女的日常).
  * All rights reserved.
  *
  * *----------------------------------------------------------------------------*/

/**
 * ============================================================================
 * @brief   多项目检测系统 — 程序入口与主控流程
 * ----------------------------------------------------------------------------
 * @details 本文件是整个 MultiProject 系统的程序入口（main.cpp），
 *          负责以下五项顶层职责：
 *
 *          1. 命令行参数解析
 *             通过 getopt_long() 支持 7 个选项（-h/-c/-l/-d/-m/-p/--simulate），
 *             允许用户在运行时指定配置文件路径、日志级别、数据目录、
 *             运行模式和模拟数据路径
 *
 *          2. 信号处理
 *             注册 SIGINT（Ctrl+C）和 SIGTERM 的信号处理函数
 *             signal_handler()，置位全局原子标志 g_running=false，
 *             触发主循环退出和优雅关机；
 *             不注册 SIGSEGV，确保段错误产生 core dump 便于 GDB 调试
 *
 *          3. 硬件底层初始化
 *             调用 wiringPiSetupGpio()（BCM 编号模式）初始化 GPIO；
 *             调用 initOLED() 推送全黑帧消除上电雪花；
 *             pigpio 的初始化由 ExtendedFeaturesFramework 构造时自动完成
 *
 *          4. 项目对象生命周期管理
 *             根据编译期激活的项目宏（FIRE_DETECTION_PROJECT 或
 *             BEARING_VIBRATION_PROJECT）创建、初始化、启动并最终关闭
 *             对应的项目编排对象；支持 normal / calibration / test 三种运行模式
 *
 *          5. 主循环与诊断心跳
 *             主线程以 100 ms 为周期休眠，每 DIAG_INTERVAL_S（5 秒）
 *             打印一次三条工作线程的 13 个诊断计数器快照，
 *             用于实时监控流水线各阶段是否正常推进；
 *             若某计数器在连续两次快照间长期不变，即可定位线程卡死位置
 *
 * @note    全局项目对象（fire_project / vibration_project）使用 unique_ptr
 *          管理，并通过条件编译宏隔离，编译时只有一个会被激活。
 *          主循环外部包裹 try-catch，捕获所有 std::exception 并
 *          打印错误信息后返回 EXIT_FAILURE。
 * ============================================================================
 */

#include "../include/common/ProjectDefines.hpp"
// 引入全局预处理宏入口，提供：
//   FIRE_DETECTION_PROJECT / BEARING_VIBRATION_PROJECT — 项目选择宏
//   SYSTEM_VERSION — 系统版本号字符串（用于 print_help() 显示）
//   PROJECT_NAME   — 当前项目名称宏（用于初始化进度打印）
// 同时通过此头文件间接引入所有公共标准库和 FrameworkBase.hpp 中的类型

#include "../include/projects/BearingVibrationProject.hpp"
// 引入轴承振动监测项目顶层编排类；
// 同上，main.cpp 需要完整类定义以实例化 unique_ptr

#include "../include/common/oled/oled.h"
// 引入 OLED 驱动 C 接口头文件；
// 提供 initOLED()（初始化并推送全黑帧）和 drawOLED*() 系列函数；
// 因为 oled.h 使用 extern "C" 包裹，C++ 编译单元可直接调用

#include <iostream>
// 引入 std::cout / std::cerr，用于初始化进度提示、
// 诊断心跳输出和错误信息打印

#include <csignal>
// 引入 signal() 函数和信号常量（SIGINT、SIGTERM、SIGSEGV）；
// 用于注册自定义信号处理函数 signal_handler()

#include <cstdlib>
// 引入 EXIT_SUCCESS（0）和 EXIT_FAILURE（1）宏，
// 作为 main() 的标准返回值；比硬编码整数更具可读性

#include <getopt.h>
// 引入 POSIX getopt_long() 函数和 struct option 类型；
// 用于解析命令行长选项（--help / --config / --simulate 等）和
// 对应的短选项（-h / -c 等）

#include <filesystem>
// 引入 std::filesystem（C++17），用于 fs::create_directories() 递归创建
// 数据目录（./data/logs、./data/models、./data/config）

#include <thread>
// 引入 std::this_thread::sleep_for()，用于主循环中的 100 ms 休眠；
// 以及 std::chrono::steady_clock 用于诊断心跳的时间间隔计算

#include <wiringPi.h>
// 引入 WiringPi 库头文件，提供 wiringPiSetupGpio() 函数；
// 以 BCM GPIO 编号模式初始化树莓派 GPIO 接口，
// 供 OLED 的 u8g2 I2C 驱动使用；
// 注意：pigpio 的初始化（gpioInitialise()）由 ExtendedFeaturesFramework
// 构造函数自动完成，不在 main.cpp 中显式调用

namespace fs = std::filesystem;
// 为 std::filesystem 命名空间创建别名 fs，
// 减少后续代码中 fs::create_directories() 等调用的书写长度

std::atomic<bool> g_running(true);
// 全局原子布尔标志，控制主循环是否继续运行；
// 初始为 true（程序正常运行）；
// signal_handler() 在收到 SIGINT 或 SIGTERM 时置为 false，
// 触发主循环（while (g_running)）退出；
// std::atomic 保证信号处理函数（异步上下文）与主线程之间的写入可见性，
// 无需互斥锁


#ifdef BEARING_VIBRATION_PROJECT
std::unique_ptr<MultiProject::BearingVibrationProject> vibration_project;
// 轴承振动监测项目对象的全局 unique_ptr（仅在 BEARING_VIBRATION_PROJECT 条件下存在）；
// 与 fire_project 同理，声明为全局以支持异步关机路径的访问
#endif

void signal_handler(int signum) {
    // 信号处理函数：在收到 SIGINT（Ctrl+C）或 SIGTERM（kill 命令）时调用；
    // 打印信号编号和关机提示，置位 g_running=false 触发主循环退出；
    //
    // 重要约束：信号处理函数在异步上下文（独立的信号栈）中执行，
    // 只能调用"异步信号安全"（async-signal-safe）的函数；
    // POSIX 标准规定的异步信号安全函数包括 write()、strlen() 等，
    // 但不包括 std::cout、printf、malloc 等（这些函数可能在信号触发时
    // 持有内部锁，重入调用会导致死锁）；
    // 因此本函数使用 write(STDERR_FILENO, ...) 替代 std::cerr，
    // 并手动实现整数→字符串转换以避免 sprintf/snprintf 的潜在不安全性

    const char* prefix = "\n接收到信号 ";
    // 输出前缀字符串，包含换行符以确保提示独占一行

    const char* suffix = ", 正在关闭系统...\n";
    // 输出后缀字符串，提示用户系统正在执行关机流程

    char num_buf[16];
    // 用于存储信号编号十进制字符串的固定大小栈缓冲区（16 字节绰绰有余）
    int num_len = 0;
    // num_buf 中已写入的字符数量（长度计数器），初始为 0

    int num = signum;
    // 将 signum 参数复制到局部变量 num，
    // 避免后续修改操作影响原始信号编号值

    if (num < 0) { num_buf[0] = '-'; num_len = 1; num = -num; }
    // 处理负数信号编号（极少见但需防御性处理）：
    // 在缓冲区首位写入负号字符 '-'，num_len 置为 1，
    // 然后将 num 取绝对值继续处理数字部分

    if (num == 0) {
        num_buf[num_len++] = '0';
        // 特判零值：直接写入 '0'，避免后续 while 循环因 num==0 不进入
    } else {
        char temp[16]; int temp_len = 0;
        // temp：存储逆序数字字符的临时缓冲区
        // temp_len：temp 中已写入的字符数

        while (num > 0) { temp[temp_len++] = '0' + (num % 10); num /= 10; }
        // 通过取模运算逐位提取数字，从个位到高位依次存入 temp（逆序）；
        // 例如 signum=2：temp = ['2']，temp_len=1
        //      signum=15：temp = ['5','1']，temp_len=2

        for (int i = temp_len - 1; i >= 0; i--) num_buf[num_len++] = temp[i];
        // 将 temp 中逆序存储的数字反转写入 num_buf（得到正序十进制字符串）；
        // 例如 temp=['5','1'] → num_buf = "15"
    }

    write(STDERR_FILENO, prefix, strlen(prefix));
    // 将前缀字符串写入标准错误（文件描述符 2）；
    // write() 是 POSIX 定义的异步信号安全函数，可在信号处理函数中安全调用

    write(STDERR_FILENO, num_buf, num_len);
    // 将信号编号字符串写入标准错误

    write(STDERR_FILENO, suffix, strlen(suffix));
    // 将后缀字符串写入标准错误

    g_running = false;
    // 置位全局运行标志为 false；
    // std::atomic<bool> 的 store 操作是原子的，在信号处理函数中安全；
    // 主线程在下一次 while (g_running) 检查时退出循环
}

void print_help() {
    // 打印命令行帮助信息到标准输出；
    // 在 main() 解析到 -h/--help 选项时调用，打印后直接返回 EXIT_SUCCESS
    std::cout << "多项目检测系统 - 版本 " << SYSTEM_VERSION << "\n"
    // 首行显示系统名称和版本号（SYSTEM_VERSION 宏由 ProjectDefines.hpp 定义为 "2.0.0"）
              << "用法: multi_project_system [选项]\n\n"
              << "选项:\n"
              << "  -h, --help             显示帮助信息\n"
              << "  -c, --config FILE      配置文件路径\n"
              << "  -l, --log-level LEVEL  日志级别 (debug, info, warn, error)\n"
              << "  -d, --data-dir DIR     数据目录路径\n"
              << "  -m, --mode MODE        运行模式 (normal, calibration, test)\n"
              << "  -p, --project PROJECT  指定项目 (fire, vibration)\n"
              << "  --simulate PATH        启用模拟模式并从 PATH 读取数据\n\n"
              << "示例:\n"
              << "  ./multi_project_system --project fire --config config/fire.json\n"
              << "  ./multi_project_system --project vibration --mode calibration\n"
              << "  ./multi_project_system --project fire --simulate ./datas/fire/\n";
}

int main(int argc, char* argv[]) {
    // 程序入口函数，完整执行流程：
    //   1. 解析命令行参数
    //   2. 注册信号处理函数
    //   3. 初始化 wiringPi 和 OLED
    //   4. 创建项目对象（根据编译宏选择）
    //   5. 配置模拟模式（若 --simulate 已指定）
    //   6. 创建数据目录
    //   7. 初始化项目（硬件初始化 + 模型加载）
    //   8. 按运行模式启动（normal/calibration/test）
    //   9. 主循环（含 5s 诊断心跳）
    //  10. 优雅关机

    std::string config_path    = "";
    // 配置文件路径，默认为空字符串（使用各子框架的编译期默认值）；
    // -c/--config 选项可指定 JSON 格式的配置文件路径

    std::string data_dir       = "./data";
    // 数据目录根路径，默认 "./data"（相对于可执行文件工作目录）；
    // 主函数会在此目录下递归创建 logs/、models/、config/ 三个子目录

    std::string log_level      = "info";
    // 日志级别字符串，默认 "info"；
    // 可选值：debug / info / warn / error（由 LoggingConfig::log_level 使用）；
    // 当前版本中此参数被解析但未直接传入各框架（框架通过配置文件获取日志级别）

    std::string mode           = "normal";
    // 运行模式字符串，默认 "normal"（正常持续检测）；
    //   "normal"      → start()（启动三线程持续运行）
    //   "calibration" → calibrate_cameras() / calibrate_sensors()（一次性校准后退出）
    //   "test"        → run_self_test() / run_diagnostic_test()（一次性自检后退出）

    std::string project_override = "";
    // 项目类型覆盖参数（-p/--project 选项的值）；
    // 当前版本中仅打印警告"项目切换需要重新编译"而不实际切换；
    // 项目类型在编译期由 FIRE_DETECTION_PROJECT / BEARING_VIBRATION_PROJECT 宏确定

    bool  simulation_enabled   = false;
    // 模拟模式开关，默认 false（使用真实硬件）；
    // --simulate 选项将此标志置为 true

    std::string simulation_path = "";
    // 模拟数据文件的根目录路径，由 --simulate PATH 选项赋值；
    // 火焰检测：需包含 fire_images/ 和 normal_images/ 子目录
    // 振动监测：需包含振动数据 CSV 文件

    static struct option long_options[] = {
        // getopt_long() 所需的长选项描述表；
        // 每个条目格式：{长选项名, 参数要求, 标志指针, 短选项字符/唯一整数}
        {"help",     no_argument,       0, 'h'},
        // --help：无需参数（no_argument），对应短选项 -h

        {"config",   required_argument, 0, 'c'},
        // --config FILE：必须跟一个参数（required_argument），对应短选项 -c

        {"log-level",required_argument, 0, 'l'},
        // --log-level LEVEL：必须参数，对应短选项 -l

        {"data-dir", required_argument, 0, 'd'},
        // --data-dir DIR：必须参数，对应短选项 -d

        {"mode",     required_argument, 0, 'm'},
        // --mode MODE：必须参数，对应短选项 -m

        {"project",  required_argument, 0, 'p'},
        // --project PROJECT：必须参数，对应短选项 -p

        {"simulate", required_argument, 0, 2000},
        // --simulate PATH：必须参数，无对应短选项；
        // 使用整数 2000 作为唯一标识（与 ASCII 可打印字符范围不重叠），
        // 在 switch-case 中通过 case 2000: 处理

        {0, 0, 0, 0}
        // 表哨兵条目（全零），标记选项表结束
    };

    int opt, opt_idx = 0;
    // opt：getopt_long() 每次返回的选项字符或整数标识
    // opt_idx：在选项表中的当前索引（由 getopt_long() 更新）

    while ((opt = getopt_long(argc, argv, "hc:l:d:m:p:", long_options, &opt_idx)) != -1) {
        // 循环解析命令行参数，直至 getopt_long() 返回 -1（所有选项已处理完）；
        // 第三个参数 "hc:l:d:m:p:" 为短选项字符串：
        //   无冒号：无需参数（如 h）
        //   跟冒号：需要一个参数（如 c:）
        switch (opt) {
            case 'h': print_help(); return EXIT_SUCCESS;
            // -h/--help：打印帮助信息后立即以成功状态退出

            case 'c': config_path = optarg; break;
            // -c/--config：将 optarg（指向当前选项参数字符串）赋值给 config_path

            case 'l': log_level   = optarg; break;
            // -l/--log-level：记录日志级别字符串（当前版本暂未直接传入框架）

            case 'd': data_dir    = optarg; break;
            // -d/--data-dir：覆盖默认数据目录路径

            case 'm': mode        = optarg; break;
            // -m/--mode：覆盖运行模式（normal / calibration / test）

            case 'p': project_override = optarg;
                      std::cout << "警告: 项目切换需要重新编译\n"; break;
            // -p/--project：记录项目覆盖参数，并打印警告；
            // 项目类型由编译宏决定，运行时无法动态切换，
            // 此警告提示用户需要重新编译对应的项目宏

            case 2000: simulation_enabled = true; simulation_path = optarg; break;
            // --simulate PATH：启用模拟模式并记录数据路径；
            // 整数 2000 与 long_options 表中 {"simulate", ..., 2000} 对应

            default: print_help(); return EXIT_FAILURE;
            // 遇到未知选项（getopt_long 返回 '?'）：打印帮助后以失败状态退出
        }
    }

    signal(SIGINT,  signal_handler);
    // 注册 SIGINT（信号编号 2）的处理函数；
    // SIGINT 由用户按 Ctrl+C 触发（键盘中断），是最常用的交互式停止方式

    signal(SIGTERM, signal_handler);
    // 注册 SIGTERM（信号编号 15）的处理函数；
    // SIGTERM 由 kill 命令发送（不指定信号时的默认信号），
    // 用于 systemd 服务停止（systemctl stop）时的优雅关机触发

    // SIGSEGV 不能"优雅处理"：段错误说明程序状态已损坏，
    // 继续运行会掩盖真正的 bug。恢复默认行为（生成 core dump），
    // 便于用 GDB 定位崩溃位置。
    // signal(SIGSEGV, signal_handler);  // ← 已移除，不再掩盖崩溃
    //
    // 以上为原有注释（保留），设计决策说明：
    // SIGSEGV（段错误，信号编号 11）表示程序访问了非法内存地址；
    // 若注册了自定义处理函数，signal_handler 执行后返回，
    // 程序可能继续执行损坏的状态，导致更难排查的后续错误；
    // 保持 SIGSEGV 默认行为（终止进程并生成 core dump 文件）更安全：
    //   gdb ./multi_project_system core  ← 直接定位崩溃栈帧

    // ── 初始化 wiringPi（BCM 编号模式）和 OLED ────────────────
    // 注意：pigpio 由 ExtendedFeaturesFramework 构造时自动初始化；
    //       wiringPiSetupGpio 仅用于 OLED 的 u8g2 I2C 驱动。
    //
    // 以上为原有注释（保留），说明两个 GPIO 库的职责分工：
    //   wiringPi：初始化 I2C GPIO 引脚，供 oled.h 的 u8g2 底层驱动使用
    //   pigpio：由 ExtendedFeaturesFramework 内部自动初始化，
    //           用于按键 alert 回调、PWR_OFF 中断检测等高精度 GPIO 事件

    if (wiringPiSetupGpio() == -1) {
        // wiringPiSetupGpio() 使用 BCM GPIO 编号方案初始化 WiringPi 库；
        // 返回 -1 表示初始化失败（常见原因：非 root 权限或 /dev/mem 访问被拒）；
        // 失败时打印警告但不退出——系统仍可运行，仅 OLED 显示功能不可用
        std::cerr << "wiringPi(GPIO模式) 初始化失败" << std::endl;
    } else {
        if (!initOLED()) {
            // initOLED() 向 SSD1306/SSD1309 发送初始化指令序列，
            // 并推送 1024 字节全零帧（全黑）消除上电雪花噪点；
            // 返回 false 表示 I2C 设备文件打开失败或写入失败；
            // 打印警告但不退出，系统在无 OLED 的情况下仍可运行
            std::cerr << "OLED 初始化失败，显示功能不可用" << std::endl;
        }
        // 不再手动 drawOLED，由菜单系统接管
        // 原有注释（保留）：OLED 的后续内容由 ExtendedFeaturesFramework 的
        // initialize_menu() 启动的菜单线程负责渲染，
        // main.cpp 只负责上电全黑帧初始化，不再直接调用 drawOLED*()
    }

    try {
        // 将项目初始化和运行的全部逻辑包裹在 try-catch 中；
        // 捕获 std::exception 派生类（包括 std::runtime_error、std::bad_alloc 等），
        // 打印错误信息后返回 EXIT_FAILURE；
        // 未捕获的异常（如 std::terminate）将由系统默认处理（通常终止进程）

        // ── 创建项目对象 ──────────────────────────────────────
#ifdef BEARING_VIBRATION_PROJECT
        vibration_project = std::make_unique<MultiProject::BearingVibrationProject>();
        // 在堆上构造 BearingVibrationProject 实例并以 unique_ptr 管理；
        // 与 fire_project 同理，此时不执行任何硬件初始化
        std::cout << "已创建轴承振动项目对象" << std::endl;
#endif

        // ── 模拟模式 ──────────────────────────────────────────
        if (simulation_enabled) {
            // 若命令行 --simulate PATH 选项已指定，在 initialize() 之前
            // 通知项目对象使用模拟数据源（而非真实硬件）；
            // 须在 initialize() 之前调用，确保初始化流程选择正确的数据通道
#ifdef BEARING_VIBRATION_PROJECT
            if (vibration_project) { vibration_project->enableSimulation(true, simulation_path); }
            // 为振动监测项目启用模拟模式：
            // 内部预加载 simulation_path 下的振动数据 CSV 文件
#endif
            std::cout << "已启用模拟模式，数据源: " << simulation_path << std::endl;
        }

        // ── 创建数据目录 ──────────────────────────────────────
        if (!data_dir.empty()) {
            // 仅在 data_dir 非空时尝试创建目录（空字符串表示不需要本地数据存储）
            try {
                fs::create_directories(data_dir + "/logs");
                // 递归创建日志子目录（如 ./data/logs/）；
                // 目录已存在时静默返回，不报错（create_directories 的幂等特性）

                fs::create_directories(data_dir + "/models");
                // 递归创建模型文件子目录（如 ./data/models/）；
                // 用于存储 .onnx 模型文件的本地备份或 OTA 更新下载

                fs::create_directories(data_dir + "/config");
                // 递归创建配置文件子目录（如 ./data/config/）；
                // 用于存储运行时热更新的 JSON 配置文件

            } catch (const std::exception& e) {
                std::cerr << "创建数据目录失败: " << e.what() << std::endl;
                // 目录创建失败（通常因为权限不足）时打印错误并终止程序；
                // 数据目录不可用可能导致后续日志写入失败，属于致命错误
                return EXIT_FAILURE;
            }
        }

        // ── 初始化项目（内部自动启动 OLED 菜单）─────────────
        // 原有注释说明：initialize() 内部会调用 ExtendedFeaturesFramework::initialize_menu()，
        // 启动 OLED 菜单刷新线程，无需 main.cpp 显式处理菜单初始化
        std::cout << "初始化 " << PROJECT_NAME << "..." << std::endl;
        // 打印项目初始化进度，PROJECT_NAME 宏由 ProjectDefines.hpp 中的
        // 条件编译块定义（"Fire Detection System" 或 "Bearing Vibration Detection System"）

        MultiProject::ErrorCode status;
        // 声明局部变量 status，接收各操作的返回码，复用于后续多个调用

#ifdef BEARING_VIBRATION_PROJECT
        status = vibration_project->initialize(config_path);
        // 执行轴承振动监测项目的完整初始化：
        //   SPI 总线初始化 + AD4134 寄存器配置 → FFTW3 规划 → ONNX 模型加载 → OLED 菜单线程启动
#endif
        if (status != MultiProject::ErrorCode::SUCCESS) {
            std::cerr << "系统初始化失败: " << static_cast<int>(status) << std::endl;
            // static_cast<int>(status) 将 ErrorCode 强类型枚举转为整数打印，
            // 便于对照 FrameworkBase.hpp 中的 ErrorCode 枚举值排查失败原因
            return EXIT_FAILURE;
        }

        // ── 运行系统 ──────────────────────────────────────────
#ifdef BEARING_VIBRATION_PROJECT
        if (mode == "calibration")      status = vibration_project->calibrate_sensors();
        // 校准模式：调用 AD4134 零偏校准（须在设备静止时执行）
        else if (mode == "test")        status = vibration_project->run_diagnostic_test();
        // 诊断模式：执行 SPI 通信、ONNX 模型、OLED、GPIO 的完整自检
        else                            status = vibration_project->start();
        // 正常运行模式（默认）：启动三条工作线程，进入持续监测状态
#endif
        if (status != MultiProject::ErrorCode::SUCCESS) {
            std::cerr << "系统启动失败: " << static_cast<int>(status) << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "\n系统运行中，按 Ctrl+C 停止\n"
                  << std::string(50, '=') << std::endl;
        // 打印运行提示，std::string(50, '=') 生成 50 个 '=' 字符的分隔线

        // ── 主循环（含诊断心跳）────────────────────────────────
        // 每 DIAG_INTERVAL_S 秒打印一次各线程关键步骤计数快照。
        // 若某计数器长期不变，说明线程卡在其后面的步骤。
        // 正常运行时 infer_done 约每 3~6s +1（CWT耗时），
        // 若连续两次快照 proc_got_raw 不变说明 raw_queue 空/阻塞，
        // 若 proc_cwt_done 不变说明 CWT 卡死，
        // 若 proc_infer_done 不变说明推理卡死，
        // 若 proc_output 不变说明 output_ai_result_current 卡死（PWM/RS485）。
        //
        // 以上为原有注释（保留），提供了主循环的核心设计意图：
        // 心跳打印不是"健康检查"，而是"卡死位置定位工具"——
        // 通过观察哪个计数器停止递增，可以精确定位线程阻塞在哪条语句之后

        static constexpr int DIAG_INTERVAL_S = 5;
        // 诊断心跳打印间隔（秒），编译期常量，默认 5 秒；
        // 5 秒间隔在"实时性"和"终端输出噪声"之间取得平衡；
        // 若需要更密集的监控，可缩短至 1~2 秒

        auto diag_last = std::chrono::steady_clock::now();
        // 记录上一次打印诊断心跳的时间点（单调时钟，不受 NTP 调整影响）；
        // 初始化为程序进入主循环的当前时刻

        while (g_running) {
            // 主循环：每 100 ms 检查一次 g_running 标志，
            // 并在达到 DIAG_INTERVAL_S 间隔时打印诊断快照；
            // 主线程不参与任何采集/处理/日志操作，仅作监控和关机触发

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // 主线程休眠 100 ms，将 CPU 时间让给三条工作线程；
            // 100 ms 的唤醒间隔保证信号响应延迟不超过约 100 ms

            auto now = std::chrono::steady_clock::now();
            // 获取当前单调时钟时间点

            if (std::chrono::duration_cast<std::chrono::seconds>(
                    now - diag_last).count() >= DIAG_INTERVAL_S) {
                // 计算自上次打印以来经过的秒数；
                // duration_cast<std::chrono::seconds> 截断为整秒（向零取整）；
                // 若 >= DIAG_INTERVAL_S（5 秒），执行诊断打印

                diag_last = now;
                // 更新心跳基准时间点，准备下一个 5 秒计时窗口

#ifdef BEARING_VIBRATION_PROJECT
                if (vibration_project) {
                    // 读取振动项目的 13 个诊断计数器快照（使用 .load() 原子读取）
                    auto acq_iter    = vibration_project->diag_acq_loop_iter_.load();
                    // 采集线程主循环迭代计数快照
                    auto acq_got     = vibration_project->diag_acq_got_data_.load();
                    // 成功读取振动数据帧的计数快照
                    auto acq_push    = vibration_project->diag_acq_pushed_.load();
                    // 成功推入 raw_queue_ 的计数快照
                    auto proc_iter   = vibration_project->diag_proc_loop_iter_.load();
                    // 处理线程主循环迭代计数快照
                    auto proc_raw    = vibration_project->diag_proc_got_raw_.load();
                    // 从 raw_queue_ 弹出数据的计数快照
                    auto proc_cwt    = vibration_project->diag_proc_cwt_done_.load();
                    // CWT 时频图生成完成的计数快照
                    auto proc_istart = vibration_project->diag_proc_infer_start_.load();
                    // 推理启动计数快照
                    auto proc_idone  = vibration_project->diag_proc_infer_done_.load();
                    // 推理完成计数快照（连续两次快照均不变说明推理卡死）
                    auto proc_out    = vibration_project->diag_proc_output_.load();
                    // RS485 电流输出完成的计数快照（不变说明 RS485 阻塞）
                    auto proc_snap   = vibration_project->diag_proc_snapshot_.load();
                    // 菜单快照更新完成的计数快照
                    auto proc_save   = vibration_project->diag_proc_save_rec_.load();
                    // 异常记录 CSV 写入完成的计数快照
                    auto proc_push   = vibration_project->diag_proc_pushed_.load();
                    // 结果推入 result_queue_ 的计数快照
                    auto log_iter    = vibration_project->diag_log_loop_iter_.load();
                    // 日志线程主循环迭代计数快照

                    std::cout << "\n[DIAG " << DIAG_INTERVAL_S << "s心跳] ──────────────────────────\n"
                              << "  采集线程:  loop=" << acq_iter
                              << "  got_data=" << acq_got
                              << "  pushed=" << acq_push << "\n"
                              << "  处理线程:  loop=" << proc_iter
                              << "  got_raw=" << proc_raw
                              << "  cwt_done=" << proc_cwt << "\n"
                              // cwt_done 对应 CWT 时频图生成，是振动项目的处理核心步骤
                              << "             infer_start=" << proc_istart
                              << "  infer_done=" << proc_idone
                              << "  output=" << proc_out << "\n"
                              << "             snapshot=" << proc_snap
                              << "  save_rec=" << proc_save
                              << "  pushed=" << proc_push << "\n"
                              << "  日志线程:  loop=" << log_iter << "\n"
                              << "  raw_queue=" << vibration_project->raw_queue_size()
                              // raw_queue_ 当前积压帧数（持续增大说明处理线程速度跟不上）
                              << "  result_queue=" << vibration_project->result_queue_size()
                              // result_queue_ 当前积压条目数（持续增大说明日志线程速度跟不上）
                              << "\n──────────────────────────────────────\n";
                }
#endif
            }
        }
        // 主循环结束：g_running 已被 signal_handler 置为 false

    } catch (const std::exception& e) {
        // 捕获所有 std::exception 派生类异常（含 bad_alloc、runtime_error、
        // filesystem_error 等）；
        // 打印异常描述后返回 EXIT_FAILURE
        std::cerr << "系统异常: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // ── 关机 ──────────────────────────────────────────────────
    // 执行优雅关机序列：先停止工作线程（stop），再释放硬件资源（shutdown）
    std::cout << "\n正在关闭系统..." << std::endl;

#ifdef BEARING_VIBRATION_PROJECT
    if (vibration_project) { vibration_project->stop(); vibration_project->shutdown(); }
    // 与火焰检测项目同理，先停线程再释放硬件
#endif

    std::cout << "系统关闭完成" << std::endl;
    return EXIT_SUCCESS;
    // 正常退出，返回 0（EXIT_SUCCESS）；
    // 程序退出后 unique_ptr（fire_project / vibration_project）的析构函数
    // 也会被调用（若 stop()/shutdown() 之前未手动调用），
    // 提供最后一层资源释放保障
}