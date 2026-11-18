/**
  ******************************************************************************
  * @file    bearing_vibration_config.hpp
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
 * @brief   轴承振动监测系统 — 全局配置头文件
 * ----------------------------------------------------------------------------
 * @details 本文件以 C++ 嵌套结构体（struct）的形式，集中定义了轴承振动监测系统
 *          在树莓派（Linux）平台上运行所需的全部参数配置。
 *          文件不包含任何可执行逻辑，仅作为编译期常量与默认值的声明载体，
 *          供各功能模块（采集、分析、AI推理、报警、日志等）以值初始化方式引用。
 *
 *          内部共定义以下 8 个一级配置结构体，最终汇聚于顶层结构体
 *          BearingVibrationConfig：
 *
 *          1. AD4134Config               — AD4134 ADC 传感器硬件接口配置
 *             包含 SPI 总线参数、采样模式、数字滤波器、PGA 增益及寄存器初值。
 *
 *          2. VibrationAnalysisConfig    — 振动信号分析算法配置
 *             包含时域统计量、FFT/功率谱密度、包络解调及多维特征提取开关。
 *
 *          3. BearingAssessmentConfig    — 轴承健康状态评估配置
 *             包含 ISO 10816 振动等级标准、轴承故障特征频率库及健康指数权重。
 *
 *          4. AIModelConfig              — ONNX 格式 AI 推理模型配置
 *             包含模型路径、输入图像规格（224×224 单通道 CWT 融合图）、
 *             归一化参数（mean=33.405, std=78.795）及输出类别映射。
 *             注：本版本已完成 4 处关键修复（路径、输入规格、类别名称、推理缓存）。
 *
 *          5. PredictiveMaintenanceConfig — 预测性维护配置
 *             包含 RUL（剩余使用寿命）预测参数与条件维护（CBM）调度策略。
 *
 *          6. AlarmConfig                — 多级报警配置
 *             包含速度/加速度/峭度报警阈值、触发延迟及 LED/蜂鸣器/继电器输出配置。
 *
 *          7. DisplayLoggingConfig       — 显示与数据记录配置
 *             包含 OLED 刷新内容、本地 CSV 日志策略及远程 HTTP 上传配置。
 *
 *          8. SystemConfig               — 系统级运行参数配置
 *             包含采样缓冲、定期自检项目/阈值、电源管理模式及以太网网络参数。
 *
 * @note    所有默认值均在结构体成员声明处通过"= 值"方式完成值初始化，
 *          运行时可通过 BearingVibrationConfig::loadFromFile() 从外部文件覆盖。
 *
 * @par     命名空间：BearingVibrationConfig
 *          所有类型均位于该命名空间内，避免与系统全局符号冲突。
 * ============================================================================
 */

// config/bearing_vibration_config.hpp
// 文件路径注释，标识本文件在项目目录树中的相对位置（config 子目录）

#ifndef BEARING_VIBRATION_CONFIG_HPP
// 头文件保护宏：若本头文件已被包含（宏已定义），则跳过全部内容，防止重复包含
#define BEARING_VIBRATION_CONFIG_HPP
// 定义头文件保护宏，与上方 #ifndef 配对使用

#include <string>   // 引入 std::string，用于存储路径、类型名称、IP 地址等字符串成员
#include <vector>   // 引入 std::vector，用于存储动态数量的分类标签（classes 字段）
#include <map>      // 引入 std::map，用于存储轴承型号到故障频率系数的键值对映射表

namespace BearingVibrationConfig {
// 开启命名空间 BearingVibrationConfig，将以下所有类型定义包裹其中，
// 避免与其他模块（如 FireDetectionConfig）的同名结构体产生符号冲突

// ============================================================================
// AD4134传感器配置
// ============================================================================
// 分节注释：以下结构体 AD4134Config 集中管理 AD4134 四通道 ADC 芯片的全部硬件参数

struct AD4134Config {
    // 本结构体为 AD4134 ADC 芯片的顶层配置容器，
    // 通过嵌套子结构体分类管理 SPI 总线、采样、滤波器、增益和寄存器五类参数

    // SPI接口配置
    // 子结构体 SPIInterface：描述树莓派与 AD4134 之间 SPI 总线的通信参数
    struct SPIInterface {
        int channel = 0;              // SPI通道（0或1）
                                      // 指定使用的 SPI 总线编号：0 对应 /dev/spidev0.x，
                                      // 1 对应 /dev/spidev1.x；树莓派默认使用通道 0

        int speed_hz = 1000000;       // SPI速度（Hz）
                                      // SPI 时钟频率，单位为赫兹（Hz）
                                      // 默认 1,000,000 Hz = 1 MHz；
                                      // AD4134 最大支持 80 MHz，此处保守设置以保障稳定性

        int mode = 3;                 // SPI模式（0-3）
                                      // SPI 工作模式由 CPOL（时钟极性）和 CPHA（时钟相位）决定：
                                      // 模式0: CPOL=0, CPHA=0；模式1: CPOL=0, CPHA=1；
                                      // 模式2: CPOL=1, CPHA=0；模式3: CPOL=1, CPHA=1（默认）
                                      // AD4134 数据手册要求使用模式 3

        int bits_per_word = 8;        // 每字位数
                                      // SPI 每次传输的数据位宽，标准值为 8 位（1 字节）；
                                      // 设置为 8 时，驱动层按字节为单位完成读写

        int cs_pin = 8;               // 片选引脚（BCM编号）
                                      // GPIO BCM 编号 8 对应物理引脚 24（SPI0_CE0），
                                      // 低电平有效；芯片选中期间 SPI 总线处于激活状态
    } spi;
    // 声明 spi 成员变量，类型为上方匿名结构体 SPIInterface，
    // 通过值初始化方式设定各默认值

    // 采样配置
    // 子结构体 Sampling：描述 AD4134 的数字采样行为参数
    struct Sampling {
        float sample_rate_hz = 50000.0f; // 采样率（Hz）
                                          // 每通道每秒采集的样本数，默认 50 kHz；
                                          // 根据奈奎斯特定理，可分析的最高频率为 25 kHz，
                                          // 满足轴承高频故障特征（通常 < 20 kHz）的检测需求

        int num_channels = 3;         // 通道数（X, Y, Z）
                                      // 同时采集三轴加速度信号：X 轴、Y 轴、Z 轴，
                                      // 用于三维振动分析，捕获不同方向的故障激励

        int samples_per_read = 100;   // 每次读取的样本数
                                      // 每次 SPI 读取操作返回的样本数量，
                                      // 100 个样本 / 50000 Hz = 2 ms 的数据块，
                                      // 决定数据处理的最小时间粒度和 CPU 调度频率

        bool continuous_mode = true;  // 连续模式
                                      // 为 true 时 AD4134 持续转换并推送数据（连续转换模式）；
                                      // 为 false 时每次转换需主机发送触发指令（单次触发模式）

        bool use_fifo = true;         // 使用FIFO
                                      // 为 true 时启用 AD4134 内部 FIFO 缓冲区；
                                      // FIFO 可吸收 SPI 读取延迟抖动，避免数据丢失，
                                      // 在系统负载波动时尤为重要
    } sampling;
    // 声明 sampling 成员变量

    // 滤波器配置
    // 子结构体 Filter：配置 AD4134 片上数字滤波器参数
    struct Filter {
        bool enable_filter = true;    // 启用滤波器
                                      // 为 true 时激活 AD4134 内置数字滤波器；
                                      // 滤波器在 ADC 内部完成，不消耗主机 CPU 资源

        int filter_type = 0;          // 滤波器类型（0-3）
                                      // AD4134 支持多种滤波器拓扑，按数据手册编码：
                                      // 0: Sinc5+Sinc1（默认，低通特性好，适合振动分析）；
                                      // 1~3: 其他可选滤波器类型，需参照数据手册确认

