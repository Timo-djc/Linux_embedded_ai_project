/**
  ******************************************************************************
  * @file    DataProcessingFramework.cpp
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
 * @brief   数据处理框架实现文件（v4.0 / v17.2）
 * ----------------------------------------------------------------------------
 * @details 本文件实现 DataProcessingFramework.hpp 中声明的全部方法。
 *          文件按以下顺序组织：
 *
 *  [v4.0] 静态预处理工具函数（文件作用域，不对外暴露）
 *    BEARING：enhance_before_fusion — CLAHE+Unsharp Masking，融合前增强（v17.1 CV_32F修复）
 *             postprocess_fused    — 同上，参数更保守，融合后增强
 *    FIRE：   fire_biased_gray     — BGR→加权灰度(R×0.50+G×0.40+B×0.10)，输出CV_32F
 *             fire_postprocess     — 防黑兜底(dark_thresh=30)+CLAHE(clip=2.5)
 *
 *  [v4.0] CrossCorrelationWeightedFusion（CCWF）
 *    ncc_f32_block — float32 归一化互相关（两遍扫描：第一遍求均值，第二遍求协方差）
 *    ccwf_fuse     — 多路图像8×8块互相关加权融合→NORM_MINMAX→CV_8U
 *
 *  DataProcessingFramework 主类方法
 *    构造/析构、process_data（主流水线：火焰三路融合 / 振动三轴CWT融合）
 *    extract_features/augment_data/fuse_data（桩实现）
 *    preprocess_fire_image/extract_flame_features（FIRE专用）
 *
 *  [v15.0~v17.2] CWT 时频图（BEARING专用）
 *    next_fast_len   — scipy.fft.next_fast_len等效实现（11-smooth数，22/22用例匹配）
 *    compute_cwt_image — v17.2精确复现pywt.cwt('cmor1.5-1')全部数值行为
 *    extract_vibration_features — 均值/标准差/最大值/最小值
 *
 *  通用工具与嵌套类方法
 *    normalize_data(min-max) / apply_noise_reduction(三点均值) / segment_image(桩)
 *    SignalFilter::init_bandpass / apply_filter（直接型IIR差分方程）
 *    FeatureExtractor::extract_time_features（均值/RMS/峰值；频域/统计为桩）
 *
 * @note 版本修复历史（累计5处数值差异修复）：
 *   v17.2：只取CWT实部（isrealobj fix），消除竖纹错位
 *   v17.1：CV_32F中间类型消除Unsharp Masking ±1~2像素误差
 *   v17.0：N_psi=1024，j_len=floor(sd)+1，j_val分母=1023
 *   v16.0：float32截断后取绝对值（非复数模sqrt(re²+im²)）
 *   v15.0：next_fast_len(11-smooth)替代next_pow2
 * ============================================================================
 */

#include "DataProcessingFramework.hpp"  // 框架完整声明：含嵌套类/成员变量/方法签名
#include <cmath>                        // std::sqrt/log1p/fabs/floor/exp/cos/sin
#include <iostream>                     // std::cout/cerr 调试与版本信息输出
#include <chrono>                       // std::chrono::system_clock：处理时间戳
#include <cfloat>                       // FLT_MAX/DBL_MAX 浮点极值常量（预留）
#include <numeric>                      // std::accumulate：累积运算（均值/能量）
#include <fftw3.h>                      // FFTW3库：fftw_complex/plan/malloc/execute/free
#include <complex>                      // std::complex 标准复数（当前由fftw_complex代替，预留）
#include <opencv2/opencv.hpp>           // OpenCV：Mat/resize/normalize/CLAHE/GaussianBlur等
#include <vector>                       // std::vector：信号/特征/图像列表/FFT缓冲区
#include <algorithm>                    // std::min_element/max_element/min/fill

namespace MultiProject {               // 开启项目命名空间

// ============================================================================
// [v4.0] 预处理工具函数
// ============================================================================

#ifdef BEARING_VIBRATION_PROJECT  // 条件编译：仅在轴承振动项目中编译以下代码
// ── 轴承振动：融合前增强（CLAHE + Unsharp Masking）
// 对应 Python：enhance_before_fusion(sigma=1.0, strength=1.5 → ksize=7)
// ── 轴承振动：融合前增强（CLAHE + Unsharp Masking）
// 对应 Python：enhance_before_fusion(sigma=1.0, strength=1.5 → ksize=7)
//
// [v17.1 FIX] 中间类型 CV_8U → CV_32F
//   Python cv2.addWeighted(CV_8U, CV_8U) 内部以 float64 计算，最终 round+saturate。
//   C++ cv::addWeighted(CV_8U → CV_8U) 以 float32 计算，saturate_cast<uint8> 截断。
//   两者在高 strength（1.5）下对 [100,200] 区间像素产生系统性 ±1~2 误差。
//
//   修复：将 enhanced/blurred 先升到 CV_32F，用 float64 系数做 addWeighted（输出 CV_32F），
//   再 round+clamp[0,255] 转回 CV_8U，完全对齐 Python 行为。
static cv::Mat enhance_before_fusion(const cv::Mat& img,      // 输入：CV_8U单通道灰度图
                                      double clahe_clip      = 3.0,  // CLAHE对比度限制因子
                                      int    clahe_tile      = 8,    // CLAHE分块边长（8×8像素）
                                      double unsharp_sigma   = 1.0,  // Unsharp Masking高斯标准差σ
                                      double unsharp_strength = 1.5) // 锐化强度（对应ksize=7）
{  // 函数/块起始
    CV_Assert(img.type() == CV_8U);    // 断言输入为CV_8U；不符时抛出cv::Exception
    // ① CLAHE
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clahe_clip,  // 续：createCLAHE 调用，传入 clip 参数
                                                cv::Size(clahe_tile, clahe_tile)); // clip=3.0；8×8局部块
    cv::Mat enhanced;                  // 存储CLAHE均衡化结果（CV_8U）
    clahe->apply(img, enhanced);       // 执行CLAHE：局部对比度增强→enhanced

    // ② Unsharp Masking — 用 CV_32F 中间类型精确对齐 Python float64 路径
    int ksize = static_cast<int>(6 * unsharp_sigma + 1); // ksize=6σ+1（覆盖±3σ即99.7%高斯能量）
    if (ksize % 2 == 0) ksize += 1;   // OpenCV要求高斯核为奇数；偶数时加1修正
    cv::Mat blurred;                   // 存储高斯模糊结果（低频分量）
    cv::GaussianBlur(enhanced, blurred, cv::Size(ksize, ksize), unsharp_sigma); // σ=1.0，ksize=7

    // 升到 float32，addWeighted 输出也是 float32
    cv::Mat enh_f, blur_f, sharp_f;   // CV_32F中间矩阵（v17.1修复引入）
    enhanced.convertTo(enh_f,  CV_32F); // CV_8U[0,255]→CV_32F[0.0,255.0]，保留精度
    blurred.convertTo(blur_f, CV_32F);  // 低频分量同样升至CV_32F
    cv::addWeighted(enh_f,  1.0 + unsharp_strength,  // 续：addWeighted 调用，传入增强图及权重
                    blur_f, -unsharp_strength, 0.0, sharp_f); // sharp=(1+str)×enh-str×blur
                    // v17.1修复：CV_32F运算消除与Python float64路径的±1~2像素误差

    // round + saturate → uint8（对齐 Python np.clip(sharpened,0,255).astype(uint8)）
    cv::Mat result;  // 声明输出容器
    sharp_f.convertTo(result, CV_8U);   // OpenCV saturate_cast + round → CV_8U
    return result;                      // 返回增强后的CV_8U灰度图
}  // 函数/块结束

// ── 轴承振动：融合后处理（CLAHE + Unsharp Masking）
// 对应 Python：postprocess_fused(sigma=0.8, strength=1.2 → ksize=5)
//
// [v17.1 FIX] 同 enhance_before_fusion，中间改用 CV_32F
static cv::Mat postprocess_fused(const cv::Mat& img,          // 输入：融合后CV_8U灰度图
                                  double clahe_clip      = 4.0,  // 更强CLAHE限制（融合后对比度已增强）
                                  int    clahe_tile      = 8,    // 8×8局部块
                                  double unsharp_sigma   = 0.8,  // 较小σ→ksize=5，保留更多细节
                                  double unsharp_strength = 1.2) // 较温和锐化强度
{  // 函数/块起始
    CV_Assert(img.type() == CV_8U);    // 断言输入为CV_8U格式
    // ① CLAHE
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clahe_clip,  // 续：createCLAHE 调用，传入 clip 参数
                                                cv::Size(clahe_tile, clahe_tile)); // clip=4.0，8×8块
    cv::Mat enhanced;                  // 存储CLAHE结果
    clahe->apply(img, enhanced);       // 执行CLAHE均衡化

    // ② Unsharp Masking — 用 CV_32F 中间类型精确对齐 Python float64 路径
    int ksize = static_cast<int>(6 * unsharp_sigma + 1); // ksize=6×0.8+1=5.8→截断为5（奇数）
    if (ksize % 2 == 0) ksize += 1;   // 确保奇数核（σ=0.8时ksize=5，无需修正）
    cv::Mat blurred;                   // 存储高斯模糊低频分量
    cv::GaussianBlur(enhanced, blurred, cv::Size(ksize, ksize), unsharp_sigma); // σ=0.8，ksize=5

    cv::Mat enh_f, blur_f, sharp_f;   // CV_32F中间矩阵
    enhanced.convertTo(enh_f,  CV_32F); // CV_8U→CV_32F提升精度
    blurred.convertTo(blur_f, CV_32F);  // CV_8U→CV_32F提升精度
    cv::addWeighted(enh_f,  1.0 + unsharp_strength,  // 续：addWeighted 调用，传入增强图及权重
                    blur_f, -unsharp_strength, 0.0, sharp_f); // sharp=2.2×enh-1.2×blur

    cv::Mat result;  // 声明输出容器
    sharp_f.convertTo(result, CV_8U);  // CV_32F→CV_8U：saturate_cast四舍五入截断
    return result;                     // 返回后处理完成的CV_8U灰度图
}  // 函数/块结束
#endif // BEARING_VIBRATION_PROJECT   // 结束轴承振动预处理函数条件编译块

