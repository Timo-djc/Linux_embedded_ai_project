/**
  ******************************************************************************
  * @file    DataProcessingFramework.hpp
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
 * @brief   数据处理框架 — 信号预处理、特征提取与图像变换层头文件
 * ----------------------------------------------------------------------------
 * @details 本文件定义了 MultiProject 框架数据处理层的核心类
 *          DataProcessingFramework，继承自 IDataProcessor 接口，
 *          负责将原始采集数据（RawData）转换为 AI 模型可直接消费的
 *          标准化输入（ProcessedData / 特征向量）。
 *
 *          类内按功能层次组织以下私有嵌套类：
 *
 *          1. ProcessingConfig   — 四维处理参数配置结构体
 *             包含图像配置（ImageConfig）、信号配置（SignalConfig）、
 *             特征配置（FeatureConfig）和数据增强配置（AugmentationConfig）
 *
 *          2. SignalFilter       — IIR 带通/低通滤波器
 *             封装 Butterworth 滤波器系数（b/a 系数）和状态变量，
 *             提供初始化和实时滤波接口
 *
 *          3. FeatureExtractor   — 振动信号多域特征提取器
 *             支持时域统计特征、频域 FFT 特征和高阶统计特征的独立提取
 *
 *          4. ProcessingCache    — cv::Mat 处理结果缓存
 *             以键值对方式缓存耗时处理结果（如 CWT 时频图），
 *             避免对相同输入重复计算
 *
 *          对外公开接口按职能分为三组：
 *            核心处理流水线：process_data / extract_features / augment_data / fuse_data
 *            项目特化接口（条件编译）：
 *              FIRE_DETECTION_PROJECT    → preprocess_fire_image / extract_flame_features
 *              BEARING_VIBRATION_PROJECT → compute_cwt_image / extract_vibration_features
 *            通用数据操作：normalize_data / apply_noise_reduction / segment_image
 *
 * @note    本文件使用 #pragma once 防止重复包含。
 *          CWT（连续小波变换）使用 FFTW3 加速计算，编译时须链接 -lfftw3；
 *          小波参数已从 v1.0 的 cmor100-1 更新为 v2.0 的 cmor1.5-1，
 *          以获得更均衡的时频分辨率。
 * ============================================================================
 */

#pragma once
// 替代传统 #ifndef 头文件保护，保证本文件在同一编译单元中只被展开一次

#include "../common/UnifiedDataTypes.hpp"
// 引入统一数据类型头文件，提供 IDataProcessor 接口、RawData、ProcessedData
// 等跨框架共享数据结构；IDataProcessor 是 DataProcessingFramework 的虚基类

#include "../common/ProjectDefines.hpp"
// 引入全局预处理宏入口，提供 FIRE_DETECTION_PROJECT / BEARING_VIBRATION_PROJECT
// 项目选择宏，以及通过 FrameworkBase.hpp 传递的 MultiProject 命名空间公共类型

#include <opencv2/opencv.hpp>
// 引入 OpenCV 完整功能头文件；
// 提供 cv::Mat（图像矩阵）、cv::resize()、cv::cvtColor()、
// cv::equalizeHist()（直方图均衡化）等图像处理接口；
// DataProcessingFramework 中图像处理操作（resize/灰度化/直方图均衡）均依赖此库

#include <vector>
// 引入 std::vector，用于存储滤波器系数、特征向量、状态变量、
// 均值/标准差参数等动态数组

#include <memory>
// 引入 std::unique_ptr，用于管理 SignalFilter、FeatureExtractor、
// ProcessingCache 三个嵌套类实例的所有权

#include <unordered_map>
// 引入 std::unordered_map，用于 ProcessingCache 的键值对存储
// （键为处理参数的哈希字符串，值为对应的 cv::Mat 结果）

#include <mutex>
// 引入 std::mutex，用于 ProcessingCache::mutex_ 保护缓存的并发读写；
// mutable 修饰允许在 const 方法中加锁

#include <cmath>
// 引入 C 数学函数库：sqrt、pow、log、exp、fabs 等；
// 用于时域特征计算（RMS = sqrt(均方值)）、
// 频域分析（对数功率谱）和小波母函数（Morlet 高斯包络）的数值计算