        int filter_order = 3;         // 滤波器阶数
                                      // 滤波器的阶数，数值越高滚降越陡，带外抑制越强，
                                      // 但同时引入的群延迟也越大

        float cutoff_frequency = 20000.0f; // 截止频率（Hz）
                                           // 低通滤波器的 -3 dB 截止频率，默认 20 kHz；
                                           // 覆盖轴承高频故障特征频率范围（< 20 kHz）

        bool enable_notch = true;     // 启用陷波滤波器
                                      // 为 true 时叠加工频陷波（带阻）滤波器，
                                      // 专门抑制电源工频干扰

        float notch_frequency = 50.0f; // 陷波频率（Hz）
                                       // 陷波滤波器的中心频率，默认 50 Hz；
                                       // 对应中国工频标准（50 Hz），在欧洲、亚洲等地区适用
    } filter;
    // 声明 filter 成员变量

    // 增益配置
    // 子结构体 Gain：配置 AD4134 可编程增益放大器（PGA）参数
    struct Gain {
        int pga_gain = 1;             // PGA增益（1, 2, 4, 8, 16, 32, 64, 128）
                                      // 可编程增益放大器的增益系数，合法值为 2 的幂次方；
                                      // 默认值 1 表示不放大；在传感器信号微弱时应增大增益

        float reference_voltage = 2.5f; // 参考电压（V）
                                        // ADC 满量程参考电压，单位伏特（V），默认 2.5 V；
                                        // 输入信号的最大可测量电压 = reference_voltage / pga_gain

        bool enable_burnout_current = false; // 启用烧毁电流检测
                                              // 为 true 时向模拟输入端注入微小电流，
                                              // 用于检测传感器线路断路（烧毁/断线故障）；
                                              // 默认关闭，避免对正常传感器信号造成干扰
    } gain;
    // 声明 gain 成员变量

    // 寄存器配置（根据AD4134数据手册）
    // 子结构体 Registers：存储 AD4134 各关键寄存器的初始化值
    struct Registers {
        uint8_t device_config = 0x01;     // 设备配置寄存器
                                          // 对应 AD4134 DEVICE_CONFIG 寄存器（地址参见数据手册）；
                                          // 0x01 通常表示使能器件、选择默认工作模式

        uint8_t data_packet_config = 0x00; // 数据包配置
                                           // 控制 SPI 数据包的格式，包括通道使能掩码、
                                           // 数据头/尾字节是否附加等；0x00 为出厂默认值

        uint8_t dig_if_config = 0x00;     // 数字接口配置
                                          // 配置数字接口特性，如 SPI 字节顺序（MSB/LSB first）、
                                          // DOUT 极性等；0x00 为默认配置

        uint8_t filter_config = 0x00;     // 滤波器配置
                                          // 映射至 AD4134 片上滤波器选择寄存器；
                                          // 具体位域含义需结合 filter_type/filter_order 写入

        uint8_t gain_config = 0x00;       // 增益配置
                                          // 映射至 AD4134 PGA 增益选择寄存器；
                                          // 运行时应根据 pga_gain 字段计算并写入对应位域
    } registers;
    // 声明 registers 成员变量，存储各寄存器的字节初值
};
// 结束 AD4134Config 结构体定义

// ============================================================================
// 振动分析配置
// ============================================================================
// 分节注释：以下结构体 VibrationAnalysisConfig 集中管理信号处理与特征提取算法参数

struct VibrationAnalysisConfig {
    // 本结构体为振动信号分析算法的参数容器，
    // 通过嵌套子结构体分类管理时域分析、频域分析、包络分析和特征提取四类算法开关

    // 时域分析
    // 子结构体 TimeDomain：控制各时域统计量的计算开关
    struct TimeDomain {
        bool calculate_rms = true;        // 计算RMS
                                          // 为 true 时计算信号均方根值（Root Mean Square），
                                          // 反映振动能量的总体大小，是 ISO 10816 的核心评估指标

        bool calculate_peak = true;       // 计算峰值
                                          // 为 true 时计算信号波形的绝对最大值（Peak Value）；
                                          // 对冲击性故障（如滚动体点蚀）具有较高敏感性

        bool calculate_peak_to_peak = true; // 计算峰峰值
                                            // 为 true 时计算正峰值与负峰值之差（Peak-to-Peak），
                                            // 反映振动位移的全幅变化范围

        bool calculate_crest_factor = true; // 计算峰值因子
                                            // 为 true 时计算峰值与 RMS 之比（Crest Factor），
                                            // 无量纲指标；正常轴承约 2.5~3.5，
                                            // 出现点蚀故障时通常升至 6 以上

        bool calculate_kurtosis = true;   // 计算峭度
                                          // 为 true 时计算信号的四阶统计矩（Kurtosis）；
                                          // 高斯信号理论峭度值为 3，轴承早期故障时峭度显著升高，
                                          // 是检测冲击性故障的灵敏指标

        bool calculate_skewness = true;   // 计算偏度
                                          // 为 true 时计算信号分布的三阶矩（Skewness）；
                                          // 反映信号幅值分布的不对称性，正常轴承偏度接近 0
    } time_domain;
    // 声明 time_domain 成员变量

    // 频域分析
    // 子结构体 FrequencyDomain：配置 FFT 及频谱分析参数
    struct FrequencyDomain {
        bool enable_fft = true;           // 启用FFT
                                          // 为 true 时对采集数据执行快速傅里叶变换（FFT），
                                          // 将时域信号转换至频域，提取频率成分信息

        int fft_size = 1024;              // FFT大小
                                          // FFT 计算点数，必须为 2 的幂次方；
                                          // 1024 点 @ 50 kHz 的频率分辨率 = 50000/1024 ≈ 48.8 Hz；
                                          // 更大的 FFT 尺寸提升频率分辨率，但增加计算量

        std::string window_type = "hann"; // 窗函数类型
                                          // FFT 前加窗以抑制频谱泄漏效应，可选值示例：
                                          // "hann"（汉宁窗，适合振动信号，泄漏小）、
                                          // "hamming"（汉明窗）、"blackman"（旁瓣抑制更强）

        bool calculate_spectrum = true;   // 计算频谱
                                          // 为 true 时输出幅度谱或功率谱的频谱数组，
                                          // 用于可视化及故障频率成分识别

        bool calculate_psd = true;        // 计算功率谱密度
                                          // 为 true 时计算功率谱密度（Power Spectral Density，PSD），
                                          // 对频谱进行归一化处理（除以频率分辨率），
                                          // 便于不同采样条件下的能量比较

        bool enable_band_analysis = true; // 启用频带分析
                                          // 为 true 时按预定义频带分别统计各频段内的能量，
                                          // 辅助定位故障所在频带

        // 频带定义（Hz）
        // 子结构体 FrequencyBands：定义低/中/高三个频带的分界频率
        struct FrequencyBands {
            float low_freq = 10.0f;       // 低频带
                                          // 低频段上限，默认 10 Hz；
                                          // 该频带主要反映不平衡、不对中等低速机械故障

            float mid_freq = 100.0f;      // 中频带
                                          // 中频段上限，默认 100 Hz；
                                          // 该频带包含轴频及低阶轴承故障特征频率

            float high_freq = 1000.0f;    // 高频带
                                          // 高频段上限，默认 1000 Hz；
                                          // 该频带包含高阶轴承故障特征频率及共振频率
        } bands;
        // 声明 bands 成员变量
    } frequency_domain;
    // 声明 frequency_domain 成员变量

