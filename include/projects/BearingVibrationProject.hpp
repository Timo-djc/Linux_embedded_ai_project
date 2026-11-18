/**
  ******************************************************************************
  * @file    BearingVibrationProject.hpp
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
 * @brief   轴承振动监测项目 — 顶层项目编排类头文件
 * ----------------------------------------------------------------------------
 * @details 本文件定义了 MultiProject 框架中轴承振动监测子项目的顶层编排类
 *          BearingVibrationProject，负责将五个子框架（数据采集、数据处理、
 *          AI 推理、显示日志、扩展功能）组装为完整的端到端监测流水线，
 *          并通过三条独立工作线程实现生产者/消费者并发架构。
 *
 *          系统架构（数据流）：
 *
 *            [AD4134 SPI ADC] ──采集线程──▶ raw_queue_ ──处理线程──▶
 *            CWT时频图生成 ──▶ ONNX推理 ──▶ result_queue_ ──日志线程──▶
 *            OLED显示 / LED更新 / 异常记录CSV / RS485电流输出 / 继电器动作
 *
 *          三条工作线程职责：
 *            acquisition_thread_ → acquisition_loop()
 *              驱动 DataAcquisitionFramework 以 50 kSPS 速率连续读取三轴
 *              振动数据，打包为 RawData 推入 raw_queue_
 *            processing_thread_ → processing_loop()
 *              从 raw_queue_ 取出 RawData，经 DataProcessingFramework 生成
 *              CWT 时频图，调用 AIModelFramework 执行 ONNX 推理，
 *              将 DataLog 结果推入 result_queue_，同步更新菜单快照
 *            logging_thread_ → logging_loop()
 *              从 result_queue_ 取出 DataLog，驱动 DisplayLoggingFramework
 *              更新 OLED、ExtendedFeaturesFramework 更新 LED/继电器/RS485
 *
 *          对外公开接口按职能分为三组：
 *            生命周期管理：initialize / start / stop / shutdown
 *            业务功能：monitor_vibration / calibrate_sensors /
 *                      run_diagnostic_test / generate_report /
 *                      analyze_vibration_trend / predict_maintenance_schedule
 *            状态查询与配置：is_running / get_system_status /
 *                            get_vibration_history / update_config / get_config /
 *                            enableSimulation
 *            诊断接口：raw_queue_size / result_queue_size /
 *                      diag_* 原子计数器（供 main.cpp 心跳打印）
 *
 * @note    本文件使用 #pragma once 防止重复包含。
 *          AnomalyDetection 辅助结构体定义于同一文件，
 *          供 VibrationAnalyzer 等内部子类使用。
 *          BearingVibrationProject 不继承 IFramework，
 *          可视为顶层应用层的项目编排对象（Orchestrator），
 *          与 IFramework 接口层并行存在。
 * ============================================================================
 */

#pragma once
// 替代传统 #ifndef 头文件保护，保证本文件在同一编译单元中只被展开一次

#include "../common/UnifiedDataTypes.hpp"
// 引入统一数据类型头文件，提供：
//   RawData       — raw_queue_ 的元素类型（原始三轴振动数据包）
//   DataLog       — result_queue_ 的元素类型（推理结果结构化记录）
//   SystemStatus  — current_status_ 和 get_system_status() 的数据类型
//   ErrorCode     — 各公共方法的返回类型
//   MenuSnapshot  — processing_loop() 向 ExtendedFeaturesFramework 推送的快照类型

#include "../common/ConcurrentQueue.hpp"
// 引入线程安全并发队列模板，实例化为：
//   ConcurrentQueue<RawData>  → raw_queue_（采集线程→处理线程的数据通道）
//   ConcurrentQueue<DataLog>  → result_queue_（处理线程→日志线程的数据通道）

#include "../frameworks/DataAcquisitionFramework.hpp"
// 引入数据采集框架，提供 DataAcquisitionFramework 类；
// 负责通过 SPI 驱动 AD4134 ADC 以 50 kSPS 采集三轴振动加速度数据