#include <fftw3.h>
// 引入 FFTW3（Fastest Fourier Transform in the West）头文件；
// 提供 fftw_plan、fftw_execute、fftw_malloc 等接口；
// 用于 CWT（连续小波变换）中对信号和小波核执行高效 FFT/IFFT；
// 编译时须在 CMakeLists.txt 中链接 -lfftw3；
// 树莓派 ARM 平台可使用 -lfftw3f（单精度版本）以节省内存和提升速度

namespace MultiProject {
// 开启命名空间 MultiProject

class DataProcessingFramework : public IDataProcessor {
// DataProcessingFramework：数据处理框架的具体实现类；
// 继承 IDataProcessor 抽象接口（定义于 UnifiedDataTypes.hpp），
// 负责原始数据到 AI 模型输入的全链路预处理，
// 包括图像变换（resize、灰度化、归一化）、
// 信号处理（带通滤波、FFT、CWT 时频变换）和特征提取

public:
    DataProcessingFramework();
    // 构造函数：创建 SignalFilter、FeatureExtractor、ProcessingCache 实例；
    // 使用 ProcessingConfig 中的默认值初始化各配置参数；
    // 调用私有 initialize() 方法完成滤波器系数预计算

    ~DataProcessingFramework() override;
    // 析构函数：释放 signal_filter_、feature_extractor_、cache_ 三个 unique_ptr；
    // override 关键字明确标注覆盖 IDataProcessor 的虚析构函数；
    // FFTW3 内部分配的规划（fftw_plan）须在此或 initialize() 的逆操作中通过
    // fftw_destroy_plan() 释放，防止内存泄漏

    ErrorCode process_data(const RawData& raw_data, ProcessedData& processed_data) override;
    // 覆盖 IDataProcessor::process_data()：执行完整的数据处理流水线；
    // 根据 RawData 的数据类型（图像或振动信号）自动选择处理路径：
    //   图像路径（FIRE_DETECTION）：灰度化 → resize(224×224) → 归一化
    //   振动路径（BEARING_VIBRATION）：带通滤波 → CWT 时频图 → 归一化
    // 处理结果写入 ProcessedData，供 AIModelFramework 直接调用 infer()

    ErrorCode extract_features(const RawData& raw_data, std::vector<float>& features) override;
    // 覆盖 IDataProcessor::extract_features()：从原始数据中提取特征向量；
    // 根据项目类型分发至 extract_flame_features() 或 extract_vibration_features()；
    // 特征向量维度由 ProcessingConfig::FeatureConfig::num_features（默认 20）决定；
    // 与 AIModelConfig::InputConfig 中的特征维度须保持一致

    ErrorCode augment_data(RawData& data) override;
    // 覆盖 IDataProcessor::augment_data()：对原始数据执行在线数据增强；
    // 仅在 ProcessingConfig::AugmentationConfig::enable_augmentation=true 时执行；
    // 增强操作包括：随机旋转、缩放、亮度/对比度扰动、高斯噪声添加；
    // 用于在小样本数据集上提升模型泛化能力，推理阶段须关闭

    ErrorCode fuse_data(const std::vector<RawData>& multi_source_data, RawData& fused_data) override;
    // 覆盖 IDataProcessor::fuse_data()：融合多路数据源为单一 RawData；
    //   火焰检测：将三路摄像头图像（left/center/right）拼接或取最大置信度帧；
    //   振动监测：将三轴（X/Y/Z）振动信号合并为多通道 RawData；
    // 融合策略由 ProcessingConfig 配置，支持拼接（concat）和加权融合两种模式

#ifdef BEARING_VIBRATION_PROJECT
    // 振动项目：生成单通道时频图
    // 以下方法仅在 BEARING_VIBRATION_PROJECT 编译条件下生效

    ErrorCode compute_cwt_image(const std::vector<float>& signal, cv::Mat& output_image);
    // 轴承振动专用 CWT 时频图生成接口（核心处理方法）；
    // 参数 signal：单通道振动加速度时域信号（单位 g，长度通常为 5000 点）；
    // 参数 output_image：输出的 CWT 时频图（224×224 单通道灰度图，CV_8U 格式）；
    // 处理步骤：
    //   1. 构造 Morlet 小波核（cmor1.5-1：中心频率 1.0 Hz，带宽参数 1.5）
    //      注：v2.0 将带宽从 100 更新为 1.5，以获得更均衡的时频分辨率
    //   2. 对信号和小波核执行 FFTW3 FFT（频域相乘代替时域卷积，O(N logN) 复杂度）
    //   3. 对各尺度（scales）执行 IFFT 得到 CWT 系数矩阵（128×N）
    //   4. 取系数矩阵的模值（实部² + 虚部²）得到时频能量图
    //   5. 对能量图进行 log 压缩和 min-max 归一化到 [0, 255]
    //   6. cv::resize(时频图, 224×224, INTER_LINEAR) 缩放至模型输入尺寸
    // 最终输出与训练时使用相同的 CWT 参数生成，保证特征分布一致

