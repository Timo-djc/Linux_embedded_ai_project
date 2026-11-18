/**
  ******************************************************************************
  * @file    ProjectDefines.hpp
  * @author  工科女的日常:The Daily Life of An Engineering Girl - RedBook - NewCoder - CSDN_ WeChat Official Account
  * @details link——RedBook: http://xhslink.com/a/ipmoHKmrFZWfb     NewCoder:https://www.nowcoder.com/share/jump/7241315122413047270
  * @version V1.0.0
  * @date    8-April-2026
  * @brief   Header File
  ******************************************************************************
  * @attention
  * Copyright (c) 2026-2036 The Daily Life of An Engineering Girl(工科女的日常).
  * All rights reserved.
  *
  * *----------------------------------------------------------------------------*/

/**
 * ============================================================================
 * @brief   多项目检测系统 — 全局预处理宏与公共头文件聚合入口
 * ----------------------------------------------------------------------------
 * @details 本文件是整个 MultiProject 系统在编译阶段的统一配置入口，
 *          承担以下四项职责：
 *
 *          1. 公共头文件聚合（#include 区）
 *             集中包含 C++ 标准库、POSIX 系统接口、Linux 内核接口
 *             及第三方库头文件，供所有编译单元通过包含本文件一次性获取
 *             全部依赖，无需在各 .cpp 文件中重复列举。
 *
 *          2. 项目选择与条件编译（#ifdef 区）
 *             通过 FIRE_DETECTION_PROJECT / BEARING_VIBRATION_PROJECT 宏
 *             在编译期确定当前激活的子项目，并为该项目注入专属宏定义
 *             （项目名称、模型路径、硬件参数等）。
 *             若两个宏均未定义，#error 指令强制编译失败，防止产生
 *             无项目类型的无效可执行文件。
 *
 *          3. 系统级全局宏（版本、调试、性能、日志、安全）
 *             提供 SYSTEM_VERSION、LOG_DEBUG、ENABLE_PERFORMANCE_MONITOR
 *             等跨模块使用的通用宏开关，集中管理编译期特性开关。
 *
 *          4. 通用宏函数（SAFE_DELETE 系列、CHECK_STATUS、TIME_MEASURE）
 *             封装常用的安全内存释放、错误码检查和性能计时操作，
 *             以 do-while(0) 惯用法保证宏展开后的语句块安全性。
 *
 * @note    本文件使用 #pragma once 代替传统 #ifndef 保护宏，
 *          语义等价但书写更简洁，受所有主流编译器（GCC/Clang/MSVC）支持。
 *
 *          ProjectType / TaskType 枚举统一使用 FrameworkBase.hpp 中
 *          namespace MultiProject 下的定义，本文件不重复声明，
 *          避免两套枚举数值不同导致的逻辑错误。
 * ============================================================================
 */

#pragma once
// 替代传统 #ifndef/#define/#endif 头文件保护的现代写法；
// 编译器级别保证本文件在同一编译单元中只被包含一次，
// 语义上等价于 #ifndef PROJECTDEFINES_HPP / #define PROJECTDEFINES_HPP / #endif，
// 但无需手动维护宏名称的唯一性

// ============================================================================
// 标准库与系统头文件聚合包含区
// ============================================================================

#include <iostream>
// C++ 标准输入输出流：提供 std::cout（标准输出）、std::cerr（标准错误输出）、
// std::cin（标准输入）；用于 LOG_DEBUG 宏的终端打印及 CHECK_STATUS 宏的错误输出

#include <chrono>
// C++ 标准时间库：提供 std::chrono::high_resolution_clock、
// std::chrono::system_clock、duration_cast 等；
// 用于 TIME_MEASURE_START/END 宏的高精度计时（微秒级）

#include <thread>
// C++ 标准线程库：提供 std::thread、std::this_thread::sleep_for 等；
// 用于采集线程、推理线程、报警线程等工作线程的创建与管理

#include <atomic>
// C++ 原子操作库：提供 std::atomic<T>；
// 用于线程间共享的标志变量（如停止标志 stop_、事件 ID 计数器等）的无锁并发访问

#include <memory>
// C++ 智能指针库：提供 std::shared_ptr、std::unique_ptr、std::make_shared 等；
// 用于框架各子模块对象的所有权管理，防止内存泄漏