#include "../frameworks/DataProcessingFramework.hpp"   // 必须包含
// 引入数据处理框架，提供 DataProcessingFramework 类；
// 注释"必须包含"说明此头文件在早期版本中曾被遗漏，
// 导致 processing_loop() 中 compute_cwt_image() 调用链接失败；
// 负责 CWT 时频图生成（cmor1.5-1 小波，128 尺度，FFTW3 加速）和
// 振动特征提取（时域/频域/统计特征）

#include "../frameworks/AIModelFramework.hpp"
// 引入 AI 推理框架，提供 AIModelFramework 类；
// 负责加载 bearing_vibration_model.onnx 并对 224×224 灰度 CWT 图像执行
// 四分类推理（denormal/deballfault/decagefault/deouterrace）

#include "../frameworks/DisplayLoggingFramework.hpp"
// 引入显示日志框架，提供 DisplayLoggingFramework 类；
// 负责 OLED 状态显示、运行日志写入和异常记录 CSV 持久化

#include "../frameworks/ExtendedFeaturesFramework.hpp"
// 引入扩展功能框架，提供 ExtendedFeaturesFramework 类；
// 负责 LED 状态更新、继电器控制、RS485 电流信号输出和 OLED 菜单交互

#include <memory>
// 引入 std::unique_ptr，用于管理五个子框架实例和三个内部子类
// （VibrationAnalyzer、PredictiveMaintenance、VibrationAlarmManager）的所有权

#include <atomic>
// 引入 std::atomic，用于 running_、initialized_ 线程安全标志，
// 以及所有 diag_* 心跳计数器的无锁并发递增

#include <thread>
// 引入 std::thread，用于 acquisition_thread_、processing_thread_、
// logging_thread_ 三条工作线程的创建和管理

#include <chrono>
// 引入 std::chrono，用于 predict_maintenance_schedule() 的
// std::chrono::system_clock::time_point 返回类型（下次维护时间点）

namespace MultiProject {
// 开启命名空间 MultiProject

struct AnomalyDetection {
// 异常检测结果辅助结构体；
// 封装单次振动信号分析中识别到的异常类型和严重程度；
// 供 VibrationAnalyzer 内部子类和 analyze_vibration_trend() 使用

    std::string type;
    // 异常类型描述字符串；
    // 取值来自模型输出类别名称（如 "deballfault"、"decagefault"、"deouterrace"）
    // 或人工定义的异常标签（如 "high_vibration"、"impulse_detected"）

    float severity;
    // 异常严重程度归一化值（范围 0.0~1.0）；
    // 通常与 AI 模型的故障类别置信度直接对应，
    // 或由振动幅值与健康阈值的比值计算得到；
    // 0.0 表示无异常，1.0 表示极度严重
};
// 结束 AnomalyDetection 结构体定义

class BearingVibrationProject {
// BearingVibrationProject：轴承振动监测子项目的顶层编排类；
// 不继承 IFramework 接口，作为应用层编排对象（Orchestrator），
// 将五个子框架实例和三条工作线程组装为完整的端到端监测流水线；
// 三线程并发架构通过 ConcurrentQueue 解耦各阶段，
// 采集、处理和日志线程可在不同速率下独立运行

public:
    BearingVibrationProject();
    // 构造函数：创建五个子框架的 unique_ptr 实例（不执行硬件初始化）：
    //   data_acquisition_  → DataAcquisitionFramework
    //   data_processing_   → DataProcessingFramework
    //   ai_model_          → AIModelFramework(ProjectType::BEARING_VIBRATION)
    //   display_logger_    → DisplayLoggingFramework
    //   extended_features_ → ExtendedFeaturesFramework
    // 创建三个内部子类实例：vibration_analyzer_、predictive_maintenance_、alarm_manager_；
    // running_、initialized_ 初始化为 false；
    // 不执行任何 GPIO/SPI/ONNX 相关的实际初始化操作

    ~BearingVibrationProject();
    // 析构函数：若 running_=true，先调用 stop() 安全停止三条工作线程；
    // 通过 unique_ptr 自动销毁所有子框架实例（触发各框架的析构，
    // 关闭 SPI 文件描述符、OLED I2C 连接、释放 ONNX Runtime Session 等）

