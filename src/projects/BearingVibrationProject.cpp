/**
  ******************************************************************************
  * @file    BearingVibrationProject.cpp
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
 * @brief   轴承振动监测项目 — 实现文件
 * ----------------------------------------------------------------------------
 * @details 本文件实现 BearingVibrationProject.hpp 中声明的全部方法，
 *          是整个轴承振动监测系统的顶层编排层，负责将五大框架组件
 *          （数据采集、数据处理、AI推理、显示日志、扩展功能）
 *          串联为完整的实时闭环检测流水线。
 *
 *  文件内容（按代码顺序）：
 *
 *  静态工具函数
 *    bearing_class_name — 将 class_id (0~3) 映射为中文故障类别名称
 *    current_datetime   — 格式化当前时间为 "YYYY-MM-DD HH:MM:SS"
 *    current_time_hhmm  — 格式化当前时间为 "HH:MM"（OLED 显示用）
 *
 *  辅助嵌套类（桩实现）
 *    VibrationAnalyzer        — 时域/频域特征提取（RMS、峰值）
 *    PredictiveMaintenance    — 剩余寿命预测、健康指数计算
 *    VibrationAlarmManager    — 振动等级评估、趋势报警
 *
 *  构造 / 析构
 *    BearingVibrationProject() — 统一用 make_unique 构造六大组件
 *    ~BearingVibrationProject() — 调用 stop()+shutdown() 安全清理
 *
 *  initialize
 *    数据采集初始化 → AI 模型加载（MODEL_PATH）→
 *    PWR_OFF 掉电回调注册 → OLED 菜单初始化 → 超期日志清理
 *
 *  start / stop / shutdown
 *    start   — 启动三条工作线程（采集/处理/日志）
 *    stop    — 停止三条线程（原子标志 + 队列停止 + join）
 *    shutdown — stop + 菜单/流/模型清理
 *
 *  acquisition_loop（采集线程）
 *    循环调用 acquire_data()，成功则推入 raw_queue_，失败则休眠 10 ms
 *    每次采集后额外休眠 500 ms（控制约 2 Hz 采集频率）
 *
 *  processing_loop（处理线程）
 *    从 raw_queue_ 阻塞弹出 → CWT 时频图生成 → AI 推理 →
 *    置信度阈值 + 报警延迟判断 → 4-20mA/RS485/继电器/LED 输出 →
 *    OLED 快照更新 → AbnormalRecord 持久化 → 结果推入 result_queue_
 *
 *  logging_loop（日志线程）
 *    从 result_queue_ 阻塞弹出 → 结构化日志写入 → OLED 结果显示
 *
 *  其他接口（桩/委托实现）
 *    calibrate_sensors、run_diagnostic_test、is_running、get_system_status
 *    get_vibration_history、update_config、get_config
 *    monitor_vibration、generate_report
 *    analyze_vibration_trend、predict_maintenance_schedule
 *    enableSimulation
 *
 * @note  processing_loop 中的置信度阈值和报警延迟均从
 *        ExtendedFeaturesFramework 实时读取（用户可通过 OLED 按键调整），
 *        实现了推理参数的运行时热更新；
 *        报警延迟通过 static 局部变量实现跨循环状态保持，
 *        仅在连续异常持续超过 delay 秒后才触发输出，
 *        防止单次抖动误报。
 * ============================================================================
 */

#include "BearingVibrationProject.hpp"  // 引入头文件（详见上方 // 注释）
// 引入轴承振动项目完整声明，含三个辅助嵌套类的前向声明
// 及所有成员变量（框架组件 unique_ptr、工作线程、ConcurrentQueue、
// 诊断计数器 diag_* 等）和方法签名

#include <chrono>  // 引入头文件（详见上方 // 注释）
// 引入 std::chrono：steady_clock（报警延迟的单调时间测量）
// 和 system_clock（predict_maintenance_schedule 的时间点计算）

#include <thread>  // 引入头文件（详见上方 // 注释）
// 引入 std::thread：acquisition_thread_ / processing_thread_ / logging_thread_
// 三条工作线程的创建和 join

#include <iostream>  // 引入头文件（详见上方 // 注释）
// 引入 std::cout / std::cerr：初始化进度信息、调试输出和错误信息打印

#include <ctime>  // 引入头文件（详见上方 // 注释）
// 引入 std::time / std::localtime / std::strftime：
// current_datetime() 和 current_time_hhmm() 中的时间格式化

#include <iomanip>  // 引入头文件（详见上方 // 注释）
// 引入 std::setw / std::setprecision 等 I/O 控制符（为字符串格式化预留）

#include <sstream>  // 引入头文件（详见上方 // 注释）
// 引入 std::ostringstream（为复杂字符串拼接预留，当前未直接使用）