#include <vector>
// C++ 动态数组容器：提供 std::vector<T>；
// 用于存储摄像头参数列表、事件历史、特征向量等动态大小的序列数据

#include <string>
// C++ 字符串类：提供 std::string；
// 用于路径名、标识符、日志消息、JSON 字符串等文本数据的存储和操作

#include <cstring>
// C 字符串操作函数：提供 memcpy、memset、strlen、strcmp 等；
// 用于底层字节缓冲区操作（如 SPI 数据帧组装、图像缓冲区清零等）

#include <cmath>
// C 数学函数库：提供 sqrt、pow、fabs、sin、cos、log 等浮点运算函数；
// 用于振动特征计算（RMS、峭度）和 FFT 相关的数学运算

#include <algorithm>
// C++ 算法库：提供 std::sort、std::min、std::max、std::find、std::copy 等；
// 用于特征排序、阈值比较、数据复制等通用算法操作

#include <numeric>
// C++ 数值算法库：提供 std::accumulate、std::inner_product、std::iota 等；
// 用于均值计算（accumulate + size）、向量内积（特征相似度）等数值聚合操作

#include <functional>
// C++ 函数对象库：提供 std::function、std::bind、std::ref 等；
// 用于定义和传递回调函数（ErrorCallback、EventCallback 等类型别名的基础）

#include <mutex>
// C++ 互斥锁库：提供 std::mutex、std::lock_guard、std::unique_lock 等；
// 用于 ConcurrentQueue 及各框架模块中共享状态的线程安全访问保护

#include <condition_variable>
// C++ 条件变量库：提供 std::condition_variable；
// 与 std::mutex 配合用于生产者/消费者线程之间的高效阻塞唤醒同步

#include <map>
// C++ 有序关联容器：提供 std::map<Key, Value>（红黑树，按键有序）；
// 用于轴承型号到故障频率系数的有序映射（BearingAssessmentConfig::bearing_types）

#include <unordered_map>
// C++ 无序关联容器：提供 std::unordered_map<Key, Value>（哈希表，O(1) 平均查找）；
// 用于需要高频查找但不要求键排序的场景（如帧 ID 到检测结果的映射）

#include <queue>
// C++ 队列适配器：提供 std::queue<T>（FIFO 队列）和 std::priority_queue<T>；
// std::queue 是 ConcurrentQueue 的底层存储容器

#include <deque>
// C++ 双端队列容器：提供 std::deque<T>（std::queue 的默认底层实现）；
// 支持 O(1) 头尾插入删除，适合滑动窗口缓冲区和历史帧缓冲区

#include <fstream>
// C++ 文件流库：提供 std::ifstream（文件读取）、std::ofstream（文件写入）；
// 用于读取 JSON 配置文件、写入 CSV/JSON 日志文件

#include <sstream>
// C++ 字符串流库：提供 std::ostringstream、std::istringstream；
// 用于构建格式化字符串（如 JSON 序列化）和解析字符串（如配置文件解析）

#include <iomanip>
// C++ 输入输出格式化操控器：提供 std::setw、std::setprecision、std::fixed 等；
// 用于格式化输出浮点数（如振动值保留两位小数）

#include <cstdint>
// C 固定宽度整数类型：提供 uint8_t、uint16_t、uint32_t、uint64_t、int32_t 等；
// 保证跨平台（32 位/64 位 ARM）整数位宽一致性

#include <cfloat>
// C 浮点数极值常量：提供 FLT_MAX、FLT_MIN、DBL_MAX、DBL_EPSILON 等；
// 用于浮点数边界检查（如判断置信度是否为非法值 NaN/Inf）

#include <csignal>
// C 信号处理库：提供 signal()、sigaction()、SIGINT、SIGTERM 等；
// 用于捕获 Ctrl+C（SIGINT）和系统终止（SIGTERM）信号，
// 触发框架的优雅关机（graceful shutdown）流程

#include <getopt.h>
// POSIX 命令行参数解析库：提供 getopt()、getopt_long()、struct option；
// 用于解析 main() 函数的命令行参数（如 --project、--config、--debug 等选项）

