/**
  ******************************************************************************
  * @file    DisplayLoggingFramework.cpp
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
 * @brief   显示与日志框架 — 实现文件
 * ----------------------------------------------------------------------------
 * @details 本文件实现 DisplayLoggingFramework.hpp 中声明的全部方法，
 *          按以下五个模块组织：
 *
 *  构造 / 析构
 *      构造函数：创建必要的日志目录树，打开系统日志文件（追加模式）
 *      析构函数：安全关闭日志文件
 *
 *  原有接口（轻量实现）
 *      display_text / display_image / display_results / display_status：
 *          向标准输出打印简短诊断信息（OLED 显示桩或控制台输出）
 *      log_data：将 DataLog 追加到内存循环缓冲区并写入系统日志文件
 *      get_recent_logs：从内存缓冲区读取最近 N 条日志
 *      export_logs：委托 export_week_logs() 执行
 *      determine_task：编译期根据项目宏返回任务类型枚举值
 *
 *  工具函数
 *      now_datetime：返回当前时间字符串（格式 "YYYY-MM-DD HH:MM:SS"）
 *      today_date：返回当前日期字符串（格式 "YYYY-MM-DD"）
 *      within_days：判断给定日期距今是否在 N 天以内
 *      ensure_dir：递归创建目录（失败时只打印警告不中止）
 *      copy_file：带覆盖选项的文件复制（失败时只打印警告）
 *      log_dir_for：根据项目类型返回对应的日志根目录路径
 *      today_csv：拼接当日 CSV 文件的完整路径
 *
 *  CSV 序列化 / 反序列化
 *      record_to_csv：将 AbnormalRecord 序列化为 CSV 行字符串
 *      csv_to_record：将 CSV 行字符串反序列化回 AbnormalRecord
 *      CSV 列顺序：timestamp, class_id, class_name, confidence,
 *                  current_ma, image_path, error_reason
 *
 *  异常记录 / 日志读写 / 导出 / 清理
 *      save_abnormal_record：保存异常记录；火焰项目额外保存截图 JPEG
 *      load_week_abnormal：读取近 KEEP_DAYS 天的所有异常记录
 *      export_week_logs：将近期日志和截图复制到导出目录，生成清单文件
 *      purge_old_logs：删除超出 KEEP_DAYS 的过期 CSV 和截图文件
 *
 * @note  日志目录树结构（相对于可执行文件目录）：
 *          ../log/
 *            BearingVibration/
 *              YYYY-MM-DD.csv   （轴承异常记录）
 *            FireDetection/
 *              YYYY-MM-DD.csv   （火焰异常记录）
 *              images/
 *                YYYY-MM-DD_HH-MM-SS.jpg  （火焰截图）
 *        系统日志路径：由成员 log_path_ 指定（默认 "../log/system.log"）
 * ============================================================================
 */

#include "DisplayLoggingFramework.hpp"
// 引入本框架完整类声明（含 AbnormalRecord、DataLog、CSV 辅助方法声明）

#include "../common/ProjectDefines.hpp"
// 引入项目全局宏定义和常量：
// LOG_DIR_BEARING（轴承日志目录）、LOG_DIR_FIRE（火焰日志目录）、
// IMG_SUBDIR（图像子目录名）、KEEP_DAYS（日志保留天数，通常 7）

#include <iostream>
// 引入 std::cout / std::cerr，用于控制台诊断输出和错误提示

#include <sstream>
// 引入 std::ostringstream / std::istringstream
// record_to_csv 中用 ostringstream 拼接 CSV 行
// csv_to_record 中用 istringstream 按逗号分割字段

#include <iomanip>
// 引入 std::fixed / std::setprecision，用于 CSV 中浮点数格式化输出

#include <ctime>
// 引入 C 时间函数：std::time、std::localtime、std::strftime、std::mktime、std::difftime
// 用于生成时间戳字符串和计算日期差

#include <cstring>
// 引入 C 字符串函数（为 sscanf 解析日期字符串提供支持）

