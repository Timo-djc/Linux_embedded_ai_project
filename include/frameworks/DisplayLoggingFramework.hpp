/**
  ******************************************************************************
  * @file    DisplayLoggingFramework.hpp
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
 * @brief   显示与日志框架 — OLED 显示、运行日志与异常记录持久化层头文件
 * ----------------------------------------------------------------------------
 * @details 本文件定义了 MultiProject 框架的显示与日志层核心类
 *          DisplayLoggingFramework，继承自 IDisplayLogger 接口，
 *          统一管理以下四类输出职责：
 *
 *          1. OLED 实时显示
 *             通过 oled.h 提供的 drawOLED3()/drawOLED4() 接口在 128×64
 *             OLED 屏幕上显示文本、推理结果和系统状态
 *
 *          2. 视频流叠加显示（仅火焰检测）
 *             将检测边界框、置信度和 FPS 信息叠加绘制到 cv::Mat 帧上，
 *             通过 HDMI 或 framebuffer 输出到外接显示器
 *
 *          3. 运行时日志记录
 *             将 DataLog 条目追加写入系统日志文件（system.log），
 *             维护最近 1000 条日志的内存环形缓冲区（recent_logs_），
 *             支持按数量查询和整体导出
 *
 *          4. 异常记录持久化（菜单系统核心功能）
 *             按项目类型将异常条目（AbnormalRecord）追加写入
 *             当天的 CSV 日志文件，并可选保存火焰截图（JPEG）；
 *             提供近 7 天异常记录的读取、跨项目导出和过期文件清理功能；
 *             文件目录结构：
 *               ../log/BearingVibration/YYYY-MM-DD.csv
 *               ../log/FireDetection/YYYY-MM-DD.csv
 *               ../log/FireDetection/images/YYYY-MM-DD_HH-MM-SS.jpg
 *
 *          对外公开接口按职能分为两组：
 *            显示/日志基础接口（覆盖 IDisplayLogger）：
 *              display_text / display_image / display_results / display_status /
 *              log_data / get_recent_logs / export_logs / determine_task
 *            异常记录持久化接口（菜单系统）：
 *              save_abnormal_record / load_week_abnormal /
 *              export_week_logs / purge_old_logs
 *
 * @note    本文件使用 #pragma once 防止重复包含。
 *          私有工具方法均声明为 static，无需访问实例状态，
 *          可直接通过类名调用（内部调用）或在单元测试中独立验证。
 *          CSV 格式固定为 7 列逗号分隔，record_to_csv() 和 csv_to_record()
 *          负责 AbnormalRecord 与 CSV 行之间的双向转换。
 * ============================================================================
 */

#pragma once
// 替代传统 #ifndef 头文件保护，保证本文件在同一编译单元中只被展开一次

#include "../common/UnifiedDataTypes.hpp"
// 引入统一数据类型头文件，提供：
//   IDisplayLogger   — 本类继承的抽象接口基类
//   ModelOutput      — display_results() 的参数类型（AI 推理输出）
//   SystemStatus     — display_status() 的参数类型（系统健康状态）
//   DataLog          — log_data() / get_recent_logs() 的数据载体
//   AbnormalRecord   — 异常记录条目（含项目类型、时间戳、故障描述等字段）
//   RawData          — determine_task() 的参数类型
//   ProjectType      — 枚举，区分 FIRE_DETECTION / BEARING_VIBRATION 项目
//   TaskType         — determine_task() 的返回类型
//   ErrorCode        — 各接口方法的返回类型（错误码枚举）

#include <string>
// 引入 std::string，用于日志路径、日期时间字符串、CSV 行、导出路径等文本字段

#include <vector>
// 引入 std::vector，用于 recent_logs_ 内存缓冲区和 load_week_abnormal() 的输出列表

#include <memory>
// 引入 std::unique_ptr / std::shared_ptr（预留扩展，当前成员未直接使用智能指针，
// 但 UnifiedDataTypes.hpp 中的接口返回类型可能依赖本头文件）