#include <fcntl.h>
// POSIX 文件控制头文件：提供 open()、O_RDWR、O_NONBLOCK、F_SETFL 等；
// 用于打开 SPI（/dev/spidevX.Y）、I2C（/dev/i2c-X）、
// 摄像头（/dev/videoX）等 Linux 设备文件

#include <unistd.h>
// POSIX 通用系统调用头文件：提供 read()、write()、close()、usleep()、
// getpid() 等基础 POSIX 接口；
// 用于设备文件的读写、线程休眠（usleep 微秒级）等操作

#include <sys/ioctl.h>
// Linux ioctl 系统调用头文件：提供 ioctl() 函数；
// 用于向设备驱动发送控制命令：
//   SPI：SPI_IOC_MESSAGE（执行 SPI 传输）
//   I2C：I2C_RDWR（执行 I2C 读写事务）
//   V4L2：VIDIOC_S_FMT、VIDIOC_STREAMON 等（摄像头配置和流控制）

#include <sys/mman.h>
// Linux 内存映射头文件：提供 mmap()、munmap()、PROT_READ、MAP_SHARED 等；
// 用于 V4L2 MMAP 模式下将摄像头帧缓冲区直接映射到用户空间，
// 避免内核到用户空间的数据拷贝，提升帧采集效率

#include <sys/stat.h>
// POSIX 文件状态头文件：提供 stat()、mkdir()、S_ISDIR() 等；
// 用于检查日志目录是否存在、创建目录（如 /var/log/fire_detection）

#include <sys/select.h>
// POSIX I/O 多路复用头文件：提供 select()、FD_SET、FD_ISSET 等；
// 用于同时监听多个设备文件描述符（如三路摄像头）的可读事件，
// 实现非阻塞的多设备并发读取

#include <linux/videodev2.h>
// Linux V4L2（Video for Linux 2）内核接口头文件；
// 提供摄像头操控所需的全部结构体和宏：
//   v4l2_format：设置图像格式（分辨率、像素格式）
//   v4l2_buffer：管理 MMAP 帧缓冲区
//   VIDIOC_REQBUFS、VIDIOC_QUERYBUF、VIDIOC_STREAMON 等 ioctl 命令码
// 本头文件仅在 Linux 平台存在，不可移植到 Windows/macOS

#include <jpeglib.h>
// libjpeg 库头文件：提供 JPEG 图像压缩/解压缩接口；
// 用于将 V4L2 采集的 MJPEG 格式帧解码为原始 BGR/RGB 像素数组，
// 供图像预处理流水线（灰度化、resize、归一化）进一步处理；
// 编译时须链接 -ljpeg


// 系统版本
#define SYSTEM_VERSION "2.0.0"
// 系统整体版本号字符串，遵循语义化版本规范（Major.Minor.Patch）；
// 用于启动日志打印、远程监控心跳包及诊断报告的版本标识；
// 应与 VersionInfo 结构体中的 major/minor/patch 字段保持一致

#define SYSTEM_NAME "Multi-Project Detection System"
// 系统名称字符串，在启动横幅（banner）、OLED 首屏、日志文件头部打印；
// 反映本系统支持多个检测项目（火焰检测、轴承振动）的通用框架性质

// 调试模式
#ifdef DEBUG
// 条件编译块：仅在编译时定义了 DEBUG 宏时（如 cmake -DCMAKE_BUILD_TYPE=Debug
// 或编译命令携带 -DDEBUG）才启用 LOG_DEBUG 宏的实际输出；
// 在 Release 编译模式下 DEBUG 未定义，LOG_DEBUG 被替换为空，零运行时开销
#define LOG_DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl
// Debug 模式下的日志宏：将消息前缀 "[DEBUG] " 后打印到标准输出，并换行刷新；
// 参数 msg 支持流式拼接（如 LOG_DEBUG("FPS: " << fps)），
// 因为宏展开后整体形成一个 std::cout << ... << std::endl 表达式链
#else
#define LOG_DEBUG(msg)
// Release 模式下 LOG_DEBUG 展开为空语句（nothing），
// 编译器直接消除该宏调用，不产生任何指令和性能开销
#endif
// 结束 DEBUG 条件编译块