namespace MultiProject {  // 命名空间
// 开启项目命名空间 MultiProject

// ── 轴承分类名称（与 class_id 对应）────────────────────────────
static const char* bearing_class_name(int id)  // 静态变量
// 静态工具函数：将 AI 推理输出的 class_id（0~3）映射为中文故障类别名称
// 返回值类型为 const char* 指向字符串字面量（static 存储期，无需释放）
{  // 块开始
    switch (id) {  // 分支选择
    // 根据类别 ID 返回对应的中文名称
    case 0: return "正常";       // Normal — 轴承运行正常，无故障
    case 1: return "滚动体故障"; // Ball Fault — 滚动体（滚珠/滚柱）表面剥落或裂纹
    case 2: return "保持架故障";   // Cage Fault — 保持架变形或破裂，导致滚动体分布不均
    case 3: return "外圈故障";   // Outer Race — 轴承外圈滚道出现点蚀或剥落
    default: return "未知";  // 代码行
    // 超出 0~3 范围的类别 ID（模型异常时的防御性处理）
    }  // 块结束
}  // 块结束

// ── 当前时间字符串 ─────────────────────────────────────────────
static std::string current_datetime() {  // 静态变量
// 静态工具函数：返回当前系统时间的完整字符串（"YYYY-MM-DD HH:MM:SS"）
// 用于 AbnormalRecord.timestamp 字段，便于后续日志查询和显示
    std::time_t t = std::time(nullptr);  // 代码行
    // 获取当前 Unix 时间戳（秒精度）
    char buf[24];  // 代码行
    // 声明足够容纳 "YYYY-MM-DD HH:MM:SS\0"（20字节）的缓冲区
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));  // 代码行
    // 将时间戳格式化为本地时间字符串，写入 buf；
    // sizeof(buf) 防止溢出；std::localtime 使用静态内部缓冲区（非线程安全，但单线程调用）
    return buf;  // 返回
    // 构造 std::string 并返回（从 char* 隐式转换）
}  // 块结束
static std::string current_time_hhmm() {  // 静态变量
// 静态工具函数：返回当前时间的 "HH:MM" 短格式字符串
// 用于 MenuSnapshot.result_timestamp 字段（OLED 四行布局仅有约8字符宽度）
    std::time_t t = std::time(nullptr);  // 代码行
    // 获取当前 Unix 时间戳
    char buf[8];  // 代码行
    // 声明足够容纳 "HH:MM\0"（6字节）的缓冲区
    std::strftime(buf, sizeof(buf), "%H:%M", std::localtime(&t));  // 代码行
    // 格式化为 24 小时制 "HH:MM"（如 "14:30"）
    return buf;  // 返回
    // 构造 std::string 并返回
}  // 块结束

// ============================================================================
// 辅助类占位实现
// ============================================================================

class BearingVibrationProject::VibrationAnalyzer {  // 代码行
// 振动分析器辅助类（BearingVibrationProject 的私有嵌套类）
// 当前为桩实现，提供时域特征提取（RMS、峰值）和频域分析接口
public:  // 代码行
    ErrorCode analyze_time_domain(const std::vector<float>& data, std::vector<float>& features) {  // 代码行
    // 时域特征提取：计算 RMS（均方根）和峰值（绝对值最大）
        if (data.empty()) return ErrorCode::SUCCESS;  // 条件判断
        // 空数据直接返回，不追加特征
        float rms = 0, peak = 0;  // 代码行
        // rms：平方和累积变量；peak：绝对值峰值
        for (float v : data) { rms += v * v; if (std::abs(v) > peak) peak = std::abs(v); }  // 循环
        // 逐样本计算：rms 累积 v²，peak 跟踪最大绝对值
        rms = std::sqrt(rms / data.size());  // 代码行
        // RMS = sqrt(Σv²/n)，量化振动有效值
        features.push_back(rms); features.push_back(peak);  // 代码行
        // 将 RMS 和峰值追加到特征向量
        return ErrorCode::SUCCESS;  // 返回
        // 特征提取完成
    }  // 块结束
    ErrorCode analyze_frequency_domain(const std::vector<float>&, std::vector<float>&) { return ErrorCode::SUCCESS; }  // 代码行
    // 频域特征提取（桩实现：FFT 谱分析、特征频率提取等功能待实现）
    ErrorCode detect_anomalies(const std::vector<float>&, std::vector<AnomalyDetection>&) { return ErrorCode::SUCCESS; }  // 代码行
    // 异常检测（桩实现：基于统计阈值的时域异常检测功能待实现）
};  // 类/结构体定义结束

class BearingVibrationProject::PredictiveMaintenance {  // 代码行
// 预测性维护辅助类：预测剩余使用寿命（RUL）和健康指数
// 当前为桩实现，生产版本可接入 LSTM/退化模型
public:  // 代码行
    ErrorCode update_model(const std::vector<DataLog>&) { return ErrorCode::SUCCESS; }  // 代码行
    // 更新预测模型（桩实现：用历史数据增量训练退化模型）
    ErrorCode predict_remaining_useful_life(float& rul) { rul = 1000; return ErrorCode::SUCCESS; }  // 代码行
    // 预测剩余使用寿命（桩实现：固定返回 1000 小时）
    ErrorCode calculate_health_index(float& health) { health = 0.9f; return ErrorCode::SUCCESS; }  // 代码行
    // 计算健康指数（桩实现：固定返回 0.9，范围 [0,1]，越低越差）
};  // 类/结构体定义结束