    ErrorCode initialize(const std::string& config_path = "");
    // 执行完整的系统初始化流程：
    //   1. 若 config_path 非空，从 JSON 配置文件加载运行参数（覆盖默认值）
    //   2. data_acquisition_->initialize() → 初始化 SPI 总线和 AD4134 寄存器
    //   3. data_processing_->initialize()  → 预计算带通滤波器系数，创建 FFTW3 规划
    //   4. ai_model_->load_model(MODEL_PATH) → 加载 .onnx 模型，创建 ONNX Session
    //   5. display_logger_、extended_features_ 各自初始化
    //   6. 设置 initialized_=true；
    // 模拟模式下（enableSimulation 已调用）跳过硬件初始化，改为打开模拟数据文件；
    // 任一步骤失败均返回对应 ErrorCode 并设置 initialized_=false

    ErrorCode start();
    // 启动三条工作线程，开始持续监测：
    //   1. 校验 initialized_=true，否则返回 NOT_INITIALIZED
    //   2. 设置 running_=true
    //   3. data_acquisition_->start_streaming()（启动 AD4134 连续采样）
    //   4. 分别以 acquisition_loop / processing_loop / logging_loop 启动三条线程
    // 返回 SUCCESS 后系统进入持续运行状态（SystemState::RUNNING）

    ErrorCode stop();
    // 安全停止三条工作线程：
    //   1. 设置 running_=false
    //   2. data_acquisition_->stop_streaming()（停止 AD4134 连续采样）
    //   3. 向 raw_queue_ 和 result_queue_ 发送 stop() 信号，
    //      唤醒阻塞在 pop() 中的处理线程和日志线程
    //   4. join 三条工作线程，等待安全退出
    // 调用后系统状态切换为 SystemState::READY（可重新 start()）

    ErrorCode shutdown();
    // 执行完整关机流程（比 stop() 更彻底）：
    //   1. 调用 stop() 停止工作线程
    //   2. 触发各子框架的 shutdown() 方法释放硬件资源
    //   3. 设置 initialized_=false；
    // 通常在系统关机、固件更新前或电源断开事件触发时调用

    ErrorCode monitor_vibration(bool continuous = true);
    // 振动监测主入口方法：
    // 参数 continuous=true 时等同于已运行中的持续监测（此方法立即返回 SUCCESS）；
    // 参数 continuous=false 时执行一次性单帧采集+推理+输出（同步阻塞）；
    // 通常在 main.cpp 或上层管理器中调用，实际监测由三条工作线程持续执行

    ErrorCode calibrate_sensors();
    // 触发传感器校准流程：
    //   调用 data_acquisition_->calibrate_ad4134() 执行 AD4134 零偏校准；
    //   须在设备静止（无振动）条件下调用；
    //   校准期间系统状态切换为 SystemState::CALIBRATING；
    //   委托 extended_features_->auto_calibrate() 执行完整校准序列

    ErrorCode run_diagnostic_test();
    // 执行系统自检测试：
    //   依次检查 SPI 通信、AD4134 寄存器应答、ONNX 模型加载状态、
    //   OLED I2C 响应和 LED 驱动功能；
    //   自检结果记录到 display_logger_ 的系统日志；
    //   委托各子框架的 diagnose() 方法执行对应硬件层自检

    ErrorCode generate_report(const std::string& output_path);
    // 生成振动监测报告并保存到指定路径：
    //   报告内容：近期振动历史数据统计、AI 分类结果分布、
    //             健康指数趋势、RUL 预测、异常事件汇总；
    //   输出格式为 JSON 或 CSV（由 output_path 后缀决定）；
    //   内部调用 get_vibration_history() 读取历史记录并格式化输出