#include <filesystem>
// 引入 C++17 std::filesystem（别名为 fs）
// 提供目录创建、遍历、文件复制/删除、路径操作等功能

#include <fstream>
// 引入 std::ofstream / std::ifstream，用于 CSV 文件读写和系统日志文件操作

#include <algorithm>
// 引入 std::sort，用于在 load_week_abnormal 中对 CSV 文件路径列表排序

namespace fs = std::filesystem;
// 为 std::filesystem 创建简短别名 fs，减少路径操作代码的冗余

namespace MultiProject {
// 开启项目命名空间 MultiProject

// ============================================================================
// 构造 / 析构
// ============================================================================

DisplayLoggingFramework::DisplayLoggingFramework() {
// 构造函数：初始化日志目录树并打开系统日志文件
    ensure_dir("../log");
    // 创建日志根目录 ../log（相对于可执行文件目录）
    ensure_dir(LOG_DIR_BEARING);
    // 创建轴承振动日志子目录（通常 ../log/BearingVibration）
    ensure_dir(LOG_DIR_FIRE);
    // 创建火焰检测日志子目录（通常 ../log/FireDetection）
    ensure_dir(std::string(LOG_DIR_FIRE) + "/" + IMG_SUBDIR);
    // 创建火焰检测截图子目录（../log/FireDetection/images）

    log_file_.open(log_path_, std::ios::app);
    // 以追加模式打开系统日志文件（log_path_ 默认为 "../log/system.log"）
    // std::ios::app：每次写入自动定位到文件末尾，保留历史日志
    if (!log_file_.is_open())
        std::cerr << "[DisplayLog] 无法打开系统日志: " << log_path_ << std::endl;
        // 文件打开失败时打印错误提示（不抛出异常，系统继续运行）
}

DisplayLoggingFramework::~DisplayLoggingFramework() {
// 析构函数：安全关闭系统日志文件
    if (log_file_.is_open()) log_file_.close();
    // 检查文件是否处于打开状态后再关闭，防止对已关闭文件调用 close()
}

// ============================================================================
// 原有接口
// ============================================================================

ErrorCode DisplayLoggingFramework::display_text(const std::string& text, int /*line*/) {
// 在控制台输出带 [OLED] 前缀的文本（实际项目中可替换为 SSD1306 OLED 驱动调用）
// 参数 line 未使用（注释掉的参数名防止"未使用参数"警告）
    std::cout << "[OLED] " << text << std::endl;
    // 将文本内容以 "[OLED] " 前缀打印到标准输出
    return ErrorCode::SUCCESS;
    // 返回成功（OLED 输出不会失败）
}
ErrorCode DisplayLoggingFramework::display_image(const cv::Mat& /*image*/) {
// 图像显示接口（桩实现；OLED 分辨率有限，图像显示通常不适用）
    return ErrorCode::SUCCESS;
    // 直接返回成功，不执行任何操作
}
ErrorCode DisplayLoggingFramework::display_results(const ModelOutput& results) {
// 在控制台输出 AI 推理结果摘要（class_id 和置信度）
    std::cout << "[Display] class=" << results.classification.class_id
    // 打印分类结果的类别 ID
              << " conf="  << results.classification.confidence << std::endl;
    // 打印置信度，std::endl 同时刷新缓冲区
    return ErrorCode::SUCCESS;
}
ErrorCode DisplayLoggingFramework::display_status(const SystemStatus& status) {
// 在控制台输出系统资源占用状态（CPU 使用率和内存占用）
    std::cout << "[Display] CPU=" << status.performance.cpu_usage_percent
    // 打印 CPU 使用率百分比
              << "% MEM=" << status.performance.memory_usage_mb << "MB" << std::endl;
    // 打印内存占用（单位 MB）
    return ErrorCode::SUCCESS;
}

ErrorCode DisplayLoggingFramework::log_data(const DataLog& data_log) {
// 记录一条运行时日志：写入内存循环缓冲区并追加到系统日志文件
    std::lock_guard<std::mutex> lk(log_mutex_);
    // RAII 互斥锁：保护 recent_logs_ 和 log_file_ 的并发写入
    if (recent_logs_.size() >= max_logs_) recent_logs_.erase(recent_logs_.begin());
    // 若内存缓冲区已满（达到 max_logs_ 上限），移除最旧的一条（FIFO 淘汰）
    recent_logs_.push_back(data_log);
    // 将新日志追加到缓冲区末尾
    if (log_file_.is_open()) {
    // 系统日志文件已成功打开时才写入
        log_file_ << "[" << data_log.timestamp << "] "
        // 写入带方括号的时间戳
                  << "project=" << static_cast<int>(data_log.project_type)
                  // 写入项目类型整数值（方便解析）
                  << " class="  << data_log.ai_results.classification.class_id
                  // 写入 AI 分类结果的类别 ID
                  << " conf="   << data_log.ai_results.classification.confidence << "\n";
                  // 写入置信度并换行（'\n' 比 std::endl 更快，不触发缓冲区刷新）
        log_file_.flush();
        // 手动刷新缓冲区，确保日志立即写入磁盘（避免崩溃时数据丢失）
    }
    return ErrorCode::SUCCESS;
}

std::vector<DataLog> DisplayLoggingFramework::get_recent_logs(int count) {
// 从内存缓冲区读取最近 count 条日志
    std::lock_guard<std::mutex> lk(log_mutex_);
    // RAII 互斥锁：保护 recent_logs_ 的并发读取
    if (count <= 0 || static_cast<size_t>(count) >= recent_logs_.size())
        return recent_logs_;
        // count≤0 或请求量不小于缓冲区实际大小时，返回全部日志
    return {recent_logs_.end() - count, recent_logs_.end()};
    // 从缓冲区末尾取最近 count 条（区间 [end-count, end)）
}

ErrorCode DisplayLoggingFramework::export_logs(const std::string& export_path) {
// 日志导出接口（委托 export_week_logs 执行完整的近期日志导出）
    return export_week_logs(export_path);
    // 将 export_path 作为导出根目录，复制近 KEEP_DAYS 天的日志和截图
}

TaskType DisplayLoggingFramework::determine_task(const RawData& /*data*/) {
// 根据编译期项目宏返回对应的任务类型枚举值
// 参数 data 未使用（任务类型由编译宏在编译期决定）
#ifdef BEARING_VIBRATION_PROJECT
    return TaskType::ANOMALY_DETECTION;
    // 轴承振动项目：异常检测任务
#else
    return TaskType::UNKNOWN;
    // 未定义任何项目宏时返回未知任务类型（防御性代码）
#endif
}

// ============================================================================
// 工具函数
// ============================================================================

std::string DisplayLoggingFramework::now_datetime() {
// 返回当前本地时间字符串，格式："YYYY-MM-DD HH:MM:SS"
    std::time_t t = std::time(nullptr);
    // 获取当前 Unix 时间戳（自 1970-01-01T00:00:00Z 起的秒数）
    char buf[24];
    // 声明足够长的字符缓冲区（"2026-04-08 12:30:45" 为 19 字节，含终止符 24 足够）
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    // 将时间戳格式化为本地时间字符串：%Y=四位年，%m=月，%d=日，%H=时，%M=分，%S=秒
    return buf;
    // 返回格式化后的时间字符串
}

std::string DisplayLoggingFramework::today_date() {
// 返回当前本地日期字符串，格式："YYYY-MM-DD"（用作 CSV 文件名和日期比对）
    std::time_t t = std::time(nullptr);
    // 获取当前 Unix 时间戳
    char buf[12];
    // 声明缓冲区（"2026-04-08" 为 10 字节，含终止符 12 足够）
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
    // 格式化为 "YYYY-MM-DD" 日期字符串
    return buf;
    // 返回日期字符串
}

bool DisplayLoggingFramework::within_days(const std::string& date_str, int days) {
// 判断给定日期字符串距今是否在 days 天以内（含当天）
// 参数 date_str：格式 "YYYY-MM-DD" 的日期字符串（来自文件名前缀或 CSV 字段）
// 参数 days：保留天数阈值（通常为 KEEP_DAYS=7）
    if (date_str.size() < 10) return false;
    // 字符串长度不足 10 时无法解析 "YYYY-MM-DD"，直接返回 false
    int y = 0, m = 0, d = 0;
    // 声明年月日整数变量
    if (sscanf(date_str.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return false;
    // 用 sscanf 按 "年-月-日" 格式解析日期字符串；返回值不为 3 表示解析失败
    std::tm tf{};
    // 声明并零初始化 tm 结构体（{} 保证所有字段为 0）
    tf.tm_year = y - 1900; tf.tm_mon = m - 1; tf.tm_mday = d;
    // 填充 tm 结构体：tm_year 以 1900 年为基准，tm_mon 从 0 起（1月=0）
    std::time_t t_file = std::mktime(&tf);
    // 将 tm 结构体转换为 Unix 时间戳（本地时间）
    std::time_t t_now  = std::time(nullptr);
    // 获取当前 Unix 时间戳
    return std::difftime(t_now, t_file) / 86400.0 <= static_cast<double>(days);
    // std::difftime 返回 t_now - t_file 的秒数差
    // 除以 86400.0（每天秒数）得到天数差，与 days 比较
    // ≤days 表示文件日期在保留窗口内（含当天和未来日期）
}

void DisplayLoggingFramework::ensure_dir(const std::string& path) {
// 递归创建指定路径的目录（包含所有中间层目录）
// 若目录已存在则静默成功；创建失败时只打印警告不中止程序
    try { fs::create_directories(path); }
    // fs::create_directories 等价于 mkdir -p：递归创建所有不存在的父目录
    catch (const std::exception& e) {
    // 捕获所有标准异常（如权限不足、路径无效等）
        std::cerr << "[DisplayLog] 创建目录失败 " << path << ": " << e.what() << std::endl;
        // 打印目录路径和异常信息，便于调试
    }
}

void DisplayLoggingFramework::copy_file(const std::string& src, const std::string& dst) {
// 将源文件复制到目标路径（若目标已存在则覆盖）
// 复制失败时只打印警告不中止程序
    try { fs::copy_file(src, dst, fs::copy_options::overwrite_existing); }
    // overwrite_existing：若目标文件已存在则覆盖（导出时幂等）
    catch (const std::exception& e) {
    // 捕获所有标准异常（如源文件不存在、磁盘满等）
        std::cerr << "[DisplayLog] 复制失败 " << src << ": " << e.what() << std::endl;
        // 打印源路径和异常信息
    }
}

std::string DisplayLoggingFramework::log_dir_for(ProjectType pt) {
// 根据项目类型返回对应的日志根目录路径字符串
    return (pt == ProjectType::BEARING_VIBRATION) ? LOG_DIR_BEARING : LOG_DIR_FIRE;
    // 轴承振动项目返回 LOG_DIR_BEARING，否则（包括火焰检测）返回 LOG_DIR_FIRE
}

std::string DisplayLoggingFramework::today_csv(ProjectType pt) {
// 拼接当日 CSV 文件的完整路径：日志根目录 + "/" + 当前日期 + ".csv"
// 例："../log/BearingVibration/2026-04-08.csv"
    return log_dir_for(pt) + "/" + today_date() + ".csv";
    // log_dir_for(pt)：获取项目对应的日志目录
    // today_date()：返回格式 "YYYY-MM-DD" 的当日日期字符串
}

// ============================================================================
// CSV 序列化 / 反序列化
// 列：timestamp, class_id, class_name, confidence, current_ma, image_path, error_reason
// ============================================================================

std::string DisplayLoggingFramework::record_to_csv(const AbnormalRecord& r) {
// 将 AbnormalRecord 结构体序列化为一行 CSV 格式字符串
// 字段顺序：timestamp, class_id, class_name, confidence, current_ma, image_path, error_reason
    std::ostringstream os;
    // 创建字符串输出流，用于拼接 CSV 字段
    os << r.timestamp  << ","
    // 写入时间戳字段（格式 "YYYY-MM-DD HH:MM:SS"）及逗号分隔符
       << r.class_id   << ","
    // 写入分类 ID（整数）及逗号
       << r.class_name << ","
    // 写入分类名称（字符串）及逗号
       << std::fixed << std::setprecision(4) << r.confidence << ","
    // 置信度：固定小数点格式，保留 4 位小数（如 0.9876），与 Python 精度保持一致
       << std::fixed << std::setprecision(2) << r.current_ma << ","
    // 电流值（mA）：保留 2 位小数（如 12.34），单位毫安
       << r.image_path  << ","
    // 写入截图文件路径（火焰项目有值，轴承项目通常为空字符串）
       << r.error_reason;
    // 写入错误原因描述（末尾字段无逗号）
    return os.str();
    // 返回拼接完成的 CSV 行字符串
}

bool DisplayLoggingFramework::csv_to_record(const std::string& line,
                                             ProjectType pt, AbnormalRecord& r) {
// 将一行 CSV 字符串反序列化回 AbnormalRecord 结构体
// 参数 line：CSV 文件中的一行（可能包含注释行和空行）
// 参数 pt：项目类型（作为元数据填入 AbnormalRecord）
// 参数 r：输出参数，反序列化结果写入此结构体
// 返回值：成功返回 true，格式不符或解析失败返回 false
    if (line.empty() || line[0] == '#') return false;
    // 跳过空行和以 '#' 开头的注释行（CSV 文件头部的列名注释）
    std::vector<std::string> f;
    // 声明字段向量，存储按逗号分割的各字段
    std::istringstream ss(line);
    // 将 CSV 行字符串包装为输入字符串流，便于逐字段读取
    std::string tok;
    // 临时存储每个字段的字符串
    while (std::getline(ss, tok, ',')) f.push_back(tok);
    // 按逗号分隔符逐字段读取，将每个字段推入 f 向量
    if (f.size() < 7) return false;
    // 字段数不足 7 时（残缺行或格式错误）返回 false
    try {
    // 使用 try-catch 捕获 stoi/stof 的数字解析异常
        r.timestamp    = f[0];
        // 字段 0：时间戳字符串（直接赋值）
        r.project_type = pt;
        // 项目类型：从调用方参数传入（CSV 中不存储项目类型）
        r.class_id     = std::stoi(f[1]);
        // 字段 1：分类 ID，字符串转 int
        r.class_name   = f[2];
        // 字段 2：分类名称字符串（直接赋值）
        r.confidence   = std::stof(f[3]);
        // 字段 3：置信度，字符串转 float
        r.current_ma   = std::stof(f[4]);
        // 字段 4：电流值（mA），字符串转 float
        r.image_path   = f[5];
        // 字段 5：截图路径字符串（直接赋值）
        r.error_reason = f[6];
        // 字段 6：错误原因描述（直接赋值）
    } catch (...) { return false; }
    // 捕获所有异常（包括 stoi/stof 遇到非数字字符串时抛出的 std::invalid_argument）
    // 解析失败时返回 false，调用方跳过该行
    return true;
    // 所有字段解析成功，返回 true
}

// ============================================================================
// 保存异常记录
// ============================================================================

ErrorCode DisplayLoggingFramework::save_abnormal_record(const AbnormalRecord& record,
// 参数 record：待保存的异常记录（含时间戳、类别、置信度、电流值等）
                                                         const cv::Mat& frame) {
// 参数 frame：对应帧的图像数据（仅火焰项目使用；轴承项目可传空 Mat）
    std::lock_guard<std::mutex> lk(log_mutex_);
    // RAII 互斥锁：保护并发写入（多线程安全）
    ProjectType pt = record.project_type;
    // 从记录中提取项目类型，后续多处使用
    ensure_dir(log_dir_for(pt));
    // 确保对应项目的日志目录存在（第一次写入前可能目录已不存在）

    AbnormalRecord rec = record;
    // 深拷贝输入记录到局部变量（后续可能修改 image_path 字段）

    // 火焰项目：保存截图
    if (pt == ProjectType::FIRE_DETECTION && !frame.empty()) {
    // 仅火焰检测项目且帧图像非空时才保存截图
        std::string img_dir = std::string(LOG_DIR_FIRE) + "/" + IMG_SUBDIR;
        // 拼接截图存储目录路径（../log/FireDetection/images）
        ensure_dir(img_dir);
        // 确保截图目录存在
        // 用时间戳命名，冒号/空格替换为横线/下划线（FAT32 兼容）
        std::string ts = rec.timestamp;
        // 将时间戳字符串复制到局部变量以便修改
        for (char& c : ts) {
        // 逐字符替换文件名中的非法字符
            if (c == ':') c = '-';
            // 将冒号（Windows/FAT32 文件名非法字符）替换为横线
            else if (c == ' ') c = '_';
            // 将空格替换为下划线（增强兼容性）
        }
        std::string img_path = img_dir + "/" + ts + ".jpg";
        // 拼接截图完整路径，如 "../log/FireDetection/images/2026-04-08_12-30-45.jpg"
        if (cv::imwrite(img_path, frame))
        // 使用 OpenCV 将帧图像以 JPEG 格式写入磁盘
            rec.image_path = img_path;
            // 截图保存成功：将路径写入记录的 image_path 字段
        else
            std::cerr << "[DisplayLog] 截图保存失败: " << img_path << std::endl;
            // 截图保存失败时打印错误（image_path 保持 record 中的原始值）
    }

    // 追加写 CSV
    std::string csv_path  = today_csv(pt);
    // 获取当日 CSV 文件的完整路径（如 "../log/FireDetection/2026-04-08.csv"）
    bool need_header = !fs::exists(csv_path);
    // 判断 CSV 文件是否为新建：若文件不存在则需要写入列名注释行
    std::ofstream ofs(csv_path, std::ios::app);
    // 以追加模式打开（或创建）CSV 文件
    if (!ofs.is_open()) {
    // 文件打开失败时返回错误码
        std::cerr << "[DisplayLog] 无法写入: " << csv_path << std::endl;
        // 打印错误信息（含文件路径便于排查权限或磁盘问题）
        return ErrorCode::FILE_WRITE_ERROR;
        // 返回文件写入错误码
    }
    if (need_header)
        ofs << "#timestamp,class_id,class_name,confidence,current_ma,"
               "image_path,error_reason\n";
        // 新文件写入以 '#' 开头的列名注释行（csv_to_record 会跳过此行）
    ofs << record_to_csv(rec) << "\n";
    // 将异常记录序列化为 CSV 行并写入文件，末尾换行
    ofs.flush();
    // 强制刷新缓冲区，确保数据立即写入磁盘
    std::cout << "[DisplayLog] 异常记录已保存: " << csv_path << std::endl;
    // 打印保存成功确认信息（含文件路径）
    return ErrorCode::SUCCESS;
    // 返回成功
}

// ============================================================================
// 读取近 7 天异常记录
// ============================================================================

ErrorCode DisplayLoggingFramework::load_week_abnormal(ProjectType pt,
// 参数 pt：项目类型（决定读取哪个日志目录）
                                                        std::vector<AbnormalRecord>& out) {
// 参数 out：输出容器，接收读取到的所有异常记录
    out.clear();
    // 清空输出容器，防止调用方传入有旧数据的容器
    std::string root = log_dir_for(pt);
    // 获取对应项目的日志根目录路径
    if (!fs::exists(root)) return ErrorCode::SUCCESS;
    // 日志目录不存在时直接返回空结果（首次运行或目录被删除时）

    std::vector<std::string> csvs;
    // 存储满足条件的 CSV 文件路径列表
    for (const auto& e : fs::directory_iterator(root)) {
    // 遍历日志根目录下的所有条目
        if (!e.is_regular_file() || e.path().extension() != ".csv") continue;
        // 跳过非普通文件和非 CSV 文件（如截图子目录）
        if (within_days(e.path().stem().string(), KEEP_DAYS))
        // 文件名（不含扩展名）即为日期字符串（如 "2026-04-08"）
        // within_days 检查该日期距今是否在 KEEP_DAYS 天以内
            csvs.push_back(e.path().string());
            // 将满足条件的 CSV 文件路径加入列表
    }
    std::sort(csvs.begin(), csvs.end());
    // 按文件名（日期）升序排列，保证读取记录的时间顺序

    for (const auto& p : csvs) {
    // 逐文件读取异常记录
        std::ifstream ifs(p);
        // 打开 CSV 文件
        if (!ifs.is_open()) continue;
        // 文件打开失败时跳过（权限问题或文件被删除）
        std::string line;
        // 临时存储当前行
        while (std::getline(ifs, line)) {
        // 逐行读取 CSV 文件
            AbnormalRecord r;
            // 声明临时记录结构体
            if (csv_to_record(line, pt, r)) out.push_back(r);
            // 解析成功时将记录追加到输出容器
        }
    }
    return ErrorCode::SUCCESS;
    // 所有文件读取完成，返回成功
}

// ============================================================================
// 导出近 7 天日志
// ============================================================================

ErrorCode DisplayLoggingFramework::export_week_logs(const std::string& export_dir) {
// 将近 KEEP_DAYS 天的日志文件和火焰截图复制到导出目录，并生成清单文件
    ensure_dir(export_dir);
    // 确保导出根目录存在
    int cnt_b = 0, cnt_f = 0;
    // 轴承异常记录总数和火焰异常记录总数（用于写入清单）

    // 轴承
    {
    // 内层作用域：处理轴承振动日志的导出
        std::string dst = export_dir + "/BearingVibration";
        // 导出目录下的轴承子目录路径
        ensure_dir(dst);
        // 确保轴承导出子目录存在
        if (fs::exists(LOG_DIR_BEARING)) {
        // 轴承日志源目录存在时才执行导出
            for (const auto& e : fs::directory_iterator(LOG_DIR_BEARING)) {
            // 遍历轴承日志目录下的所有条目
                if (!e.is_regular_file() || e.path().extension() != ".csv") continue;
                // 跳过非普通文件和非 CSV 文件
                if (!within_days(e.path().stem().string(), KEEP_DAYS)) continue;
                // 跳过超出保留窗口的过期文件
                copy_file(e.path().string(), dst + "/" + e.path().filename().string());
                // 将满足条件的 CSV 文件复制到导出子目录（保持原文件名）
                std::ifstream ifs(e.path().string());
                // 打开源 CSV 文件用于计数
                std::string ln;
                // 临时行字符串
                while (std::getline(ifs, ln))
                // 逐行读取
                    if (!ln.empty() && ln[0] != '#') ++cnt_b;
                    // 跳过空行和注释行，统计有效数据行数（即异常记录数）
            }
        }
    }

    // 火焰 + 截图
    {
    // 内层作用域：处理火焰检测日志和截图的导出
        std::string dst     = export_dir + "/FireDetection";
        // 导出目录下的火焰子目录路径
        std::string dst_img = dst + "/" + IMG_SUBDIR;
        // 导出目录下的火焰截图子目录路径
        ensure_dir(dst);
        // 确保火焰导出子目录存在
        ensure_dir(dst_img);
        // 确保截图导出子目录存在
        if (fs::exists(LOG_DIR_FIRE)) {
        // 火焰日志源目录存在时才执行导出
            for (const auto& e : fs::directory_iterator(LOG_DIR_FIRE)) {
            // 遍历火焰日志目录下的所有条目
                if (e.is_regular_file() && e.path().extension() == ".csv") {
                // 处理 CSV 文件（异常记录日志）
                    if (!within_days(e.path().stem().string(), KEEP_DAYS)) continue;
                    // 跳过超出保留窗口的过期 CSV 文件
                    copy_file(e.path().string(), dst + "/" + e.path().filename().string());
                    // 复制 CSV 文件到导出子目录
                    std::ifstream ifs(e.path().string());
                    // 打开 CSV 文件用于计数
                    std::string ln;
                    // 临时行字符串
                    while (std::getline(ifs, ln))
                    // 逐行读取
                        if (!ln.empty() && ln[0] != '#') ++cnt_f;
                        // 统计火焰异常记录的有效数据行数
                }
                if (e.is_directory() && e.path().filename() == IMG_SUBDIR) {
                // 处理截图子目录（images/）
                    for (const auto& img : fs::directory_iterator(e.path())) {
                    // 遍历截图目录下的所有图像文件
                        std::string fn = img.path().filename().string();
                        // 获取截图文件名（如 "2026-04-08_12-30-45.jpg"）
                        // 文件名前 10 位是日期
                        if (fn.size() >= 10 && within_days(fn.substr(0, 10), KEEP_DAYS))
                        // 截图文件名前 10 位为 "YYYY-MM-DD" 日期，检查是否在保留窗口内
                            copy_file(img.path().string(), dst_img + "/" + fn);
                            // 将满足条件的截图复制到导出截图子目录
                    }
                }
            }
        }
    }

    // manifest
    std::ofstream mf(export_dir + "/export_manifest.txt");
    // 创建导出清单文件（../export_dir/export_manifest.txt）
    if (mf.is_open()) {
    // 文件打开成功时写入清单内容
        mf << "Export Time : " << now_datetime() << "\n"
        // 写入导出执行时刻的时间戳
           << "Period      : last " << KEEP_DAYS << " days\n"
           // 写入导出的时间窗口（最近 N 天）
           << "Bearing     : " << cnt_b << " abnormal records\n"
           // 写入轴承异常记录总数
           << "Fire        : " << cnt_f << " abnormal records\n";
           // 写入火焰异常记录总数
    }
    std::cout << "[DisplayLog] 导出完成 -> " << export_dir
    // 打印导出完成确认信息及导出目录路径
              << " 轴承:" << cnt_b << " 火焰:" << cnt_f << std::endl;
              // 打印轴承和火焰各自的记录计数
    return ErrorCode::SUCCESS;
    // 导出完成，返回成功
}

// ============================================================================
// 清理超期日志
// ============================================================================

void DisplayLoggingFramework::purge_old_logs() {
// 删除超出 KEEP_DAYS 保留窗口的过期日志文件（CSV）和截图（JPEG）
    auto purge = [&](const std::string& root) {
    // lambda：清理指定根目录下的过期文件
    // 按引用捕获 this（通过 [&]），可访问成员函数 within_days
        if (!fs::exists(root)) return;
        // 目录不存在时直接返回（首次运行或已被删除）
        for (const auto& e : fs::directory_iterator(root)) {
        // 遍历日志根目录下的所有条目
            if (e.is_regular_file() && e.path().extension() == ".csv"
            // 当前条目是普通文件且扩展名为 .csv
                && !within_days(e.path().stem().string(), KEEP_DAYS)) {
                // 文件日期（stem 即不含扩展名的文件名）超出保留窗口
                fs::remove(e.path());
                // 删除过期 CSV 文件
                std::cout << "[DisplayLog] 清理过期: " << e.path() << std::endl;
                // 打印删除确认信息（含文件路径）
            }
        }
        std::string img_dir = root + "/" + IMG_SUBDIR;
        // 拼接截图子目录路径（如 ../log/FireDetection/images）
        if (fs::exists(img_dir)) {
        // 截图子目录存在时才执行截图清理
            for (const auto& img : fs::directory_iterator(img_dir)) {
            // 遍历截图目录下的所有文件
                std::string fn = img.path().filename().string();
                // 获取截图文件名
                if (fn.size() >= 10 && !within_days(fn.substr(0, 10), KEEP_DAYS))
                // 文件名前 10 位为日期，检查是否超出保留窗口
                    fs::remove(img.path());
                    // 删除过期截图文件（无打印，批量删除时避免日志泛滥）
            }
        }
    };
    purge(LOG_DIR_BEARING);
    // 清理轴承振动日志目录下的过期文件
    purge(LOG_DIR_FIRE);
    // 清理火焰检测日志目录下的过期文件和截图
}

} // namespace MultiProject
// 关闭命名空间 MultiProject