class BearingVibrationProject::VibrationAlarmManager {  // 代码行
// 振动报警管理器：根据振动特征评估报警等级和趋势
// 当前为桩实现，可扩展为基于 ISO 10816 标准的多级报警逻辑
public:  // 代码行
    ErrorCode evaluate_vibration_levels(const std::vector<float>&, std::string& alarm_level) {  // 代码行
    // 振动等级评估（桩实现：固定返回 "NORMAL"）
        alarm_level = "NORMAL"; return ErrorCode::SUCCESS;  // 代码行
        // 生产版本应对比 ISO 10816 的 A/B/C/D 四级振动速度阈值
    }  // 块结束
    ErrorCode check_trend_alarms(const std::vector<DataLog>&) { return ErrorCode::SUCCESS; }  // 代码行
    // 趋势报警检查（桩实现：基于历史数据的振动趋势分析待实现）
};  // 类/结构体定义结束

// ============================================================================
// 构造 / 析构
// ============================================================================

BearingVibrationProject::BearingVibrationProject()  // 代码行
// 构造函数：通过成员初始化列表统一用 make_unique 在堆上构造全部六大组件
// 使用 make_unique 而非裸 new 确保异常安全（构造失败时自动释放已分配内存）
    : data_acquisition_(std::make_unique<DataAcquisitionFramework>()),  // 代码行
    // 数据采集框架：负责 AD4134 SPI ADC 三轴振动数据的实时采集
      data_processing_(std::make_unique<DataProcessingFramework>()),  // 代码行
    // 数据处理框架：负责对三轴时域信号执行 CWT 并生成 224×224 时频图
      ai_model_(std::make_unique<AIModelFramework>(ProjectType::BEARING_VIBRATION)),  // 代码行
    // AI 模型框架：以 BEARING_VIBRATION 类型构造，加载四分类 ONNX 模型
      display_logger_(std::make_unique<DisplayLoggingFramework>()),  // 代码行
    // 显示与日志框架：负责结构化日志写入、AbnormalRecord 持久化和周报查询
      extended_features_(std::make_unique<ExtendedFeaturesFramework>()),  // 代码行
    // 扩展功能框架：负责 PWM 电流环/RS485/继电器/LED/OLED 菜单/按键
      vibration_analyzer_(std::make_unique<VibrationAnalyzer>()),  // 代码行
    // 振动分析器辅助类：时域/频域特征提取
      predictive_maintenance_(std::make_unique<PredictiveMaintenance>()),  // 代码行
    // 预测性维护辅助类：RUL 和健康指数
      alarm_manager_(std::make_unique<VibrationAlarmManager>()) {}  // 代码行
    // 振动报警管理器辅助类：报警等级评估和趋势分析

BearingVibrationProject::~BearingVibrationProject() {  // 代码行
// 析构函数：按顺序安全清理所有资源
    stop();  // 代码行
    // 先调用 stop()：置位 running_=false，向两个队列发送停止信号，
    // 等待三条工作线程全部 join（确保线程安全退出后再销毁组件）
    shutdown();  // 代码行
    // 再调用 shutdown()：内部再次调用 stop()（幂等），
    // 然后依次关闭 OLED 菜单、停止数据流和卸载 AI 模型
}  // 块结束

// ============================================================================
// initialize
// ============================================================================