// ============================================================================
// [v4.0] CrossCorrelationWeightedFusion (CCWF)
// ============================================================================

// float32 归一化互相关（与 Python _ncc_f32 完全一致）
static float ncc_f32_block(const cv::Mat& p1_f32, const cv::Mat& p2_f32)  // NCC 函数签名
// 计算两个CV_32F图像块的NCC系数；范围[-1,1]；两遍扫描对齐Python _ncc_f32
{  // 函数/块起始
    const int n = p1_f32.rows * p1_f32.cols; // 图像块总像素数
    if (n == 0) return 0.0f;           // 空块返回0，防御性处理
    float s1 = 0.0f, s2 = 0.0f;       // s1：p1像素总和；s2：p2像素总和
    for (int r = 0; r < p1_f32.rows; ++r) {  // 逐行遍历图像块
        const float* row1 = p1_f32.ptr<float>(r); // 获取p1第r行float32指针
        const float* row2 = p2_f32.ptr<float>(r); // 获取p2第r行float32指针
        for (int c = 0; c < p1_f32.cols; ++c) {  // 逐列遍历当前行
            s1 += row1[c];             // 累加p1第(r,c)像素
            s2 += row2[c];             // 累加p2第(r,c)像素
        }  // 函数/块结束
    }  // 函数/块结束
    // 第一遍循环结束：s1/s2分别为两块像素总和
    const float m1 = s1 / n;           // p1均值 = Σp1/n
    const float m2 = s2 / n;           // p2均值 = Σp2/n
    float num = 0.0f, den1 = 0.0f, den2 = 0.0f; // 协方差分子/p1方差分子/p2方差分子
    for (int r = 0; r < p1_f32.rows; ++r) {  // 逐行遍历图像块
        const float* row1 = p1_f32.ptr<float>(r); // 第二遍：再次获取p1行指针
        const float* row2 = p2_f32.ptr<float>(r); // 再次获取p2行指针
        for (int c = 0; c < p1_f32.cols; ++c) {  // 逐列遍历当前行
            const float d1 = row1[c] - m1; // p1当前像素去均值差
            const float d2 = row2[c] - m2; // p2当前像素去均值差
            num  += d1 * d2;           // 累加协方差：Σ(p1-m1)(p2-m2)
            den1 += d1 * d1;           // 累加p1方差分子：Σ(p1-m1)²
            den2 += d2 * d2;           // 累加p2方差分子：Σ(p2-m2)²
        }  // 函数/块结束
    }  // 函数/块结束
    // 第二遍循环结束：num/den1/den2已计算完毕
    const float denom = std::sqrt(den1 * den2); // NCC分母=sqrt(Σ(p1-m1)²×Σ(p2-m2)²)
    if (denom < 1e-6f) return 0.0f;   // 分母接近零（纯色块）返回0，防除零
    return num / denom;                // NCC=协方差/分母，范围[-1,1]
}  // 函数/块结束

// 统一互相关加权融合
static cv::Mat ccwf_fuse(const std::vector<cv::Mat>& gray_f32_imgs, // 多路CV_32F灰度图
                          int img_size   = 224,                      // 图像尺寸（正方形）
                          int block_size = 8)                        // 分块大小（8×8像素）
{  // 函数/块起始
    const int n_imgs = static_cast<int>(gray_f32_imgs.size()); // 输入路数（通常3）
    cv::Mat fused = cv::Mat::zeros(img_size, img_size, CV_32F); // 融合结果矩阵，全零CV_32F

    for (int y = 0; y < img_size; y += block_size) { // 按行遍历图像块，步长=block_size
        const int ye = std::min(y + block_size, img_size); // 当前块下边界（边界块可能更小）
        const int bh = ye - y;         // 当前块实际高度（通常=block_size）
        for (int x = 0; x < img_size; x += block_size) { // 按列遍历图像块
            const int xe = std::min(x + block_size, img_size); // 当前块右边界
            const int bw = xe - x;     // 当前块实际宽度
            const cv::Rect roi(x, y, bw, bh); // 当前块ROI：左上角(x,y)，尺寸bw×bh

            std::vector<float> w(n_imgs, 0.5f); // 各路权重初始化为0.5（平权基准值）
            for (int i = 0; i < n_imgs; ++i) { // 对第i路计算其与其他路的平均NCC
                float ncc_sum = 0.0f;  // i路与其他路NCC映射值的累积和
                int   cnt     = 0;     // 有效比较次数计数器
                for (int j = 0; j < n_imgs; ++j) {  // 遍历所有其他路图像
                    if (i == j) continue; // 跳过自比（NCC自比恒为1，无区分意义）
                    cv::Mat p1 = gray_f32_imgs[i](roi).clone(); // 第i路当前块（深拷贝）
                    cv::Mat p2 = gray_f32_imgs[j](roi).clone(); // 第j路当前块（深拷贝）
                    const float ncc = ncc_f32_block(p1, p2); // 计算i路与j路NCC[-1,1]
                    ncc_sum += (ncc + 1.0f) * 0.5f; // 映射[-1,1]→[0,1]：(ncc+1)/2
                    ++cnt;             // 增加有效比较计数
                }  // 函数/块结束
                w[i] = (cnt > 0) ? (ncc_sum / cnt) : 0.5f; // 权重=平均映射NCC；单路时回退0.5
            }  // 函数/块结束

            float w_sum = 0.0f;  // 权重总和累积变量
            for (float wi : w) w_sum += wi; // 计算所有路权重总和（用于归一化）
            if (w_sum < 1e-6f) w_sum = 1.0f; // 权重全零时防除零，退化为不归一化

            cv::Mat patch_fused = cv::Mat::zeros(bh, bw, CV_32F); // 当前块融合结果，全零
            for (int k = 0; k < n_imgs; ++k) {  // 逐路加权叠加
                patch_fused += w[k] * gray_f32_imgs[k](roi); // 加权叠加：第k路块×对应权重
            }  // 函数/块结束
            patch_fused /= w_sum;      // 除以权重总和，完成归一化加权平均
            patch_fused.copyTo(fused(roi)); // 将融合块复制到结果矩阵对应位置
        }  // 函数/块结束
    }  // 函数/块结束
    // 所有块处理完毕，fused含完整融合结果（CV_32F）

    cv::Mat result, result_u8;  // 声明归一化和 uint8 输出容器
    cv::normalize(fused, result, 0, 255, cv::NORM_MINMAX); // 线性归一化：(x-min)/(max-min)×255
    result.convertTo(result_u8, CV_8U); // CV_32F→CV_8U：四舍五入截断
    return result_u8;                  // 返回融合完成的CV_8U灰度图
}  // 函数/块结束

// ============================================================================
// DataProcessingFramework 实现
// ============================================================================

DataProcessingFramework::DataProcessingFramework() {  // 构造函数实现
// 构造函数：在堆上创建三个嵌套组件实例
    signal_filter_     = std::make_unique<SignalFilter>();  // 代码行
    // 创建 IIR 带通滤波器实例（系数在 init_bandpass() 中预计算）
    feature_extractor_ = std::make_unique<FeatureExtractor>();  // 代码行
    // 创建特征提取器实例
    cache_             = std::make_unique<ProcessingCache>();  // 代码行
    // 创建处理结果缓存实例（用于存储 CWT 等耗时计算结果）
}  // 函数/块结束

DataProcessingFramework::~DataProcessingFramework() = default;  // 代码行
// 编译器默认析构；unique_ptr 成员析构时自动释放堆内存

