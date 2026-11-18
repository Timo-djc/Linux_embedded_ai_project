/**
  ******************************************************************************
  * @file    UnifiedDataTypes.hpp
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
 * @brief   多项目检测系统 — 统一数据类型与抽象接口头文件
 * ----------------------------------------------------------------------------
 * @details 本文件定义了 MultiProject 框架中各功能模块之间数据交换所使用的
 *          全部统一数据结构，以及驱动模块间解耦协作的五个抽象接口类。
 *          所有定义位于命名空间 MultiProject 内。
 *
 *          文件内容按以下层次组织：
 *
 *          ── 统一数据结构（struct）──
 *            RawData         — 原始数据包：封装采集阶段输出的多模态原始数据
 *                              （图像帧、传感器浮点数组、二进制字节流及元数据）
 *            ProcessedData   — 处理后数据包：封装预处理阶段输出的特征向量、
 *                              特征图、处理后图像及统计量
 *            ModelInput      — 模型输入包：封装推理前的输入张量和预处理参数
 *            ModelOutput     — 模型输出包：以四个子结构体分别承载分类、回归、
 *                              目标检测和异常检测四种任务的推理结果
 *            SystemStatus    — 系统状态快照：聚合硬件健康、软件运行和性能三类状态指标
 *            DataLog         — 完整数据日志条目：聚合上述全部数据结构及错误信息，
 *                              用于持久化存储和事后分析
 *
 *          ── 菜单系统数据结构（struct）──
 *            AbnormalRecord  — 异常事件记录条目，持久化到 CSV 日志文件
 *            MenuSnapshot    — Project Status 菜单页面的实时数据快照
 *
 *          ── 统一接口类（纯虚类）──
 *            IDataAcquisition  — 数据采集模块接口（5 个纯虚方法）
 *            IDataProcessor    — 数据处理模块接口（4 个纯虚方法）
 *            IAIModel          — AI 推理模型接口（8 个纯虚方法）
 *            IDisplayLogger    — 显示日志模块接口（11 个纯虚方法，含菜单系统扩展）
 *            IExtendedFeatures — 扩展功能模块接口（12 个纯虚方法）
 *
 * @note    本文件使用 #pragma once 代替传统头文件保护宏。
 *          依赖 OpenCV（cv::Mat、cv::Rect）进行图像数据的传递，
 *          编译时须链接 OpenCV 库（-lopencv_core 等）。
 * ============================================================================
 */

#pragma once
// 替代 #ifndef/#define/#endif 的现代头文件保护写法；
// 编译器级别保证本文件在同一编译单元中只被展开一次

#include <vector>
// 引入 std::vector，用于图像帧列表、传感器数据数组、特征向量、
// 检测框列表、概率输出等动态大小的序列成员

#include <string>
// 引入 std::string，用于来源名称、类别名称、版本字符串、
// 时间戳文本、错误描述等文本字段

#include <memory>
// 引入 std::shared_ptr / std::unique_ptr，
// 供后续可能的接口方法参数和返回值使用（当前版本未直接引用，为扩展预留）

#include <opencv2/opencv.hpp>
// 引入 OpenCV 核心头文件：提供 cv::Mat（图像矩阵）和 cv::Rect（矩形框）；
// cv::Mat 是本文件中图像数据传递的核心类型，用于原始帧、处理后图像和显示输出；
// 编译时须确保 OpenCV 已安装并配置了正确的 pkg-config 或 CMake find_package 路径

#include <cstdint>
// 引入固定宽度整数类型：uint64_t（时间戳、日志 ID）、
// uint32_t（序列号、帧计数）、uint8_t（二进制数据字节）等；
// 保证跨平台位宽一致性

#include "FrameworkBase.hpp"
// 引入框架基础层头文件，获取 MultiProject 命名空间下的公共类型：
// ErrorCode（各接口方法的返回值类型）、
// ProjectType（DataLog 和 AbnormalRecord 中的项目标识）、
// TaskType（DataLog 中的任务类型标识）等枚举

namespace MultiProject {
// 开启命名空间 MultiProject，将以下全部定义包裹其中，
// 避免与其他模块中同名结构体（如 RawData、ModelOutput）产生符号冲突

// ============================================================================
// 统一数据结构
// ============================================================================
// 分节注释：以下结构体定义各处理阶段之间传递数据所使用的统一载体类型

struct RawData {
    // 原始数据包结构体：封装数据采集阶段（IDataAcquisition::acquire_data）
    // 的全部输出内容，向下游处理阶段（IDataProcessor）传递多模态原始数据；
    // 同一结构体同时支持图像类项目（火焰检测）和信号类项目（轴承振动），
    // 各项目只填充与自身相关的字段，其余字段保持默认空值

    uint64_t timestamp;
    // 数据采集完成时刻的 UNIX 纳秒时间戳；
    // uint64_t 可表示约 292 年，无溢出风险；
    // 与 Timestamp 结构体的 seconds×10^9+nanoseconds 等价，此处合并为单一 uint64_t 简化传递

    uint32_t sequence_number;
    // 本数据包的全局单调递增序列号；
    // 用于检测丢帧（相邻包序列号不连续）和数据包排序

    std::string source_name;
    // 数据来源的标识字符串，如 "video0"（摄像头 0）、
    // "AD4134_Ch0"（ADC 通道 0）；
    // 用于多路数据源场景下的来源追溯

    std::vector<cv::Mat> images;
    // 图像帧列表：火焰检测场景下存储三路摄像头的原始帧（BGR 格式）；
    // 轴承振动场景下此列表为空（无图像数据）；
    // cv::Mat 为引用计数对象，vector 拷贝时共享底层图像数据（浅拷贝），
    // 若需独立副本应调用 mat.clone()