    ErrorCode analyze_vibration_trend(const std::vector<DataLog>& historical_data,
                                     std::string& trend_analysis);
    // 对一批历史振动日志数据执行趋势分析：
    // 参数 historical_data：由 get_vibration_history() 或历史文件读取的日志列表；
    // 参数 trend_analysis：输出的分析报告字符串（含退化趋势、预测 RUL、建议措施）；
    // 内部委托 VibrationAnalyzer 执行时间序列分析（线性回归/指数退化模型）；
    // 分析结果可用于 predict_maintenance_schedule() 的输入

    ErrorCode predict_maintenance_schedule(const std::vector<DataLog>& operational_data,
                                          std::chrono::system_clock::time_point& next_maintenance);
    // 基于历史运行数据预测下次建议维护时间点：
    // 参数 operational_data：历史运行日志数据（含振动值、健康指数时间序列）；
    // 参数 next_maintenance：输出的建议维护时间点
    //   （std::chrono::system_clock::time_point 类型，精度为秒）；
    // 内部委托 PredictiveMaintenance 子类执行指数退化模型拟合和 RUL 计算；
    // 与 BearingAssessmentConfig::HealthIndex 中的阈值配合触发维护建议

    bool is_running() const;
    // 返回当前是否处于持续运行状态（读取 running_ 原子变量）；
    // main.cpp 心跳循环中通过此方法判断是否需要重新 start()

    SystemStatus get_system_status() const;
    // 线程安全地返回当前系统状态快照（加锁复制 current_status_）；
    // 含 CPU 使用率、内存占用、推理帧率、最新分类结果等信息；
    // const 修饰配合 mutable status_mutex_ 允许在 const 方法中加锁

    std::vector<DataLog> get_vibration_history(int hours = 24);
    // 返回最近 hours 小时内的振动日志记录列表；
    // 默认返回最近 24 小时的数据；
    // 内部从 display_logger_ 的日志缓冲区和磁盘日志文件中读取，
    // 按时间升序排列（最旧的记录在前）

    ErrorCode update_config(const std::string& config_json);
    // 从 JSON 字符串热更新运行时配置（无需重新初始化）：
    //   支持更新置信度阈值、采样率、CWT 尺度数、报警阈值等参数；
    //   内部将 JSON 分发到对应子框架的 configure() 方法

    std::string get_config() const;
    // 将当前全部运行时配置序列化为 JSON 字符串并返回；
    // 聚合各子框架的 get_config() 输出为嵌套 JSON

    void enableSimulation(bool enable, const std::string& dataPath = "");
    // 启用或禁用模拟模式（须在 initialize() 之前调用）：
    // 参数 enable=true 时从 dataPath 目录读取预录的振动数据文件
    //   代替真实 AD4134 硬件采集，无需实物硬件即可调试处理和推理流水线；
    // 参数 dataPath：预录数据根目录（如 "datas/BearingVibration"），
    //   空字符串时使用默认路径；
    // 内部委托 data_acquisition_->enableSimulation(enable, dataPath)

    // ── 诊断接口（供 main.cpp 心跳打印使用）─────────────────────

    size_t raw_queue_size() const { return const_cast<ConcurrentQueue<RawData>&>(raw_queue_).size(); }
    // 内联方法：返回 raw_queue_ 当前积压的 RawData 帧数；
    // 供 main.cpp 的心跳日志打印当前队列深度，监控采集/处理速率是否均衡；
    // 使用 const_cast 绕过 ConcurrentQueue::size() 的 const 限定问题
    // （size() 内部需要加锁，未声明为 const）；
    // 返回值为调用瞬间的快照，不保证在多线程环境中的精确性

    size_t result_queue_size() const { return const_cast<ConcurrentQueue<DataLog>&>(result_queue_).size(); }
    // 内联方法：返回 result_queue_ 当前积压的 DataLog 条目数；
    // 若 result_queue_size() 持续增大，说明 logging_loop() 处理速度跟不上推理速度，
    // 可能需要降低推理帧率或优化日志写入性能

    // 诊断心跳计数器（public，原子操作，读取无需加锁）
    // 以下 13 个原子计数器为公共成员，供 main.cpp 的心跳打印协程周期性读取，
    // 无需加锁即可安全读取当前值（std::atomic 提供 load() 的顺序一致保证）；
    // 各计数器名称中的 diag_ 前缀和 acq/proc/log 中缀分别标识"诊断"和所属线程