ErrorCode BearingVibrationProject::initialize(const std::string& config_path) {  // 代码行
// 系统初始化：按顺序初始化数据采集、AI模型、断电保护、OLED菜单和日志清理
    (void)config_path;  // 代码行
    // (void) 消除未使用参数的编译器警告；配置文件路径功能保留供未来扩展
    std::cout << "Initializing Bearing Vibration Project" << std::endl;  // 打印信息
    // 打印初始化开始信息

    auto status = data_acquisition_->initialize();  // 变量声明
    // 初始化数据采集框架：配置 GPIO 引脚 → 打开 SPI 设备文件
    // → 写入 AD4134 寄存器（6 个寄存器 + 读回校验）
    if (status != ErrorCode::SUCCESS) return status;  // 条件判断
    // 采集初始化失败时提前返回（SPI 设备不存在或 GPIO 权限不足）

    std::cout << "Loading AI model from: " << MODEL_PATH << std::endl;  // 打印信息
    // 打印模型路径（MODEL_PATH 在 bearing_vibration_config.hpp 中定义，
    // 通常为 "../models/bearing_model_gray.onnx"）
    status = ai_model_->load_model(MODEL_PATH);  // 代码行
    // 加载 ONNX 模型：创建 ONNX Runtime 会话 → 配置 XNNPACK 执行提供者
    // → 分配输入/输出张量内存
    if (status != ErrorCode::SUCCESS) {  // 条件判断
    // 模型加载失败（文件不存在、格式错误或内存不足）
        std::cerr << "模型加载失败: " << MODEL_PATH << std::endl;  // 打印错误
        // 打印具体失败路径，便于诊断文件路径配置问题
        return status;  // 返回
        // 向上层传播错误码
    }  // 块结束
    std::cout << "模型加载成功" << std::endl;  // 打印信息
    // 打印模型加载成功确认

    // 注册 PWR_OFF 掉电回调
    extended_features_->set_shutdown_callback([this]() {  // 代码行
    // 注册断电安全关机回调：当 BCM4 检测到断电信号时，
    // pigpio alert 回调会调用此 lambda，安全停止所有线程
        std::cerr << "[PWR_OFF] 轴承振动项目：停止所有线程..." << std::endl;  // 打印错误
        // 打印断电警告到 stderr（不缓冲，断电场景下更可靠）
        running_ = false;  // 代码行
        // 原子置位 running_=false：通知采集和处理线程退出主循环
        raw_queue_.stop();  // 代码行
        // 停止原始数据队列：唤醒阻塞在 raw_queue_.pop() 的处理线程
        result_queue_.stop();  // 代码行
        // 停止结果队列：唤醒阻塞在 result_queue_.pop() 的日志线程
    });  // 代码行
    extended_features_->initialize_pwr_off();  // 代码行
    // 配置 BCM4 为输入并注册 pigpio alert 回调（监听断电信号上升沿）

    // ── 启动 OLED 菜单 ────────────────────────────────────────
    if (extended_features_->initialize_menu(display_logger_.get()) != ErrorCode::SUCCESS) {  // 条件判断
    // 初始化 OLED 菜单系统（配置三键 GPIO + 启动 100ms 刷新线程）
    // 传入 display_logger_.get() 供日志页面调用 load_week_abnormal()
        std::cerr << "[Menu] OLED 菜单初始化失败（可忽略，不影响主功能）" << std::endl;  // 打印错误
        // 菜单初始化失败不是致命错误（可能在无 OLED 硬件的开发机上运行）
        // 故意不 return，让系统继续初始化主采集推理流水线
    }  // 块结束

    // 清理超期日志（启动时执行一次）
    display_logger_->purge_old_logs();  // 代码行
    // 删除超过保留期限（通常 7 天）的旧日志文件，防止存储空间耗尽

    initialized_ = true;  // 代码行
    // 标记系统已成功初始化，start() 中检查此标志
    return ErrorCode::SUCCESS;  // 返回
    // 初始化全部完成
}  // 块结束

// ============================================================================
// start / stop / shutdown
// ============================================================================

ErrorCode BearingVibrationProject::start() {  // 代码行
// 启动三条工作线程（必须在 initialize() 成功后调用）
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;  // 条件判断
    // 前置检查：未初始化时拒绝启动（防止在资源未就绪时访问 nullptr 组件）
    if (running_) return ErrorCode::ALREADY_INITIALIZED;  // 条件判断
    // 防止重复启动：running_=true 说明线程已在运行
    running_ = true;  // 代码行
    // 设置运行标志（工作线程的主循环检查此标志）
    acquisition_thread_ = std::thread(&BearingVibrationProject::acquisition_loop, this);  // 代码行
    // 启动采集线程：绑定 acquisition_loop 成员函数，传入 this 指针
    processing_thread_  = std::thread(&BearingVibrationProject::processing_loop, this);  // 代码行
    // 启动处理线程：绑定 processing_loop，执行 CWT + AI 推理 + 输出
    logging_thread_     = std::thread(&BearingVibrationProject::logging_loop, this);  // 代码行
    // 启动日志线程：绑定 logging_loop，写入结构化日志并更新 OLED 显示
    return ErrorCode::SUCCESS;  // 返回
    // 三条线程已启动
}  // 块结束

ErrorCode BearingVibrationProject::stop() {  // 代码行
// 停止三条工作线程（幂等，可多次调用）
    running_ = false;  // 代码行
    // 通知所有工作线程退出主循环（每个 while(running_) 检查此标志）
    raw_queue_.stop();  // 代码行
    // 停止原始数据队列：向 ConcurrentQueue 发送停止信号，
    // 使阻塞在 pop() 的处理线程立即返回 false 并退出
    result_queue_.stop();  // 代码行
    // 停止结果队列：使阻塞在 pop() 的日志线程立即返回 false 并退出
    if (acquisition_thread_.joinable()) acquisition_thread_.join();  // 条件判断
    // 等待采集线程完全退出（joinable() 防止对未启动/已 join 线程调用 join）
    if (processing_thread_.joinable())  processing_thread_.join();  // 条件判断
    // 等待处理线程完全退出
    if (logging_thread_.joinable())     logging_thread_.join();  // 条件判断
    // 等待日志线程完全退出
    return ErrorCode::SUCCESS;  // 返回
    // 所有线程已安全停止
}  // 块结束

ErrorCode BearingVibrationProject::shutdown() {  // 代码行
// 完整关闭系统：停止线程 + 清理所有外设资源
    stop();  // 代码行
    // 先调用 stop() 确保所有工作线程已停止（幂等，即使已调用过也安全）
    extended_features_->shutdown_menu();  // 代码行
    // 停止 OLED 菜单刷新线程，注销三键 pigpio alert 回调
    data_acquisition_->stop_streaming();  // 代码行
    // 标记数据采集流停止（streaming_=false）
    ai_model_->unload_model();  // 代码行
    // 释放 ONNX Runtime 会话和推理张量内存
    initialized_ = false;  // 代码行
    // 重置初始化标志，允许重新调用 initialize() 进行热重启
    return ErrorCode::SUCCESS;  // 返回
    // 关闭完成
}  // 块结束