ErrorCode DataProcessingFramework::process_data(const RawData& raw_data,  // 方法实现
// raw_data：上游采集框架传入的原始数据（含图像或传感器时域数据）
                                                 ProcessedData& processed_data) {  // 代码行
// processed_data：输出参数，接收 processed_image / features / stats
    processed_data.processing_timestamp =  // 将处理开始时刻的 Unix 毫秒时间戳写入输出
    // 将处理开始时刻的 Unix 毫秒时间戳写入输出数据
        std::chrono::duration_cast<std::chrono::milliseconds>(  // 续：将时间段转换为毫秒精度
        // 将时间段转换为毫秒精度
            std::chrono::system_clock::now().time_since_epoch()).count();  // 续：now() 当前时间；count() 取整数毫秒值
    // now()：当前系统时间；time_since_epoch()：自纪元起的时间段；count()：整数毫秒值

#ifdef BEARING_VIBRATION_PROJECT  // 条件编译：仅在轴承振动项目中编译以下代码
// ── 轴承振动处理流水线（条件编译）
    if (!raw_data.sensor_data.empty()) {  // 条件判断
    // sensor_data 非空：有振动传感器数据待处理
        size_t total = raw_data.sensor_data.size();  // 总采样点数 = 3轴拼接
        // 总采样点数 = 3轴各 samples_per_axis 点的顺序拼接
        if (total < 3 * 5000) return ErrorCode::INVALID_PARAMETER;  // 条件判断
        // 数据量不足（少于 3×5000=15000 点）时返回参数无效；5000 为每批次固定采集点数

        int samples_per_axis = total / 3;  // 每轴采样点数（通常 5000），整除保证轴间对齐
        // 每轴采样点数（通常 5000），整除保证轴间对齐
        std::vector<float> x_data(samples_per_axis);  // X 轴振动时域序列（预分配）
        // X 轴振动加速度时域序列（预分配）
        std::vector<float> y_data(samples_per_axis);  // Y 轴振动时域序列（预分配）
        // Y 轴振动加速度时域序列（预分配）
        std::vector<float> z_data(samples_per_axis);  // Z 轴振动时域序列（预分配）
        // Z 轴振动加速度时域序列（预分配）
        std::copy(raw_data.sensor_data.begin(),  // std::copy：从 sensor_data 前段复制到 x_data
        // std::copy：从源区间 [begin, begin+samples_per_axis) 复制到 x_data
                  raw_data.sensor_data.begin() + samples_per_axis,  // 续：第二个 copy — Y 轴源区间起点
                  // 源区间终点（前 5000 个元素）
                  x_data.begin());  // 续：目标起始迭代器（X 轴）
                  // 目标起始迭代器
        std::copy(raw_data.sensor_data.begin() + samples_per_axis,  // 代码行
        // 中段 [begin+5000, begin+10000) 复制到 y_data
                  raw_data.sensor_data.begin() + 2 * samples_per_axis,  // 续：Y 轴源区间终点 / Z 轴源区间起点
                  // 源区间：Y 轴 5000 个元素
                  y_data.begin());  // 续：目标起始迭代器（Y 轴）
                  // 目标起始迭代器
        std::copy(raw_data.sensor_data.begin() + 2 * samples_per_axis,  // 代码行
        // 后段 [begin+10000, end) 复制到 z_data
                  raw_data.sensor_data.end(),  // 续：Z 轴源区间终点
                  // 源区间：Z 轴 5000 个元素
                  z_data.begin());  // 续：目标起始迭代器（Z 轴）
                  // 目标起始迭代器

        cv::Mat img_x, img_y, img_z;  // 声明三轴 CWT 时频图容器（CV_8U，224×224）
        // 声明三轴 CWT 时频图容器（CV_8U，224×224）
        auto status = compute_cwt_image(x_data, img_x);  // X 轴信号生成 CWT 时频图（v17.2）
        // X 轴信号生成 CWT 时频图（v17.2 精确复现 pywt）
        if (status != ErrorCode::SUCCESS) return status;  // 失败时向上层传播错误码
        // CWT 失败时传播错误码
        status = compute_cwt_image(y_data, img_y);  // Y 轴信号生成 CWT 时频图
        // Y 轴信号生成 CWT 时频图
        if (status != ErrorCode::SUCCESS) return status;  // 失败时向上层传播错误码
        // Y 轴 CWT 失败时传播错误
        status = compute_cwt_image(z_data, img_z);  // Z 轴信号生成 CWT 时频图
        // Z 轴信号生成 CWT 时频图
        if (status != ErrorCode::SUCCESS) return status;  // 失败时向上层传播错误码
        // Z 轴 CWT 失败时传播错误

        img_x = enhance_before_fusion(img_x);  // X 轴 CWT 图融合前增强（v17.1 修复版）
        // X 轴 CWT 图融合前增强（CLAHE+Unsharp Masking，v17.1 修复版）
        img_y = enhance_before_fusion(img_y);  // Y 轴 CWT 图融合前增强
        // Y 轴 CWT 图融合前增强
        img_z = enhance_before_fusion(img_z);  // Z 轴 CWT 图融合前增强
        // Z 轴 CWT 图融合前增强

        std::vector<cv::Mat> gray_f32(3);  // 预分配 3 个元素的 CV_32F 图像容器
        // 预分配 3 个元素的 CV_32F 图像容器
        img_x.convertTo(gray_f32[0], CV_32F);  // X 轴增强图 CV_8U → CV_32F
        // X 轴增强图 CV_8U → CV_32F，供 ccwf_fuse 使用
        img_y.convertTo(gray_f32[1], CV_32F);  // Y 轴增强图 CV_8U → CV_32F
        // Y 轴增强图 CV_8U → CV_32F
        img_z.convertTo(gray_f32[2], CV_32F);  // Z 轴增强图 CV_8U → CV_32F
        // Z 轴增强图 CV_8U → CV_32F

        cv::Mat fused = ccwf_fuse(gray_f32);  // CCWF 互相关加权融合，输出 CV_8U
        // 三轴 CWT 时频图 CCWF 互相关加权融合，输出 CV_8U
        fused = postprocess_fused(fused);  // 融合后处理（v17.1 修复版）
        // 融合结果后处理（CLAHE+Unsharp Masking，v17.1 修复版）

        cv::imwrite("test_cpp_img_fused.png", fused);  // 写入磁盘调试文件
        // 写入调试输出文件

        if (raw_data.source_name.rfind("BEARING_PARAM|", 0) == 0) {  // 条件判断
        // source_name 以 "BEARING_PARAM|" 开头（模拟模式）
            FILE* fp = std::fopen("test_cpp_img_fused_bearing.txt", "w");  // 打开调试元数据文件（写模式）
            // 以写模式打开调试元数据文件
            if (fp) {  // 条件判断
            // 文件打开成功
                std::string s = raw_data.source_name;  // 复制 source_name 到局部变量，供 lambda 按引用捕获
                // 复制 source_name 供 lambda 捕获
                auto get_val = [&](const std::string& key) -> std::string {  // lambda 定义：从 KEY=VALUE|... 格式提取指定键的值
                // lambda：从 "KEY=VALUE|..." 格式提取指定键的值
                    auto p = s.find(key + "=");  // 自动类型推断变量/lambda
                    // 查找 "KEY=" 位置
                    if (p == std::string::npos) return "";  // 条件判断
                    // 键不存在时返回空字符串
                    p += key.size() + 1;  // 代码行
                    // 跳过键名和 '='，移至值起始
                    auto e = s.find("|", p);  // 自动类型推断变量/lambda
                    // 查找值结束的 '|' 分隔符
                    return s.substr(p, (e == std::string::npos) ? std::string::npos : e - p);  // 返回
                    // 提取值子字符串
                };  // 结构体/类/lambda 定义结束
                std::fprintf(fp, "--csv        %s\n", get_val("csv").c_str());  // 代码行
                // 写入 CSV 文件相对路径
                std::fprintf(fp, "--category   %s\n", get_val("category").c_str());  // 代码行
                // 写入故障类别标签（如 "de_normal"）
                std::fprintf(fp, "--start_row  %s\n", get_val("start_row").c_str());  // 代码行
                // 写入 CSV 起始行号（用于数据对齐）
                std::fflush(fp);  // 强制刷新缓冲区，确保数据写入磁盘
                // 强制刷新缓冲区确保写入磁盘
                std::fclose(fp);  // 关闭文件，释放文件描述符
                // 关闭文件
            } else {  // 否则
                std::cerr << "[BEARING] 无法写入 test_cpp_img_fused_bearing.txt" << std::endl;  // 打印错误信息
                // 文件打开失败时输出错误信息
            }  // 函数/块结束
        }  // 函数/块结束

        std::cout << "\n[BEARING v4.0] 已输出 test_cpp_img_fused.png + test_cpp_img_fused_bearing.txt" << std::endl;  // 打印调试信息
        // 打印调试确认信息

        processed_data.processed_image = fused;  // 将融合图像写入 ProcessedData
        // 三轴融合后处理图像写入 ProcessedData，供 AIModelFramework 推理
        std::vector<float> features;  // 声明特征向量容器
        // 声明特征容器
        status = extract_vibration_features(raw_data.sensor_data, features);  // 提取时域振动特征
        // 提取时域振动特征（均值/标准差/最大值/最小值）
        if (status == ErrorCode::SUCCESS) {  // 条件判断
        // 提取成功时写入
            processed_data.features = features;  // 将特征向量写入 ProcessedData
            // 将特征向量写入 ProcessedData
        }  // 函数/块结束
    }  // 函数/块结束
#endif  // 结束条件编译块
// 结束 BEARING_VIBRATION_PROJECT 处理流水线编译块

    if (!processed_data.features.empty()) {  // 条件判断
    // 特征向量非空时计算并写入统计量
        processed_data.stats.mean =  // 计算并写入特征均值
        // 计算特征向量算术均值
            std::accumulate(processed_data.features.begin(),  // 续：从 features 起始迭代器开始累积
            // 从 features 起始迭代器开始累积
                            processed_data.features.end(), 0.0f)  // 续：累积到末尾，初始值 0.0f
            // 累积到末尾，初始值 0.0f
            / processed_data.features.size();  // 续：除以元素个数得到均值
            // 除以元素个数得到均值
        float variance = 0;  // 方差累积变量
        // 方差累积变量，初始为 0
        for (float v : processed_data.features) {  // 逐元素计算方差
        // 逐元素计算方差
            variance += (v - processed_data.stats.mean)  // 续：(v-μ)
            // (v - μ)
                      * (v - processed_data.stats.mean);
            // × (v - μ) = (v-μ)²；逐步累积到 variance
        }  // 函数/块结束
        processed_data.stats.std_dev = std::sqrt(  // 标准差 = sqrt(方差)
        // 标准差 = sqrt(方差)
            variance / processed_data.features.size());  // 续：总体方差 = Σ(x-μ)²/n
            // 总体方差：Σ(x-μ)² / n
        processed_data.stats.min = *std::min_element(  // 特征最小值
        // 特征向量最小值（解引用最小元素迭代器）
            processed_data.features.begin(), processed_data.features.end());  // 续：在 features 全范围查找
            // 在 features 全范围内查找最小元素
        processed_data.stats.max = *std::max_element(  // 特征最大值
        // 特征向量最大值
            processed_data.features.begin(), processed_data.features.end());  // 续：在 features 全范围查找
            // 在 features 全范围内查找最大元素
        processed_data.stats.energy = std::accumulate(  // 特征能量 = Σ(feature²)
        // 特征能量 = Σ(feature²)，即 L2 范数的平方
            processed_data.features.begin(), processed_data.features.end(), 0.0f,  // 续：初始值 0.0f
            // 初始值 0.0f，在 features 范围内累积
            [](float s, float v) { return s + v * v; });  // 续：累积函数：s + v²
            // 自定义累积函数：每次将当前元素的平方加入累积和
    }  // 函数/块结束
    return ErrorCode::SUCCESS;  // 返回
    // 主处理流水线全部完成，返回成功
}  // 函数/块结束