#include <mutex>
// 引入 std::mutex，用于 log_mutex_ 保护日志文件写入和 recent_logs_ 的并发访问；
// 日志写入在报警线程（异步）和主线程中可能并发发生，互斥锁是必要的

#include <fstream>
// 引入 std::ofstream，用于 log_file_ 对系统日志文件（system.log）的追加写入；
// std::ifstream 在 load_week_abnormal() 读取 CSV 文件时使用

#include <opencv2/opencv.hpp>
// 引入 OpenCV 完整功能头文件；
// 提供 cv::Mat（图像矩阵）用于 display_image()（视频帧叠加显示）
// 和 save_abnormal_record()（火焰截图 JPEG 保存）；
// cv::imwrite() 用于将截图编码为 JPEG 文件写入磁盘

namespace MultiProject {
// 开启命名空间 MultiProject

class DisplayLoggingFramework : public IDisplayLogger {
// DisplayLoggingFramework：显示与日志框架的具体实现类；
// 继承 IDisplayLogger 抽象接口（定义于 UnifiedDataTypes.hpp），
// 同时承担 OLED 实时显示、运行日志写入和异常记录持久化三类职责；
// 所有日志文件写入均通过 log_mutex_ 串行化，保证多线程并发安全

public:
    DisplayLoggingFramework();
    // 构造函数：
    //   打开系统日志文件（log_path_ 指定的路径，以追加模式 std::ios::app 打开）；
    //   初始化 recent_logs_ 为空向量；
    //   调用 ensure_dir() 确保 ../log/BearingVibration 和 ../log/FireDetection 目录存在；
    //   调用 purge_old_logs() 清理上次运行遗留的过期日志文件

    ~DisplayLoggingFramework() override;
    // 析构函数：安全关闭 log_file_（std::ofstream 析构时自动 flush 并 close）；
    // override 标注覆盖 IDisplayLogger 的虚析构函数；
    // 析构时不再调用 purge_old_logs()，避免析构路径中的文件系统操作耗时

    // ── 原有显示 / 日志接口 ──────────────────────────────────
    // 以下方法覆盖 IDisplayLogger 定义的纯虚接口

    ErrorCode display_text(const std::string& text, int line = 0) override;
    // 覆盖 IDisplayLogger::display_text()：在 OLED 屏幕的指定行显示文本；
    // 参数 text：UTF-8 编码的文本字符串（中英文均可）；
    // 参数 line：目标显示行（0 为第一行）；
    // 内部调用 oled.h 提供的 drawOLED3() 或 drawOLED4()，
    // 根据文本内容中是否含有中文字符自动选择字体（wqy16 或 9x18B/6x13）；
    // OLED 刷新率由 DisplayLoggingConfig::OLEDDisplay::refresh_rate 控制

    ErrorCode display_image(const cv::Mat& image) override;
    // 覆盖 IDisplayLogger::display_image()：在外接显示器上显示视频帧；
    // 参数 image：已叠加检测结果（边界框、置信度）的 cv::Mat 图像帧；
    // 内部调用 cv::imshow() 或写入 /dev/fb0（framebuffer）；
    // 在无头部署（headless，UIConfig::enable_gui=false）时本方法直接返回 SUCCESS

    ErrorCode display_results(const ModelOutput& results) override;
    // 覆盖 IDisplayLogger::display_results()：在 OLED 和/或外接显示器上显示推理结果；
    // 参数 results：AI 模型的推理输出（含预测类别、置信度、健康指数等）；
    // 针对不同项目选择对应的显示格式：
    //   火焰检测：显示 "FIRE!" 或 "Normal" + 置信度百分比
    //   振动监测：显示故障类别 + 置信度 + 健康指数（0~1）+ RUL 预测天数

    ErrorCode display_status(const SystemStatus& status) override;
    // 覆盖 IDisplayLogger::display_status()：在 OLED 上显示系统运行状态；
    // 参数 status：系统状态聚合对象（含 CPU 温度、内存占用、帧率等）；
    // 根据 DisplayLoggingConfig::OLEDDisplay::DisplayContent 中的开关
    // 选择性显示各状态项（振动值、健康指数、RUL、报警状态等）