// ============================================================================
// acquisition_loop
// ============================================================================

void BearingVibrationProject::acquisition_loop() {  // 代码行
// 数据采集线程主函数：循环从 AD4134 读取三轴振动数据并推入 raw_queue_
    data_acquisition_->start_streaming();  // 代码行
    // 标记采集流开始（streaming_=true），采集框架的内部状态更新
    while (running_) {  // 循环
    // 主采集循环：running_=false 时（stop() 调用后）退出
        ++diag_acq_loop_iter_;  // 代码行
        // 诊断计数器：记录采集循环总迭代次数（用于 run_diagnostic_test 和心跳日志）
        RawData raw;  // 代码行
        // 声明原始数据容器（包含三轴 5000 点时域数据和元数据）
        if (data_acquisition_->acquire_data(raw) == ErrorCode::SUCCESS) {  // 条件判断
        // 从 AD4134 读取一批三轴数据（5000 点 @ 50 kSPS = 100 ms 时域窗口）
            ++diag_acq_got_data_;  // 代码行
            // 诊断计数器：采集成功次数
            raw_queue_.push(raw);  // 代码行
            // 将原始数据推入线程安全队列，处理线程阻塞等待此数据
            ++diag_acq_pushed_;  // 代码行
            // 诊断计数器：成功推入队列次数
        } else {  // else 分支
            std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 代码行
            // 采集失败（AD4134 ODR 超时或 SPI 错误）时休眠 10 ms 后重试
            continue;  // 跳过本次循环
            // 跳过本次迭代的休眠，立即重试采集
        }  // 块结束
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 代码行
        // 每次成功采集后休眠 500 ms，控制约 2 Hz 的推理频率
        // （AD4134 @ 50 kSPS 可连续采集，此处限速防止 CPU 满载和 OLED 刷新过快）
    }  // 块结束
}  // 块结束

// ============================================================================
// processing_loop
// 新增：
//   1. 推理成功后更新菜单快照（update_menu_snapshot）
//   2. 异常结果（class_id != 0）保存 AbnormalRecord
//   3. 推理失败时快照状态标为 FAILED
// ============================================================================