    // 包络分析（用于轴承故障诊断）
    // 子结构体 EnvelopeAnalysis：配置包络解调分析参数，专用于提取轴承故障冲击的调制特征
    struct EnvelopeAnalysis {
        bool enable_envelope = true;      // 启用包络分析
                                          // 为 true 时启用包络分析流程：
                                          // 带通滤波 → Hilbert 变换 → 取模 → 包络谱 FFT；
                                          // 是识别轴承内外圈、滚动体故障的核心算法

        float demodulation_freq = 1000.0f; // 解调频率
                                           // 包络解调所用带通滤波器的中心频率，单位 Hz；
                                           // 默认 1000 Hz，应对准共振峰频率以最大化信噪比

        int envelope_order = 4;           // 包络阶数
                                          // 包络谱分析时考虑的最高谐波阶次；
                                          // 例如：阶次=4 时检查基频及 2f、3f、4f 谐波

        bool enable_cepstrum = false;     // 启用倒频谱分析
                                          // 为 true 时对对数功率谱再做一次 FFT（倒频谱，Cepstrum）；
                                          // 可检测频谱中等间隔谐波族，有助于齿轮与轴承复合故障分析；
                                          // 默认关闭以节省计算资源
    } envelope;
    // 声明 envelope 成员变量

    // 特征提取
    // 子结构体 FeatureExtraction：控制送入 AI 模型前的特征工程参数
    struct FeatureExtraction {
        int num_features = 20;            // 特征数量
                                          // 最终输出的特征向量维度，默认 20 维；
                                          // 包含时域、频域、统计特征的组合，
                                          // 需与 AI 模型的输入特征数保持一致

        bool time_features = true;        // 时域特征
                                          // 为 true 时将时域统计量（RMS、峰值、峭度等）
                                          // 纳入特征向量

        bool frequency_features = true;   // 频域特征
                                          // 为 true 时将频域特征（频带能量、主频幅值等）
                                          // 纳入特征向量

        bool statistical_features = true; // 统计特征
                                          // 为 true 时将高阶统计特征（偏度、峰度等）
                                          // 纳入特征向量

        bool wavelet_features = false;    // 小波特征
                                          // 为 true 时计算离散小波变换（DWT）系数的统计量，
                                          // 提供时频联合特征；默认关闭以降低计算开销
    } features;
    // 声明 features 成员变量
};
// 结束 VibrationAnalysisConfig 结构体定义

// ============================================================================
// 轴承状态评估配置
// ============================================================================
// 分节注释：以下结构体 BearingAssessmentConfig 管理轴承健康状态评估所需的标准与算法参数

struct BearingAssessmentConfig {
    // 本结构体为轴承状态评估的参数容器，
    // 通过嵌套子结构体管理振动等级标准、故障特征频率库和健康指数计算三类配置

    // 振动等级标准（根据ISO 10816）
    // 子结构体 VibrationStandards：依照 ISO 10816 标准定义多级振动判断阈值
    struct VibrationStandards {
        // 速度有效值（mm/s） - 针对不同转速
        // 子结构体 VelocityRMS：振动速度有效值（mm/s）的五级分级阈值
        struct VelocityRMS {
            float excellent = 0.45f;      // 优秀
                                          // 速度有效值 ≤ 0.45 mm/s：振动状态优秀，
                                          // 对应 ISO 10816 Zone A（新安装机组）

            float good = 0.71f;           // 良好
                                          // 速度有效值 ≤ 0.71 mm/s：振动状态良好，
                                          // 对应 ISO 10816 Zone A/B 边界区域

            float acceptable = 1.12f;     // 可接受
                                          // 速度有效值 ≤ 1.12 mm/s：振动尚可接受，
                                          // 对应 ISO 10816 Zone B（长期运行允许范围）

            float unsatisfactory = 1.8f;  // 不满意
                                          // 速度有效值 ≤ 1.80 mm/s：振动不满意，
                                          // 对应 ISO 10816 Zone C（允许短期运行，需计划维护）

            float unacceptable = 2.8f;    // 不可接受
                                          // 速度有效值 > 2.80 mm/s：振动不可接受，
                                          // 对应 ISO 10816 Zone D（立即停机检修）
        } velocity;
        // 声明 velocity 成员变量

        // 加速度峰值（g）
        // 子结构体 AccelerationPeak：振动加速度峰值（以 g 为单位）的四级分级阈值
        struct AccelerationPeak {
            float normal = 0.5f;          // 正常
                                          // 加速度峰值 ≤ 0.5 g：设备运行状态正常

            float attention = 1.0f;       // 注意
                                          // 加速度峰值 ≤ 1.0 g：需关注振动变化趋势

            float alarm = 2.0f;           // 报警
                                          // 加速度峰值 ≤ 2.0 g：触发报警，需尽快安排检查

            float danger = 4.0f;          // 危险
                                          // 加速度峰值 > 4.0 g：进入危险状态，须立即处置
        } acceleration;
        // 声明 acceleration 成员变量
    } standards;
    // 声明 standards 成员变量

    // 故障频率库（不同轴承类型）
    // 子结构体 FaultFrequencies：存储不同型号轴承的故障特征频率系数映射表
    struct FaultFrequencies {
        // 轴承故障频率系数
        // 子结构体 BearingTypeA：单一轴承型号的四类故障特征频率系数定义
        // 实际故障频率 = 系数 × 轴转频（Hz）
        struct BearingTypeA {
            float ball_pass_frequency_outer = 3.05f;  // 外圈故障频率系数
                                                       // 滚动体通过外圈故障点的特征频率系数
                                                       // (Ball Pass Frequency Outer Race, BPFO)；
                                                       // 外圈故障时在包络谱中以此频率出现冲击

            float ball_pass_frequency_inner = 4.95f;  // 内圈故障频率系数
                                                       // 滚动体通过内圈故障点的特征频率系数
                                                       // (Ball Pass Frequency Inner Race, BPFI)；
                                                       // 内圈故障时包络谱中以此频率出现调制边带

            float ball_spin_frequency = 2.0f;         // 滚动体故障频率系数
                                                       // 滚动体自身旋转产生的故障特征频率系数
                                                       // (Ball Spin Frequency, BSF)；
                                                       // 滚动体剥落时在包络谱中以此频率出现成分

            float fundamental_cage_frequency = 0.4f;  // 保持架故障频率系数
                                                       // 保持架公转产生的故障特征频率系数
                                                       // (Fundamental Train Frequency, FTF)；
                                                       // 保持架磨损时在振动信号中出现该频率成分
        };
        // 结束 BearingTypeA 内嵌结构体定义

        // 不同类型轴承的配置
        // bearing_types：轴承型号字符串到 BearingTypeA 系数结构体的映射表；
        // 通过型号字符串（如 "SKF_6205"）快速查询对应的四类故障频率系数
        std::map<std::string, BearingTypeA> bearing_types = {
            {"SKF_6205", {3.05f, 4.95f, 2.0f, 0.4f}},
            // SKF 6205 型号深沟球轴承的故障频率系数：
            // BPFO=3.05, BPFI=4.95, BSF=2.0, FTF=0.4

            {"NSK_6206", {3.04f, 4.96f, 1.98f, 0.41f}},
            // NSK 6206 型号深沟球轴承的故障频率系数：
            // BPFO=3.04, BPFI=4.96, BSF=1.98, FTF=0.41
        };
        // 映射表末尾逗号符合 C++11 聚合初始化语法，便于后续追加新型号
    } fault_frequencies;
    // 声明 fault_frequencies 成员变量

    // 健康指数计算
    // 子结构体 HealthIndex：配置综合健康指数（Health Index）的计算参数
    struct HealthIndex {
        bool enable_health_index = true;  // 启用健康指数
                                          // 为 true 时系统周期性计算归一化健康指数（0~1），
                                          // 1.0 表示完全健康，0.0 表示严重故障