    std::vector<float> sensor_data;
    // 传感器浮点数据数组：轴承振动场景下存储 AD4134 三轴加速度采样值
    // （X/Y/Z 轴交织排列，每采样点 3 个 float）；
    // 火焰检测场景下此数组为空

    std::vector<uint8_t> binary_data;
    // 二进制原始字节流：用于存储编码后的数据（如 MJPEG 压缩帧字节流、
    // SPI 原始字节序列等）；
    // 需由接收方根据已知格式解码，灵活性高但类型安全性较低

    struct Metadata {
        // 子结构体 Metadata：描述本数据包内容的元信息，
        // 供下游处理模块正确解析图像尺寸、采样率等参数
        int width = 0;
        // 图像宽度（像素），images 非空时有效；
        // 对应摄像头采集分辨率宽度（如 640）

        int height = 0;
        // 图像高度（像素），images 非空时有效；
        // 对应摄像头采集分辨率高度（如 480）

        int channels = 0;
        // 图像通道数：3（BGR 彩色）或 1（灰度）；
        // 与模型输入的 input_channels 配合使用

        float sample_rate = 0.0f;
        // 传感器采样率（Hz），sensor_data 非空时有效；
        // 对应 AD4134 的 50 kSPS 采样率

        std::vector<float> calibration_params;
        // 传感器校准参数列表（如零偏、增益校正系数）；
        // 由传感器校准流程写入，数据处理阶段读取后对 sensor_data 进行修正
    } metadata;
    // 声明 metadata 成员变量

    std::vector<uint8_t> serialize() const { return {}; }
    // 将本数据包序列化为字节数组，用于网络传输或文件持久化；
    // 当前为桩实现，返回空字节数组；
    // 完整实现应将 timestamp、sequence_number、images（编码为 JPEG）、
    // sensor_data 等字段按固定协议打包

    bool deserialize(const std::vector<uint8_t>&) { return false; }
    // 从字节数组反序列化并填充本数据包各字段；
    // 与 serialize() 互为逆操作；
    // 当前为桩实现，始终返回 false（未实现）
};
// 结束 RawData 结构体定义

struct ProcessedData {
    // 处理后数据包结构体：封装数据处理阶段（IDataProcessor::process_data）
    // 的全部输出内容，向下游 AI 推理阶段（IAIModel::infer）传递特征数据

    std::vector<float> features;
    // 一维特征向量：由特征提取流程（时域/频域/统计特征）生成的浮点数组；
    // 轴承振动场景下默认 20 维（与 VibrationAnalysisConfig::FeatureExtraction::num_features 一致）；
    // 火焰检测场景下此字段通常为空（图像模型直接接受 processed_image）

    std::vector<std::vector<float>> feature_maps;
    // 二维特征图列表：用于存储 CWT（连续小波变换）时频图的浮点矩阵，
    // 或卷积神经网络中间层的激活特征图；
    // 每个元素为一张特征图的逐像素浮点值（行优先展开）

    cv::Mat processed_image;
    // 经预处理后的图像矩阵：已完成灰度化、resize 至 224×224 和归一化的图像；
    // 直接作为 ONNX 模型的输入张量来源；
    // 火焰检测和轴承振动（CWT 图像）均使用此字段

    struct Statistics {
        // 子结构体 Statistics：记录处理数据的统计量，
        // 用于时域特征计算结果的汇总展示和健康评估
        float mean    = 0.0f;
        // 数据均值（如振动加速度的时序均值）；
        // 正常运行时振动信号均值应接近 0（零均值）

        float std_dev = 0.0f;
        // 标准差，反映数据的离散程度；
        // 振动信号的标准差约等于 RMS 值（零均值信号下）

        float min     = 0.0f;
        // 数据最小值（负峰值）

        float max     = 0.0f;
        // 数据最大值（正峰值）；
        // max - min 即为峰峰值（Peak-to-Peak）

        float energy  = 0.0f;
        // 信号能量（各采样点平方和），与 RMS 的平方成正比；
        // 是频带能量分析（enable_band_analysis）的基础计算量
    } stats;
    // 声明 stats 成员变量

    uint64_t processing_timestamp;
    // 数据处理完成时刻的 UNIX 纳秒时间戳；
    // 与 RawData::timestamp 的差值即为预处理阶段的端到端耗时
};
// 结束 ProcessedData 结构体定义

struct ModelInput {
    // 模型输入数据包结构体：封装送入 AI 推理引擎（IAIModel::infer）
    // 前的全部输入数据及预处理参数，支持图像输入和张量输入两种模式

    std::vector<cv::Mat> input_images;
    // 待推理的图像列表：通常包含一张经预处理的 224×224 灰度图；
    // ONNX Runtime 接受 cv::Mat 时须转换为 float32 并调整通道顺序（HWC → NCHW）

    std::vector<std::vector<float>> input_tensors;
    // 以二维浮点数组形式表示的输入张量列表：
    // 每个元素对应一个输入节点的展开数据（CHW 格式的逐元素浮点值）；
    // 当模型有多个输入节点时（如多模态融合模型），此列表包含多个元素

    std::vector<int> input_shapes;
    // 输入张量的形状描述，格式为 [N, C, H, W]（NCHW）；
    // 用于 ONNX Runtime 创建输入 Tensor 时指定维度

    struct PreprocessInfo {
        // 子结构体 PreprocessInfo：描述推理前图像预处理的参数配置；
        // 供推理层在执行归一化和缩放时使用，保证与训练时的预处理一致
        bool normalize = true;
        // 为 true 时对像素值执行归一化处理（Z-score 或 Min-Max）；
        // 须与训练时的 normalize 设置完全一致