void BearingVibrationProject::processing_loop() {  // 代码行
// 数据处理线程主函数：CWT → AI推理 → 阈值判断 → 输出 → 快照更新 → 日志
    std::cout << "【调试】processing_loop 已启动" << std::endl;  // 打印信息
    // 打印线程启动确认信息（调试用）
    extended_features_->output_standby();  // 代码行
    // 处理线程就绪后立即输出 4 mA 待机电流，通知上位机系统已就绪待推理

    MenuSnapshot snap;  // 代码行
    // 声明 OLED 菜单快照对象（将随每次推理结果更新）
    snap.run_state = MenuSnapshot::RunState::RUNNING;  // 代码行
    // 初始状态设为 RUNNING（推理尚未完成第一次）
    extended_features_->update_menu_snapshot(snap);  // 代码行
    // 立即推送初始快照，使 OLED 第一帧显示 "Running..."

    while (running_) {  // 循环
    // 主处理循环
        ++diag_proc_loop_iter_;  // 代码行
        // 诊断计数器：处理循环总迭代次数

        RawData raw;  // 代码行
        // 声明原始数据容器
        if (!raw_queue_.pop(raw)) break;   // 队列停止时退出
        // 阻塞等待直到 raw_queue_ 有数据可弹出
        // 若 raw_queue_.stop() 已调用（stop() 时），pop 返回 false，退出循环
        ++diag_proc_got_raw_;  // 代码行
        // 诊断计数器：从队列成功取出数据次数

        // ── CWT 处理 ──────────────────────────────────────────────
        ProcessedData processed;  // 代码行
        // 声明处理结果容器（将包含 224×224 的 CWT 时频图）
        auto status = data_processing_->process_data(raw, processed);  // 变量声明
        // 对三轴振动数据执行完整处理流水线：
        // 分轴切分 → 三路 CWT（预加重→积分小波→FFT卷积→差分→log1p→CLAHE）
        // → 融合前增强 → CCWF 互相关加权融合 → 融合后处理 → 输出 224×224 图像
        if (status != ErrorCode::SUCCESS) {  // 条件判断
        // CWT 处理失败（通常是信号长度不足或 FFTW3 内存分配失败）
            std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 代码行
            // 短暂休眠后重试
            continue;  // 跳过本次循环
            // 跳过本次推理，继续等待下一批数据
        }  // 块结束
        if (processed.processed_image.empty()) continue;  // 条件判断
        // 处理图像为空时跳过（防御性检查，确保后续 AI 推理不接收空输入）

        double imgMin, imgMax;  // 代码行
        // 声明图像像素最小值和最大值
        cv::minMaxLoc(processed.processed_image, &imgMin, &imgMax);  // 代码行
        // 查找 CWT 时频图的全局最小值和最大值（检测是否为全黑/异常图像）
        if (imgMax == 0) continue;  // 条件判断
        // 图像最大值为 0（全黑图像）：可能是信号全零或 CWT 计算异常，跳过推理
        ++diag_proc_cwt_done_;  // 代码行
        // 诊断计数器：CWT 时频图生成成功次数

        // ── AI 推理 ───────────────────────────────────────────────
        ModelInput input;  // 代码行
        // 声明模型输入容器
        input.input_images.push_back(processed.processed_image);  // 代码行
        // 将 224×224 CWT 时频图（CV_8U 灰度图）加入输入图像列表
        ModelOutput output;  // 代码行
        // 声明模型输出容器（将包含 class_id 和 confidence）
        ++diag_proc_infer_start_;  // 代码行
        // 诊断计数器：推理启动次数
        status = ai_model_->infer(input, output);  // 代码行
        // 执行 ONNX Runtime 推理：前处理（灰度归一化）→ 推理 → 后处理（Softmax）
        ++diag_proc_infer_done_;  // 代码行
        // 诊断计数器：推理完成次数（含成功和失败）

        if (status == ErrorCode::SUCCESS) {  // 条件判断
        // 推理成功分支
            std::cout << "【调试】推理成功 class="  // 打印信息
                      << output.classification.class_id  // 代码行
                      << " conf=" << output.classification.confidence << std::endl;  // 代码行
            // 打印推理结果（class_id 和置信度），便于调试验证

            int   cid  = output.classification.class_id;  // 代码行
            // 提取类别 ID（0=正常/1=滚动体/2=保持架/3=外圈）
            float conf = output.classification.confidence;  // 代码行
            // 提取置信度（Softmax 最大值，范围 [0,1]）

            const float thresh = extended_features_->get_conf_threshold();  // 常量声明
            // 从 ExtendedFeaturesFramework 实时读取用户设定的置信度阈值
            // （用户可通过 OLED 按键在 EDIT 模式下调整，运行时热更新）
            const int   delay  = extended_features_->get_alarm_delay();  // 常量声明
            // 从 ExtendedFeaturesFramework 实时读取报警延迟秒数（防抖）
            bool is_anomaly = (cid != 0) && (conf >= thresh);  // 代码行
            // 初步异常判断：类别非正常（cid!=0）且置信度超过阈值

            static std::chrono::steady_clock::time_point first_anomaly_tp{};  // 静态变量
            // static 局部变量：记录首次检测到异常的时间点（跨循环保持状态）
            static bool in_alarm_delay = false;  // 静态变量
            // static 局部变量：标记当前是否处于报警延迟等待状态

            if (!is_anomaly) {  // 条件判断
            // 本次推理结果为正常（无异常）
                in_alarm_delay   = false;  // 代码行
                // 清除报警延迟状态（连续正常打断异常序列）
                first_anomaly_tp = {};  // 代码行
                // 重置首次异常时间点（零值表示未记录）
            } else if (delay > 0) {  // 代码行
            // 本次为异常且用户设置了报警延迟（delay > 0 秒）
                auto now_tp = std::chrono::steady_clock::now();  // 变量声明
                // 获取当前单调时钟时间点
                if (!in_alarm_delay) {  // 条件判断
                // 尚未开始报警延迟计时（首次检测到此段连续异常）
                    in_alarm_delay   = true;  // 代码行
                    // 标记进入报警延迟等待
                    first_anomaly_tp = now_tp;  // 代码行
                    // 记录连续异常序列的起始时间点
                }  // 块结束
                auto elapsed_s = std::chrono::duration_cast<std::chrono::seconds>(  // 变量声明
                    now_tp - first_anomaly_tp).count();  // 代码行
                // 计算从首次异常到现在的连续异常持续秒数
                if (elapsed_s < delay) {  // 条件判断
                // 持续时间尚未达到报警延迟阈值
                    is_anomaly = false;  // 代码行
                    // 暂时抑制报警输出（等待连续异常时间超过 delay 秒）
                    std::cout << "【阈值】报警延迟中 " << elapsed_s  // 打印信息
                              << "/" << delay << "s  (conf=" << conf << ")" << std::endl;  // 代码行
                    // 打印延迟等待进度（当前秒/总延迟秒和当前置信度）
                }  // 块结束
            }  // 块结束

            // ── 输出 4-20mA / RS485 ──────────────────────────────
            if (!is_anomaly) {  // 条件判断
            // 最终判断为正常（或处于延迟等待中）
                ModelOutput normal_output = output;  // 代码行
                // 复制推理输出（深拷贝，避免修改原始 output）
                normal_output.classification.class_id  = 0;  // 代码行
                // 强制将类别 ID 覆盖为 0（正常），无论实际推理结果
                normal_output.classification.confidence = conf;  // 代码行
                // 保留原始置信度（供 4-20mA 电流计算参考）
                extended_features_->output_ai_result_current(normal_output);  // 代码行
                // 以正常状态输出：PWM=11mA（denormal 满置信度），继电器断开，LED 正常灯
            } else {  // else 分支
                extended_features_->output_ai_result_current(output);  // 代码行
                // 以实际推理结果输出：PWM 电流按故障类别计算，继电器闭合，LED+蜂鸣器激活
            }  // 块结束
            ++diag_proc_output_;  // 代码行
            // 诊断计数器：输出操作次数

            // ── 菜单快照 ──────────────────────────────────────────
            snap.run_state          = MenuSnapshot::RunState::SUCCESS;  // 代码行
            // 更新快照运行状态为推理成功
            snap.fail_reason        = "";  // 代码行
            // 清空失败原因（本次成功）
            snap.result_label       = bearing_class_name(cid);  // 代码行
            // 更新类别标签（中文名称，如 "外圈故障"）
            snap.result_confidence  = conf;  // 代码行
            // 更新置信度（用于 OLED 百分比显示）
            snap.result_timestamp   = current_time_hhmm();  // 代码行
            // 更新推理完成时间（"HH:MM" 格式）
            snap.relay_bearing      = is_anomaly;  // 代码行
            // 更新轴承继电器状态（报警延迟通过后为 true）
            snap.relay_fire         = false;  // 代码行
            // 轴承项目不使用火焰继电器，固定为 false
            const float MA_TABLE[4] = {11.0f, 14.0f, 17.0f, 20.0f};  // 常量声明
            // 四个类别对应的额定电流值（正常 11 mA，滚珠 14 mA，保持架 17 mA，外圈 20 mA）
            snap.current_ma = is_anomaly && (cid >= 0 && cid < 4)  // 代码行
                ? 8.0f + (MA_TABLE[cid] - 8.0f) * conf  // 代码行
                // 异常时电流 = 8 mA + (额定-8) × 置信度（与 output_ai_result_current 公式一致）
                : 11.0f;  // 代码行
                // 正常或延迟等待时显示 denormal 满置信度电流（11 mA）
            extended_features_->update_menu_snapshot(snap);  // 代码行
            // 将更新后的快照推送给 OLED 菜单刷新线程（线程安全，内部加锁）
            ++diag_proc_snapshot_;  // 代码行
            // 诊断计数器：快照更新次数

            // ── 异常记录 ──────────────────────────────────────────
            if (is_anomaly) {  // 条件判断
            // 仅在通过报警延迟判断后才持久化异常记录
                AbnormalRecord rec;  // 代码行
                // 声明异常记录对象
                rec.timestamp    = current_datetime();  // 代码行
                // 记录发生时间（"YYYY-MM-DD HH:MM:SS"）
                rec.project_type = ProjectType::BEARING_VIBRATION;  // 代码行
                // 记录项目类型（便于日志页面分类查询）
                rec.class_id     = cid;  // 代码行
                // 记录故障类别 ID（1~3）
                rec.class_name   = bearing_class_name(cid);  // 代码行
                // 记录中文故障名称（如 "外圈故障"）
                rec.confidence   = conf;  // 代码行
                // 记录推理置信度
                rec.current_ma   = snap.current_ma;  // 代码行
                // 记录输出电流（与 OLED 显示一致）
                rec.image_path   = "";  // 代码行
                // 轴承项目不保存时频图文件（可扩展为保存 PNG 路径）
                display_logger_->save_abnormal_record(rec);  // 代码行
                // 将异常记录追加写入持久化日志文件
                ++diag_proc_save_rec_;  // 代码行
                // 诊断计数器：异常记录保存次数
            }  // 块结束

            // ── 结果推入日志队列 ──────────────────────────────────
            DataLog log;  // 代码行
            // 声明结构化日志对象
            log.timestamp    = std::to_string(raw.timestamp);  // 代码行
            // 记录原始数据的 Unix 毫秒时间戳（转为字符串）
            log.project_type = ProjectType::BEARING_VIBRATION;  // 代码行
            // 记录项目类型
            log.ai_results   = output;  // 代码行
            // 记录完整 AI 推理结果（含 class_id、confidence 和原始 logits）
            result_queue_.push(log);  // 代码行
            // 将日志对象推入结果队列，由日志线程异步处理
            ++diag_proc_pushed_;  // 代码行
            // 诊断计数器：结果成功推入队列次数

        } else {  // else 分支
        // 推理失败分支
            std::cerr << "【调试】推理失败 ErrorCode=" << static_cast<int>(status) << std::endl;  // 打印错误
            // 打印推理失败的错误码（便于区分 ORT 运行时错误类型）
            extended_features_->output_infer_failed();  // 代码行
            // 输出推理失败状态：PWM=5 mA，所有继电器断开，LED 全灭

            snap.run_state   = MenuSnapshot::RunState::FAILED;  // 代码行
            // 更新快照状态为推理失败
            snap.fail_reason = "推理失败";  // 代码行
            // 写入失败原因字符串（OLED Project Status 页第2行显示）
            snap.current_ma  = 5.0f;  // 代码行
            // 更新显示电流为推理失败对应值（5 mA）
            extended_features_->update_menu_snapshot(snap);  // 代码行
            // 推送失败状态快照给 OLED 菜单
        }  // 块结束
    }  // 块结束
}  // 块结束