    // 特征提取（可选）
    ErrorCode extract_vibration_features(const std::vector<float>& vibration_data, std::vector<float>& features);
    // 轴承振动专用特征提取接口（可选，与 CWT 图像推理互补）；
    // 参数 vibration_data：单通道振动加速度时域序列（单位 g）；
    // 参数 features：输出的特征向量，维度与 FeatureConfig::num_features（默认 20）一致；
    // 内部组合调用 FeatureExtractor 的三类特征提取方法：
    //   时域特征（RMS、峰值、峭度等）+ 频域特征（FFT 幅值、峰值频率等）
    //   + 统计特征（偏度、方差等）；
    // 特征向量可作为 AI 推理的辅助决策依据
#endif
// 结束 BEARING_VIBRATION_PROJECT 条件编译块

    // 通用数据操作
    ErrorCode normalize_data(std::vector<float>& data, float min_val = 0.0f, float max_val = 1.0f);
    // 通用数据归一化接口：将 data 中的每个元素线性映射到 [min_val, max_val] 区间；
    // 归一化公式：normalized = (x - x_min) / (x_max - x_min) * (max_val - min_val) + min_val；
    // 默认映射到 [0.0f, 1.0f]；
    // 注意：本方法为 min-max 归一化，与 AI 模型输入所需的 Z-score 归一化
    // （(pixel - mean) / std）不同，调用方须根据使用场景选择合适的归一化方式

    ErrorCode apply_noise_reduction(std::vector<float>& data);
    // 对时域信号执行噪声抑制：
    //   委托 signal_filter_->apply_filter() 应用已初始化的带通滤波器；
    //   带通滤波器参数由 SignalConfig::high_pass_freq（10 Hz）和
    //   SignalConfig::low_pass_freq（20000 Hz）决定；
    //   工频干扰（50 Hz）通过 AD4134 硬件陷波滤波器消除（见 bearing_vibration_config）；
    //   此处软件滤波器作为补充，进一步抑制带外噪声

    ErrorCode segment_image(const cv::Mat& image, std::vector<cv::Mat>& segments);
    // 将输入图像分割为多个子区域（图像分块）；
    // 用于多区域并行检测（如将 640×480 图像分成多个 224×224 子块）
    // 或基于区域的特征提取；
    // segments：输出的子图像列表，各子图维度由配置或输入图像尺寸决定

    ErrorCode configure(const std::string& config_json);
    // 从 JSON 字符串解析并更新 ProcessingConfig 中的全部参数；
    // 支持运行时热更新（无需重新初始化即可调整 CWT 尺度数、FFT 大小等）；
    // 解析失败返回 CONFIG_PARSE_ERROR；参数非法返回 CONFIG_VALIDATION_FAILED

    std::string get_config() const;
    // 将当前 ProcessingConfig 序列化为 JSON 字符串并返回；
    // 用于诊断报告、配置导出和上层框架的配置管理；
    // const 修饰确保不修改对象状态

private:
// 私有区域：配置结构体、嵌套类和辅助方法

    struct ProcessingConfig {
    // 处理参数配置聚合结构体：按处理类型分为四个子结构体，
    // 存储影响各处理算法行为的全部可调参数

        struct ImageConfig {
        // 子结构体 ImageConfig：图像预处理的尺寸和归一化参数
            int target_width = 224;
            // 图像缩放目标宽度（像素），默认 224；
            // 与 VariableMobileNet 模型输入宽度（ModelConfig::input_width=224）严格对齐

            int target_height = 224;
            // 图像缩放目标高度（像素），默认 224

            bool keep_aspect_ratio = false;
            // 为 true 时保持原始宽高比（在目标尺寸内填充黑边）；
            // 默认 false（直接拉伸缩放），与训练时相同的 resize 策略