        std::vector<float> mean = {0.485f, 0.456f, 0.406f};
        // 归一化均值列表（ImageNet 标准均值，RGB 三通道）；
        // 注意：本项目实际使用单通道灰度图（mean = [33.405/255.0]），
        // 此默认值为 ImageNet RGB 标准值，需在构造具体 ModelInput 实例时
        // 替换为与训练 img_norm_cfg 一致的值（参见 AIModelConfig 中的修复）

        std::vector<float> std  = {0.229f, 0.224f, 0.225f};
        // 归一化标准差列表（ImageNet 标准标准差，RGB 三通道）；
        // 同上，实际使用时应替换为 [78.795/255.0]（灰度图归一化参数）

        bool resize       = true;
        // 为 true 时在归一化前将图像缩放至 target_width × target_height；
        // 须与模型输入尺寸（224×224）保持一致

        int  target_width  = 224;
        // 缩放目标宽度（像素），与 ModelConfig::ModelParams::input_width 一致

        int  target_height = 224;
        // 缩放目标高度（像素），与 ModelConfig::ModelParams::input_height 一致
    } preprocess_info;
    // 声明 preprocess_info 成员变量
};
// 结束 ModelInput 结构体定义

struct ModelOutput {
    // 模型推理输出数据包结构体：通过四个子结构体分别承载
    // 分类、回归、目标检测和异常检测四种 AI 任务的推理结果；
    // 具体项目根据 TaskType 只填充对应的子结构体，其余保持默认值

    struct ClassificationResult {
        // 子结构体 ClassificationResult：分类任务推理结果
        int   class_id   = -1;
        // 预测类别的整数编号，-1 表示尚未推理或置信度不足；
        // 具体类别映射见 AIModelConfig::OutputConfig::Classification::classes；
        // 火焰检测：0=normal, 1=fire；
        // 轴承振动：0=denormal, 1=deballfault, 2=decagefault, 3=deouterrace

        std::string class_name;
        // 预测类别的名称字符串，从 classes 列表中按 class_id 索引得到；
        // 用于 OLED 显示和日志记录中的人类可读类别标注

        float confidence = 0.0f;
        // 预测类别的 softmax 概率值（置信度），范围 [0.0, 1.0]；
        // 低于 confidence_threshold 时不触发报警（视为置信度不足）

        std::vector<float> probabilities;
        // 完整的 softmax 概率分布向量，长度等于 num_classes；
        // argmax(probabilities) = class_id；
        // 保留完整分布以支持多类别联合判断（如前两类概率之和）
    } classification;
    // 声明 classification 成员变量

    struct RegressionResult {
        // 子结构体 RegressionResult：回归任务推理结果
        float value       = 0.0f;
        // 模型预测的连续数值输出（如 RUL 剩余使用寿命天数）；
        // 范围由训练时的目标量程决定（对应 AIModelConfig::OutputConfig::Regression::min/max_value）

        float uncertainty = 0.0f;
        // 预测值的不确定度（如贝叶斯神经网络的预测标准差）；
        // 用于输出预测置信区间（value ± uncertainty）

        std::vector<float> predictions;
        // 回归模型的多步预测序列（如未来 N 天的 RUL 预测值列表）；
        // 单步预测时 predictions.size() == 1，其值等于 value
    } regression;
    // 声明 regression 成员变量

    struct DetectionResult {
        // 子结构体 DetectionResult：目标检测任务推理结果
        std::vector<cv::Rect>   bounding_boxes;
        // 检测到的目标边界框列表，每个 cv::Rect 描述一个目标的
        // 左上角坐标（x, y）和尺寸（width, height）（像素坐标，相对于原始输入图像）；
        // 经 NMS（非极大值抑制）去重后的最终边界框集合

        std::vector<float>      confidences;
        // 各边界框对应的目标置信度分数，与 bounding_boxes 一一对应；
        // confidences[i] 为 bounding_boxes[i] 包含目标的概率

        std::vector<int>        class_ids;
        // 各边界框对应的目标类别 ID，与 bounding_boxes 一一对应；
        // class_ids[i] 为 bounding_boxes[i] 中目标的类别编号

        std::vector<std::string> labels;
        // 各边界框对应的目标类别名称字符串，与 bounding_boxes 一一对应；
        // 由 class_ids[i] 通过类别名称列表映射得到，用于视频叠加显示
    } detection;
    // 声明 detection 成员变量

    struct AnomalyResult {
        // 子结构体 AnomalyResult：异常检测任务推理结果
        bool  is_anomaly   = false;
        // 当前输入是否被判定为异常，true 表示检测到异常；
        // is_anomaly = (anomaly_score > anomaly_threshold)

        float anomaly_score = 0.0f;
        // 异常得分，范围 [0.0, 1.0]；
        // 自编码器模型中通常为重构误差经归一化后的值；
        // 得分越高表示输入与训练时正常样本的分布偏差越大

        std::string anomaly_type;
        // 异常类型描述字符串（若模型支持细粒度异常分类）；
        // 如 "vibration_spike"（振动突刺）、"image_overexposed"（图像过曝）等

        float severity     = 0.0f;
        // 异常严重程度量化值，范围 [0.0, 1.0]；
        // 可用于多级报警阈值判断（对应 BearingAssessmentConfig::HealthIndex 的阈值分级）
    } anomaly;
    // 声明 anomaly 成员变量

    uint64_t    inference_time_us = 0;
    // 本次推理的执行耗时（微秒），由推理层填写；
    // 用于更新 PerformanceMetrics::TimeMetrics::inference_time_ms（除以 1000 转换单位）

    std::string model_version;
    // 执行本次推理所用模型的版本标识字符串；
    // 用于日志记录和多版本模型对比分析