    ErrorCode log_data(const DataLog& data_log) override;
    // 覆盖 IDisplayLogger::log_data()：将一条运行日志条目写入持久化存储；
    // 操作路径：
    //   1. 加锁（log_mutex_）
    //   2. 将 data_log 序列化为日志行（含时间戳、级别、消息）写入 log_file_
    //   3. 将 data_log 追加到 recent_logs_ 内存缓冲区
    //   4. 若 recent_logs_.size() > max_logs_（1000），删除最旧条目（front pop）
    //   5. 解锁

    std::vector<DataLog> get_recent_logs(int count = 100) override;
    // 覆盖 IDisplayLogger::get_recent_logs()：返回最近 count 条运行日志的副本；
    // 默认返回最近 100 条（按时间倒序，最新在前）；
    // 加锁访问 recent_logs_ 以保证线程安全；
    // 若 recent_logs_ 中条目数不足 count，返回全部可用条目

    ErrorCode export_logs(const std::string& export_path) override;
    // 覆盖 IDisplayLogger::export_logs()：将 recent_logs_ 中所有日志条目
    // 导出到指定路径的文件（JSON 或文本格式）；
    // 与 export_week_logs() 的区别：本方法导出运行时日志（DataLog），
    // export_week_logs() 导出异常记录（AbnormalRecord）和截图

    TaskType determine_task(const RawData& data) override;
    // 覆盖 IDisplayLogger::determine_task()：根据原始数据特征推断当前任务类型；
    // 通过检查 RawData 中的数据格式或元数据字段判断：
    //   若为图像数据（cv::Mat 类型）→ 返回 TaskType::CLASSIFICATION（火焰检测）
    //   若为时序浮点数据 → 返回 TaskType::CLASSIFICATION（振动分类）
    // 通常由 AIModelFramework 在调用推理前使用，以选择正确的后处理逻辑

    // ── 菜单系统：异常记录持久化 ─────────────────────────────
    // 以下方法提供面向菜单系统的异常记录管理功能，
    // 是本类相对于基础 IDisplayLogger 接口的核心扩展

    /**
     * @brief 将一条异常记录追加到当天 CSV 并（可选）保存截图
     *
     * 路径规则：
     *   轴承：log/BearingVibration/YYYY-MM-DD.csv
     *   火焰：log/FireDetection/YYYY-MM-DD.csv
     *         截图：log/FireDetection/images/YYYY-MM-DD_HH-MM-SS.jpg
     * 超过 7 天的文件由 purge_old_logs() 自动清理。
     *
     * @param record  异常条目（project_type 必填）
     * @param frame   火焰检测时传入当前帧；轴承或不需要截图时传 cv::Mat()
     */
    ErrorCode save_abnormal_record(const AbnormalRecord& record,
                                   const cv::Mat& frame = cv::Mat()) override;
    // 覆盖 IDisplayLogger::save_abnormal_record()：
    // 将一条异常记录持久化到当天的 CSV 文件，并可选地保存截图；
    //
    // 完整执行流程：
    //   1. ensure_dir(log_dir_for(record.project_type))
    //      → 确保目标日志目录存在（不存在则递归创建）
    //   2. today_csv(record.project_type)
    //      → 构造当天 CSV 文件路径（如 ../log/FireDetection/2026-04-08.csv）
    //   3. record_to_csv(record)
    //      → 将 AbnormalRecord 序列化为 7 列 CSV 行字符串
    //   4. 以追加模式（std::ios::app）打开 CSV 文件并写入一行
    //   5. 若 frame 非空（!frame.empty()）且 project_type==FIRE_DETECTION：
    //      a. 构造截图文件名：YYYY-MM-DD_HH-MM-SS.jpg（now_datetime() 派生）
    //      b. ensure_dir(log_dir_for(pt) + "/" + IMG_SUBDIR)
    //         → 确保截图子目录存在
    //      c. cv::imwrite(截图路径, frame)
    //         → 将当前火焰帧编码为 JPEG 保存；
    //         JPEG 有损压缩节省存储空间，适合长期保存截图证据
    //   6. 加锁（log_mutex_），写入后解锁
    //
    // 参数说明：
    //   record.project_type：必填字段，决定写入哪个项目目录的 CSV 文件
    //   frame：火焰检测项目传入含叠加信息的帧，
    //          轴承项目或不需要截图时传 cv::Mat()（空矩阵，frame.empty()=true）