// ============================================================================
// logging_loop
// ============================================================================

void BearingVibrationProject::logging_loop() {  // 代码行
// 日志记录线程主函数：从 result_queue_ 阻塞弹出推理结果并写入日志和更新 OLED
    std::cout << "【调试】logging_loop 已启动" << std::endl;  // 打印信息
    // 打印日志线程启动确认（调试用）
    while (running_) {  // 循环
    // 主日志循环
        ++diag_log_loop_iter_;  // 代码行
        // 诊断计数器：日志循环总迭代次数
        DataLog log;  // 代码行
        // 声明结构化日志容器
        if (!result_queue_.pop(log)) break;  // 条件判断
        // 阻塞等待直到 result_queue_ 有数据（result_queue_.stop() 后返回 false 退出）
        display_logger_->log_data(log);  // 代码行
        // 将结构化日志写入持久化文件（CSV/JSON 格式，含时间戳、类别、置信度等）
        display_logger_->display_results(log.ai_results);  // 代码行
        // 通过 OLED 或 stdout 显示最新推理结果（标签 + 置信度）
    }  // 块结束
}  // 块结束

// ============================================================================
// 其他接口
// ============================================================================

ErrorCode BearingVibrationProject::calibrate_sensors() {  // 代码行
// 传感器校准接口：委托给数据采集框架执行 AD4134 校准
    return data_acquisition_->calibrate_ad4134();  // 返回
    // 调用 AD4134 校准（当前为桩实现，完整版须在静止条件下采集零偏后写入寄存器）
}  // 块结束