        int history_days = 30;            // 历史数据天数
                                          // 计算健康指数趋势所使用的历史时间窗口长度（天）；
                                          // 用于趋势拟合及退化速率估计

        float warning_threshold = 0.7f;   // 警告阈值（0-1）
                                          // 健康指数低于 0.7 时触发预警通知，
                                          // 建议安排计划性检查

        float alarm_threshold = 0.5f;     // 报警阈值
                                          // 健康指数低于 0.5 时触发报警，
                                          // 需在近期内安排维护或更换

        float critical_threshold = 0.3f;  // 临界阈值
                                          // 健康指数低于 0.3 时进入临界状态，
                                          // 应立即停机处理以防止次生损坏

        // 健康指数权重
        // 子结构体 Weights：定义各子评估维度对综合健康指数的贡献权重（各项总和应为 1.0）
        struct Weights {
            float vibration_level = 0.3f;   // 振动水平权重
                                             // 振动有效值等能量指标对健康指数的贡献占比 30%

            float trend_analysis = 0.25f;   // 趋势分析权重
                                             // 振动趋势（退化速率）对健康指数的贡献占比 25%

            float fault_frequency = 0.25f;  // 故障频率权重
                                             // 故障特征频率成分幅值对健康指数的贡献占比 25%

            float statistical_features = 0.2f; // 统计特征权重
                                                // 峭度、偏度等统计特征对健康指数的贡献占比 20%
        } weights;
        // 声明 weights 成员变量；
        // 四个权重之和：0.30 + 0.25 + 0.25 + 0.20 = 1.00，满足归一化约束
    } health_index;
    // 声明 health_index 成员变量
};
// 结束 BearingAssessmentConfig 结构体定义

// ============================================================================
// AI模型配置
// ============================================================================
// 分节注释：以下结构体 AIModelConfig 集中管理 ONNX 推理模型的路径、输入规格及输出映射

struct AIModelConfig {
    // 本结构体为 AI 推理模型的全局参数容器；
    // 本版本已针对与训练配置不一致的问题完成 4 处关键修复（见各字段注释）

    // 【修复1】模型文件路径 — 与实际部署的 .onnx 文件名一致
    // 原来: "../models/bearing_vibration/vibration_model.onnx"  ← 文件不存在！
    std::string model_path = "../models/bearing_vibration/bearing_vibration_model.onnx";
    // 修复后的 ONNX 模型文件相对路径，可执行文件工作目录应在 build/ 下；
    // 文件名 bearing_vibration_model.onnx 与实际导出文件名保持一致，
    // 原路径中的 vibration_model.onnx 不存在，会导致运行时加载失败

    // 模型类型
    std::string model_type = "classification"; // classification/regression/anomaly
    // 当前模型的任务类型标识字符串，供框架层选择对应的后处理逻辑：
    // "classification"：多分类任务，输出各类别概率；
    // "regression"：回归任务，输出连续数值（如 RUL）；
    // "anomaly"：异常检测任务，输出异常得分

    // 【修复2】输入配置 — 与训练 config 一致
    // 原来: sequence_length=1000, num_features=20 ← 这是时序模型的参数，不是图像模型的
    // 实际: 输入为 1通道 224x224 灰度 CWT 融合图，归一化 (pixel-33.405)/78.795
    struct InputConfig {
        // 修复后，输入类型由时序特征向量更改为单通道灰度图像，
        // 与训练数据预处理流程（CWT 时频图 → 灰度 → 224×224 → 归一化）完全一致

        int input_channels = 1;            // 单通道灰度图
                                           // 模型输入张量的通道数为 1（灰度图），
                                           // 对应 img_norm_cfg 中 to_rgb=False 的设置

        int input_height = 224;            // 图像高度
                                           // 输入图像高度为 224 像素，
                                           // 与训练时的 resize 目标尺寸一致

        int input_width = 224;             // 图像宽度
                                           // 输入图像宽度为 224 像素，
                                           // 与训练时的 resize 目标尺寸一致

        bool normalize_input = true;       // 启用归一化
                                           // 为 true 时在送入模型前对像素值执行 Z-score 归一化：
                                           // normalized = (pixel - mean) / std

        // 与训练 config 完全一致:
        //   img_norm_cfg = dict(mean=[33.405], std=[78.795], to_rgb=False)
        //   预处理: (pixel - mean) / std
        float mean = 33.405f;
        // 训练数据集灰度像素值的均值，由 CWT 图像统计得到；
        // 推理时须使用与训练完全相同的均值，否则导致输入分布偏移，降低模型精度

        float std = 78.795f;
        // 训练数据集灰度像素值的标准差，由 CWT 图像统计得到；
        // 推理时须使用与训练完全相同的标准差
    } input;
    // 声明 input 成员变量
    
    struct OutputConfig {
        // 本结构体管理模型输出端的解析配置，按任务类型分为三个子结构体

        // 分类任务
        struct Classification {
            // 原来: {"正常","轻微磨损","中度磨损","严重磨损","故障"} ← 5个中文类，完全错误！
            // 修复: 与训练数据的 4 个类别一致，顺序对应 class_id 0/1/2/3
            std::vector<std::string> classes = {
                "denormal",      // 0: 正常
                                 // class_id=0，对应训练标签"正常运行状态"，
                                 // ONNX 输出 softmax 向量的第 0 个元素

                "deballfault",   // 1: 滚动体故障
                                 // class_id=1，对应训练标签"滚动体（Ball）故障"

                "decagefault",   // 2: 保持架故障
                                 // class_id=2，对应训练标签"保持架（Cage）故障"

                "deouterrace"    // 3: 外圈故障
                                 // class_id=3，对应训练标签"外圈（Outer Race）故障"
            };
            // vector 长度须与模型最后一层 softmax/sigmoid 的输出节点数完全一致（此处为 4）；
            // 推理后取 argmax 所得 class_id 即作为 classes 向量的索引，映射至类别名称

            float confidence_threshold = 0.7f; // 置信度阈值
                                                // 推理输出的最高类别概率若低于 0.7，
                                                // 则视为"置信度不足"，不触发故障判定，
                                                // 避免将模糊边界样本误判为故障
        } classification;
        // 声明 classification 成员变量
        
        // 回归任务
        // 子结构体 Regression：当 model_type="regression" 时的输出范围配置
        struct Regression {
            float min_value = 0.0f;        // 最小值
                                           // 回归输出的物理量最小值（如 RUL=0 天表示已到寿命终点）

            float max_value = 10.0f;       // 最大值
                                           // 回归输出的物理量最大值，用于输出值的合理性校验
        } regression;
        // 声明 regression 成员变量
        
        // 异常检测
        // 子结构体 AnomalyDetection：当 model_type="anomaly" 时的异常判断配置
        struct AnomalyDetection {
            float anomaly_threshold = 0.8f; // 异常阈值
                                            // 异常得分超过 0.8 时判定为异常状态；
                                            // 阈值越低越灵敏（易误报），越高越保守（易漏报）

            bool use_autoencoder = true;   // 使用自编码器
                                           // 为 true 时使用自编码器（Autoencoder）模型，
                                           // 通过重构误差计算异常得分
        } anomaly;
        // 声明 anomaly 成员变量
    } output;
    // 声明 output 成员变量
    
    // 【修复4】推理配置 — 关闭缓存
    struct InferenceConfig {
        // 子结构体 InferenceConfig：控制 ONNX Runtime 的推理加速与缓存策略

        bool use_tensorrt = false;         // 使用TensorRT加速
                                           // 为 true 时启用 NVIDIA TensorRT 执行提供者（EP）；
                                           // 树莓派 CPU 推理场景下应保持 false，
                                           // TensorRT 仅在配备 NVIDIA GPU 的平台上可用