            int interpolation_method = cv::INTER_LINEAR;
            // OpenCV 缩放插值方法，默认 INTER_LINEAR（双线性插值）；
            // 可选：INTER_NEAREST（最近邻，最快但有锯齿）、
            //       INTER_CUBIC（双三次，质量更高但较慢）、
            //       INTER_AREA（区域插值，降采样效果最好）

            bool convert_to_rgb = true;
            // 为 true 时将 BGR 图像转换为 RGB 格式；
            // 注意：此默认值（true）与训练配置（to_rgb=False）不一致；
            // 若模型使用灰度输入，应将此字段改为 false 并依靠 convert_to_grayscale()

            std::vector<float> mean = {0.485f, 0.456f, 0.406f};
            // RGB 三通道归一化均值，默认为 ImageNet 预训练模型的标准均值；
            // 重要警告：训练配置（img_norm_cfg）使用 mean=[33.405]（灰度单通道像素均值）；
            // 此处的 ImageNet RGB 均值仅适用于 RGB 三通道输入的模型，
            // 与实际训练配置不匹配，实际使用时须同步更新为 [33.405f/255.0f]

            std::vector<float> std = {0.229f, 0.224f, 0.225f};
            // RGB 三通道归一化标准差，默认为 ImageNet 预训练模型的标准标准差；
            // 同上警告：与训练配置（std=[78.795]）不一致；
            // 灰度单通道应使用 [78.795f/255.0f] 以确保推理输入分布与训练一致
        } image;
        // 声明 image 成员变量

        struct SignalConfig {
        // 子结构体 SignalConfig：振动信号处理算法的关键参数
            int fft_size = 1024;
            // FFT 计算点数，须为 2 的幂次方，默认 1024；
            // 频率分辨率 = sample_rate / fft_size = 50000 / 1024 ≈ 48.8 Hz；
            // 增大 fft_size 可提高频率分辨率，但增加计算量

            float sampling_rate = 50000.0f;       // 振动采样率
            // AD4134 每通道采样率（Hz），默认 50000 Hz（50 kSPS）；
            // 与 bearing_vibration_config.hpp 中 AD4134Config::Sampling::sample_rate_hz
            // 及 ProjectDefines.hpp 中 SAMPLE_RATE_HZ 保持一致；
            // 决定奈奎斯特频率上限（25 kHz）和 FFT 频率轴的物理刻度

            float low_pass_freq = 20000.0f;
            // 带通滤波器的高频截止频率（Hz），默认 20000 Hz；
            // 对应 AD4134Config::Filter::cutoff_frequency，
            // 覆盖轴承高频故障特征（< 20 kHz）的完整频带

            float high_pass_freq = 10.0f;
            // 带通滤波器的低频截止频率（Hz），默认 10 Hz；
            // 滤除直流偏置和超低频漂移，保留有效振动信号成分（> 10 Hz）

            bool enable_bandpass = true;
            // 为 true 时在特征提取前对信号应用带通滤波；
            // 与 AD4134 硬件滤波器（Sinc5+Sinc1）配合使用，提供双重噪声抑制

            bool enable_normalization = true;
            // 为 true 时对滤波后的信号执行 Z-score 归一化，消除量程差异

            // 小波配置
            int cwt_scales = 128;
            // CWT 分析的尺度数量，默认 128；
            // 对应 CWT 时频图的频率轴分辨率（128 个频率尺度，映射至 224 像素高度）；
            // 尺度数越多，时频图频率分辨率越高，但 FFTW3 计算量线性增加

            float cwt_center_freq = 1.0f;         // Morlet 中心频率
            // Morlet 小波母函数的中心频率参数（归一化频率），默认 1.0；
            // 与小波工具箱中 cmor1.5-1 的第二个参数（-1 表示中心频率 1）对应；
            // 该参数控制小波在频率轴上的锚点位置

            float bandwidth = 1.5f;               // [v2.0] cmor100-1 → cmor1.5-1，时频分辨率均衡
            // Morlet 小波的带宽参数（高斯包络宽度），已在 v2.0 从 100 更新为 1.5；
            // cmor100-1（带宽=100）：频率分辨率极高但时间分辨率极低（全局频率分析）；
            // cmor1.5-1（带宽=1.5）：时间与频率分辨率均衡，适合轴承故障瞬态特征捕捉；
            // 此更新使生成的 CWT 图像更适合 VariableMobileNet 的卷积特征提取
        } signal;
        // 声明 signal 成员变量