ErrorCode BearingVibrationProject::run_diagnostic_test() {  // 代码行
// 运行硬件诊断测试：查询传感器连接状态并打印
    auto status = data_acquisition_->diagnose();  // 变量声明
    // 获取硬件状态（检查 AD4134 是否已初始化/连接）
    std::cout << "Hardware: sensors=" << status.sensors_healthy << std::endl;  // 打印信息
    // 打印传感器健康状态（true=正常，false=异常）
    return ErrorCode::SUCCESS;  // 返回
    // 返回成功（诊断本身不视为失败操作）
}  // 块结束

bool BearingVibrationProject::is_running() const { return running_; }  // 代码行
// 查询系统是否正在运行（running_ 为 std::atomic<bool>，线程安全读取）

SystemStatus BearingVibrationProject::get_system_status() const {  // 代码行
// 线程安全地获取当前系统状态快照
    std::lock_guard<std::mutex> lock(status_mutex_);  // 代码行
    // 加锁保护 current_status_ 的读取（防止其他线程同时写入导致数据竞争）
    return current_status_;  // 返回
    // 返回系统状态副本（值语义，调用方持有独立副本）
}  // 块结束

std::vector<DataLog> BearingVibrationProject::get_vibration_history(int /*hours*/) { return {}; }  // 代码行
// 获取历史振动日志（桩实现：返回空列表，完整版须从日志文件按时间范围查询）
ErrorCode BearingVibrationProject::update_config(const std::string&) { return ErrorCode::SUCCESS; }  // 代码行
// 更新配置（桩实现：热配置更新功能待实现）
std::string BearingVibrationProject::get_config() const { return "{}"; }  // 代码行
// 获取当前配置（桩实现：返回空 JSON，完整版须序列化当前配置对象）
ErrorCode BearingVibrationProject::monitor_vibration(bool) { return ErrorCode::SUCCESS; }  // 代码行
// 振动监控开关（桩实现：振动等级实时监控功能待实现）
ErrorCode BearingVibrationProject::generate_report(const std::string&) { return ErrorCode::SUCCESS; }  // 代码行
// 生成诊断报告（桩实现：周期性健康报告生成功能待实现）

ErrorCode BearingVibrationProject::analyze_vibration_trend(const std::vector<DataLog>&,  // 代码行
                                                           std::string& trend) {  // 代码行
// 振动趋势分析（桩实现：基于历史日志序列判断趋势方向）
    trend = "Stable trend"; return ErrorCode::SUCCESS;  // 代码行
    // 固定返回 "Stable trend"，完整版须对振动特征序列执行趋势检测算法
}  // 块结束
ErrorCode BearingVibrationProject::predict_maintenance_schedule(const std::vector<DataLog>&,  // 代码行
                                                                std::chrono::system_clock::time_point& tp) {  // 代码行
// 预测下次维护时间点（桩实现：基于 RUL 估计下次维护日期）
    tp = std::chrono::system_clock::now() + std::chrono::hours(24 * 30);  // 代码行
    // 固定返回当前时间 + 30 天（占位返回值，完整版须接入 PredictiveMaintenance 模型）
    return ErrorCode::SUCCESS;  // 返回
    // 返回成功
}  // 块结束

void BearingVibrationProject::enableSimulation(bool enable, const std::string& dataPath) {  // 代码行
// 启用或禁用模拟模式（委托给数据采集框架）
    if (data_acquisition_) data_acquisition_->enableSimulation(enable, dataPath);  // 条件判断
    // 若采集框架已创建，设置模拟模式：enable=true 时从 dataPath 指定的 CSV 读取数据，
    // enable=false 时回到真实 AD4134 硬件采集模式
}  // 块结束

} // namespace MultiProject
// 关闭命名空间 MultiProject