    /**
     * @brief 从磁盘读取指定项目近 7 天所有异常记录（按时间升序）
     */
    ErrorCode load_week_abnormal(ProjectType pt,
                                  std::vector<AbnormalRecord>& out) override;
    // 覆盖 IDisplayLogger::load_week_abnormal()：
    // 从磁盘读取指定项目近 7 天内的所有异常记录并按时间升序排列；
    //
    // 执行流程：
    //   1. 遍历 log_dir_for(pt) 目录下的所有 .csv 文件
    //   2. 对每个文件名提取日期字符串，调用 within_days(date_str, KEEP_DAYS)
    //      过滤超出 7 天范围的文件
    //   3. 对符合条件的 CSV 文件逐行读取，调用 csv_to_record() 反序列化
    //   4. 将所有解析成功的 AbnormalRecord 追加到 out 向量
    //   5. 对 out 按 AbnormalRecord::timestamp 字段升序排列
    //      （多天数据合并后时间顺序可能乱序）
    //
    // 参数说明：
    //   pt：指定读取哪个项目的异常记录（FIRE_DETECTION 或 BEARING_VIBRATION）；
    //   out：输出的异常记录列表，调用前应为空向量，函数追加而非覆盖

    /**
     * @brief 导出近 7 天两个项目的异常日志与截图到目标目录
     *
     * 输出结构：
     *   <export_dir>/
     *     BearingVibration/YYYY-MM-DD.csv …
     *     FireDetection/YYYY-MM-DD.csv …
     *     FireDetection/images/[*].jpg …
     *     export_manifest.txt
     *
     * @param export_dir  目标根目录（不存在时自动创建）
     */
    ErrorCode export_week_logs(const std::string& export_dir) override;
    // 覆盖 IDisplayLogger::export_week_logs()：
    // 将近 7 天两个项目的全部异常 CSV 文件和截图导出到目标目录；
    //
    // 执行流程：
    //   1. ensure_dir(export_dir) → 创建目标根目录
    //   2. 对两个项目（FIRE_DETECTION、BEARING_VIBRATION）分别：
    //      a. ensure_dir(export_dir + "/" + 项目子目录名)
    //      b. 遍历源日志目录，对 within_days() 通过的 CSV 文件调用 copy_file()
    //         将文件复制到目标子目录
    //   3. 火焰检测项目额外复制：
    //      a. ensure_dir(export_dir + "/FireDetection/" + IMG_SUBDIR)
    //      b. 遍历 ../log/FireDetection/images/，
    //         对文件名中日期 within_days() 通过的 .jpg 文件调用 copy_file()
    //   4. 生成 export_manifest.txt：
    //      记录导出时间、导出文件数量和各项目的记录条数摘要
    //
    // 参数说明：
    //   export_dir：目标根目录路径（可为相对路径或绝对路径）；
    //   不存在时由 ensure_dir() 递归创建；已存在时在目录内增量写入

    /**
     * @brief 清理两个项目目录中超过 7 天的文件（启动时和每天定时调用）
     */
    void purge_old_logs();
    // 清理过期日志文件，维护磁盘空间在可控范围内；
    //
    // 执行流程：
    //   1. 遍历 LOG_DIR_BEARING（../log/BearingVibration/）目录下的所有 .csv 文件
    //   2. 对每个文件名提取日期字符串，调用 within_days(date_str, KEEP_DAYS)
    //   3. 返回 false（超过 7 天）的文件调用 std::filesystem::remove() 删除
    //   4. 对 LOG_DIR_FIRE（../log/FireDetection/）重复上述步骤
    //   5. 遍历 LOG_DIR_FIRE/images/ 下的所有 .jpg 文件，
    //      从文件名（YYYY-MM-DD_HH-MM-SS.jpg）中解析日期并执行相同的过期检查和删除
    //
    // 调用时机：
    //   构造函数中调用一次（清理上次运行遗留的过期文件）；
    //   外部定时器（如 MaintenanceConfig 中的 DATA_CLEANUP 计划任务）每天调用一次

private:
// 私有区域：日志文件句柄、互斥锁、配置常量和工具方法