    float       model_confidence  = 0.0f;
    // 模型整体置信度指标（0~1）；
    // 可用于综合评估本次推理结果的可信程度，
    // 与 classification.confidence 不同，此字段反映模型对整体预测的自信程度
};
// 结束 ModelOutput 结构体定义

struct SystemStatus {
    // 系统状态快照结构体：聚合硬件健康、软件运行和性能指标三类实时状态，
    // 供 IDisplayLogger::display_status() 显示、
    // IExtendedFeatures::send_diagnostic_data() 上报，
    // 以及 IDataAcquisition::diagnose() 填充硬件子结构体

    struct HardwareStatus {
        // 子结构体 HardwareStatus：各硬件子系统的健康状态布尔标志
        bool  cameras_healthy = false;
        // 摄像头是否正常：三路摄像头全部能正常读帧时为 true；
        // 轴承振动项目无摄像头，此字段保持 false（不参与健康评估）

        bool  sensors_healthy = false;
        // 传感器（AD4134 ADC）是否正常：SPI 通信正常且数据非全零时为 true；
        // 火焰检测项目无 ADC，此字段保持 false

        bool  gpio_healthy    = false;
        // GPIO 子系统是否正常：所有配置引脚的读写操作均成功时为 true

        bool  spi_healthy     = false;
        // SPI 总线是否正常：/dev/spidevX.Y 可正常打开和收发数据时为 true

        bool  i2c_healthy     = false;
        // I2C 总线是否正常：OLED（0x3C）等设备响应 I2C 探测时为 true

        float cpu_temperature = 0.0f;
        // CPU 当前结温（℃），通过读取
        // /sys/class/thermal/thermal_zone0/temp（单位 millidegree）并除以 1000 获取；
        // 与 SystemConfig::HardwareConfig::CPUConfig::thermal_threshold（85 ℃）比较

        float memory_usage    = 0.0f;
        // 系统物理内存使用率（%），通过读取 /proc/meminfo 计算；
        // 与 PerformanceConfig::memory_usage_limit（80%）比较

        float disk_usage      = 0.0f;
        // 日志目录所在分区的磁盘使用率（%），通过 statvfs() 系统调用获取；
        // 超过 90% 时应触发日志清理或停止写入
    } hardware;
    // 声明 hardware 成员变量

    struct SoftwareStatus {
        // 子结构体 SoftwareStatus：各软件功能线程的运行状态
        bool        acquisition_running = false;
        // 数据采集线程（IDataAcquisition）是否正在运行

        bool        processing_running  = false;
        // 数据处理线程（IDataProcessor）是否正在运行

        bool        ai_running          = false;
        // AI 推理线程（IAIModel）是否正在运行

        bool        display_running     = false;
        // 显示日志线程（IDisplayLogger）是否正在运行

        bool        features_running    = false;
        // 扩展功能线程（IExtendedFeatures，如报警、远程监控）是否正在运行

        std::string current_state;
        // 系统当前状态机状态的字符串表示（如 "RUNNING"、"ERROR"）；
        // 通过 systemStateToString(getState()) 转换得到

        uint32_t    frames_processed    = 0;
        // 自上次统计周期重置以来成功处理的累计帧数；
        // 与 PerformanceMetrics::SuccessMetrics::success_frames 语义相同，此处为轻量快照

        uint32_t    errors_count        = 0;
        // 自上次统计周期重置以来累计的错误次数；
        // 错误率 = errors_count / (frames_processed + errors_count)
    } software;
    // 声明 software 成员变量

    struct PerformanceMetrics {
        // 子结构体 PerformanceMetrics：关键性能指标的轻量快照；
        // 与 FrameworkBase.hpp 中的 PerformanceMetrics 结构体（包含四个子结构体）不同，
        // 本结构体是其简化版本，用于状态快照和 OLED 单行显示
        float acquisition_fps   = 0.0f;
        // 数据采集阶段的当前帧率（fps）

        float processing_fps    = 0.0f;
        // 数据处理阶段的当前帧率（fps）

        float ai_inference_fps  = 0.0f;
        // AI 推理阶段的当前帧率（fps）；
        // = 1000000.0f / ModelOutput::inference_time_us（微秒转换）

        float total_latency_ms  = 0.0f;
        // 端到端总延时（ms）：从数据采集完成到推理结果输出的全链路时间

        float cpu_usage_percent = 0.0f;
        // 系统 CPU 使用率（%），与 HardwareStatus 中的字段来源相同，
        // 此处归入性能维度方便统一展示

        float memory_usage_mb   = 0.0f;
        // 系统内存使用量（MB），绝对值形式（区别于 HardwareStatus 中的百分比形式）
    } performance;
    // 声明 performance 成员变量
};
// 结束 SystemStatus 结构体定义

struct DataLog {
    // 完整数据日志条目结构体：聚合单次推理循环中产生的全部数据，
    // 作为持久化存储（save_to_file）和事后离线分析的完整记录单元；
    // 每次推理循环（acquisition → processing → inference → display）
    // 产生一个 DataLog 条目

    uint64_t    log_id = 0;
    // 日志条目的全局唯一 ID，单调递增；
    // 用于按 ID 排序或查询特定日志条目

    std::string timestamp;
    // 日志记录时刻的时间戳字符串（建议 ISO 8601 格式）；
    // 注意：此处为 std::string 而非 uint64_t，直接存储格式化文本，便于日志文件阅读

    ProjectType project_type;
    // 本日志条目归属的项目类型（FIRE_DETECTION 或 BEARING_VIBRATION）；
    // 用于多项目混合日志中的过滤和分类统计