    std::atomic<uint64_t> diag_acq_loop_iter_{0};
    // 采集线程主循环迭代计数：acquisition_loop() 每次循环开始时递增；
    // 用于验证采集线程是否正在持续运行（若长时间不变则线程可能已阻塞）

    std::atomic<uint64_t> diag_acq_got_data_{0};
    // 采集线程成功从硬件/模拟源读取到数据的次数；
    // 与 diag_acq_loop_iter_ 之差反映空采集循环（等待数据）的频率

    std::atomic<uint64_t> diag_acq_pushed_{0};
    // 采集线程成功将 RawData 推入 raw_queue_ 的次数；
    // 若 diag_acq_pushed_ < diag_acq_got_data_，说明存在推送失败（队列已满被丢弃）

    std::atomic<uint64_t> diag_proc_loop_iter_{0};
    // 处理线程主循环迭代计数：processing_loop() 每次循环开始时递增

    std::atomic<uint64_t> diag_proc_got_raw_{0};
    // 处理线程成功从 raw_queue_ 弹出 RawData 的次数（pop() 返回 true）；
    // 与 diag_acq_pushed_ 的差值反映队列积压情况

    std::atomic<uint64_t> diag_proc_cwt_done_{0};
    // 处理线程完成 CWT 时频图生成的次数；
    // 若显著低于 diag_proc_got_raw_，说明 CWT 计算存在错误或被跳过

    std::atomic<uint64_t> diag_proc_infer_start_{0};
    // 处理线程启动 ONNX 推理的次数（ai_model_->infer() 调用前递增）；
    // 与 diag_proc_cwt_done_ 对比确认预处理到推理的完整链路

    std::atomic<uint64_t> diag_proc_infer_done_{0};
    // 处理线程完成 ONNX 推理的次数（ai_model_->infer() 成功返回后递增）；
    // diag_proc_infer_start_ - diag_proc_infer_done_ 为当前正在推理的任务数（通常为 0 或 1）

    std::atomic<uint64_t> diag_proc_output_{0};
    // 处理线程完成后处理（postprocess_vibration_output）并生成 DataLog 的次数

    std::atomic<uint64_t> diag_proc_snapshot_{0};
    // 处理线程调用 extended_features_->update_menu_snapshot() 更新菜单快照的次数；
    // 每次推理完成后应调用一次，此计数器应与 diag_proc_output_ 保持一致

    std::atomic<uint64_t> diag_proc_save_rec_{0};
    // 处理线程调用 display_logger_->save_abnormal_record() 保存异常记录的次数；
    // 正常运行时此计数器应远低于 diag_proc_output_（只有检测到故障时才记录）

    std::atomic<uint64_t> diag_proc_pushed_{0};
    // 处理线程成功将 DataLog 推入 result_queue_ 的次数；
    // 应与 diag_proc_output_ 基本一致（推送失败极少发生）

    std::atomic<uint64_t> diag_log_loop_iter_{0};
    // 日志线程主循环迭代计数：logging_loop() 每次循环开始时递增；
    // 若长时间不变则日志线程可能已阻塞或退出

private:
// 私有区域：子框架实例、工作线程、数据队列、状态管理和内部子类

    std::unique_ptr<DataAcquisitionFramework> data_acquisition_;
    // 数据采集子框架实例：管理 AD4134 SPI 驱动和三轴振动数据采集；
    // acquisition_loop() 通过此对象调用 read_vibration_data()

    std::unique_ptr<DataProcessingFramework> data_processing_;   // 新增
    // 数据处理子框架实例（新增）：管理 CWT 时频图生成和振动特征提取；
    // 注释"新增"说明此成员在较早版本中不存在，是后续重构中从 AIModelFramework
    // 内部分离出来的独立处理层；
    // processing_loop() 通过此对象调用 compute_cwt_image() 生成 CWT 时频图