ErrorCode DataProcessingFramework::extract_features(const RawData& raw_data, std::vector<float>& features) {  // 方法实现
// 通用特征提取接口（桩实现）
    (void)raw_data; (void)features;  // 代码行
    // (void) 消除两个未使用参数的编译器警告
    return ErrorCode::NOT_IMPLEMENTED;  // 返回
    // 桩实现：具体特征提取已在 process_data() 的专用方法中完成
}  // 函数/块结束

ErrorCode DataProcessingFramework::augment_data(RawData& data) {  // 方法实现
// 数据增强接口（桩实现）
    (void)data;  // 代码行
    // (void) 消除未使用参数警告
    return ErrorCode::NOT_IMPLEMENTED;  // 返回
    // 桩实现：数据增强功能尚未实现
}  // 函数/块结束

ErrorCode DataProcessingFramework::fuse_data(const std::vector<RawData>& multi_source_data, RawData& fused_data) {  // 方法实现
// 多数据源融合接口（桩实现）
    (void)multi_source_data; (void)fused_data;  // 代码行
    // (void) 消除两个未使用参数的编译器警告
    return ErrorCode::NOT_IMPLEMENTED;  // 返回
    // 桩实现：多源融合逻辑已内嵌在 process_data() 的三路图像/三轴数据处理中
}  // 函数/块结束

#ifdef BEARING_VIBRATION_PROJECT  // 条件编译：仅在轴承振动项目中编译以下代码
// 轴承振动监测 CWT 和特征提取方法编译块

// ============================================================================
// [v15.0] next_fast_len：返回最小的 2^a × 3^b × 5^c × 7^d × 11^e >= n
//
// 与 scipy.fft.next_fast_len（pocketfft）完全一致，22/22 测试用例匹配。
// pywt conv 方法通过 scipy.signal.fftconvolve 内部调用此逻辑确定 FFT 尺寸。
//
// v14.0 用 next_pow2，导致 FFT 尺寸偏大（最多 +78%），
// IFFT 浮点路径偏离 Python → SSIM=0.879 的根本原因之一。
// ============================================================================
static int next_fast_len(int n) {  // 代码行
// 返回 ≥n 的最小 11-smooth 数；FFTW3 对 11-smooth 尺寸有专用高效算法
    if (n <= 1) return 1;  // 条件判断
    // 输入 n≤1 时直接返回 1（平凡情形）

    // 以 next_pow2(n) 作为安全上界
    int pow2 = 1;  // 初始化 2 的幂次累积变量
    // 初始化 2 的幂次累积变量
    while (pow2 < n) pow2 <<= 1;  // 循环左移（×2）直到 pow2 ≥ n
    // 循环左移（×2）直到 pow2 ≥ n，得到 next_pow2(n)
    int best = pow2;  // 初始最优解为纯 2 的幂次方（11-smooth 特例）
    // 初始最优解为纯 2 的幂次方（11-smooth 的特例）

    // 枚举所有 2^a × 3^b × 5^c × 7^d × 11^e，找最小的 >= n
    for (long long p11 = 1; p11 <= (long long)best; p11 *= 11) {  // 枚举 11 的幂次
    // 枚举 11 的幂次（11^0, 11^1, 11^2, …）
        for (long long p711 = p11; p711 <= (long long)best; p711 *= 7) {  // 枚举 7 的幂次
        // 在 11 幂次基础上枚举 7 的幂次（p711 = 11^e × 7^d）
            for (long long p5711 = p711; p5711 <= (long long)best; p5711 *= 5) {  // 枚举 5 的幂次
            // 继续枚举 5 的幂次（p5711 = 11^e × 7^d × 5^c）
                for (long long p35711 = p5711; p35711 <= (long long)best; p35711 *= 3) {  // 枚举 3 的幂次
                // 继续枚举 3 的幂次（p35711 = 11^e × 7^d × 5^c × 3^b）
                    long long p = p35711;  // p 含 3^b×5^c×7^d×11^e
                    // p 已包含 3^b × 5^c × 7^d × 11^e 的组合
                    while (p < (long long)n) p <<= 1;  // 继续乘 2 直到 p ≥ n（补 2^a 因子）
                    // 继续乘以 2 直到 p ≥ n（补充 2^a 因子）
                    if (p <= (long long)best && (int)p < best) {  // 找到更小的合法候选
                    // 找到比当前最优解更小的合法候选（≥n 且 <best）
                        best = (int)p;  // 更新最优解
                        // 更新最优解
                    }  // 函数/块结束
                }  // 函数/块结束
            }  // 函数/块结束
        }  // 函数/块结束
    }  // 函数/块结束
    return best;  // 返回
    // 返回 ≥n 的最小 11-smooth 数（FFTW3 最优 FFT 尺寸）
}  // 函数/块结束