// 统一使用 FrameworkBase.hpp 中 namespace MultiProject 里的 ProjectType / TaskType，
// 不在全局命名空间重复定义，避免两套枚举数值不同导致的逻辑错误。
#include "FrameworkBase.hpp"
// 包含框架基础层头文件，引入 namespace MultiProject 下的全部公共类型定义：
//   ProjectType、SystemState、TaskType、ErrorCode、SeverityLevel、ProcessingMode（枚举类）
//   Timestamp、VersionInfo、PerformanceMetrics、Event（数据结构）
//   ErrorCallback、ProgressCallback、EventCallback 等（回调类型别名）
//   IFramework、IFrameworkManager、IFrameworkFactory（抽象接口类）
//   errorCodeToString() 等（工具函数）
// 本注释明确说明了此处不重复定义枚举的设计决策，防止后续维护者误添加重复定义

// 性能监控
#define ENABLE_PERFORMANCE_MONITOR
// 性能监控总开关宏；定义后激活 TIME_MEASURE_START/END 宏的计时逻辑；
// 若需在特定编译目标中关闭性能监控（降低开销），注释或删除本行即可

#define MAX_FPS 30
// 系统处理帧率上限，单位帧/秒；
// 用于帧率控制循环：若当前处理帧率超过此值，主循环应插入适当延时
// （sleep_for(1000ms/MAX_FPS - elapsed)）以避免 CPU 空转；
// 30 fps 对应单帧处理周期约 33.3 ms

#define BUFFER_SIZE 10
// 线程间帧缓冲队列（ConcurrentQueue）的深度上限；
// 与 SystemConfig::Performance::buffer_size 的默认值（10）保持一致；
// 缓冲区越深，对采集/推理速率差异的容忍能力越强，
// 但端到端检测延时也随之增大（最大延时 ≈ BUFFER_SIZE / MAX_FPS ≈ 0.33 s）

// 数据记录
#define ENABLE_DATA_LOGGING
// 数据记录总开关宏；定义后框架层将启用本地日志文件写入功能；
// 注释本行可在存储受限的场景中完全禁用日志持久化

#define LOG_FILE_PATH "/var/log/multi_project_system.log"
// 系统级主日志文件的绝对路径；
// 使用 /var/log/ 符合 Linux FHS 标准；
// 各子项目的专属日志目录（/var/log/fire_detection、/var/log/bearing_vibration）
// 在各自的 DisplayLoggingConfig 中单独配置

#define MAX_LOG_FILES 10
// 日志文件滚动时保留的最大文件数量；
// 超过此数量后，最旧的日志文件将被自动删除，防止磁盘空间耗尽

#define MAX_LOG_SIZE_MB 100
// 单个日志文件的最大大小，单位 MB；
// 当前日志文件达到 100 MB 时触发滚动（Rotation），创建新文件继续写入；
// 与 MAX_LOG_FILES 共同控制总日志磁盘占用上限（最多 10 × 100 MB = 1 GB）

// 安全特性
#define ENABLE_WATCHDOG
// 看门狗（Watchdog）总开关宏；
// 定义后系统启用软件/硬件看门狗定时器；
// 若主循环因死锁或死循环超过 WATCHDOG_TIMEOUT_MS 未喂狗，触发系统复位

#define WATCHDOG_TIMEOUT_MS 5000
// 看门狗超时时间，单位毫秒（ms），默认 5000 ms（5 秒）；
// 主循环每次迭代须在 5 秒内完成喂狗操作；
// 超时后系统视为死锁并触发复位，确保设备在异常后自动恢复运行

#define ENABLE_CRC_CHECK
// CRC 校验总开关宏；
// 定义后对 SPI/I2C 传输数据执行 CRC 校验，
// 检测通信链路上的比特翻转和数据损坏；
// 轴承振动数据对精度要求高，CRC 校验尤为必要

#define ENABLE_SECURE_BOOT
// 安全启动总开关宏；
// 定义后系统在启动时验证固件/模型文件的数字签名或哈希值，
// 防止篡改的恶意固件或模型文件被加载执行

// ============================================================================
// 项目特定宏
// ============================================================================
// 分节注释：以下条件编译块根据激活的项目选择宏，
// 注入该项目专属的编译期常量，确保各模块使用正确的硬件参数和路径