    TaskType    task_type;
    // 本次推理执行的 AI 任务类型（CLASSIFICATION、REGRESSION 等）；
    // 用于选择正确的结果字段（classification/regression/detection/anomaly）进行解析

    RawData       raw_data;
    // 本次采集的原始数据快照（含图像帧或传感器数据）；
    // 注意：RawData::images 含 cv::Mat，存储开销较大；
    // 若需控制日志文件大小，可在写入前清空 images 字段（仅保留 image_path 引用）

    ProcessedData processed_data;
    // 本次预处理后的数据快照（含特征向量、统计量等）

    ModelOutput   ai_results;
    // 本次 AI 推理的完整输出结果（分类/回归/检测/异常四个子结构体）

    SystemStatus  system_status;
    // 本次推理时刻的系统状态快照（硬件健康、软件运行、性能指标）

    struct ErrorInfo {
        // 子结构体 ErrorInfo：本次处理循环中发生的错误信息
        ErrorCode   error_code    = ErrorCode::SUCCESS;
        // 本次循环中最严重错误的错误码；
        // ErrorCode::SUCCESS 表示本次循环无错误

        std::string error_message;
        // 错误的人类可读描述字符串，与 errorCodeToString(error_code) 互补

        std::string error_context;
        // 错误发生时的上下文信息（如发生错误的函数名、输入参数值等），
        // 用于辅助错误定位和根因分析
    } error_info;
    // 声明 error_info 成员变量

    std::string to_json()   const { return "{}"; }
    // 将本日志条目序列化为 JSON 字符串；
    // 当前为桩实现，返回空 JSON 对象；
    // 完整实现应递归序列化全部子结构体成员

    bool from_json(const std::string&) { return false; }
    // 从 JSON 字符串反序列化并填充本日志条目；
    // 当前为桩实现，始终返回 false；
    // 参数为未命名参数（编译器不产生未使用参数警告，同时保留接口签名完整性）

    bool save_to_file(const std::string&) const { return false; }
    // 将本日志条目持久化保存到指定路径的文件中；
    // 当前为桩实现，始终返回 false；
    // 完整实现应调用 to_json() 后写入 .json 文件，或按 CSV 格式写入

    bool load_from_file(const std::string&) { return false; }
    // 从指定路径的文件中加载并反序列化日志条目；
    // 当前为桩实现，始终返回 false
};
// 结束 DataLog 结构体定义

// ============================================================================
// 菜单系统数据结构
// ============================================================================
// 分节注释：以下结构体专用于 OLED 菜单系统的数据持久化和界面快照

/**
 * @brief 异常记录条目 — 持久化到 log/BearingVibration/ 或 log/FireDetection/
 *
 * 每次 AI 推理出异常（class_id != 0）时由 logging_loop 写入一条。
 * CSV 列顺序：
 *   timestamp, class_id, class_name, confidence, current_ma, image_path, error_reason
 */
struct AbnormalRecord {
    // 异常事件记录结构体：每次 AI 推理检测到异常（class_id != 0，即非正常类别）时
    // 由 logging_loop 创建一个实例并写入到对应项目的 CSV 日志文件；
    // CSV 列顺序严格按注释中声明的顺序存储，确保读取端解析正确

    std::string timestamp;          // "YYYY-MM-DD HH:MM:SS"
    // 异常发生的时间戳字符串，格式固定为 "YYYY-MM-DD HH:MM:SS"（本地时间）；
    // 用于日志条目的时间排序和近 7 天筛选（load_week_abnormal）

    ProjectType project_type = ProjectType::UNKNOWN;
    // 本异常记录归属的项目类型；
    // 用于菜单系统按项目筛选显示异常记录（轴承监测/火焰检测分开统计）

    int         class_id     = -1;
    // 异常类别 ID，对应 ModelOutput::ClassificationResult::class_id；
    // -1 为初始未赋值状态；0 为正常类别（AbnormalRecord 不应存储 class_id=0 的记录）

    std::string class_name;         // "外圈故障" / "fire" 等
    // 异常类别的中文或英文名称字符串，用于菜单界面和报警通知的人类可读显示；
    // 轴承振动：如 "deballfault"（滚动体故障）、"deouterrace"（外圈故障）；
    // 火焰检测：如 "fire"（检测到火焰）

    float       confidence   = 0.0f;
    // 本次推理的异常类别置信度（0~1），对应 classification.confidence；
    // 低置信度的异常记录可能是误报，供运维人员参考判断

    float       current_ma   = 0.0f;
    // 本次异常发生时的 4-20 mA 电流输出值（mA）；
    // 4 mA 对应正常状态，20 mA 对应最严重故障状态；
    // 用于与外部 PLC/DCS 系统的模拟量接口对应

    std::string image_path;         // 火焰截图路径（轴承留空）
    // 异常发生时保存的截图文件路径（绝对路径）；
    // 火焰检测项目：保存标注了检测框的 JPEG 截图路径；
    // 轴承振动项目：无截图，此字段保持空字符串

    std::string error_reason;       // 推理失败时的原因描述
    // 若本次推理失败（如模型加载错误、输入数据异常），记录失败原因描述；
    // 推理成功时此字段为空字符串
};
// 结束 AbnormalRecord 结构体定义

/**
 * @brief 菜单 Project Status 页面的数据快照
 *
 * 由 logging_loop 在每次推理后通过 extended_features_->update_menu_snapshot() 更新。
 */
struct MenuSnapshot {
    // 菜单 Project Status 页面的实时数据快照结构体；
    // 由 logging_loop 在每次推理完成后调用
    // extended_features_->update_menu_snapshot() 写入最新状态；
    // OLED 菜单显示线程周期性读取本快照并刷新 Project Status 页面