// ============================================================================
// [v17.0] compute_cwt_image
//
// 在 v16.0 基础上修复三处与 pywt 实测行为不一致的 bug：
//
// [v17.0 FIX-1] N_psi：1025 → 1024
//   实测 pywt.integrate_wavelet('cmor1.5-1', precision=10) 返回 1024 点，
//   不是 2^10+1=1025。C++ 之前用 1025 导致：
//     · dx = 16/1024（正好对），但 int_psi 多算一个点（k=1024 处）
//     · j_val 的分母 (N_psi-1) = 1024，而 Python 用 1023
//   修复：N_psi = 1024，dx = 16.0/(N_psi-1) = 16/1023
//
// [v17.0 FIX-2] j_len：移除 sd_is_int 分支
//   Python pywt 直接：j_len = int(np.floor(sd)) + 1
//   C++ 之前对非整数 sd 多加 1，导致部分尺度的 j_len 偏大，
//   sz = next_fast_len(n+j_len-1) 偏大，卷积结果偏移。
//   修复：j_len = static_cast<int>(std::floor(sd)) + 1（无条件）
//
// [v17.0 FIX-3] j_val 分母：(N_psi-1) 随 N_psi 同步修正
//   N_psi=1024 → 分母 = 1023，与 Python j[k] = int(k * 1023 / sd) 对齐
//   （Python 实际：j = np.floor(np.arange(j_len) / sd * (len(int_psi)-1)).astype(int)
//    len(int_psi)-1 = 1023）
//
// 累计修复列表（v15 + v16 + v17）：
//   1. next_pow2 → next_fast_len（11-smooth）       [v15.0]
//   2. float64 abs → float32 截断后 abs              [v16.0]
//   3. N_psi 1025→1024，j_len floor，j_val 分母 1023 [v17.0]
// ============================================================================
ErrorCode DataProcessingFramework::compute_cwt_image(  // 方法实现
    const std::vector<float>& signal,    // 输入：单轴振动时域信号，float32，通常 5000 点
    cv::Mat& output_image)               // 输出：224×224 CWT 时频图，CV_8U
{  // 函数/块起始
    static bool version_printed = false;  // 代码行
    // 静态标志：版本信息在程序运行期间只打印一次
    if (!version_printed) {  // 条件判断
    // 首次进入时打印版本信息
        std::cerr << "[CWT v17.2] next_fast_len + N_psi=1024 + j_len=floor(sd)+1"  // 续：打印 CWT 版本信息（第一行）
                     " + abs(Re(coef)) [isrealobj fix]\n";  // 代码行
        // 打印 CWT 版本号和五项关键修复摘要
        version_printed = true;  // 代码行
        // 标记为已打印
    }  // 函数/块结束

    const int n = static_cast<int>(signal.size());  // 信号长度 n（通常 5000）
    // 信号长度 n（通常 5000）
    if (n <= 0) return ErrorCode::INVALID_PARAMETER;  // 条件判断
    // 空信号或负长度返回参数无效错误

    // ① 预加重滤波（α=0.97）
    // 模拟 Python float32 截断：float64 计算 → cast to float32 → back to float64
    std::vector<double> sig(n);  // 预加重后信号的 double 缓冲区
    // 预加重后信号的 double 缓冲区（FFTW3 使用 double 精度）
    sig[0] = static_cast<double>(signal[0]);  // 第 0 点直接转 double，不做预加重
    // 第 0 点不做预加重，直接转 double
    for (int i = 1; i < n; ++i) {  // 从第 1 点开始计算预加重差分
    // 从第 1 点开始计算预加重差分
        const float pre_f32 = static_cast<float>(  // 截断到 float32，精确模拟 Python float32 路径
        // 截断到 float32：精确模拟 Python float32 信号路径的精度损失
            static_cast<double>(signal[i]) - 0.97 * static_cast<double>(signal[i-1]));  // 续：float64 精度差分 y[i]=x[i]-0.97×x[i-1]
            // float64 精度差分：y[i] = x[i] - 0.97 × x[i-1]
        sig[i] = static_cast<double>(pre_f32);  // float32 截断结果转回 double 供 FFTW3 使用
        // float32 截断结果转回 double，供 FFTW3 使用
    }  // 函数/块结束

    // ② CWT 参数
    const int    totalscal = config_.signal.cwt_scales;       // 128
    // 总尺度数（CWT 频率分析层数），默认 128（时频图行数）
    const double fc        = config_.signal.cwt_center_freq;  // 1.0
    // Morlet 小波中心频率（归一化），默认 1.0（cmor1.5-1 的 fc=1）
    const double B         = config_.signal.bandwidth;        // 1.5
    // Morlet 小波带宽参数（高斯包络宽度），默认 1.5（cmor1.5-1 的 B=1.5）
    if (totalscal <= 0) return ErrorCode::INVALID_PARAMETER;  // 尺度数必须为正整数
    // 尺度数必须为正整数

    const double cparam = 2.0 * fc * totalscal;  // 尺度参数：cparam = 2×fc×totalscal
    // 尺度参数：cparam = 2×1.0×128 = 256.0
    std::vector<double> scales_vec(totalscal);  // 尺度序列数组，长度 128
    // 尺度序列数组，长度 128
    for (int i = 0; i < totalscal; ++i)  // 逐尺度计算
    // 逐尺度计算
        scales_vec[i] = cparam / static_cast<double>(totalscal - i);  // scales[i]=cparam/(128-i)；i=0 最高频
        // scales[i] = 256/(128-i)；i=0 最小（高频），i=127 最大（低频）

    // ③ 生成 int_psi（[v17.0 FIX-1] N_psi=1024，precision=10，保留复数）
    //
    // pywt.integrate_wavelet('cmor1.5-1', precision=10) 实测返回 1024 点。
    // pywt 源码：N = 2^precision = 1024，不是 2^precision+1。
    // dx = (x_max - x_min) / (N - 1) = 16 / 1023
    // int_psi = conj(cumsum(psi) * dx)，实部不变，虚部取反
    const int    N_psi    = 1024;                  // [v17.0 FIX-1] 1025 → 1024
    // v17.0 修复：N_psi 从 1025 改为 1024，与 pywt 实测一致
    const double dx       = 16.0 / (N_psi - 1);   // 16 / 1023
    // 采样间隔：x 范围 [-8,+8]（长度 16），1024 个点；v17.0 分母修正为 1023
    const double norm_psi = 1.0 / std::sqrt(M_PI * B);  // Morlet 归一化系数 = 1/sqrt(π×B)，确保小波能量归一化
    // Morlet 归一化系数 = 1/sqrt(π×B)，确保小波能量归一化

    std::vector<double> int_psi_re(N_psi), int_psi_im(N_psi);  // 积分小波实部和虚部序列，各 1024 个 double
    // 积分小波的实部和虚部序列（复共轭形式），各 1024 个 double
    {  // 函数/块起始
    // 内层作用域限制 cum_re/cum_im 的生命周期
        double cum_re = 0.0, cum_im = 0.0;  // 实部/虚部黎曼积分累积变量
        // 实部/虚部黎曼积分累积变量
        for (int k = 0; k < N_psi; ++k) {  // 逐点构造积分小波
        // 逐点构造积分小波
            const double t  = -8.0 + k * dx;  // 当前采样点时间坐标（k=0→t=-8，k=1023→t≈+8）
            // 当前采样点时间坐标（k=0 时 t=-8，k=1023 时 t≈+8）
            const double g  = std::exp(-t * t / B);  // Morlet 高斯包络：exp(-t²/B)
            // Morlet 高斯包络：exp(-t²/B)
            cum_re += norm_psi * g * std::cos(2.0 * M_PI * fc * t) * dx;  // 实部积分黎曼和
            // 实部积分：Σ norm_psi × g × cos(2π×fc×t) × dx（黎曼和）
            cum_im += norm_psi * g * std::sin(2.0 * M_PI * fc * t) * dx;  // 虚部积分黎曼和
            // 虚部积分：Σ norm_psi × g × sin(2π×fc×t) × dx
            int_psi_re[k] =  cum_re;  // 存储第 k 点积分小波实部
            // 存储第 k 点积分小波实部
            int_psi_im[k] = -cum_im;   // conj
            // 虚部取负（复共轭 conj：对应 Python conj(cumsum(psi)*dx) 的虚部取反）
        }  // 函数/块结束
    }  // 函数/块结束

    // ④ CWT 系数矩阵 (totalscal × n)
    cv::Mat coeffs(totalscal, n, CV_32F, cv::Scalar(0.0f));  // 128×5000 系数矩阵，全零 CV_32F；行=频率，列=时间
    // 128×5000 的 CV_32F 系数矩阵，全零初始化；行=尺度（频率），列=时间

    // 信号 FFT 缓存（不同 sz 各算一次）
    std::map<int, std::pair<std::vector<double>, std::vector<double>>> sig_fft_cache;  // FFT 缓存：键=FFT 尺寸，值=(实部,虚部)向量对
    // FFT 缓存：键=FFT 尺寸 sz，值=(实部向量, 虚部向量)，避免相同 sz 重复计算

    auto get_sig_fft = [&](int sz) -> const std::pair<std::vector<double>, std::vector<double>>& {  // lambda 定义：获取尺寸 sz 的信号 FFT（带缓存）
    // lambda：获取信号在尺寸 sz 下的 FFT（带缓存）
        auto it = sig_fft_cache.find(sz);  // 在缓存 map 中查找 sz 键
        // 在缓存 map 中查找 sz 键
        if (it != sig_fft_cache.end()) return it->second;  // 缓存命中：直接返回，避免重复计算
        // 缓存命中：直接返回，避免重复计算

        fftw_complex* buf_in  = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * sz);  // 分配 FFTW3 专用复数输入缓冲区（SIMD 对齐）
        // 分配 FFTW3 专用复数输入缓冲区（保证 SIMD 内存对齐）
        fftw_complex* buf_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * sz);  // 分配 FFT 输出缓冲区
        // 分配 FFT 输出缓冲区
        for (int k = 0; k < sz; ++k) {  // 逐点处理
        // 填充信号数据并进行零填充
            buf_in[k][0] = (k < n) ? sig[k] : 0.0;  // 实部：k<n 取信号，k≥n 补零（线性卷积）
            // 实部：k<n 取预加重信号值，k≥n 补零（实现线性卷积）
            buf_in[k][1] = 0.0;  // 虚部置零（实数信号）
            // 虚部置零（信号为实数序列）
        }  // 函数/块结束
        fftw_plan p = fftw_plan_dft_1d(sz, buf_in, buf_out, FFTW_FORWARD, FFTW_ESTIMATE);  // 创建正向 FFT 规划（信号）
        // 创建正向 DFT 规划（FFTW_FORWARD=-1，FFTW_ESTIMATE 快速规划无需测量）
        fftw_execute(p);  // 执行 FFT/IFFT 计算
        // 执行 FFT，结果写入 buf_out
        fftw_destroy_plan(p);  // 销毁规划，释放内部资源
        // 销毁规划，释放内部算法资源
        fftw_free(buf_in);  // 释放 fftw_malloc 分配的输入缓冲区
        // 释放 fftw_malloc 分配的输入缓冲区（必须用 fftw_free）

        std::vector<double> re(sz), im(sz);  // 标准 vector 存储 FFT 输出（便于缓存）
        // 标准 vector 存储 FFT 输出
        for (int k = 0; k < sz; ++k) {  // 逐点处理
        // 将 fftw_complex 格式拷贝到标准 vector
            re[k] = buf_out[k][0];  // 提取第 k 个频率分量实部
            // 提取第 k 个频率分量实部
            im[k] = buf_out[k][1];  // 提取第 k 个频率分量虚部
            // 提取第 k 个频率分量虚部
        }  // 函数/块结束
        fftw_free(buf_out);  // 释放 FFT 输出缓冲区
        // 释放 FFT 输出缓冲区

        sig_fft_cache[sz] = {std::move(re), std::move(im)};  // 移动语义存入缓存（避免大向量深拷贝）
        // 移动语义存入缓存（避免大向量深拷贝）
        return sig_fft_cache[sz];  // 返回新插入条目的引用
        // 返回新插入条目的引用
    };  // 结构体/类/lambda 定义结束

    for (int s_idx = 0; s_idx < totalscal; ++s_idx) {  // 逐尺度计算 CWT 系数行
    // 逐尺度（频率层）计算 CWT 系数
        const double scale  = scales_vec[s_idx];  // 当前尺度值
        // 当前尺度值（scale 越大对应越低频）
        const double sqrt_s = std::sqrt(scale);  // 能量归一化系数 sqrt(scale)
        // 能量归一化系数 sqrt(scale)，消除不同尺度间能量差异

        // a. j_len：[v17.0 FIX-2] 直接 floor(sd)+1，移除 sd_is_int 分支
        //
        // Python pywt：
        //   j = np.arange(-np.floor((len(int_psi)-1)/2), np.floor((len(int_psi)-1)/2)+2)
        //   j *= scale * (len(int_psi)-1) / (N_psi-1) / (x[-1]-x[0])
        //   但对于 cwt 的 j_len，等效为：
        //   j_len = int(np.floor(sd)) + 1  where sd = scale * psi_domain
        const double psi_domain = 16.0;  // 小波定义域长度 = x_max - x_min = 8-(-8) = 16.0
        // 小波定义域长度 = 8-(-8) = 16.0
        const double sd         = scale * psi_domain;  // sd = scale × 16；决定当前尺度离散小波数组的点数
        // sd = scale × 16；决定当前尺度的离散小波数组点数
        const int    j_len      = static_cast<int>(std::floor(sd)) + 1;  // [v17.0 FIX-2]
        // j_len = floor(sd)+1；v17.0 修复：无条件 floor，与 Python pywt 完全对齐

        // b. [v15.0] next_fast_len 匹配 scipy.fft.next_fast_len（11-smooth）
        const int sz = next_fast_len(n + j_len - 1);  // FFT 尺寸 = n+j_len-1 的最小 11-smooth 上界
        // FFT 尺寸 = n+j_len-1 的最小 11-smooth 上界（线性卷积最优尺寸）

        // c. 获取信号 FFT（缓存）
        const auto& [X_re, X_im] = get_sig_fft(sz);  // C++17 结构化绑定：获取信号 FFT 的实部和虚部向量
        // C++17 结构化绑定：获取尺寸 sz 的信号 FFT 实部和虚部向量

        // d. 构建 reversed complex int_psi_scale
        //
        // Python pywt:
        //   j_f = np.arange(j_len) / sd * (N_psi - 1)   # float indices
        //   j   = np.floor(j_f).astype(int)              # floor → int
        //   j   = np.clip(j, 0, N_psi - 1)
        //   wav = int_psi[j][::-1]                       # reversed
        //
        // [v17.0 FIX-3] 分母 (N_psi-1) = 1023（N_psi=1024）
        // C++ truncation == Python floor for non-negative values ✓
        fftw_complex* wav = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * sz);  // 分配小波核时域缓冲区（sz 个 fftw_complex，SIMD 对齐）
        // 分配小波核时域缓冲区（sz 个 fftw_complex）
        for (int p = 0; p < sz; ++p) {  // 循环
        // 逐点填充离散化反转积分小波
            if (p >= j_len) {  // 条件判断
            // 超出小波有效长度
                wav[p][0] = 0.0;  // 超出 j_len 部分零填充（线性卷积需要）
                // 超出 j_len 的位置零填充（线性卷积零填充区域）
                wav[p][1] = 0.0;  // 虚部同样置零
                // 虚部同样置零
            } else {  // 否则
            // p 在有效范围 [0, j_len) 内
                // reversed index: forward index = j_len-1-p
                const int fwd = j_len - 1 - p;  // 反转索引：对应 Python[::-1] 翻转
                // 反转索引：p=0 对应正向末尾 j_len-1，实现 Python[::-1] 翻转
                const int j_val_raw = (j_len <= 1) ? 0 :  // j_len≤1 取 0 防除零；否则计算映射下标
                // j_len≤1 时直接取 0，防止 sd=0 除零
                    static_cast<int>(  // 续：float→int 截断（等价 Python floor 对非负数）
                    // 计算正向索引 fwd 在积分小波数组中的映射下标
                        static_cast<double>(fwd) / sd * (N_psi - 1));  // [v17.0 FIX-3]
                        // j_val = floor(fwd/sd × 1023)；v17.0：分母 1024→1023
                const int j_val = std::min(j_val_raw, N_psi - 1);  // 裁剪到 [0,1023]，对应 Python np.clip
                // 裁剪到 [0, 1023]，对应 Python np.clip(j, 0, N_psi-1)
                wav[p][0] = int_psi_re[j_val];  // 从积分小波实部数组取对应点
                // 从积分小波实部数组取对应点
                wav[p][1] = int_psi_im[j_val];  // 从积分小波虚部数组取对应点（含 conj 负号）
                // 从积分小波虚部数组取对应点（含 conj 负号）
            }  // 函数/块结束
        }  // 函数/块结束

        // e. 小波 FFT
        fftw_complex* W = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * sz);  // 分配小波核 FFT 输出缓冲区
        // 分配小波核 FFT 输出缓冲区
        {  // 函数/块起始
        // 内层作用域限制 fftw_plan 变量生命周期
            fftw_plan p = fftw_plan_dft_1d(sz, wav, W, FFTW_FORWARD, FFTW_ESTIMATE);  // 创建小波核正向 FFT 规划
            // 创建小波核正向 FFT 规划
            fftw_execute(p);  // 执行 FFT/IFFT 计算
            // 执行小波核 FFT（时域→频域）
            fftw_destroy_plan(p);  // 销毁规划，释放内部资源
            // 销毁规划，释放内部资源
        }  // 函数/块结束
        fftw_free(wav);  // 释放小波核时域缓冲区
        // 释放小波核时域缓冲区（FFT 已完成）

        // f. 频域乘法
        fftw_complex* prod = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * sz);  // 分配频域乘积缓冲区
        // 分配频域乘积缓冲区
        for (int k = 0; k < sz; ++k) {  // 逐点处理
        // 逐频率分量执行复数乘法 prod = X × W
            prod[k][0] = X_re[k] * W[k][0] - X_im[k] * W[k][1];  // 频域乘积实部：Re(X×W)
            // 乘积实部：Re(X×W) = Re(X)×Re(W) - Im(X)×Im(W)
            prod[k][1] = X_re[k] * W[k][1] + X_im[k] * W[k][0];  // 频域乘积虚部：Im(X×W)
            // 乘积虚部：Im(X×W) = Re(X)×Im(W) + Im(X)×Re(W)
        }  // 函数/块结束
        fftw_free(W);  // 释放小波核 FFT 缓冲区
        // 释放小波核 FFT 缓冲区

        // g. IFFT
        fftw_complex* conv_buf = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * sz);  // 分配 IFFT 输出缓冲区（时域卷积结果）
        // 分配 IFFT 输出缓冲区（时域卷积结果）
        {  // 函数/块起始
        // 内层作用域
            fftw_plan p = fftw_plan_dft_1d(sz, prod, conv_buf, FFTW_BACKWARD, FFTW_ESTIMATE);  // 创建逆向 FFT 规划（BACKWARD 不自动归一化）
            // 创建逆向 FFT 规划（FFTW_BACKWARD=+1；FFTW3 不自动归一化，须手动除 sz）
            fftw_execute(p);  // 执行 FFT/IFFT 计算
            // 执行 IFFT，频域乘积→时域卷积结果
            fftw_destroy_plan(p);  // 销毁规划，释放内部资源
            // 销毁规划
        }  // 函数/块结束
        fftw_free(prod);  // 释放频域乘积缓冲区
        // 释放频域乘积缓冲区

        // h. 提取 CWT 系数
        //
        // pywt 完整流程（v17.2 精确复现）：
        //
        //   conv_py[k]  = conv_fftw[k] / sz                     （归一化）
        //   diff_py[k]  = conv_py[k+1] - conv_py[k]             （差分，复数）
        //   coef_f64[j] = -sqrt_s * diff_py[d+j]                （float64 复数）
        //   coef_f32[j] = coef_f64[j].astype(complex64)         （float32 截断）
        //
        //   [v17.2 关键修复] pywt 对实数信号使用复数小波的行为：
        //     if np.isrealobj(data):          ← signal 是 float32 → 满足！
        //         coef = coef.real            ← 只取实部！丢弃虚部！
        //     output = np.abs(coef)           ← np.abs(float32) = fabsf(coef.real)
        //
        //   v16.0 的错误：np.abs(complex64) = sqrt(re²+im²)（复数模）
        //   正确做法：    np.abs(float32)  = |re|（绝对值）
        //
        //   两者差异：虚部不为零时 sqrt(re²+im²) > |re|，
        //   NORM_MINMAX 消除全局比例，但改变局部相对幅度 → 竖纹错位。
        //
        // d = floor((j_len-1)/2)，norm_factor = sz
        const int    d           = (j_len - 1) / 2;  // 偏移量 d=floor((j_len-1)/2)，定位有效系数段起点
        // 偏移量 d = floor((j_len-1)/2)；定位卷积结果中的有效系数段起点
        const double norm_factor = static_cast<double>(sz);  // IFFT 归一化因子=sz（FFTW3 不自动除以 N）
        // IFFT 归一化因子 = sz；FFTW3 IFFT 不自动除以 N，须手动归一化

        // 防御性验证：确保 d+n 不越界（d + n - 1 + 1 < sz）
        // 理论上 sz = next_fast_len(n + j_len - 1) >= n + j_len - 1
        // d = (j_len-1)/2，d + n + 1 <= (j_len-1)/2 + n + 1 <= n + j_len - 1 < sz ✓
        // 但仍加边界保护，防止极端浮点情况
        if (d + n >= sz) {  // 条件判断
        // 越界检测（理论上不触发）
            std::cerr << "[CWT] Warning: idx out of range s_idx=" << s_idx  // 打印越界警告
                      << " d=" << d << " n=" << n << " sz=" << sz  // 续：越界警告参数
                      << " j_len=" << j_len << std::endl;  // 续：越界警告末行
            // 打印越界警告及所有相关参数
            fftw_free(conv_buf);  // 释放 IFFT 输出缓冲区
            // 释放缓冲区后跳过（保留当前尺度为零行）
            continue;  // 跳到下一迭代
            // 跳到下一尺度
        }  // 函数/块结束

        float* row = coeffs.ptr<float>(s_idx);  // 获取 coeffs 第 s_idx 行的 float32 指针
        // 获取 CWT 系数矩阵第 s_idx 行的 float32 原始指针
        for (int j_out = 0; j_out < n; ++j_out) {  // 对当前尺度的 n 个时间点提取系数
        // 对当前尺度的 n 个时间点逐一提取 CWT 系数
            const int idx_a = d + j_out;  // 差分起始索引：偏移 d + 时间点 j_out
            // 差分起始索引：偏移 d + 时间点索引 j_out
            const int idx_b = d + j_out + 1;  // 差分终止索引：idx_a + 1
            // 差分终止索引：idx_a + 1（相邻点差分）

            // float64 差分，除以 sz 归一化（只取实部，虚部丢弃）
            const double re_diff = (conv_buf[idx_b][0] - conv_buf[idx_a][0]) / norm_factor;  // 实部差分+IFFT归一化；v17.2 只取实部
            // 实部差分并归一化：(Re(conv[d+j+1]) - Re(conv[d+j])) / sz
            // [v17.2 关键修复] 只取实部（Python isrealobj=True 时 coef=coef.real）

            // [v17.2] 先乘 -sqrt_s（float64），截断到 float32，再取绝对值
            const float coef_re = static_cast<float>(-sqrt_s * re_diff);  // float64→float32 截断，对应 Python astype(complex64)实部
            // float64 乘法后截断到 float32（对应 Python .astype(complex64) 后取 .real）
            row[j_out] = std::fabs(coef_re);  // 取绝对值 |coef_re|；v17.2：fabsf 而非复数模
            // 取绝对值 |coef_re|；v17.2：fabsf（实数绝对值）而非复数模 sqrt(re²+im²)
        }  // 函数/块结束

        fftw_free(conv_buf);  // 释放 IFFT 输出缓冲区
        // 释放 IFFT 输出缓冲区（当前尺度系数已提取完毕）
    }  // 函数/块结束
    // 128 个尺度全部处理完毕，coeffs 包含完整 CWT 系数矩阵（128×5000，CV_32F）

    // ⑤ log1p → resize(224×224) → NORM_MINMAX([0,255]) → CLAHE
    for (int r = 0; r < coeffs.rows; ++r) {  // 逐行对 CWT 系数执行 log1p 变换
    // 逐行对 CWT 系数执行 log1p 变换
        float* row = coeffs.ptr<float>(r);  // 获取第 r 行 float32 指针
        // 获取第 r 行 float32 原始指针
        for (int c = 0; c < coeffs.cols; ++c)  // 逐列处理
        // 逐列处理每个系数
            row[c] = std::log1p(row[c]);  // log1p(x)=ln(1+x)：对数压缩大动态范围
            // log1p(x)=ln(1+x)：对数压缩大动态范围；比 log(x) 数值稳定（x=0 时 log1p=0）
    }  // 函数/块结束
    cv::Mat resized, normed, normed_u8;  // 声明 resize/归一化/uint8 转换中间容器
    // 声明 resize/归一化/uint8 转换中间容器
    cv::resize(coeffs, resized, cv::Size(224, 224), 0, 0, cv::INTER_LINEAR);  // 128×5000→224×224 双线性插值缩放
    // 将 128×5000 的 CWT 系数矩阵双线性插值缩放至 224×224
    cv::normalize(resized, normed, 0, 255, cv::NORM_MINMAX);  // 线性归一化至 [0,255]
    // 线性归一化至 [0,255]：(x-min)/(max-min)×255
    normed.convertTo(normed_u8, CV_8U);  // CV_32F→CV_8U 截断四舍五入
    // CV_32F → CV_8U：saturate_cast 截断四舍五入
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(3.0, cv::Size(8, 8));  // 创建 CLAHE：clip=3.0，8×8 分块
    // 创建 CLAHE 对象：clip=3.0，8×8 分块
    clahe->apply(normed_u8, output_image);  // 执行 CLAHE，结果写入 output_image（最终 CWT 时频图）
    // 执行 CLAHE，结果写入 output_image（CV_8U 224×224）；即最终 CWT 时频图
    return ErrorCode::SUCCESS;  // 返回
    // CWT 时频图生成完成
}  // 函数/块结束