        int max_batch_size = 1;            // 最大批量大小
                                           // 推理时每次处理的最大样本数，默认 1（单样本推理）；
                                           // 批量推理可提高 GPU 吞吐量，但树莓派 CPU 场景无收益

        // 原来: enable_cache = true ← 缓存会把第一次错误结果永久缓存！
        bool enable_cache = false;         // 关闭推理缓存
                                           // 修复：将 enable_cache 从 true 改为 false；
                                           // 开启缓存时，若第一次推理结果有误（如输入归一化错误），
                                           // 错误结果将被永久缓存并在后续所有调用中返回，
                                           // 极难排查；关闭缓存确保每次推理使用最新输入

        int cache_size = 100;              // 缓存大小
                                           // 当 enable_cache=true 时，LRU 缓存的最大条目数；
                                           // 当前 enable_cache=false，此字段无实际效果，
                                           // 保留以备后续条件性启用
    } inference;
    // 声明 inference 成员变量
};
// 结束 AIModelConfig 结构体定义

// ============================================================================
// 预测性维护配置
// ============================================================================
// 分节注释：以下结构体 PredictiveMaintenanceConfig 管理基于状态监测的预测性维护参数

struct PredictiveMaintenanceConfig {
    // 本结构体为预测性维护模块的参数容器，
    // 通过嵌套子结构体分类管理 RUL 预测和维护计划两类配置

    // RUL（剩余使用寿命）预测
    // 子结构体 RULPrediction：配置剩余使用寿命（Remaining Useful Life, RUL）预测参数
    struct RULPrediction {
        bool enable_rul_prediction = true; // 启用RUL预测
                                           // 为 true 时系统周期性运行 RUL 预测算法，
                                           // 估计当前轴承在正常维护条件下的剩余可用天数

        int prediction_horizon = 30;       // 预测范围（天）
                                           // RUL 预测算法向未来推算的最大时间跨度（天）；
                                           // 超出此范围的预测值不予输出（不确定性过大）

        float warning_rul = 15.0f;         // RUL警告阈值（天）
                                           // 预测 RUL ≤ 15 天时触发预警通知，
                                           // 建议提前采购备件并安排计划维护窗口

        float critical_rul = 7.0f;         // RUL临界阈值（天）
                                           // 预测 RUL ≤ 7 天时进入临界状态，
                                           // 须在近期内强制安排更换，防止非计划停机

        // 预测模型
        // 子结构体 PredictionModel：配置 RUL 预测所用退化模型的参数
        struct PredictionModel {
            std::string model_type = "exponential_degradation";
            // RUL 预测使用的退化模型类型标识字符串；
            // "exponential_degradation"：指数退化模型，假设健康指数随时间指数衰减；
            // 该模型参数少、计算简单，适合嵌入式平台实时预测

            float degradation_rate = 0.01f; // 退化率
                                             // 指数退化模型的衰减系数（每天）；
                                             // 健康指数 H(t) = H₀ × exp(-degradation_rate × t)；
                                             // 退化率越大，RUL 估计值越短

            float uncertainty = 0.1f;      // 不确定性
                                           // RUL 预测的相对不确定度（标准差/均值），
                                           // 用于输出预测区间；
                                           // 0.1 表示预测值 ±10% 为置信区间
        } model;
        // 声明 model 成员变量
    } rul;
    // 声明 rul 成员变量

    // 维护计划
    // 子结构体 MaintenanceSchedule：配置基于时间和状态的双轨维护调度策略
    struct MaintenanceSchedule {
        int inspection_interval = 7;       // 检查间隔（天）
                                           // 日常人工巡检周期，默认每 7 天执行一次，
                                           // 包括目测、手动检查松动及简单振动测量

        int lubrication_interval = 90;     // 润滑间隔（天）
                                           // 定期补充润滑脂的时间间隔，默认 90 天；
                                           // 润滑不足是轴承疲劳失效的主要原因之一

        int overhaul_interval = 365;       // 大修间隔（天）
                                           // 轴承全面拆解检修或更换的计划周期，默认 365 天；
                                           // 当 RUL 低于临界阈值时可提前触发大修

        // 基于条件的维护
        bool condition_based_maintenance = true;
        // 为 true 时启用基于状态监测的维护（Condition-Based Maintenance, CBM）策略；
        // CBM 根据振动和温度等实时监测指标动态调整维护触发时机，
        // 优先级高于基于时间的定期维护计划

        float vibration_cbm_threshold = 2.0f; // 振动CBM阈值
                                               // 振动速度有效值超过 2.0 mm/s 时触发 CBM 维护事件；
                                               // 应与 AlarmConfig 中的 warning 阈值保持协调

        float temperature_cbm_threshold = 70.0f; // 温度CBM阈值
                                                  // 轴承座温度超过 70.0 ℃ 时触发 CBM 维护事件；
                                                  // 温升过高表明润滑失效或轴承预紧力过大
    } maintenance;
    // 声明 maintenance 成员变量

    // 历史数据分析
    // 子结构体 HistoricalAnalysis：配置长时间序列分析功能开关
    struct HistoricalAnalysis {
        int max_history_days = 365;        // 最大历史天数
                                           // 系统保留的最大历史数据时间跨度（天），
                                           // 超出部分将被自动归档或删除以控制存储占用

        bool enable_trend_analysis = true; // 启用趋势分析
                                           // 为 true 时对历史健康指数序列执行线性回归或
                                           // 移动平均，识别退化趋势并估计 RUL

        bool enable_seasonal_analysis = false; // 启用季节性分析
                                               // 为 true 时检测振动信号中的周期性季节变化；
                                               // 对户外或环境温差大的场景有用；默认关闭

        bool enable_correlation_analysis = true; // 启用相关性分析
                                                  // 为 true 时计算不同通道（X/Y/Z轴）及
                                                  // 不同特征指标间的相关系数矩阵，
                                                  // 辅助识别故障的各向异性传播特征
    } history;
    // 声明 history 成员变量
};
// 结束 PredictiveMaintenanceConfig 结构体定义

// ============================================================================
// 报警配置
// ============================================================================
// 分节注释：以下结构体 AlarmConfig 管理多级报警阈值、触发延迟和报警输出方式

struct AlarmConfig {
    // 本结构体为报警子系统的参数容器，
    // 通过嵌套子结构体分类管理振动阈值、触发延迟和输出方式三类配置

    // 振动报警阈值
    // 子结构体 VibrationThresholds：定义速度、加速度和峭度三类指标的多级报警门限
    struct VibrationThresholds {
        // 速度有效值（mm/s）
        // 子结构体 Velocity：振动速度有效值（mm/s）的四级报警门限
        struct Velocity {
            float normal_max = 1.8f;       // 正常最大值
                                           // 速度有效值 ≤ 1.8 mm/s 时系统处于正常运行状态

            float warning = 2.8f;          // 警告阈值
                                           // 速度有效值超过 2.8 mm/s 时进入警告状态，
                                           // 触发预警通知，无需立即停机

            float alarm = 4.5f;            // 报警阈值
                                           // 速度有效值超过 4.5 mm/s 时触发报警，
                                           // 需在短时间内安排检修

            float shutdown = 7.1f;         // 停机阈值
                                           // 速度有效值超过 7.1 mm/s 时触发保护性停机；
                                           // 继续运行可能导致设备损坏或安全事故
        } velocity;
        // 声明 velocity 成员变量
        
        // 加速度峰值（g）
        // 子结构体 Acceleration：加速度峰值（g）的四级报警门限
        struct Acceleration {
            float normal_max = 0.5f;       // 正常最大值
                                           // 加速度峰值 ≤ 0.5 g 时处于正常状态

            float warning = 1.0f;          // 警告阈值
                                           // 加速度峰值超过 1.0 g 时触发预警