    std::unique_ptr<AIModelFramework> ai_model_;
    // AI 推理子框架实例：管理 ONNX Runtime Session 和四分类推理；
    // processing_loop() 在 CWT 图像生成后调用 ai_model_->infer() 执行推理

    std::unique_ptr<DisplayLoggingFramework> display_logger_;
    // 显示日志子框架实例：管理 OLED 显示更新和异常记录 CSV 持久化；
    // logging_loop() 调用此对象的 display_results() 和 save_abnormal_record()

    std::unique_ptr<ExtendedFeaturesFramework> extended_features_;
    // 扩展功能子框架实例：管理 LED/继电器/RS485 电流输出和 OLED 菜单交互；
    // logging_loop() 调用 led_update()、relay_update()、output_ai_result_current()；
    // processing_loop() 调用 update_menu_snapshot() 更新菜单状态快照

    std::thread acquisition_thread_;
    // 数据采集工作线程，运行 acquisition_loop() 函数体；
    // start() 中通过 std::thread(&BearingVibrationProject::acquisition_loop, this) 创建；
    // stop() 中通过 acquisition_thread_.join() 等待安全退出

    std::thread processing_thread_;
    // 数据处理工作线程，运行 processing_loop() 函数体；
    // 阻塞在 raw_queue_.pop() 等待采集线程推送数据；
    // 执行：RawData → 带通滤波 → CWT 时频图 → ONNX 推理 → DataLog

    std::thread logging_thread_;
    // 日志与输出工作线程，运行 logging_loop() 函数体；
    // 阻塞在 result_queue_.pop() 等待处理线程推送推理结果；
    // 执行：DataLog → OLED 显示 / LED 更新 / 继电器控制 / CSV 记录 / RS485 输出

    std::atomic<bool> running_{false};
    // 工作线程运行状态标志（原子变量），初始为 false；
    // start() 成功后置 true，stop() 中置 false；
    // 三条工作线程在各自循环中检查此标志决定是否继续运行；
    // 与 ConcurrentQueue::stop() 信号配合实现优雅停止

    std::atomic<bool> initialized_{false};
    // 系统初始化完成标志（原子变量），初始为 false；
    // initialize() 成功完成后置 true；
    // start() 调用前检查此标志，未初始化时返回 NOT_INITIALIZED；
    // shutdown() 后复位为 false

    ConcurrentQueue<RawData> raw_queue_;
    // 采集线程→处理线程的无锁线程安全队列；
    // acquisition_loop() 通过 push() 放入原始振动数据帧；
    // processing_loop() 通过 pop() 阻塞取出并处理；
    // 队列深度应能缓冲采集和处理速率之间的短暂差异（约 1~5 帧）

    ConcurrentQueue<DataLog> result_queue_;
    // 处理线程→日志线程的无锁线程安全队列；
    // processing_loop() 通过 push() 放入推理结果；
    // logging_loop() 通过 pop() 阻塞取出并执行输出动作；
    // 队列深度通常保持在 0~2 之间（日志线程处理速度快于推理速度）

    SystemStatus current_status_;
    // 当前系统运行状态快照，由 logging_loop() 在每次输出操作后更新；
    // 包含推理帧率、最新分类结果、健康指数、RUL 预测值等信息；
    // 通过 status_mutex_ 保护并发访问

    mutable std::mutex status_mutex_;
    // 系统状态互斥锁（mutable 允许在 const 方法 get_system_status() 中加锁）；
    // 保护 current_status_ 的并发读写：
    //   写操作：logging_loop() 更新 current_status_
    //   读操作：get_system_status()（const 方法，加锁复制后返回）

    void acquisition_loop();
    // 私有方法：采集线程主循环函数体；
    // 执行流程（循环直至 running_=false）：
    //   1. diag_acq_loop_iter_++
    //   2. data_acquisition_->acquire_data(raw_data)
    //      → 从 AD4134 或模拟文件读取一帧三轴振动数据
    //   3. diag_acq_got_data_++
    //   4. raw_queue_.push(raw_data)
    //   5. diag_acq_pushed_++
    // 采集速率受 AD4134 硬件定时（50 kSPS），无需 sleep