ErrorCode DataProcessingFramework::extract_vibration_features(const std::vector<float>& vibration_data, std::vector<float>& features) {  // 方法实现
// 提取振动信号四个时域统计特征：均值、标准差、最大值、最小值
    if (vibration_data.empty()) return ErrorCode::SUCCESS;  // 条件判断
    // 空信号不追加特征，返回成功
    float mean = std::accumulate(vibration_data.begin(), vibration_data.end(), 0.0f) / vibration_data.size();  // 均值 = Σx/n
    // 均值：std::accumulate 从 0.0f 开始累积所有元素，除以元素个数
    float variance = 0;  // 方差累积变量
    // 方差累积变量
    float max_val = vibration_data[0];  // 最大值初始化为首元素
    // 最大值，初始化为首元素
    float min_val = vibration_data[0];  // 最小值初始化为首元素
    // 最小值，初始化为首元素
    for (float v : vibration_data) {  // 逐采样点更新方差和极值
    // 逐采样点更新方差和极值
        variance += (v - mean) * (v - mean);  // 累加方差分子：Σ(x-μ)²
        // 累加方差分子：Σ(x-μ)²
        if (v > max_val) max_val = v;  // 更新最大值
        // 若当前点大于已知最大值则更新
        if (v < min_val) min_val = v;  // 更新最小值
        // 若当前点小于已知最小值则更新
    }  // 函数/块结束
    variance /= vibration_data.size();  // 总体方差 = Σ(x-μ)²/n
    // 总体方差：Σ(x-μ)² / n
    features.push_back(mean);  // 追加均值特征
    // 特征 1：均值（直流偏置分量）
    features.push_back(std::sqrt(variance));  // 追加标准差特征
    // 特征 2：标准差 = sqrt(variance)（振动幅度离散程度）
    features.push_back(max_val);  // 追加最大值特征
    // 特征 3：最大值（峰值幅度，反映冲击强度）
    features.push_back(min_val);  // 追加最小值特征
    // 特征 4：最小值（谷值幅度）
    return ErrorCode::SUCCESS;  // 返回
    // 特征提取完成
}  // 函数/块结束
#endif  // 结束条件编译块
// 结束 BEARING_VIBRATION_PROJECT CWT 和特征提取编译块