            float alarm = 2.0f;            // 报警阈值
                                           // 加速度峰值超过 2.0 g 时触发报警

            float shutdown = 4.0f;         // 停机阈值
                                           // 加速度峰值超过 4.0 g 时触发停机保护
        } acceleration;
        // 声明 acceleration 成员变量
        
        // 峭度（无量纲）
        // 子结构体 Kurtosis：峭度（无量纲）的三级报警门限
        struct Kurtosis {
            float normal_max = 3.5f;       // 正常最大值
                                           // 峭度 ≤ 3.5（略高于高斯分布理论值 3.0）属于正常范围；
                                           // 高斯分布的理论峭度为 3，正常轴承信号近似高斯分布

            float warning = 4.0f;          // 警告阈值
                                           // 峭度超过 4.0 时表明信号中已出现明显冲击成分，
                                           // 触发预警，建议加密监测频率

            float alarm = 5.0f;            // 报警阈值
                                           // 峭度超过 5.0 时冲击成分显著增强，触发报警；
                                           // 峭度无停机门限（高峭度仅反映信号特征，需结合其他指标）
        } kurtosis;
        // 声明 kurtosis 成员变量
    } thresholds;
    // 声明 thresholds 成员变量

    // 报警延迟
    // 子结构体 AlarmDelays：各报警级别在阈值触发后到实际执行动作的延迟时间（秒）
    struct AlarmDelays {
        int initial_delay = 0;             // 初始延迟（秒）
                                           // 系统上电后等待传感器稳定的初始静默期，
                                           // 默认 0 秒（即上电后立即进入监测状态）

        int warning_delay = 10;            // 警告延迟（秒）
                                           // 指标持续超过警告阈值 10 秒后才触发警告通知；
                                           // 延迟可过滤短暂瞬态干扰，避免误报

        int alarm_delay = 5;               // 报警延迟（秒）
                                           // 指标持续超过报警阈值 5 秒后触发报警；
                                           // 报警延迟短于警告延迟，因报警优先级更高

        int shutdown_delay = 2;            // 停机延迟（秒）
                                           // 指标持续超过停机阈值 2 秒后触发停机保护；
                                           // 延迟 2 秒可避免单次冲击导致不必要的停机
    } delays;
    // 声明 delays 成员变量

    // 报警方式
    // 子结构体 AlarmMethods：配置各类报警输出通道的启用状态
    struct AlarmMethods {
        bool enable_visual = true;         // 视觉报警（LED）
                                           // 为 true 时通过 GPIO 控制 LED 指示灯闪烁，
                                           // 提供可视化的本地故障指示

        bool enable_audible = true;        // 声音报警（蜂鸣器）
                                           // 为 true 时通过 GPIO 驱动蜂鸣器发出声音警示

        bool enable_sms = true;            // 短信报警
                                           // 为 true 时通过短信网关向维护人员发送故障短信通知；
                                           // 需在网络配置中提供网关地址及接收号码

        bool enable_email = true;          // 邮件报警
                                           // 为 true 时通过 SMTP 发送故障告警邮件；
                                           // 需在系统配置中提供邮件服务器信息

        bool enable_relay = true;          // 继电器输出
                                           // 为 true 时通过 GPIO 驱动继电器，
                                           // 可联动外部设备（如停机控制回路）

        // 继电器控制
        // 子结构体 RelayControl：配置各继电器输出通道对应的 GPIO BCM 引脚编号
        struct RelayControl {
        int bearing_relay_pin = 17;   // BCM17 → K1，轴承故障触发
                                      // GPIO BCM 17 对应物理引脚 11，
                                      // 连接继电器模块 K1，轴承报警时输出低电平触发继电器动作；
                                      // 可接外部声光报警器或机器使能控制回路

        // 注：K2=BCM27 为火焰继电器，由 fire_detection 项目管理
        // 继电器 K2（连接 GPIO BCM 27/物理引脚 13）用于火焰检测系统的报警输出，
        // 由 FireDetectionProject 模块独立管理，本结构体不做声明，避免资源冲突
        } relays;
        // 声明 relays 成员变量
    } methods;
    // 声明 methods 成员变量
};
// 结束 AlarmConfig 结构体定义

// ============================================================================
// 显示和日志配置
// ============================================================================
// 分节注释：以下结构体 DisplayLoggingConfig 管理 OLED 显示、本地数据记录和远程监控配置

struct DisplayLoggingConfig {
    // 本结构体为显示与日志子系统的参数容器，
    // 通过嵌套子结构体分类管理 OLED 显示、数据记录和远程监控三类配置

    // OLED显示配置
    // 子结构体 OLEDDisplay：配置 OLED 屏幕的接口参数与显示内容
    struct OLEDDisplay {
        bool enabled = true;               // 启用OLED
                                           // 为 true 时系统周期性更新 OLED 屏幕内容；
                                           // 设为 false 可关闭屏幕以降低功耗（适合无人值守场景）

        int i2c_address = 0x3C;            // I2C地址
                                           // OLED 模块的 I2C 从机地址，常见值为 0x3C 或 0x3D；
                                           // 具体地址由模块 SA0 引脚电平决定

        int refresh_rate = 1;              // 刷新率（Hz）
                                           // OLED 屏幕每秒更新的次数，默认 1 Hz；
                                           // 较低的刷新率减少 I2C 总线占用和 CPU 负载

        // 显示内容
        // 子结构体 DisplayContent：控制 OLED 屏幕各显示项目的开关
        struct DisplayContent {
            bool show_vibration = true;    // 显示振动值
                                           // 为 true 时在 OLED 上实时显示三轴振动速度有效值（mm/s）

            bool show_health_index = true; // 显示健康指数
                                           // 为 true 时在 OLED 上显示当前综合健康指数（0~1）

            bool show_rul = true;          // 显示剩余寿命
                                           // 为 true 时在 OLED 上显示当前 RUL 预测值（天）

            bool show_alarm_status = true; // 显示报警状态
                                           // 为 true 时在 OLED 上显示当前报警级别（正常/警告/报警/停机）

            bool show_trend = false;       // 显示趋势图
                                           // 为 true 时在 OLED 上绘制健康指数趋势曲线；
                                           // 默认关闭，因小尺寸 OLED（128×64）绘图效果有限
        } content;
        // 声明 content 成员变量
    } oled;
    // 声明 oled 成员变量

    // 数据记录
    // 子结构体 DataLogging：配置本地文件系统的数据持久化策略
    struct DataLogging {
        std::string log_dir = "/var/log/bearing_vibration";
        // 日志文件的根目录路径；
        // 使用 /var/log/ 符合 Linux FHS（文件系统层次标准）的日志目录约定；
        // 系统启动时应确保该目录存在且对运行用户可写

        // 记录频率
        // 子结构体 LogFrequency：各类数据的持久化写入时间间隔（秒）
        struct LogFrequency {
            int raw_data = 10;             // 原始数据记录间隔（秒）
                                           // 每 10 秒将原始采样数据（或精简版本）写入磁盘一次；
                                           // 较短的间隔提高数据完整性，但增加磁盘 I/O

            int processed_data = 60;       // 处理数据记录间隔（秒）
                                           // 每 60 秒写入一次经过分析处理后的统计量

            int features = 300;            // 特征记录间隔（秒）
                                           // 每 300 秒（5 分钟）写入一次特征向量，
                                           // 供事后离线分析和模型再训练使用

            int health_index = 600;        // 健康指数记录间隔（秒）
                                           // 每 600 秒（10 分钟）写入一次健康指数和 RUL 预测值；
                                           // 较长间隔可控制日志文件增长速率
        } frequency;
        // 声明 frequency 成员变量