        struct FeatureConfig {
        // 子结构体 FeatureConfig：特征提取算法的开关和维度配置
            int num_features = 20;
            // 最终输出特征向量的维度，默认 20；
            // 与 VibrationAnalysisConfig::FeatureExtraction::num_features 及
            // AIModelConfig::InputConfig（序列模型时）的特征数保持一致

            bool extract_time_features = true;
            // 为 true 时提取时域统计特征（RMS、峰值、峭度、偏度等）；
            // 与 VibrationAnalysisConfig::TimeDomain 中各 calculate_* 开关对应

            bool extract_freq_features = true;
            // 为 true 时提取频域 FFT 特征（主频幅值、频带能量等）；
            // 与 VibrationAnalysisConfig::FrequencyDomain::enable_fft 对应

            bool extract_statistical_features = true;
            // 为 true 时提取高阶统计特征（方差、峰度等高阶矩）；
            // 与 VibrationAnalysisConfig::FrequencyDomain::calculate_psd 对应

            bool use_pca = false;
            // 为 true 时对提取的原始特征向量执行 PCA 降维；
            // 默认关闭（样本量有限时 PCA 可能引入偏差）

            int pca_components = 10;
            // PCA 降维后保留的主成分数量，默认 10；
            // use_pca=true 时生效，最终特征向量维度变为 10（而非 num_features）
        } feature;
        // 声明 feature 成员变量

        struct AugmentationConfig {
        // 子结构体 AugmentationConfig：在线数据增强的操作类型和强度配置
            bool enable_augmentation = false;
            // 数据增强总开关，默认关闭；
            // 仅在训练模式或数据集构建阶段启用，推理阶段须保持 false

            float rotation_range = 10.0f;
            // 随机旋转角度范围（度），默认 ±10°；
            // 适度旋转增强模型对摄像头安装角度偏差的鲁棒性

            float zoom_range = 0.1f;
            // 随机缩放范围（相对比例），默认 ±10%；
            // 模拟火焰大小变化或轴承安装距离变化

            float brightness_range = 0.2f;
            // 随机亮度扰动范围（相对比例），默认 ±20%；
            // 模拟不同光照条件下的图像采集

            float contrast_range = 0.2f;
            // 随机对比度扰动范围（相对比例），默认 ±20%；
            // 与亮度扰动组合提升模型在不同曝光条件下的鲁棒性

            bool add_noise = false;
            // 为 true 时向数据中添加高斯噪声；
            // 可提升模型对传感器噪声的抵抗能力，默认关闭

            float noise_level = 0.05f;
            // 高斯噪声的标准差（信噪比控制），默认 0.05（即信号幅值的 5%）；
            // add_noise=true 时生效；过大的噪声水平会破坏原始信号的有效特征
        } augmentation;
        // 声明 augmentation 成员变量

    } config_;
    // 声明 config_ 成员变量：ProcessingConfig 实例，
    // 通过 configure() 方法可在运行时更新

    std::atomic<bool> initialized_{false};
    // 处理框架初始化完成标志（原子变量），初始为 false；
    // 私有 initialize() 成功后置 true（滤波器系数计算完成）；
    // process_data() 在调用前检查此标志，未初始化时返回 NOT_INITIALIZED

    class SignalFilter {
    // 私有嵌套类：IIR 数字带通滤波器；
    // 封装 Butterworth 带通滤波器的系数计算和直接 II 型实现；
    // 用于在特征提取前滤除振动信号中的带外噪声
    public:
        ErrorCode init_bandpass(float low_freq, float high_freq, float sample_rate);
        // 初始化带通滤波器：根据截止频率和采样率计算 Butterworth IIR 滤波器系数；
        // 参数 low_freq：高通截止频率（Hz），对应 SignalConfig::high_pass_freq（10 Hz）；
        // 参数 high_freq：低通截止频率（Hz），对应 SignalConfig::low_pass_freq（20000 Hz）；
        // 参数 sample_rate：信号采样率（Hz），对应 SignalConfig::sampling_rate（50000 Hz）；
        // 计算结果存入 b_coeffs_（前馈系数）和 a_coeffs_（反馈系数），
        // 并初始化 prev_input_/prev_output_ 状态变量为全零