ErrorCode DataProcessingFramework::normalize_data(std::vector<float>& data, float min_val, float max_val) {  // 方法实现
// min-max 归一化：将向量所有元素线性映射到 [min_val, max_val]
    if (data.empty()) return ErrorCode::SUCCESS;  // 条件判断
    // 空向量直接返回，无需处理
    float min_elem = *std::min_element(data.begin(), data.end());  // 查找向量最小元素
    // 查找向量最小元素（解引用迭代器取值）
    float max_elem = *std::max_element(data.begin(), data.end());  // 查找向量最大元素
    // 查找向量最大元素
    float range = max_elem - min_elem;  // 数据范围 = max - min
    // 数据范围：max - min
    if (range < 1e-6) {  // 条件判断
    // 范围近零（所有元素几乎相同）
        std::fill(data.begin(), data.end(), min_val);  // 全部填充为 min_val 避免除零
        // 全部填充为 min_val（避免除以近零值产生 NaN）
        return ErrorCode::SUCCESS;  // 返回
        // 提前返回
    }  // 函数/块结束
    for (float& v : data) {  // 逐元素线性映射（v 为引用，原地修改）
    // 逐元素线性映射（v 为引用，原地修改）
        v = (v - min_elem) / range * (max_val - min_val) + min_val;  // 线性映射公式
        // 线性映射：v_norm = (v-min)/(max-min) × (max_val-min_val) + min_val
    }  // 函数/块结束
    return ErrorCode::SUCCESS;  // 返回
    // 归一化完成
}  // 函数/块结束