    // 检测状态
    enum class RunState { RUNNING, SUCCESS, FAILED } run_state = RunState::RUNNING;
    // 项目运行状态的强类型枚举，三个取值：
    //   RUNNING：推理循环正在正常执行，系统运行中
    //   SUCCESS：最近一次推理成功完成，结果有效
    //   FAILED ：最近一次推理失败（模型错误、采集超时等），结果无效
    // 默认值 RUNNING 对应系统刚启动尚未完成首次推理的状态

    std::string fail_reason;            // run_state == FAILED 时有效
    // 推理失败时的原因描述字符串；
    // run_state == FAILED 时由失败处理逻辑填写，用于 OLED 错误提示显示；
    // run_state 为 RUNNING 或 SUCCESS 时此字段为空字符串

    // 最新推理结果
    std::string result_label;           // "正常" / "外圈故障" / "fire"
    // 最近一次推理结果的类别名称字符串（中文或英文）；
    // 对应 ModelOutput::ClassificationResult::class_name；
    // 用于 OLED Project Status 页面的结果行显示

    float       result_confidence = 0.0f;
    // 最近一次推理结果的置信度（0~1）；
    // 在 OLED 上以百分比形式显示（乘以 100）

    std::string result_timestamp;       // 推理时间 "HH:MM:SS"
    // 最近一次推理完成时刻的时间字符串，格式为 "HH:MM:SS"（时:分:秒）；
    // 用于 OLED 显示最后更新时间，帮助运维人员判断数据新鲜度

    // 输出状态
    float current_ma      = 4.0f;       // 当前 4-20mA 电流值
    // 当前 4-20 mA 模拟量输出的电流值（mA），默认值 4.0 mA（对应无故障/正常状态）；
    // 取值范围 [4.0, 20.0]：4 mA=正常，线性映射至 20 mA=最严重故障；
    // 通过 DAC 或 PWM 输出模块驱动外部 4-20 mA 电流环路

    bool  relay_bearing   = false;      // 轴承继电器状态
    // 轴承故障继电器（K1，GPIO BCM17）的当前状态；
    // true 表示继电器已吸合（轴承故障报警触发），false 表示继电器断开（正常）；
    // 与 AlarmConfig::RelayControl::bearing_relay_pin=17 对应

    bool  relay_fire      = false;      // 火焰继电器状态
    // 火焰检测继电器（K2，GPIO BCM27）的当前状态；
    // true 表示继电器已吸合（火焰报警触发），false 表示继电器断开（正常）；
    // 与 fire_detection_config.hpp 中 AlarmMethods 的继电器配置对应
};
// 结束 MenuSnapshot 结构体定义

// ============================================================================
// 统一接口类
// ============================================================================
// 分节注释：以下五个纯虚类定义各功能模块的抽象契约，
// 与 FrameworkBase.hpp 中的前置声明对应，此处给出完整的方法签名定义

class IDataAcquisition {
// 数据采集模块抽象接口：规范摄像头帧采集（火焰检测）和
// ADC 传感器采样（轴承振动）的统一操作接口
public:
    virtual ~IDataAcquisition() = default;
    // 虚析构函数：确保通过基类指针销毁具体采集模块对象时正确释放资源
    // （如关闭摄像头设备文件、停止 SPI 通信）

    virtual ErrorCode initialize() = 0;
    // 纯虚方法：初始化采集硬件（打开设备文件、配置参数、启动数据流）；
    // 成功返回 ErrorCode::SUCCESS，失败返回 CAMERA_OPEN_FAILED 或 SPI_INIT_FAILED 等

    virtual ErrorCode acquire_data(RawData& data) = 0;
    // 纯虚方法：执行一次数据采集，将结果写入输出参数 data；
    // data 为输出参数（通过引用传出），调用前无需初始化；
    // 摄像头模式下阻塞等待直至新帧到达，ADC 模式下读取 FIFO 数据

    virtual bool      is_ready() const = 0;
    // 纯虚方法：查询采集模块是否已完成初始化并就绪；
    // 返回 true 时可调用 acquire_data()，否则应先调用 initialize()

    virtual ErrorCode configure(const std::string& config_str) = 0;
    // 纯虚方法：从 JSON 字符串动态更新采集参数（如帧率、分辨率、采样率）；
    // 支持运行时热配置，无需重启采集模块

    virtual SystemStatus::HardwareStatus diagnose() const = 0;
    // 纯虚方法：执行硬件自检，返回硬件状态快照；
    // 检查摄像头/传感器连通性、SPI/I2C 总线健康状态及资源占用情况
};
// 结束 IDataAcquisition 接口类定义

class IDataProcessor {
// 数据处理模块抽象接口：规范图像预处理（火焰检测）和
// 振动信号分析（轴承振动）的统一操作接口
public:
    virtual ~IDataProcessor() = default;
    // 虚析构函数：确保正确释放处理模块的内部资源（如 FFT 工作缓冲区）

    virtual ErrorCode process_data(const RawData& raw_data, ProcessedData& processed_data) = 0;
    // 纯虚方法：对原始数据执行完整预处理流程，输出处理后数据；
    // raw_data 为只读输入（const 引用），processed_data 为输出参数；
    // 图像模式：灰度化 → resize(224×224) → 归一化；
    // 振动模式：滤波 → 时域特征 → FFT → 频域特征 → 特征向量组装

    virtual ErrorCode extract_features(const RawData& raw_data, std::vector<float>& features) = 0;
    // 纯虚方法：从原始数据中提取特征向量，比 process_data 更轻量；
    // 适用于仅需要特征而不需要完整处理结果的场景（如在线特征监控）