#ifdef BEARING_VIBRATION_PROJECT
// 进入轴承振动监测项目的专属宏配置区；
    #define CURRENT_PROJECT MultiProject::ProjectType::BEARING_VIBRATION
    // 当前激活的项目类型枚举值（轴承振动监测）

    #define PROJECT_NAME "Bearing Vibration Detection System"
    // 当前项目的人类可读名称字符串

    #define MODEL_PATH "../models/bearing_vibration/bearing_vibration_model.onnx"
    // 轴承振动 ONNX 模型文件的相对路径；
    // 与 bearing_vibration_config.hpp 中 AIModelConfig::model_path 的修复后路径一致

    #define SPI_CHANNEL 0
    // AD4134 ADC 使用的 SPI 总线通道编号（0 = /dev/spidev0.x）；
    // 与 AD4134Config::SPIInterface::channel 的默认值一致

    #define SPI_SPEED 2000000
    // SPI 总线时钟频率，单位 Hz，默认 2,000,000 Hz = 2 MHz；
    // 注：此处宏定义为 2 MHz，高于 bearing_vibration_config.hpp 中
    // SPIInterface::speed_hz 的默认值（1 MHz），
    // 以提升 ADC 数据传输速率；实际使用时应以配置文件值为准

    #define ADC_CHANNELS 3  // X, Y, Z轴
    // AD4134 同时采集的通道数：3 路，分别对应加速度传感器的 X、Y、Z 三轴；
    // 与 AD4134Config::Sampling::num_channels 的默认值一致

    #define SAMPLE_RATE_HZ 50000  // 外部时钟6.4MHz, OSR=4 => 50k SPS
    // AD4134 的每通道采样率，单位 Hz，默认 50,000 Hz（50 kSPS）；
    // 推导关系：外部参考时钟 6.4 MHz ÷ 过采样率 OSR=4 ÷ 通道数 = 50 kSPS；
    // 根据奈奎斯特定理，可检测的最高频率为 25 kHz，
    // 覆盖轴承高频故障特征（通常 < 20 kHz）的检测需求；
    // 与 AD4134Config::Sampling::sample_rate_hz 及 SystemConfig::Sampling::main_sample_rate 一致

    #define DEFAULT_TASK    MultiProject::TaskType::CLASSIFICATION
    // 轴承振动监测的默认 AI 任务类型：四分类（正常/滚动体故障/保持架故障/外圈故障）；
    // 与 AIModelConfig::model_type = "classification" 语义一致

#else
    #error "No project defined! Please define FIRE_DETECTION_PROJECT or BEARING_VIBRATION_PROJECT"
    // 编译期强制错误指令：若以上两个项目宏均未定义，
    // 编译器将立即中止并输出此错误消息，防止生成缺少项目上下文的无效可执行文件；
    // #error 的参数字符串直接显示在编译器错误输出中，
    // 清晰指导开发者如何修复（定义其中一个项目宏）
#endif
// 结束项目选择条件编译块

// ============================================================================
// 通用宏函数
// ============================================================================
// 分节注释：以下宏封装常用的内存管理、错误处理和性能计时操作，
// 以 do-while(0) 惯用法保证宏在 if/else 分支中展开的正确性

#define SAFE_DELETE(ptr) \
    do { \
        if (ptr) { \
            delete ptr; \
            ptr = nullptr; \
        } \
    } while(0)
// 安全单对象内存释放宏：
// 1. if (ptr) 检查指针非空后再调用 delete，避免 delete nullptr（虽合法但可视为防御性编程）
// 2. delete ptr 释放 ptr 指向的堆内存
// 3. ptr = nullptr 将指针置空，防止释放后继续使用产生悬空指针（Use-After-Free）
// do-while(0) 惯用法保证宏展开为单个语句块，在 if/else 无花括号时语义正确：
//   if (cond) SAFE_DELETE(p);  ← 展开后等价于 if (cond) { do { ... } while(0); }
// 注意：使用现代 C++ 的 std::unique_ptr 可从根本上避免手动 delete，
//       本宏用于管理不使用智能指针的 C 风格原始指针

#define SAFE_DELETE_ARRAY(ptr) \
    do { \
        if (ptr) { \
            delete[] ptr; \
            ptr = nullptr; \
        } \
    } while(0)