        ErrorCode apply_filter(const std::vector<float>& input, std::vector<float>& output);
        // 对输入信号序列应用已初始化的 IIR 带通滤波器；
        // 使用直接 II 型差分方程实现（递推计算，不存储全部历史样本）：
        //   y[n] = b[0]*x[n] + b[1]*x[n-1] + ... - a[1]*y[n-1] - a[2]*y[n-2] - ...
        // prev_input_/prev_output_ 维护滤波器状态（因果滤波器的历史样本），
        // 支持对连续到来的数据块进行无缝滤波（跨块状态保持）

    private:
        std::vector<float> b_coeffs_;
        // 滤波器前馈（分子）系数向量，由 init_bandpass() 计算；
        // 系数数量 = 滤波器阶数 + 1，Butterworth 带通滤波器通常为偶数阶

        std::vector<float> a_coeffs_;
        // 滤波器反馈（分母）系数向量，由 init_bandpass() 计算；
        // a_coeffs_[0] 通常为 1.0（归一化），a_coeffs_[1:] 为递归系数

        std::vector<float> prev_input_;
        // 上一数据块的尾部输入样本缓存（长度 = 滤波器阶数）；
        // 用于在跨块滤波时正确计算当前块首部样本的滤波器输出，
        // 避免块边界处的滤波不连续（瞬态响应）

        std::vector<float> prev_output_;
        // 上一数据块的尾部输出样本缓存（长度 = 滤波器阶数）；
        // 与 prev_input_ 配合维护滤波器状态（直接 II 型的延迟单元）
    };
    // 结束 SignalFilter 嵌套类定义

    std::unique_ptr<SignalFilter> signal_filter_;
    // SignalFilter 实例，由构造函数创建并在 initialize() 中通过
    // signal_filter_->init_bandpass() 完成系数计算；
    // apply_noise_reduction() 委托此对象执行带通滤波

    class FeatureExtractor {
    // 私有嵌套类：振动信号多域特征提取器；
    // 提供时域、频域和统计三类特征的独立提取方法，
    // 可按 FeatureConfig 的开关灵活组合
    public:
        ErrorCode extract_time_features(const std::vector<float>& data, std::vector<float>& features);
        // 提取时域统计特征，追加到 features 向量：
        //   均方根值（RMS）：sqrt(sum(x²)/N)，反映振动能量
        //   峰值（Peak）：max(|x|)，反映冲击强度
        //   峰峰值（P2P）：max(x) - min(x)，反映振幅范围
        //   峰值因子（Crest Factor）：Peak/RMS，无量纲冲击指标
        //   峭度（Kurtosis）：E[(x-μ)⁴]/σ⁴，轴承早期故障高敏感指标
        //   偏度（Skewness）：E[(x-μ)³]/σ³，信号分布不对称性指标

        ErrorCode extract_freq_features(const std::vector<float>& data, float sample_rate, std::vector<float>& features);
        // 提取频域 FFT 特征，追加到 features 向量：
        //   参数 sample_rate：信号采样率（Hz），用于计算频率轴物理刻度；
        //   内部对 data 执行 FFT（利用 fftw3.h 提供的接口）；
        //   提取：主频幅值、主频频率、各频带（低/中/高）能量占比、
        //   谱重心（Spectral Centroid）等频域描述子

        ErrorCode extract_statistical_features(const std::vector<float>& data, std::vector<float>& features);
        // 提取高阶统计特征，追加到 features 向量：
        //   均值、方差、标准差、中位数、四分位距（IQR）；
        //   这些特征对信号的整体分布特性敏感，
        //   与时域/频域特征组合提供互补的故障判断依据
    };
    // 结束 FeatureExtractor 嵌套类定义

    std::unique_ptr<FeatureExtractor> feature_extractor_;
    // FeatureExtractor 实例，由构造函数创建；
    // extract_features() 和 extract_vibration_features() 委托此对象执行特征计算

    ErrorCode initialize();
    // 私有初始化方法：执行处理框架的内部初始化：
    //   1. 调用 signal_filter_->init_bandpass() 预计算滤波器系数
    //   2. 若需要 FFTW3 规划（plan），通过 fftw_plan_dft_r2c_1d() 创建规划
    //   3. 设置 initialized_=true
    // 由构造函数调用，失败时记录错误但不抛出异常（通过 last_error_ 传递）