    std::ofstream log_file_;
    // 系统运行日志的文件输出流；
    // 构造函数以追加模式（std::ios::app）打开 log_path_ 对应的文件；
    // log_data() 每次写入后调用 log_file_.flush() 确保数据落盘，
    // 防止程序崩溃时丢失最近的日志条目；
    // 析构函数中 std::ofstream 自动关闭文件

    std::mutex log_mutex_;
    // 日志写入互斥锁；
    // 保护 log_file_ 的并发写入操作（报警线程和主线程可能同时调用 log_data()）；
    // 同时保护 recent_logs_ 的并发读写；
    // save_abnormal_record() 写入 CSV 时也通过此锁串行化，
    // 防止多线程同时写入同一 CSV 文件造成行交叉

    std::string log_path_  = "../log/system.log";
    // 系统运行日志文件的相对路径，默认 "../log/system.log"；
    // 相对路径基准为可执行文件的工作目录（通常为 build/ 目录）；
    // 与 GeneralSystemConfig::LoggingConfig::log_directory 的绝对路径配置
    // 使用不同的路径体系，此处为框架层的轻量日志，不参与轮转管理

    std::vector<DataLog> recent_logs_;
    // 运行时日志内存环形缓冲区，最大容量 max_logs_（1000 条）；
    // 新条目追加到末尾（push_back），溢出时删除最旧条目（erase(begin())）；
    // get_recent_logs() 从此向量切片返回最近 N 条；
    // 所有读写均在 log_mutex_ 保护下执行

    const size_t max_logs_  = 1000;
    // recent_logs_ 内存缓冲区的最大容量，固定为 1000 条；
    // const 修饰表明此容量在对象生命周期内不可更改；
    // 1000 条 × 每条约 256 字节 ≈ 256 KB 内存占用，树莓派可轻松承受

    // 日志目录常量
    static constexpr const char* LOG_DIR_BEARING = "../log/BearingVibration";
    // 轴承振动监测异常记录的 CSV 文件存储目录；
    // static constexpr 使此常量在编译期即确定，不占用运行时存储；
    // 路径相对于可执行文件工作目录，对应 log_dir_for(BEARING_VIBRATION) 的返回值

    static constexpr const char* LOG_DIR_FIRE     = "../log/FireDetection";
    // 火焰检测异常记录的 CSV 文件存储目录；
    // 对应 log_dir_for(FIRE_DETECTION) 的返回值

    static constexpr const char* IMG_SUBDIR       = "images";
    // 火焰截图文件的子目录名称，与 LOG_DIR_FIRE 拼接后得到截图存储路径：
    //   ../log/FireDetection/images/；
    // 独立子目录便于截图文件的单独管理和清理

    static constexpr int KEEP_DAYS        = 7;
    // 日志文件保留天数，固定为 7 天；
    // purge_old_logs() 和 load_week_abnormal() 均以此常量为过期判断基准；
    // 7 天滚动窗口在存储占用和历史可追溯性之间取得平衡

    // 工具
    static std::string now_datetime();          // "YYYY-MM-DD HH:MM:SS"
    // 静态工具方法：返回当前系统时间的格式化字符串 "YYYY-MM-DD HH:MM:SS"；
    // 内部使用 std::chrono::system_clock::now() 或 time()/localtime_r()；
    // 用于日志条目的时间戳生成和截图文件名构造；
    // static 无需实例状态，可从任意上下文直接调用

    static std::string today_date();            // "YYYY-MM-DD"
    // 静态工具方法：返回当前日期字符串 "YYYY-MM-DD"；
    // 是 now_datetime() 的日期部分截取版本；
    // 用于 today_csv() 构造当天 CSV 文件名和 within_days() 的日期比较基准