    virtual ErrorCode augment_data(RawData& data) = 0;
    // 纯虚方法：对原始数据执行数据增强（如随机翻转、亮度调整、噪声注入）；
    // data 为输入输出参数（in-place 修改）；
    // 主要用于模型在线微调（fine-tune）时的训练样本扩充

    virtual ErrorCode fuse_data(const std::vector<RawData>& multi_source_data, RawData& fused_data) = 0;
    // 纯虚方法：将多路数据源（如三路摄像头帧）融合为单一 RawData 对象；
    // multi_source_data 为输入的多路原始数据列表；
    // fused_data 为输出的融合结果，供后续单路处理流程使用
};
// 结束 IDataProcessor 接口类定义

class IAIModel {
// AI 推理模型抽象接口：规范 ONNX 模型的加载、推理和模型优化操作接口；
// 实现类应封装 ONNX Runtime 的 Session、Input/OutputNames 等内部状态
public:
    virtual ~IAIModel() = default;
    // 虚析构函数：确保正确释放 ONNX Runtime Session 等推理引擎资源

    virtual ErrorCode load_model(const std::string& model_path) = 0;
    // 纯虚方法：从指定路径加载 ONNX 模型文件，创建推理 Session；
    // 成功返回 ErrorCode::SUCCESS，
    // 文件不存在返回 MODEL_LOAD_FAILED，格式错误返回 MODEL_VERSION_MISMATCH

    virtual ErrorCode unload_model() = 0;
    // 纯虚方法：释放已加载的模型和推理 Session，释放内存；
    // 通常在框架关闭或切换模型时调用

    virtual ErrorCode infer(const ModelInput& input, ModelOutput& output) = 0;
    // 纯虚方法：对单个输入样本执行推理（单批次推理）；
    // input 为只读输入，output 为输出参数；
    // 推理完成后 output.inference_time_us 应被填充实际耗时

    virtual ErrorCode infer_batch(const std::vector<ModelInput>& inputs, std::vector<ModelOutput>& outputs) = 0;
    // 纯虚方法：对多个输入样本执行批量推理；
    // inputs 和 outputs 的大小须一一对应；
    // 批量推理在 GPU 场景下效率更高，CPU 场景下与多次 infer() 等价

    virtual ErrorCode optimize_model() = 0;
    // 纯虚方法：对已加载模型执行图级优化（Graph Optimization）；
    // 包括算子融合（Op Fusion）、常量折叠（Constant Folding）等；
    // ONNX Runtime 通常在 Session 创建时自动执行，此方法供手动触发

    virtual ErrorCode quantize_model(bool use_int8 = true) = 0;
    // 纯虚方法：对已加载模型执行量化压缩；
    // use_int8=true 时执行 INT8 静态量化（需要标定数据集），
    // use_int8=false 时执行 FP16 半精度量化；
    // 量化可将模型大小和推理耗时减少约 2~4 倍，但可能轻微降低精度

    virtual ErrorCode prune_model(float sparsity = 0.5f) = 0;
    // 纯虚方法：对已加载模型执行权重剪枝；
    // sparsity 为目标稀疏率（0~1），0.5 表示剪去 50% 的权重；
    // 剪枝后需重新微调（fine_tune）以恢复精度

    virtual ErrorCode fine_tune(const std::vector<ModelInput>& inputs,
                                const std::vector<ModelOutput>& targets) = 0;
    // 纯虚方法：使用少量样本对已加载模型执行在线微调；
    // inputs 为微调输入样本，targets 为对应的期望输出（监督信号）；
    // 适用于设备部署后因环境变化导致的模型精度下降场景
};
// 结束 IAIModel 接口类定义

class IDisplayLogger {
// 显示日志模块抽象接口：规范 OLED 文本/图像显示、检测结果展示、
// 系统状态显示和数据日志记录操作，并扩展了菜单系统所需的异常记录接口
public:
    virtual ~IDisplayLogger() = default;
    // 虚析构函数：确保正确释放 OLED I2C 文件描述符和日志文件句柄等资源

    virtual ErrorCode display_text(const std::string& text, int line = 0) = 0;
    // 纯虚方法：在 OLED 屏幕的指定行（line）显示文本字符串；
    // line=0 对应第一行（三行/四行模式均适用）；
    // 底层应根据 text 是否含有中文字符自动选择字体（wqy16 或 9x18B/6x13）

    virtual ErrorCode display_image(const cv::Mat& image) = 0;
    // 纯虚方法：将 cv::Mat 图像转换为 OLED 单色位图并显示；
    // 通常用于显示处理后的二值化图像或简单示意图；
    // 注意：128×64 OLED 分辨率极低，图像需经过降分辨率处理后才可辨识

    virtual ErrorCode display_results(const ModelOutput& results) = 0;
    // 纯虚方法：将 AI 推理结果格式化后显示到 OLED；
    // 根据 ModelOutput 中的有效子结构体（classification/detection/anomaly）
    // 自动选择显示内容和格式

    virtual ErrorCode display_status(const SystemStatus& status) = 0;
    // 纯虚方法：将系统状态快照格式化后显示到 OLED；
    // 通常显示 CPU 温度、内存使用率、各线程运行状态等关键指标

    virtual ErrorCode log_data(const DataLog& data_log) = 0;
    // 纯虚方法：将完整数据日志条目写入本地存储（CSV/JSON 文件）；
    // 写入频率由 DisplayLoggingConfig::LogFrequency 中的各字段控制

    virtual std::vector<DataLog> get_recent_logs(int count = 100) = 0;
    // 纯虚方法：从本地存储中读取最近 count 条日志记录并返回；
    // 默认返回最近 100 条，按时间倒序排列（最新条目在最前）