    ErrorCode resize_to_224x224(const cv::Mat& input, cv::Mat& output);
    // 私有辅助方法：将输入图像缩放至 224×224 像素；
    // 使用 ProcessingConfig::ImageConfig::interpolation_method 指定的插值方法；
    // 若 keep_aspect_ratio=true，在目标尺寸内等比缩放并填充黑边；
    // 供 preprocess_fire_image() 和 compute_cwt_image() 中的最终 resize 步骤调用

    ErrorCode convert_to_grayscale(const cv::Mat& input, cv::Mat& output);
    // 私有辅助方法：将彩色 BGR 图像转换为单通道灰度图；
    // 内部调用 cv::cvtColor(input, output, cv::COLOR_BGR2GRAY)；
    // 与训练配置（to_rgb=False、单通道输入）保持一致

    ErrorCode apply_histogram_equalization(cv::Mat& image);
    // 私有辅助方法：对灰度图像执行直方图均衡化（Histogram Equalization）；
    // 内部调用 cv::equalizeHist(image, image)（原地操作）；
    // 提升低对比度图像（如光线不足时的火焰图像）中目标区域的视觉显著性；
    // 注意：直方图均衡化会改变图像的像素值分布，需评估其对归一化均值/标准差的影响

    ErrorCode signal_to_spectrogram(const std::vector<float>& signal, cv::Mat& spectrogram);
    // 私有辅助方法：将时域信号转换为 STFT（短时傅里叶变换）频谱图；
    // 与 compute_cwt_image() 的 CWT 时频图不同：
    //   STFT 使用固定窗长（时频分辨率固定）；
    //   CWT 使用可变尺度（低频高时间分辨率，高频高频率分辨率）；
    // 本方法可作为 compute_cwt_image() 的备选轻量化实现（STFT 计算量更低）；
    // 内部使用 FFTW3 对滑动窗口信号块执行 FFT，组装成二维频谱图矩阵

    class ProcessingCache {
    // 私有嵌套类：cv::Mat 处理结果缓存；
    // 以字符串键对缓存 CWT 时频图等耗时处理结果，避免对相同输入重复计算；
    // 注意：CWT 计算耗时（约 10~50 ms/帧），缓存可提升实时性；
    // 但 bearing_vibration_config 已将推理缓存修复为关闭，
    // 此处的处理结果缓存（非推理结果缓存）仍可按需启用
    public:
        bool get_cached(const std::string& key, cv::Mat& result);
        // 查询缓存：若 key 命中则将对应 cv::Mat 深拷贝到 result 并返回 true；
        // 未命中则返回 false；
        // 内部加锁（mutex_），支持多线程并发调用

        void set_cached(const std::string& key, const cv::Mat& value);
        // 写入缓存：将 (key, value) 键值对插入缓存（深拷贝 cv::Mat）；
        // 内部加锁；缓存容量无硬性上限，调用方须周期性调用 clear() 防止内存增长

        void clear();
        // 清空全部缓存条目；
        // 在模型参数更新、配置热更新或 ProcessingConfig 变更后应调用本方法，
        // 防止旧参数生成的缓存污染新配置下的处理结果

    private:
        std::unordered_map<std::string, cv::Mat> cache_;
        // 键值对缓存容器，键为处理参数+输入数据的哈希字符串；
        // 值为对应的 cv::Mat 处理结果（CWT 时频图等）；
        // unordered_map 提供 O(1) 平均查找，适合高频缓存查询场景

        mutable std::mutex mutex_;
        // 可变互斥锁（mutable 允许在 const 方法 get_cached() 中加锁）；
        // 保护 cache_ 的并发读写，确保多线程环境下缓存操作的线程安全
    };
    // 结束 ProcessingCache 嵌套类定义

    std::unique_ptr<ProcessingCache> cache_;
    // ProcessingCache 实例，由构造函数创建；
    // compute_cwt_image() 和 signal_to_spectrogram() 在执行耗时计算前
    // 先通过 cache_->get_cached() 查询是否有缓存结果，有则直接返回
};
// 结束 DataProcessingFramework 类定义

} // namespace MultiProject
// 关闭命名空间 MultiProject