    void processing_loop();
    // 私有方法：处理线程主循环函数体；
    // 执行流程（循环直至 raw_queue_.pop() 返回 false）：
    //   1. diag_proc_loop_iter_++
    //   2. raw_queue_.pop(raw_data)（阻塞等待）→ false 时退出
    //   3. diag_proc_got_raw_++
    //   4. data_processing_->compute_cwt_image(signal, cwt_image)
    //      → 生成 224×224 单通道灰度 CWT 时频图
    //   5. diag_proc_cwt_done_++
    //   6. preprocess_input() 将 cwt_image 打包为 ModelInput
    //   7. diag_proc_infer_start_++
    //   8. ai_model_->infer(model_input, model_output)
    //   9. diag_proc_infer_done_++
    //  10. postprocess → 生成 DataLog
    //  11. diag_proc_output_++
    //  12. extended_features_->update_menu_snapshot(snap)
    //  13. diag_proc_snapshot_++
    //  14. 若检测到故障：display_logger_->save_abnormal_record(record)
    //  15. diag_proc_save_rec_++
    //  16. result_queue_.push(data_log)
    //  17. diag_proc_pushed_++

    void logging_loop();
    // 私有方法：日志与输出线程主循环函数体；
    // 执行流程（循环直至 result_queue_.pop() 返回 false）：
    //   1. diag_log_loop_iter_++
    //   2. result_queue_.pop(data_log)（阻塞等待）→ false 时退出
    //   3. display_logger_->display_results(data_log.output)
    //      → 更新 OLED 显示（故障类别、置信度、健康指数、RUL）
    //   4. extended_features_->led_update(class_id)
    //      → 点亮对应故障类别 LED，激活蜂鸣器（若需要）
    //   5. extended_features_->relay_update(class_id)
    //      → 控制 BCM17（K1）继电器吸合/断开
    //   6. extended_features_->output_ai_result_current(data_log.output)
    //      → 输出 8~20 mA RS485 电流信号
    //   7. display_logger_->log_data(data_log)
    //      → 写入系统运行日志文件
    //   8. 加锁更新 current_status_（含最新推理结果和系统资源占用）

    class VibrationAnalyzer;
    // 前向声明振动信号分析内部子类；
    // 完整定义位于 BearingVibrationProject.cpp；
    // 负责时域/频域特征提取、趋势分析（线性回归）和异常检测逻辑；
    // 供 analyze_vibration_trend() 委托使用

    class PredictiveMaintenance;
    // 前向声明预测性维护内部子类；
    // 完整定义位于 BearingVibrationProject.cpp；
    // 负责基于健康指数时间序列执行指数退化模型拟合，
    // 计算 RUL（剩余使用寿命）并推算下次建议维护时间点；
    // 供 predict_maintenance_schedule() 委托使用

    class VibrationAlarmManager;
    // 前向声明振动报警管理内部子类；
    // 完整定义位于 BearingVibrationProject.cpp；
    // 实现多级报警阈值比对（速度/加速度/峭度）、报警延迟计时
    // 和报警状态机（正常→预警→报警→停机）的完整控制逻辑；
    // 与 AlarmConfig 中的 VibrationThresholds 和 AlarmDelays 配合使用

    std::unique_ptr<VibrationAnalyzer> vibration_analyzer_;
    // 振动分析子类实例，由构造函数 make_unique 创建；
    // processing_loop() 和 analyze_vibration_trend() 通过此对象调用分析方法

    std::unique_ptr<PredictiveMaintenance> predictive_maintenance_;
    // 预测性维护子类实例，由构造函数 make_unique 创建；
    // predict_maintenance_schedule() 通过此对象计算 RUL 和维护时间点

    std::unique_ptr<VibrationAlarmManager> alarm_manager_;
    // 振动报警管理子类实例，由构造函数 make_unique 创建；
    // logging_loop() 在执行硬件报警输出前，
    // 先通过 alarm_manager_ 判断是否满足延迟时间和阈值条件
};
// 结束 BearingVibrationProject 类定义

} // namespace MultiProject
// 关闭命名空间 MultiProject