        // 数据保存
        // 子结构体 DataStorage：配置各类数据文件的保存策略
        struct DataStorage {
            bool save_raw_waveform = false; // 保存原始波形
                                            // 为 true 时将原始时域波形数据全量写入磁盘；
                                            // 默认关闭，因原始波形数据量极大（50 kHz × 3 通道），
                                            // 树莓派存储空间有限

            bool save_spectrum = true;     // 保存频谱
                                           // 为 true 时将 FFT 频谱数据写入磁盘；
                                           // 频谱数据量远小于原始波形，适合长期存储

            bool save_features = true;     // 保存特征
                                           // 为 true 时将提取的特征向量写入磁盘，
                                           // 是事后模型训练和性能评估的基础数据

            bool save_alarms = true;       // 保存报警记录
                                           // 为 true 时将报警事件（时间戳、级别、指标值）写入磁盘；
                                           // 报警记录是设备健康档案的重要组成部分

            int max_storage_days = 90;     // 最大存储天数
                                           // 超过 90 天的历史数据文件将被自动清理；
                                           // 可根据磁盘容量调整，防止存储空间耗尽
        } storage;
        // 声明 storage 成员变量

        // 数据格式
        std::string data_format = "csv";   // 数据格式（csv/json/binary）
                                           // 日志文件的序列化格式：
                                           // "csv"：逗号分隔值，兼容性好，便于 Excel 直接打开；
                                           // "json"：结构化可读，便于 Web API 对接；
                                           // "binary"：体积最小，读写最快，但可读性差

        bool enable_compression = true;    // 启用压缩
                                           // 为 true 时对日志文件执行 gzip 压缩后存储；
                                           // 可显著减少磁盘占用（CSV 文件压缩率通常 > 70%），
                                           // 代价是少量 CPU 开销
    } logging;
    // 声明 logging 成员变量

    // 远程监控
    // 子结构体 RemoteMonitoring：配置向远端服务器上传数据的参数
    struct RemoteMonitoring {
        bool enabled = false;              // 启用远程监控
                                           // 为 true 时启用向远端 HTTP 服务器上传监测数据的功能；
                                           // 默认关闭，适用于无法保证网络连通性的离线场景

        std::string server_url = "http://192.168.1.100:8080";
        // 远程监控服务器的 HTTP 接口 URL；
        // 192.168.1.100 为局域网内监控服务器的默认 IP 地址；
        // 8080 为自定义 HTTP 服务端口（非 80 标准端口），需与服务端配置一致

        int upload_interval = 60;          // 上传间隔（秒）
                                           // 每 60 秒向远程服务器上传一次聚合后的监测数据；
                                           // 增大间隔可降低网络带宽占用，但实时性降低

        bool enable_realtime_stream = false; // 启用实时数据流
                                             // 为 true 时通过 WebSocket 或 SSE 推送实时数据流；
                                             // 默认关闭，因持续流式传输对网络带宽和服务器负载要求较高
    } remote;
    // 声明 remote 成员变量
};
// 结束 DisplayLoggingConfig 结构体定义

// ============================================================================
// 系统配置
// ============================================================================
// 分节注释：以下结构体 SystemConfig 管理系统级运行参数（采样缓冲、自检、电源、网络）

struct SystemConfig {
    // 本结构体为系统层面运行参数的顶层容器，
    // 通过嵌套子结构体分类管理采样缓冲、定期自检、电源管理和网络接口四类配置

    // 采样配置
    // 子结构体 Sampling：配置系统级采样缓冲参数（与 AD4134Config::Sampling 层次不同，
    // 此处为软件层的采样调度参数）
    struct Sampling {
        float main_sample_rate = 50000.0f;  // 主采样率（Hz）
                                            // 系统级主采样率，与 AD4134Config::sampling::sample_rate_hz
                                            // 保持一致（50 kHz），用于软件层时间基准计算

        float analysis_sample_rate = 100.0f; // 分析采样率（Hz）
                                             // 对原始 50 kHz 数据进行降采样后用于慢速分析的采样率；
                                             // 100 Hz 足以覆盖低中频段特征，大幅降低分析计算量

        int buffer_size = 1024;            // 缓冲区大小
                                           // 软件环形缓冲区的容量（样本数），默认 1024 个样本；
                                           // 缓冲区应大于一次分析所需最大样本数（FFT_size=1024），
                                           // 防止数据覆盖

        bool enable_oversampling = false;  // 启用过采样
                                           // 为 true 时对 ADC 输出执行软件过采样（多次采样取平均），
                                           // 可等效提升 ADC 位数；默认关闭，AD4134 硬件精度已足够

        int oversampling_rate = 4;         // 过采样率
                                           // 软件过采样的累加次数，默认 4 次；
                                           // 理论上 4 倍过采样可提升 1 bit 有效位深；
                                           // 仅在 enable_oversampling=true 时生效
    } sampling;
    // 声明 sampling 成员变量

    // 自检配置
    // 子结构体 SelfCheck：配置系统定期自检的项目、间隔和合格阈值
    struct SelfCheck {
        bool enable_self_check = true;     // 启用自检
                                           // 为 true 时系统按 check_interval 周期性执行自检程序，
                                           // 验证各子系统工作状态

        int check_interval = 300;          // 检查间隔（秒）
                                           // 定期自检的执行周期，默认 300 秒（5 分钟）；
                                           // 自检过程会短暂占用系统资源，间隔不宜过短

        // 检查项
        // 子结构体 CheckItems：各自检项目的启用开关
        struct CheckItems {
            bool check_sensor = true;      // 检查传感器
                                           // 为 true 时验证三轴加速度传感器输出是否在有效范围内，
                                           // 检测传感器脱落、断线或饱和故障

            bool check_adc = true;         // 检查ADC
                                           // 为 true 时向 AD4134 发送测试命令并校验响应，
                                           // 验证 SPI 通信及 ADC 芯片工作正常

            bool check_memory = true;      // 检查内存
                                           // 为 true 时读取系统内存使用率，
                                           // 超过阈值时记录警告（防止内存泄漏导致崩溃）

            bool check_temperature = true; // 检查温度
                                           // 为 true 时读取树莓派 CPU 温度，
                                           // 超过阈值时降频或报警（防止热关机）

            bool check_communication = true; // 检查通信
                                             // 为 true 时验证 I2C、SPI 等通信总线响应状态，
                                             // 检测总线冲突或设备脱机
        } items;
        // 声明 items 成员变量

        // 阈值
        // 子结构体 Thresholds：各自检项目的合格/警告判断阈值
        struct Thresholds {
            float adc_noise = 10.0f;       // ADC噪声阈值
                                           // 静态条件下 ADC 输出的允许噪声幅值（LSB 或 mV）；
                                           // 超过此阈值表明 ADC 存在异常噪声或电源干扰

            float memory_usage = 80.0f;    // 内存使用阈值（%）
                                           // 系统物理内存使用率超过 80% 时触发内存警告；
                                           // 树莓派内存有限（通常 ≤ 4 GB），需防止 OOM Kill

            float cpu_temperature = 85.0f; // CPU温度阈值（℃）
                                           // CPU 结温超过 85 ℃ 时触发温度警告；
                                           // 树莓派 CPU 在 85 ℃ 时开始热节流降频，
                                           // 需检查散热条件
        } thresholds;
        // 声明 thresholds 成员变量
    } self_check;
    // 声明 self_check 成员变量

    // 电源管理
    // 子结构体 PowerManagement：配置电源节能策略和工作模式
    struct PowerManagement {
        bool enable_power_saving = true;   // 启用省电模式
                                           // 为 true 时系统在空闲时自动进入较低功耗的工作模式；
                                           // 对使用电池或 PoE 供电的边缘节点尤为重要