// 安全数组内存释放宏：
// 与 SAFE_DELETE 逻辑相同，但使用 delete[]（而非 delete）释放数组内存；
// delete 和 delete[] 不可混用：用 new[] 分配的内存须用 delete[] 释放，
// 否则行为未定义（可能仅析构第一个元素或导致堆损坏）；
// 本宏用于释放如 uint8_t* buffer = new uint8_t[N] 形式分配的原始数组

#define CHECK_STATUS(status) \
    do { \
        if ((status) != MultiProject::ErrorCode::SUCCESS) { \
            std::cerr << "Error at " << __FILE__ << ":" << __LINE__ << " - Code: " << static_cast<int>(status) << std::endl; \
            return (status); \
        } \
    } while(0)
// 错误码检查宏：
// 1. (status) != MultiProject::ErrorCode::SUCCESS 检查操作是否成功；
//    将 status 用括号包裹以防止宏参数为复杂表达式时的运算符优先级问题
// 2. std::cerr 输出标准错误信息，包含：
//    __FILE__：当前源文件路径（编译器预定义宏，展开为字符串字面量）
//    __LINE__：当前行号（编译器预定义宏，展开为整数字面量）
//    static_cast<int>(status)：将 ErrorCode 强类型枚举转为整数打印，便于对照错误码表排查
// 3. return (status) 提前返回错误码，使调用方感知失败并向上层传播；
//    要求使用本宏的函数返回值类型为 MultiProject::ErrorCode 或兼容类型
// 典型用法：CHECK_STATUS(initialize());  // 初始化失败时自动打印并返回错误码

#ifdef ENABLE_PERFORMANCE_MONITOR
// 若 ENABLE_PERFORMANCE_MONITOR 已定义，启用实际计时逻辑

    #define TIME_MEASURE_START(name) \
        auto start_##name = std::chrono::high_resolution_clock::now()
    // 计时开始宏：记录名为 name 的计时点的起始时刻
    // ## 是宏的标记粘合运算符（Token Pasting Operator）：
    //   TIME_MEASURE_START(inference) 展开为：
    //   auto start_inference = std::chrono::high_resolution_clock::now();
    // high_resolution_clock 是 C++ 提供的最高精度时钟（通常为纳秒级）；
    // auto 让编译器自动推导 time_point 类型，避免冗长的类型书写

    #define TIME_MEASURE_END(name) \
        auto end_##name = std::chrono::high_resolution_clock::now(); \
        auto duration_##name = std::chrono::duration_cast<std::chrono::microseconds>(end_##name - start_##name); \
        LOG_DEBUG("Timer " #name ": " << duration_##name.count() << "us")
    // 计时结束宏：记录名为 name 的计时点的结束时刻，计算耗时并通过 LOG_DEBUG 输出
    // 展开步骤（以 TIME_MEASURE_END(inference) 为例）：
    //   auto end_inference = std::chrono::high_resolution_clock::now();
    //     → 记录结束时刻
    //   auto duration_inference = std::chrono::duration_cast<std::chrono::microseconds>(end_inference - start_inference);
    //     → 将时间差转换为微秒（microseconds）精度；
    //       duration_cast 执行截断（向零取整），微秒级精度对嵌入式推理计时已足够
    //   LOG_DEBUG("Timer " #name ": " << duration_inference.count() << "us")
    //     → # 是宏的字符串化运算符（Stringification Operator）：
    //       #name 将宏参数 name 转换为字符串字面量（如 "inference"）；
    //       .count() 返回微秒数的整数值；
    //       "us" 为微秒单位后缀；
    //       仅在 DEBUG 模式下（ENABLE_PERFORMANCE_MONITOR 且 DEBUG 均定义）输出，
    //       Release 模式下 LOG_DEBUG 为空，此行不产生任何输出

#else
    #define TIME_MEASURE_START(name)
    // 性能监控关闭时，TIME_MEASURE_START 展开为空语句，零运行时开销
    #define TIME_MEASURE_END(name)
    // 性能监控关闭时，TIME_MEASURE_END 展开为空语句，零运行时开销
#endif
// 结束 ENABLE_PERFORMANCE_MONITOR 条件编译块