    virtual ErrorCode export_logs(const std::string& export_path) = 0;
    // 纯虚方法：将日志文件导出到指定路径（如 USB 存储设备或远程目录）；
    // 支持将多个日志文件打包或复制到目标目录

    virtual TaskType  determine_task(const RawData& data) = 0;
    // 纯虚方法：根据原始数据的内容和来源推断当前应执行的 AI 任务类型；
    // 用于在运行时动态切换任务类型（如检测到振动数据则切换至分类任务）

    // 菜单系统：写入一条异常记录（class_id != 0 时调用）
    virtual ErrorCode save_abnormal_record(const AbnormalRecord& record,
                                           const cv::Mat& frame = cv::Mat()) = 0;
    // 纯虚方法（菜单系统扩展）：将异常事件记录写入对应项目的 CSV 日志文件；
    // record 包含异常的全部元数据；
    // frame 为可选的截图图像（默认为空 Mat），
    // 非空时实现层应将 frame 保存为 JPEG 文件，并将路径写入 record.image_path

    // 菜单系统：读取指定项目近 7 天异常记录
    virtual ErrorCode load_week_abnormal(ProjectType pt,
                                         std::vector<AbnormalRecord>& out) = 0;
    // 纯虚方法（菜单系统扩展）：读取指定项目近 7 天内的全部异常记录；
    // pt 指定项目类型（FIRE_DETECTION 或 BEARING_VIBRATION）；
    // out 为输出参数，成功时填充异常记录列表（按时间升序排列）；
    // 用于菜单系统 "Recent Alarms" 页面的数据来源

    // 菜单系统：导出近 7 天日志到目标目录
    virtual ErrorCode export_week_logs(const std::string& export_dir) = 0;
    // 纯虚方法（菜单系统扩展）：将近 7 天的全部日志文件（含异常记录 CSV 和截图）
    // 复制到 export_dir 指定的目标目录；
    // 通常在用户通过菜单选择"导出日志"操作时触发
};
// 结束 IDisplayLogger 接口类定义

class IExtendedFeatures {
// 扩展功能模块抽象接口：规范系统固件更新、报警通知、
// 远程监控、自动校准和电源管理等非核心检测功能的操作接口
public:
    virtual ~IExtendedFeatures() = default;
    // 虚析构函数：确保正确释放网络连接、短信客户端等外部资源

    virtual ErrorCode check_for_updates() = 0;
    // 纯虚方法：向更新服务器查询是否有新版本固件或模型可用；
    // 成功且有更新时返回 ErrorCode::SUCCESS，无更新时应返回特定状态码

    virtual ErrorCode perform_update() = 0;
    // 纯虚方法：下载并安装最新版本固件或模型；
    // 须在 check_for_updates() 确认有更新后调用；
    // 更新过程中系统状态应切换为 SystemState::UPDATING

    virtual ErrorCode switch_to_bootloader() = 0;
    // 纯虚方法：将系统切换到引导加载程序（Bootloader）模式；
    // 用于强制进入固件烧写模式，通常在硬件故障恢复场景中使用

    virtual ErrorCode switch_to_application() = 0;
    // 纯虚方法：从 Bootloader 模式切换回应用程序模式；
    // 固件烧写完成后调用，重新启动应用程序

    virtual ErrorCode verify_checksum() = 0;
    // 纯虚方法：验证固件或模型文件的完整性校验和（CRC32 或 SHA256）；
    // 对应 ProjectDefines.hpp 中 ENABLE_CRC_CHECK 宏的功能实现

    virtual ErrorCode send_sms_alert(const std::string& message,
                                     const std::vector<std::string>& phone_numbers) = 0;
    // 纯虚方法：通过短信网关向指定手机号列表发送报警通知；
    // message 为短信正文；phone_numbers 为接收号码列表；
    // 对应 AlarmConfig::Methods::enable_sms 开关

    virtual ErrorCode send_email_alert(const std::string& subject,
                                       const std::string& message) = 0;
    // 纯虚方法：通过 SMTP 向配置的邮件接收人列表发送报警邮件；
    // subject 为邮件主题，message 为邮件正文；
    // 对应 AlarmConfig::Methods::enable_email 开关

    virtual ErrorCode connect_to_server(const std::string& server_url) = 0;
    // 纯虚方法：建立与远程监控服务器的 HTTP/WebSocket 连接；
    // server_url 为服务器地址（如 "http://192.168.1.100:8080"）；
    // 对应 SystemConfig::NetworkConfig::enable_remote_access 开关

    virtual ErrorCode send_diagnostic_data(const SystemStatus& status) = 0;
    // 纯虚方法：将系统状态快照上传到远程监控服务器；
    // status 为待上传的系统状态数据；
    // 按 GeneralSystemConfig::NetworkConfig::heartbeat_interval 定时调用

    virtual ErrorCode auto_calibrate() = 0;
    // 纯虚方法：执行传感器自动校准流程（零点偏移校正、增益校准）；
    // 对应 IFramework::calibrate() 的具体实现，
    // 校准期间系统状态切换为 SystemState::CALIBRATING

    virtual ErrorCode set_power_mode(int mode) = 0;
    // 纯虚方法：切换系统电源工作模式；
    // mode 对应 PowerManagementConfig::PowerMode 列表的索引：
    //   0=HIGH_PERFORMANCE, 1=BALANCED, 2=POWER_SAVING, 3=SLEEP

    virtual float     get_battery_level() = 0;
    // 纯虚方法：读取当前电池电量百分比（0~100）；
    // 通过 ADC 或电量计 IC（如 MAX17043）获取；
    // 返回 -1.0f 表示不支持电池监测（使用市电供电的树莓派场景）
};
// 结束 IExtendedFeatures 接口类定义

} // namespace MultiProject
// 关闭命名空间 MultiProject