        int sleep_timeout = 3600;          // 睡眠超时（秒）
                                           // 系统连续空闲（无报警、无触发事件）超过 3600 秒（1 小时）
                                           // 后自动进入睡眠模式，降低 CPU 频率和外设功耗

        float low_power_threshold = 20.0f; // 低电量阈值（%）
                                           // 电池电量低于 20% 时强制切换至低功耗模式，
                                           // 优先保障核心监测功能持续运行

        // 电源模式
        // 子结构体 PowerModes：定义各电源工作模式的编码值
        struct PowerModes {
            bool enable_adaptive = true;   // 启用自适应模式
                                           // 为 true 时系统根据振动状态和电量自动切换电源模式；
                                           // 正常运行走正常模式，高危状态切高性能模式

            int high_power_mode = 0;       // 高性能模式
                                           // 模式编码 0：最高 CPU 频率，所有外设全速运行；
                                           // 在检测到报警状态时自动切换至此模式以加快响应

            int normal_power_mode = 1;     // 正常模式
                                           // 模式编码 1：标准运行频率，适合日常持续监测

            int low_power_mode = 2;        // 低功耗模式
                                           // 模式编码 2：降频运行，关闭部分外设；
                                           // 在低电量或长时间无异常时启用

            int sleep_mode = 3;            // 睡眠模式
                                           // 模式编码 3：最低功耗状态，仅保持定时唤醒；
                                           // 唤醒后恢复采集并执行快速评估，再决定是否继续睡眠
        } modes;
        // 声明 modes 成员变量
    } power;
    // 声明 power 成员变量

    // 网络配置
    // 子结构体 Network：配置树莓派的以太网网络接口参数
    struct Network {
        bool enable_ethernet = true;       // 启用以太网
                                           // 为 true 时系统使用有线以太网接口（eth0）进行数据上传
                                           // 和远程访问；有线连接稳定性优于无线

        bool enable_wifi = false;          // 启用WiFi
                                           // 为 true 时启用树莓派无线网卡（wlan0）；
                                           // 默认关闭，工业现场建议优先使用有线以避免干扰

        std::string ip_address = "192.168.1.50";
        // 树莓派在局域网内的静态 IP 地址；
        // 使用静态 IP 可避免 DHCP 地址变化导致上位机连接中断

        std::string subnet_mask = "255.255.255.0";
        // 子网掩码，与 ip_address 配合确定局域网网段（192.168.1.0/24）

        std::string gateway = "192.168.1.1";
        // 默认网关地址，用于路由跨网段数据包（如访问互联网或远程 MQTT Broker）

        std::string dns_server = "8.8.8.8";
        // DNS 服务器地址，默认使用 Google Public DNS（8.8.8.8）；
        // 用于将域名解析为 IP 地址（如远程监控服务器使用域名时）
    } network;
    // 声明 network 成员变量
};
// 结束 SystemConfig 结构体定义

// ============================================================================
// 完整配置结构
// ============================================================================
// 分节注释：以下结构体 BearingVibrationConfig 为所有子配置的顶层汇聚容器及配置管理接口

struct BearingVibrationConfig {
    // 本结构体是整个轴承振动监测系统配置的唯一入口点，
    // 聚合了以上全部 8 个子配置结构体实例，并声明配置管理相关的成员方法接口

    AD4134Config ad4134;
    // 成员变量：AD4134 传感器硬件接口配置，含 SPI/采样/滤波器/增益/寄存器五类参数

    VibrationAnalysisConfig analysis;
    // 成员变量：振动信号分析算法配置，含时域/频域/包络/特征提取四类参数

    BearingAssessmentConfig assessment;
    // 成员变量：轴承健康状态评估配置，含振动标准/故障频率库/健康指数三类参数

    AIModelConfig ai_model;
    // 成员变量：AI 推理模型配置，含模型路径/输入规格/输出类别/推理缓存四类参数

    PredictiveMaintenanceConfig predictive_maintenance;
    // 成员变量：预测性维护配置，含 RUL 预测/维护计划/历史分析三类参数

    AlarmConfig alarm;
    // 成员变量：多级报警配置，含振动阈值/触发延迟/输出方式三类参数

    DisplayLoggingConfig display_logging;
    // 成员变量：显示与日志配置，含 OLED/数据记录/远程监控三类参数

    SystemConfig system;
    // 成员变量：系统级运行参数配置，含采样缓冲/自检/电源/网络四类参数
    
    // 版本信息
    std::string version = "1.0.0";
    // 配置文件格式版本号，遵循语义化版本规范（Major.Minor.Patch）；
    // 用于加载配置文件时校验兼容性，防止旧版本配置被新版本程序误用

    std::string config_date = "2024-01-01";
    // 配置文件的创建或最后修改日期，格式为 ISO 8601（YYYY-MM-DD）；
    // 用于日志记录和配置文件溯源

    std::string machine_id = "BEARING_MONITOR_001";
    // 设备唯一标识符字符串，用于多设备部署场景下区分不同监测节点；
    // 在上传数据和报警通知中作为设备身份标签
    
    // 配置管理方法
    // 以下方法声明为配置的序列化/反序列化及校验接口，实现位于对应 .cpp 文件中

    bool loadFromFile(const std::string& filename);
    // 从指定路径的配置文件中读取并覆盖当前配置；
    // 支持 JSON 或自定义格式；成功返回 true，失败（文件不存在/格式错误）返回 false

    bool saveToFile(const std::string& filename);
    // 将当前配置序列化并写入指定路径的文件；
    // 可用于持久化运行时动态修改的参数；成功返回 true，否则返回 false

    bool validate() const;
    // 对当前配置执行合法性校验（如阈值范围、路径可达性等）；
    // 全部校验通过返回 true；存在非法配置项时记录错误日志并返回 false；
    // const 修饰表明本方法不修改任何成员状态

    std::string toJson(bool pretty = true) const;
    // 将当前全部配置序列化为 JSON 字符串并返回；
    // pretty=true 时输出格式化（缩进）的可读 JSON，pretty=false 时输出紧凑 JSON；
    // const 修饰表明不修改成员状态

    bool fromJson(const std::string& json_str);
    // 从 JSON 字符串解析并更新当前配置；
    // 与 toJson() 互为逆操作，用于网络接口或配置文件的 JSON 格式支持；
    // 解析成功返回 true，字符串格式错误返回 false
    
    // 配置更新
    bool updateParameter(const std::string& category, 
                        const std::string& parameter, 
                        const std::string& value);
    // 通过类别名（category）和参数名（parameter）字符串动态更新单个配置项的值；
    // 支持运行时热更新，无需重启即可调整参数；
    // category: 子配置名称（如 "alarm"、"ai_model"）；
    // parameter: 具体参数路径（如 "thresholds.velocity.warning"）；
    // value: 新值的字符串表示，由函数内部完成类型转换；
    // 更新成功返回 true，参数不存在或类型转换失败返回 false
    
    // 配置备份
    bool backupConfig(const std::string& backup_dir);
    // 将当前配置以带时间戳的文件名备份到指定目录；
    // 在修改配置前调用，防止误操作导致配置丢失；
    // 成功返回 true，目录不可写返回 false

    bool restoreConfig(const std::string& backup_file);
    // 从指定备份文件恢复配置，覆盖当前内存中的全部配置项；
    // 通常在 loadFromFile 失败或配置损坏时调用以回退至已知良好状态；
    // 成功返回 true，文件不存在或格式无效返回 false
};
// 结束 BearingVibrationConfig 顶层结构体定义

} // namespace BearingVibrationConfig
// 关闭命名空间 BearingVibrationConfig

#endif // BEARING_VIBRATION_CONFIG_HPP
// 结束头文件保护宏条件编译块，与文件开头的 #ifndef BEARING_VIBRATION_CONFIG_HPP 配对