    static bool within_days(const std::string& date_str, int days);
    // 静态工具方法：判断 date_str 表示的日期是否在当前日期的 days 天范围内；
    // 参数 date_str：待判断的日期字符串（格式 "YYYY-MM-DD"，从文件名解析）；
    // 参数 days：保留天数阈值（通常传入 KEEP_DAYS=7）；
    // 计算方式：today_date() 与 date_str 的日历天差 ≤ days 则返回 true；
    // 返回 false 表示该日期对应的文件已超过保留期，应被删除

    static void ensure_dir(const std::string& path);
    // 静态工具方法：确保指定路径的目录存在；
    // 内部使用 std::filesystem::create_directories(path)（C++17）；
    // 若目录已存在则静默返回（幂等操作）；
    // 若中间路径不存在则递归创建（等价于 mkdir -p）；
    // 目录创建失败时（权限不足等）记录错误但不抛出异常

    static void copy_file(const std::string& src, const std::string& dst);
    // 静态工具方法：将文件从 src 路径复制到 dst 路径；
    // 内部使用 std::filesystem::copy_file(src, dst,
    //   std::filesystem::copy_options::overwrite_existing)；
    // 覆盖已存在的同名文件（避免增量导出时因文件已存在而报错）；
    // 用于 export_week_logs() 将 CSV 和截图文件复制到目标导出目录

    static std::string log_dir_for(ProjectType pt);
    // 静态工具方法：根据项目类型返回对应的日志目录路径；
    // 参数 pt：项目类型枚举值；
    // 映射规则：
    //   ProjectType::FIRE_DETECTION    → "../log/FireDetection"（LOG_DIR_FIRE）
    //   ProjectType::BEARING_VIBRATION → "../log/BearingVibration"（LOG_DIR_BEARING）
    //   其他 → 返回空字符串或默认路径（由实现决定）

    static std::string today_csv(ProjectType pt);
    // 静态工具方法：构造当天异常记录 CSV 文件的完整路径；
    // 返回格式：log_dir_for(pt) + "/" + today_date() + ".csv"；
    // 示例：
    //   FIRE_DETECTION    → "../log/FireDetection/2026-04-08.csv"
    //   BEARING_VIBRATION → "../log/BearingVibration/2026-04-08.csv"；
    // 每天自动生成新文件（文件名即日期），无需手动轮转

    // CSV 序列化（7 列，逗号分隔）
    static std::string record_to_csv(const AbnormalRecord& r);
    // 静态工具方法：将 AbnormalRecord 结构体序列化为 7 列 CSV 行字符串；
    // CSV 列定义（固定顺序）：
    //   列 1：时间戳（"YYYY-MM-DD HH:MM:SS"）
    //   列 2：项目类型（"FireDetection" 或 "BearingVibration"）
    //   列 3：故障类别名称（如 "deballfault"、"fire"）
    //   列 4：置信度/严重程度（float，保留 4 位小数）
    //   列 5：健康指数（float，范围 0~1）
    //   列 6：附加描述（可为空，含逗号时须用引号包裹）
    //   列 7：截图文件名（仅火焰检测有值，轴承为空字符串）；
    // 与 csv_to_record() 互为逆操作，两者定义的列序必须严格一致

    static bool csv_to_record(const std::string& line,
                               ProjectType pt, AbnormalRecord& r);
    // 静态工具方法：将单行 CSV 字符串反序列化为 AbnormalRecord 结构体；
    // 参数 line：从 CSV 文件读取的一行文本（含换行符前的内容）；
    // 参数 pt：所属项目类型，用于填充 r.project_type；
    // 参数 r：输出的反序列化 AbnormalRecord，失败时内容不确定；
    // 按逗号分割 line 后按列序解析各字段：
    //   解析失败（字段数不足 7、类型转换异常）返回 false；
    //   全部字段解析成功返回 true；
    // load_week_abnormal() 在逐行读取 CSV 时调用本方法，
    // 对返回 false 的行跳过（如文件头行、空行、损坏行）
};
// 结束 DisplayLoggingFramework 类定义

} // namespace MultiProject
// 关闭命名空间 MultiProject