ErrorCode DataProcessingFramework::apply_noise_reduction(std::vector<float>& data) {  // 方法实现
// 三点滑动平均降噪（低通滤波）
    if (data.size() < 3) return ErrorCode::SUCCESS;  // 条件判断
    // 不足 3 点无法计算三点均值，直接返回
    std::vector<float> smoothed(data.size());  // 创建平滑结果缓冲区（与输入等长）
    // 创建与输入等长的平滑结果缓冲区（避免原地修改破坏后续计算）
    smoothed[0] = data[0];  // 首点直接复制（无前驱点）
    // 首点直接复制（无前驱点）
    smoothed[data.size()-1] = data.back();  // 末点直接复制（无后继点）
    // 末点直接复制（无后继点）
    for (size_t i = 1; i < data.size()-1; ++i) {  // 对中间点执行三点均值
    // 对中间点执行三点均值
        smoothed[i] = (data[i-1] + data[i] + data[i+1]) / 3.0f;  // 三点算术均值降噪
        // (前驱 + 当前 + 后继) / 3：有效抑制高频随机噪声
    }  // 函数/块结束
    data = smoothed;  // 用平滑结果覆盖原数据
    // 用平滑结果覆盖原数据
    return ErrorCode::SUCCESS;  // 返回
    // 降噪完成
}  // 函数/块结束

ErrorCode DataProcessingFramework::segment_image(const cv::Mat& image, std::vector<cv::Mat>& segments) {  // 方法实现
    (void)image; (void)segments;  // 代码行
    // (void) 消除两个未使用参数的编译器警告
    return ErrorCode::NOT_IMPLEMENTED;  // 返回
    // 桩实现：图像分割功能尚未实现
}  // 函数/块结束

ErrorCode DataProcessingFramework::configure(const std::string& config_json) {  // 方法实现
    (void)config_json;  // 代码行
    // (void) 消除未使用参数警告
    return ErrorCode::SUCCESS;  // 返回
    // 桩实现：配置热更新尚未实现，返回 SUCCESS 使调用链不中断
}  // 函数/块结束

std::string DataProcessingFramework::get_config() const {  // 方法实现
    return "{}";  // 返回
    // 返回空 JSON 对象；完整实现须将 config_ 的所有字段序列化为 JSON 字符串
}  // 函数/块结束

ErrorCode DataProcessingFramework::SignalFilter::init_bandpass(float low_freq, float high_freq, float sample_rate) {  // 方法实现
// IIR 带通滤波器系数初始化（当前为硬编码桩实现）
    (void)low_freq; (void)high_freq; (void)sample_rate;  // 代码行
    // (void) 消除三个未使用参数的编译器警告（完整实现需据此动态计算 Butterworth 系数）
    b_coeffs_ = {0.0675f, 0.0f, -0.1349f, 0.0f, 0.0675f};  // 硬编码前馈系数（b0~b4，4阶带通示例）
    // 设置前馈（分子/b）系数为硬编码固定值（4 阶带通示例：b0,b1,b2,b3,b4）
    a_coeffs_ = {1.0f, -1.1429f, 0.4128f, 0.0f, 0.0f};  // 硬编码反馈系数；a[0]=1.0 为归一化首项
    // 设置反馈（分母/a）系数；a[0]=1.0 为 IIR 差分方程的归一化分母首项
    return ErrorCode::SUCCESS;  // 返回
    // 系数设置完成
}  // 函数/块结束

ErrorCode DataProcessingFramework::SignalFilter::apply_filter(const std::vector<float>& input, std::vector<float>& output) {  // 方法实现
// 对输入信号序列应用 IIR 带通滤波器（直接形式 II 差分方程，因果递推）
    output.resize(input.size());  // 分配与输入等长的输出缓冲区
    // 分配与输入等长的输出缓冲区
    for (size_t n = 0; n < input.size(); ++n) {  // 逐采样点因果递推
    // 逐采样点计算滤波输出（从 n=0 开始的因果递推）
        float sum = 0;  // 当前样本输出累积值
        // 当前样本的输出累积值
        for (size_t k = 0; k < b_coeffs_.size() && k <= n; ++k) {  // 前馈项遍历（k≤n 防负索引）
        // 前馈项：k=0 到 min(b_size-1, n)（k<=n 防负索引越界）
            sum += b_coeffs_[k] * input[n - k];  // Σb[k]×x[n-k]：输入加权和
            // Σ b[k] × x[n-k]：当前及过去 k 个输入的加权和
        }  // 函数/块结束
        for (size_t k = 1; k < a_coeffs_.size() && k <= n; ++k) {  // 反馈项遍历（k从1起跳过a[0]）
        // 反馈项：k 从 1 起（跳过 a[0]=1.0 归一化项），k<=n 防负索引
            sum -= a_coeffs_[k] * output[n - k];  // -Σa[k]×y[n-k]：输出递推反馈
            // -Σ a[k] × y[n-k]：过去 k 个输出的加权和（负号体现递推反馈）
        }  // 函数/块结束
        output[n] = sum / a_coeffs_[0];  // y[n]=sum/a[0]；a[0]=1时无实际效果
        // y[n] = sum / a[0]；a[0]=1.0 时此除法无实际效果，保留标准 IIR 形式
    }  // 函数/块结束
    return ErrorCode::SUCCESS;  // 返回
    // 滤波完成
}  // 函数/块结束

ErrorCode DataProcessingFramework::FeatureExtractor::extract_time_features(const std::vector<float>& data, std::vector<float>& features) {  // 方法实现
// 提取三个时域统计特征：均值、RMS（均方根）、峰值（绝对值最大）
    if (data.empty()) return ErrorCode::SUCCESS;  // 条件判断
    // 空数据直接返回，不追加特征
    float mean = std::accumulate(data.begin(), data.end(), 0.0f) / data.size();  // 信号均值 Σx/n
    // 信号均值：Σx[i] / n
    float rms = 0;  // RMS 累积变量（存储平方和）
    // RMS 累积变量（存储平方和 Σv²）
    float peak = 0;  // 峰值（绝对值最大采样点），初始为 0
    // 峰值（绝对值最大采样点），初始为 0
    for (float v : data) {  // 逐采样点更新 RMS 和峰值
    // 逐采样点更新 RMS 和峰值
        rms += v * v;  // 累积平方和 Σv²
        // 累积平方和：Σv²
        if (std::abs(v) > peak) peak = std::abs(v);  // 若绝对值大于已知峰值则更新
        // 若当前点绝对值大于已知峰值则更新
    }  // 函数/块结束
    rms = std::sqrt(rms / data.size());  // RMS = sqrt(Σv²/n)
    // RMS = sqrt(Σv²/n)：信号有效值（能量相关）
    features.push_back(mean);  // 追加均值特征
    // 特征 1：信号均值
    features.push_back(rms);  // 追加 RMS 特征
    // 特征 2：RMS 均方根
    features.push_back(peak);  // 追加峰值特征
    // 特征 3：峰值（最大绝对幅度）
    return ErrorCode::SUCCESS;  // 返回
    // 特征提取完成
}  // 函数/块结束

ErrorCode DataProcessingFramework::FeatureExtractor::extract_freq_features(const std::vector<float>& data, float sample_rate, std::vector<float>& features) {  // 方法实现
    (void)data; (void)sample_rate; (void)features;  // 代码行
    // (void) 消除三个未使用参数的编译器警告
    return ErrorCode::NOT_IMPLEMENTED;  // 返回
    // 桩实现：频域特征（FFT 主频/频带能量比等）尚未实现
}  // 函数/块结束

ErrorCode DataProcessingFramework::FeatureExtractor::extract_statistical_features(const std::vector<float>& data, std::vector<float>& features) {  // 方法实现
    (void)data; (void)features;  // 代码行
    // (void) 消除两个未使用参数的编译器警告
    return ErrorCode::NOT_IMPLEMENTED;  // 返回
    // 桩实现：高阶统计特征（峭度/偏度等）尚未实现
}  // 函数/块结束

} // namespace MultiProject
// 关闭命名空间 MultiProject