/**detect_flames
  ******************************************************************************
  * @file    AIModelFramework.cpp
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
 * @brief   AI 推理模型框架 — 实现文件
 * ----------------------------------------------------------------------------
 * @details 本文件实现了 AIModelFramework.hpp 中声明的全部方法，包含：
 *
 *          ── AIModelFramework 主类实现 ──
 *            构造/析构、load_model/unload_model、infer/infer_batch、
 *            optimize_model/quantize_model/prune_model/fine_tune（桩实现）、
 *            preprocess_input/postprocess_output、状态查询方法
 *
 *          ── 项目特化函数实现（条件编译） ──
 *            FIRE_DETECTION_PROJECT：preprocess_fire_image、
 *              postprocess_fire_output、detect_flames（桩）、
 *              estimate_flame_intensity（桩）
 *            BEARING_VIBRATION_PROJECT：preprocess_vibration_data、
 *              postprocess_vibration_output、analyze_vibration_pattern（桩）、
 *              predict_remaining_life（桩）
 *
 *          ── ONNXModel 嵌套类实现 ──
 *            构造函数（ORT_LOGGING_LEVEL_WARNING 日志级别）
 *            load()：XNNPACK 执行提供者优先（失败降级至 4 线程 CPU）、
 *                    ORT_ENABLE_BASIC 优化级别（规避树莓派 4B 的算子融合 bug）
 *            unload()、infer()（支持 float32/uint8/int8 三种输入类型）
 *
 *          ── TensorRTModel / MultiTaskModel 嵌套类实现（桩） ──
 *            所有方法返回 NOT_IMPLEMENTED
 *
 *          ── InferenceCache 嵌套类实现 ──
 *            get()（浮点逐元素比对，容差 1e-6）、put()（LRU 驱逐）、clear()
 *
 * @note    infer() 中包含【调试1】~【调试6】共 6 处 std::cout 诊断输出，
 *          用于验证输入图像信息、归一化参数、tensor 内容、
 *          模型输入类型和输出原始数值；生产部署时可按需删除或改为条件编译。
 *          preprocess_fire_image() 目前使用 RGB 三通道归一化（ImageNet 均值）,
 *          而 infer() 的通用路径使用单通道灰度归一化（mean=33.405, std=78.795），
 *          两条路径的参数需根据实际部署的模型类型保持一致。
 * ============================================================================
 */

#include "AIModelFramework.hpp"
// 引入 AIModelFramework 类的完整声明，包含所有嵌套类、成员变量和方法签名；
// 同时通过此头文件间接引入 ONNX Runtime C++ API、OpenCV、UnifiedDataTypes 等依赖

#include <chrono>
// 引入 std::chrono，用于 infer() 中的推理耗时计时
// （high_resolution_clock::now() + duration_cast<microseconds>）

#include <iostream>
// 引入 std::cout / std::cerr，用于调试输出和错误信息打印


namespace MultiProject {
// 开启命名空间 MultiProject，所有类的方法实现均在此命名空间内

AIModelFramework::AIModelFramework(ProjectType project_type) : project_type_(project_type) {
    // 构造函数：通过成员初始化列表将 project_type 记录到 project_type_ 成员；
    // 不使用函数体赋值，成员初始化列表效率更高（直接构造而非默认构造再赋值）

    onnx_model_ = std::make_unique<ONNXModel>();
    // 在堆上构造 ONNXModel 实例，初始化 ONNX Runtime 运行时环境（Ort::Env）；
    // 此时未加载任何模型文件，loaded_=false；
    // make_unique 保证异常安全（若 ONNXModel 构造抛异常则不会内存泄漏）

    inference_cache_ = std::make_unique<InferenceCache>(100);
    // 创建容量为 100 条的推理结果缓存实例；
    // 注意：bearing_vibration_config.hpp 已将 enable_cache 修复为 false，
    // 实际推理中不应主动调用 cache.put()，
    // 此处仅创建实例以备后续按需启用
}

AIModelFramework::~AIModelFramework() = default;
// 使用编译器生成的默认析构函数；
// unique_ptr 成员（onnx_model_、inference_cache_ 等）在析构时自动释放；
// ONNXModel 析构函数（=default）会销毁 Ort::Session，
// 释放 ONNX Runtime 持有的模型权重内存

ErrorCode AIModelFramework::load_model(const std::string& model_path) {
    // 加载指定路径的 ONNX 模型文件并创建推理 Session；
    // 所有 ONNX Runtime 操作均在 try-catch 中保护，
    // 捕获 Ort::Exception（继承自 std::exception）

    try {
        auto status = onnx_model_->load(model_path);
        // 委托 ONNXModel::load() 执行实际的模型加载：
        //   打开文件 → 配置 SessionOptions → 创建 Ort::Session → 解析节点名称
        if (status == ErrorCode::SUCCESS) {
            model_loaded_ = true;
            // 加载成功后将原子标志置为 true；
            // model_loaded_ 是后续 infer() 调用的前置条件（未加载时返回 MODEL_NOT_LOADED）
        }
        return status;
        // 无论成功或失败，将 ONNXModel::load() 的状态码直接返回给调用方
    } catch (const std::exception& e) {
        std::cerr << "load_model exception: " << e.what() << std::endl;
        // 捕获加载过程中的所有标准异常（文件不存在、ONNX 格式错误等），
        // 打印错误信息并返回 MODEL_LOAD_FAILED
        return ErrorCode::MODEL_LOAD_FAILED;
    }
}


ErrorCode AIModelFramework::unload_model() {
    // 卸载已加载的模型，释放 Ort::Session 持有的权重内存

    onnx_model_->unload();
    // 委托 ONNXModel::unload() 执行：
    //   将 session_ 重置为 Ort::Session(nullptr)（释放 ONNX Runtime 模型权重）；
    //   清空 input_names_str_ / output_names_str_ 等字符串缓冲区；
    //   设置 loaded_=false

    model_loaded_ = false;
    // 将框架层的加载状态标志同步置为 false，
    // 使后续 infer() 调用返回 MODEL_NOT_LOADED 而非尝试用空 Session 推理

    return ErrorCode::SUCCESS;
}

ErrorCode AIModelFramework::infer(const ModelInput& input, ModelOutput& output) {
    // 核心推理方法：对单个输入样本执行完整的前向推理流程；
    // 流程：前置检查 → 输入预处理（图像灰度化/归一化/转 tensor）
    //       → ONNXModel::infer()（ONNX Runtime 前向传播）
    //       → postprocess_output()（argmax + 类别映射）
    //       → 性能统计更新

    if (!model_loaded_) return ErrorCode::MODEL_NOT_LOADED;
    // 前置检查：若模型未加载则立即返回错误，避免使用空 Ort::Session

    try {
        auto start = std::chrono::high_resolution_clock::now();
        // 记录推理开始时刻，用于后续计算端到端推理耗时

        std::vector<float> tensor;
        // 用于存储预处理完成后的输入 tensor（NCHW 格式的展平浮点数组）

        if (!input.input_images.empty()) {
            // 图像输入路径：ModelInput 包含图像帧（火焰检测或 CWT 图像）

            cv::Mat img = input.input_images[0];
            // 取第一张图像（当前实现只处理单张，批量由 infer_batch 循环调用）
            if (img.empty()) {
                return ErrorCode::INVALID_PARAMETER;
                // 空 cv::Mat 表示图像读取失败或摄像头未就绪，返回参数错误
            }

            // ====== 【调试1】检查输入图像基本信息 ======
            std::cout << "【调试1】输入图像: "
                      << img.rows << "x" << img.cols
                      << " channels=" << img.channels()
                      << " type=" << img.type()
                      << std::endl;
            // 【调试1】打印图像基本属性：
            //   rows/cols：图像分辨率（期望为 224×224 或 640×480）
            //   channels：通道数（期望灰度=1 或彩色 BGR=3）
            //   type：OpenCV 数据类型码（CV_8UC1=0, CV_8UC3=16, CV_32FC1=5 等）
            // 若 channels 为 3 且 type 为 CV_8UC3，说明输入是彩色图，
            // 后续会执行灰度化；若已为灰度则跳过

            cv::Mat gray;
            if (img.channels() == 3) {
                cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
                // 将 BGR 三通道彩色图转换为单通道灰度图；
                // 与训练配置（to_rgb=False，单通道输入）保持一致
            } else {
                gray = img;
                // 输入已为单通道灰度图，直接复用（浅拷贝，共享数据指针）
            }
            if (gray.rows != 224 || gray.cols != 224) {
                cv::resize(gray, gray, cv::Size(224, 224));
                // 若图像不是 224×224，执行双线性插值缩放；
                // cv::Size(width, height) 参数顺序与 Mat(rows, cols) 相反
            }

            // ====== 【调试2】检查灰度图像素值范围 ======
            double grayMin, grayMax;
            cv::minMaxLoc(gray, &grayMin, &grayMax);
            std::cout << "【调试2】灰度图: "
                      << "min=" << grayMin
                      << " max=" << grayMax
                      << " type=" << gray.type()
                      << std::endl;
            // 【调试2】验证灰度图像素值范围：
            //   正常 CV_8U 灰度图：min 应 ≥ 0，max 应 ≤ 255；
            //   若 max=0 说明图像为全黑（摄像头遮挡或 V4L2 读取异常）；
            //   type 期望为 0（CV_8UC1），转换正常

            // 归一化：与训练一致 (pixel - 33.405) / 78.795
            cv::Mat float_img;
            gray.convertTo(float_img, CV_32F);
            // 将 uint8 [0,255] 的灰度图转换为 float32 格式（CV_32F）；
            // convertTo 第三参数（默认 1.0）为缩放因子，此处不缩放，
            // 保留原始像素值范围 [0.0f, 255.0f]

            float_img = (float_img - 33.405f) / 78.795f;
            // 执行 Z-score 归一化：normalized = (pixel - mean) / std；
            // 参数 mean=33.405、std=78.795 与训练时 img_norm_cfg 完全一致；
            // 归一化后像素值范围约 [-0.42, 2.81]（典型值域，无硬性截断）

            // ====== 【调试3】检查归一化后的值范围 ======
            double normMin, normMax;
            cv::minMaxLoc(float_img, &normMin, &normMax);
            cv::Scalar normMean = cv::mean(float_img);
            std::cout << "【调试3】归一化后: "
                      << "min=" << normMin
                      << " max=" << normMax
                      << " mean=" << normMean[0]
                      << std::endl;
            // 【调试3】验证归一化结果：
            //   若训练数据集统计正确，归一化后的均值应接近 0.0，
            //   标准差应接近 1.0；
            //   mean 偏离 0 说明当前输入的像素分布与训练集不同（如曝光差异）；
            //   可通过此输出判断归一化参数是否与训练配置一致

            int height = float_img.rows;
            int width = float_img.cols;
            // 获取图像高度和宽度（均应为 224）

            tensor.resize(1 * 1 * height * width);
            // 分配 tensor 空间：batch=1 × channels=1 × H=224 × W=224 = 50176 个 float；
            // NCHW 格式（Batch × Channel × Height × Width），与 ONNX 模型期望的输入维度一致

            float* data = tensor.data();
            // 获取 tensor 数据的裸指针，用于后续逐像素赋值

            for (int h = 0; h < height; ++h) {
                for (int w = 0; w < width; ++w) {
                    data[h * width + w] = float_img.at<float>(h, w);
                    // 将 HWC 格式的 cv::Mat 数据逐行逐列写入 tensor；
                    // 单通道（C=1）时 HWC 和 NCHW 行主序相同，
                    // 偏移量 h*width + w 即为正确的 NCHW 线性地址
                }
            }

            // ====== 【调试4】检查 tensor 内容 ======
            std::cout << "【调试4】tensor size=" << tensor.size()
                      << ", 前10值: ";
            for (int i = 0; i < std::min(10, (int)tensor.size()); ++i) {
                std::cout << tensor[i] << " ";
            }
            std::cout << std::endl;
            // 【调试4】打印 tensor 的前 10 个归一化值，
            //   验证数据是否正确填充（期望约 -0.4 ~ 2.8 范围内的浮点数）；
            //   若全为 0.0 说明 gray 图像为空或 float 转换出错；
            //   std::min(10, ...) 防止 tensor 长度小于 10 时越界

        } else if (!input.input_tensors.empty()) {
            tensor = input.input_tensors[0];
            // 直接使用预计算好的浮点 tensor（如已经过 CWT 时频图生成的振动数据）；
            // 振动监测项目中 DataProcessingFramework::compute_cwt_image()
            // 生成的灰度图像素已在 DataProcessingFramework 层完成归一化，
            // 此处直接复用无需再次处理
        } else {
            return ErrorCode::INVALID_PARAMETER;
            // ModelInput 既无图像也无 tensor，参数无效
        }

        std::vector<float> raw_output;
        auto status = onnx_model_->infer(tensor, raw_output);
        // 调用 ONNXModel::infer() 执行 ONNX Runtime 前向传播：
        //   将 tensor 封装为 Ort::Value → session_.Run() → 提取输出为浮点向量

        if (status != ErrorCode::SUCCESS) {
            std::cerr << "【调试】onnx_model_->infer 返回错误!" << std::endl;
            return status;
            // 推理失败时打印调试信息并向上层传播错误码
        }

        status = postprocess_output(raw_output, output);
        // 调用后处理方法：对 ONNX 输出执行 argmax，
        // 将最大值对应的类别 ID 和置信度填入 output 结构体
        std::cout << "【调试】postprocess_output 完成，class_name=" << output.classification.class_name
          << ", confidence=" << output.classification.confidence << std::endl;
        // 打印最终分类结果，验证后处理是否正确执行
        if (status != ErrorCode::SUCCESS) return status;

        auto end = std::chrono::high_resolution_clock::now();
        output.inference_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        // 计算端到端推理耗时（微秒），写入 output.inference_time_us；
        // 此耗时涵盖：图像预处理 + ONNX 推理 + 后处理的全部时间

        total_inferences_++;
        // 累计推理次数，供性能统计接口 get_inference_time() 使用

        inference_times_.push_back(output.inference_time_us);
        // 将本次推理耗时追加到历史记录列表

        if (inference_times_.size() > 100) inference_times_.erase(inference_times_.begin());
        // 维护历史耗时列表长度不超过 100 条（滑动窗口）；
        // erase(begin()) 删除最旧的记录（O(N) 操作，数量固定为 100 时可接受）；
        // 若需要高频调用可改用 std::deque 以 O(1) 删除队首

        float sum = 0;
        for (auto t : inference_times_) sum += t;
        avg_inference_time_ms_ = sum / inference_times_.size() / 1000.0f;
        // 计算最近 100 次推理的算术平均耗时（毫秒）；
        // sum 为微秒总和，除以条目数得到平均微秒值，再除以 1000 转换为毫秒

        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "infer exception: " << e.what() << std::endl;
        // 捕获推理过程中的所有异常（如 OpenCV 内存错误、ONNX Runtime 内部异常等）
        return ErrorCode::MODEL_INFERENCE_FAILED;
    }
}

ErrorCode AIModelFramework::infer_batch(const std::vector<ModelInput>& inputs, std::vector<ModelOutput>& outputs) {
    // 批量推理实现：对输入列表逐个调用单次 infer()；
    // 当前为简单循环实现，在 CPU 推理场景下与真正的批量推理性能相当

    outputs.resize(inputs.size());
    // 预分配输出列表空间，避免循环中 push_back 的多次内存重分配

    for (size_t i = 0; i < inputs.size(); ++i) {
        auto status = infer(inputs[i], outputs[i]);
        // 逐个执行单次推理，复用 infer() 中的完整预处理和后处理逻辑
        if (status != ErrorCode::SUCCESS) return status;
        // 任一样本推理失败则立即返回，不继续处理后续样本（快速失败策略）
    }
    return ErrorCode::SUCCESS;
}

ErrorCode AIModelFramework::optimize_model() {
    return ErrorCode::NOT_IMPLEMENTED;
    // 桩实现：图优化功能尚未实现（ONNX Runtime 的 Graph Optimization 在 load 时已配置）
}

ErrorCode AIModelFramework::quantize_model(bool use_int8) {
    (void)use_int8;
    // (void) 消除未使用参数的编译器警告；参数值未被使用
    return ErrorCode::NOT_IMPLEMENTED;
    // 桩实现：模型量化功能尚未实现（需 Python PyTorch 后训练量化工具链）
}

ErrorCode AIModelFramework::prune_model(float sparsity) {
    (void)sparsity;
    // (void) 消除未使用参数的编译器警告
    return ErrorCode::NOT_IMPLEMENTED;
    // 桩实现：模型剪枝功能尚未实现（需 TFMOT 或 PyTorch 剪枝 API）
}

ErrorCode AIModelFramework::fine_tune(const std::vector<ModelInput>& inputs,
                                     const std::vector<ModelOutput>& targets) {
    (void)inputs;
    (void)targets;
    // 两个参数均标记为未使用，消除编译器警告
    return ErrorCode::NOT_IMPLEMENTED;
    // 桩实现：在线微调功能尚未实现（需 Python 训练框架支持）
}

ErrorCode AIModelFramework::preprocess_input(const RawData& raw_data, ModelInput& model_input) {
    // 通用预处理接口：根据编译期激活的项目宏，
    // 将 RawData 中的原始数据复制到 ModelInput 的对应字段

#ifdef BEARING_VIBRATION_PROJECT
    if (!raw_data.sensor_data.empty()) {
        model_input.input_tensors.push_back(raw_data.sensor_data);
        // 将 RawData 中的传感器数据（已完成 CWT 处理的 tensor）
        // 推入 ModelInput::input_tensors；
        // infer() 中通过 input.input_tensors[0] 直接取用
    }
#endif
    return ErrorCode::SUCCESS;
}

ErrorCode AIModelFramework::postprocess_output(const std::vector<float>& raw_output, ModelOutput& model_output) {
    // 通用后处理方法：对 ONNX 推理输出执行 argmax，映射到类别名称
    if (raw_output.empty()) return ErrorCode::INVALID_PARAMETER;
    // 防御性检查：空输出向量无法执行 argmax

    // ====== 【调试6】postprocess 收到的原始数据 ======
    std::cout << "【调试6】raw_output (共" << raw_output.size() << "个): ";
    for (size_t i = 0; i < raw_output.size(); ++i) {
        std::cout << raw_output[i] << " ";
    }
    std::cout << std::endl;
    // 【调试6】打印 ONNX 模型的全部原始输出值：
    //   火焰检测（2 分类）：期望两个 softmax 概率值之和约为 1.0，
    //     如 [0.82, 0.18]（normal 概率高）或 [0.12, 0.88]（fire 概率高）；
    //   振动监测（4 分类）：4 个 softmax 概率值之和约为 1.0；
    //   若所有值相等（如 [0.25, 0.25, 0.25, 0.25]）说明模型未正常区分类别，
    //   可能是输入归一化参数与训练不一致

#ifdef BEARING_VIBRATION_PROJECT
    // 轴承振动项目：4 分类
    static const std::vector<std::string> class_names = {
        "denormal", "deballfault", "decagefault", "deouterrace"
        // 与 bearing_vibration_config.hpp 中修复后的 classes 列表（4 个英文类别名）顺序一致：
        //   class_id=0: denormal（正常）
        //   class_id=1: deballfault（滚动体故障）
        //   class_id=2: decagefault（保持架故障）
        //   class_id=3: deouterrace（外圈故障）
    };
#else
    static const std::vector<std::string> class_names = {};
    // 未定义任何项目宏时类别列表为空（后续 max_idx 查找会返回 "Unknown_N"）
#endif

    int max_idx = 0;
    float max_val = raw_output[0];
    // argmax 初始化：假设第 0 个元素最大，以此为基准遍历

    for (size_t i = 1; i < raw_output.size(); ++i) {
        if (raw_output[i] > max_val) {
            max_val = raw_output[i];
            max_idx = i;
            // 找到更大的值时更新最大值索引和值
        }
    }
    // 遍历完成后 max_idx 为最大 logit/概率所在的类别索引

    model_output.classification.class_id = max_idx;
    // 写入预测类别 ID（0~N-1）

    model_output.classification.confidence = max_val;
    // 写入最大类别的置信度（若模型输出已经过 softmax，此值为概率；
    // 若为 logits，此值为原始未归一化得分，调用方须注意区分）

    if (max_idx >= 0 && max_idx < (int)class_names.size()) {
        model_output.classification.class_name = class_names[max_idx];
        // 合法索引：通过 class_names 查表得到类别名称字符串
    } else {
        model_output.classification.class_name = "Unknown_" + std::to_string(max_idx);
        // 索引越界（class_names 为空或 max_idx 超出范围）时使用占位字符串；
        // 这种情况说明模型输出维度与 class_names 定义不匹配，需排查
    }

    model_output.model_confidence = max_val;
    // 冗余赋值：model_confidence 与 classification.confidence 相同，
    // 供上层代码直接访问顶层字段而无需通过嵌套结构体

    return ErrorCode::SUCCESS;
}

ErrorCode AIModelFramework::save_model(const std::string& path) {
    (void)path;
    return ErrorCode::NOT_IMPLEMENTED;
    // 桩实现：模型序列化保存功能尚未实现
}

#ifdef USE_TENSORRT
ErrorCode AIModelFramework::export_to_tensorrt(const std::string& output_path) {
    (void)output_path;
    return ErrorCode::NOT_IMPLEMENTED;
    // 桩实现：TensorRT Engine 导出功能尚未实现（树莓派无 NVIDIA GPU，不适用）
}
#endif
// 结束 USE_TENSORRT 条件编译块

ErrorCode AIModelFramework::update_model_parameters(const std::vector<float>& new_params) {
    (void)new_params;
    return ErrorCode::NOT_IMPLEMENTED;
    // 桩实现：在线参数更新功能尚未实现
}

bool AIModelFramework::is_model_loaded() const {
    return model_loaded_;
    // 返回原子布尔标志，线程安全读取（std::atomic<bool> 的 load 操作）
}

std::string AIModelFramework::get_model_info() const {
    return "ONNX Model";
    // 简化实现：返回固定字符串；
    // 完整实现应返回模型路径、输入输出形状、版本号等元数据
}

float AIModelFramework::get_inference_time() const {
    return avg_inference_time_ms_;
    // 返回最近 100 次推理的平均耗时（毫秒），由 infer() 后更新
}

size_t AIModelFramework::get_model_size() const {
    return 0;
    // 桩实现：返回 0；完整实现应读取 .onnx 文件大小或 Session 占用内存量
}

// ============================================================================
// 轴承振动项目特有函数实现
// ============================================================================

#ifdef BEARING_VIBRATION_PROJECT
ErrorCode AIModelFramework::preprocess_vibration_data(const std::vector<float>& data, std::vector<float>& tensor) {
    tensor = data;
    // 直接复制（浅拷贝触发 std::vector 的拷贝赋值）；
    // 振动数据的 CWT 时频图生成和归一化已在 DataProcessingFramework::compute_cwt_image()
    // 中完成，此处无需再次处理，直接将已处理好的 tensor 传入推理层
    return ErrorCode::SUCCESS;
}

ErrorCode AIModelFramework::postprocess_vibration_output(const std::vector<float>& output, ModelOutput& result) {
    return postprocess_output(output, result);
    // 振动监测后处理直接复用通用 postprocess_output()；
    // 通用方法中的 BEARING_VIBRATION_PROJECT 分支已定义 4 个类别名称
}

ErrorCode AIModelFramework::analyze_vibration_pattern(const std::vector<float>& vibration_data,
                                                       std::string& fault_type,
                                                       float& severity) {
    (void)vibration_data;
    fault_type = "Unknown";
    // 返回"未知"类型作为桩占位符
    severity = 0.0f;
    // 返回零严重度作为桩占位符
    return ErrorCode::NOT_IMPLEMENTED;
    // 桩实现：振动模式分析功能尚未实现（完整实现需时序分析子模块）
}

ErrorCode AIModelFramework::predict_remaining_life(const std::vector<std::vector<float>>& historical_data,
                                                    float& remaining_life_hours) {
    (void)historical_data;
    remaining_life_hours = 1000.0f;
    // 返回 1000 小时（约 42 天）作为桩占位符，
    // 避免上层报警逻辑因 RUL=0 而误触发紧急报警
    return ErrorCode::NOT_IMPLEMENTED;
    // 桩实现：RUL 预测功能尚未实现（完整实现需时序退化模型）
}
#endif
// 结束 BEARING_VIBRATION_PROJECT 条件编译块

// ============================================================================
// ONNXModel 实现
// ============================================================================

AIModelFramework::ONNXModel::ONNXModel() : env_(ORT_LOGGING_LEVEL_WARNING, "MultiProject") {}
// 构造函数：通过成员初始化列表构造 Ort::Env 实例；
// 参数 1：ORT_LOGGING_LEVEL_WARNING — 仅输出警告及以上级别的 ONNX Runtime 日志，
//           过滤掉 INFO/VERBOSE 级别的冗余调试输出（在嵌入式设备上减少终端噪声）
// 参数 2："MultiProject" — 日志标签字符串，显示在 ORT 日志前缀中便于区分来源；
// Ort::Env 管理 ONNX Runtime 的全局线程池和全局内存分配器，
// 每个进程通常只创建一个（此处的 static 实例在类成员中满足此约束）

ErrorCode AIModelFramework::ONNXModel::load(const std::string& model_path) {
    // 从指定路径加载 .onnx 模型文件并创建 ONNX Runtime 推理会话

    try {
        Ort::SessionOptions session_options;
        // 创建 Session 选项对象，用于配置推理引擎行为

        // 基础配置：ORT 线程池设 1，避免与 XNNPACK 冲突
        session_options.SetIntraOpNumThreads(1);
        // 设置 ORT 默认 CPU 执行提供者的算子内部并行线程数为 1；
        // 与 XNNPACK 执行提供者共存时，ORT 主线程池线程数应设为 1
        // 以避免线程竞争（XNNPACK 自带独立的 4 线程线程池）

        session_options.AddConfigEntry("session.intra_op.allow_spinning", "0");
        // 禁用 ORT 工作线程的自旋等待（Spin-Wait）策略；
        // "1"（默认）：线程在等待任务时忙等待（占用 100% CPU）；
        // "0"：线程在等待时进入阻塞（让出 CPU），减少不必要的 CPU 占用；
        // 在树莓派四核平台上，禁用自旋可降低推理时的 CPU 热量和功耗

        // EXTENDED 及以上会将 Softmax+QL+DQ 融合为在树莓派4B上有 bug 的算子
        // ENABLE_BASIC 只做常量折叠/冗余消除，不触发该融合，输出正常
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
        // 设置图优化级别为 ORT_ENABLE_BASIC（仅基础优化）：
        //   ORT_DISABLE_ALL：不优化（用于调试）
        //   ORT_ENABLE_BASIC：常量折叠、冗余节点消除（安全）
        //   ORT_ENABLE_EXTENDED：算子融合（在树莓派 4B 上对量化模型有 bug）
        //   ORT_ENABLE_ALL：最高优化（含硬件特定优化，风险更高）
        // 原有注释说明了选择 BASIC 的关键原因：
        //   EXTENDED 优化会将 Softmax+QuantizeLinear+DequantizeLinear 融合为
        //   树莓派 4B ONNX Runtime 版本中存在 bug 的融合算子，
        //   导致分类输出概率异常（全相等或全为 0/1），必须限制为 BASIC 级别

        // XNNPACK 用自己的 4 线程
        try {
            std::unordered_map<std::string, std::string> xnnpack_opts = {
                {"intra_op_num_threads", "4"}
                // XNNPACK 执行提供者配置：使用 4 个内部线程（对应树莓派 4B 的四核）；
                // XNNPACK 在 ARM 平台上对 float32 卷积/全连接有硬件加速优化，
                // 可显著提升推理速度（比纯 ORT CPU 快 1.5~3×）
            };
            session_options.AppendExecutionProvider("XNNPACK", xnnpack_opts);
            // 将 XNNPACK 添加为优先执行提供者（EP）；
            // 模型中 XNNPACK 支持的算子（Conv/Gemm/Softmax 等）由 XNNPACK 执行，
            // 不支持的算子回退到 ORT CPU 执行

            std::cout << "[INFO] XNNPACK execution provider enabled" << std::endl;
            // 打印 XNNPACK 启用成功的信息，便于运行时确认加速路径

        } catch (const Ort::Exception& e) {
            // XNNPACK 不可用时恢复多线程 CPU
            session_options.SetIntraOpNumThreads(4);
            // XNNPACK 注册失败（如库未安装或版本不兼容）时，
            // 将 ORT CPU 执行提供者的线程数从 1 恢复为 4，
            // 以利用 CPU 多线程并行加速（退化方案）

            std::cerr << "[WARN] XNNPACK unavailable, using CPU: " << e.what() << std::endl;
            // 打印 XNNPACK 不可用的警告及具体原因
        }

        // 加载模型
        session_ = Ort::Session(env_, model_path.c_str(), session_options);
        // 从 model_path 加载 .onnx 文件并创建 ONNX Runtime 推理会话；
        // model_path.c_str() 将 std::string 转换为 C 字符串（Ort::Session 需要 const char*）；
        // 此操作较耗时（可能需要数秒），包括：模型文件读取、图解析、图优化和算子注册；
        // 若 model_path 文件不存在或格式错误，抛出 Ort::Exception

        // 解析输入节点
        Ort::AllocatorWithDefaultOptions allocator;
        // 创建 ONNX Runtime 默认内存分配器，用于为节点名称字符串分配内存

        size_t num_input_nodes = session_.GetInputCount();
        // 查询模型的输入节点数量（VariableMobileNet 通常为 1 个输入节点）

        input_names_str_.resize(num_input_nodes);
        input_names_.resize(num_input_nodes);
        // 预分配字符串存储容器和 C 字符串指针数组

        for (size_t i = 0; i < num_input_nodes; ++i) {
            auto input_name = session_.GetInputNameAllocated(i, allocator);
            // 获取第 i 个输入节点的名称，返回 OrtAllocated<char> 智能指针；
            // 内部由 ORT 分配器分配内存，input_name 析构时自动释放

            input_names_str_[i] = input_name.get();
            // 将 ORT 分配的 C 字符串复制到 std::string（触发字符串拷贝）；
            // 关键步骤：确保节点名称有永久存储，input_name 超出作用域后字符串仍然有效

            input_names_[i] = input_names_str_[i].c_str();
            // 将 std::string 的 C 字符串指针存入 input_names_ 数组；
            // 指针指向 input_names_str_[i] 的内部缓冲区，生命周期与 std::string 一致；
            // session_.Run() 需要 const char** 格式的输入节点名称数组
        }

        // 解析输出节点
        size_t num_output_nodes = session_.GetOutputCount();
        // 查询模型的输出节点数量（分类模型通常为 1 个输出节点）

        output_names_str_.resize(num_output_nodes);
        output_names_.resize(num_output_nodes);
        // 预分配输出节点名称的存储容器和指针数组

        for (size_t i = 0; i < num_output_nodes; ++i) {
            auto output_name = session_.GetOutputNameAllocated(i, allocator);
            // 获取第 i 个输出节点的名称
            output_names_str_[i] = output_name.get();
            // 复制到 std::string 永久存储
            output_names_[i] = output_names_str_[i].c_str();
            // 存储 C 字符串指针供 session_.Run() 使用
        }

        // 调试：打印模型输入输出信息
        std::cout << "========== 模型加载成功: " << model_path << " ==========" << std::endl;
        std::cout << "  输入节点数: " << num_input_nodes << std::endl;
        for (size_t i = 0; i < num_input_nodes; ++i) {
            auto type_info = session_.GetInputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            auto shape = tensor_info.GetShape();
            // 获取第 i 个输入节点的张量类型和形状信息
            std::cout << "  输入[" << i << "]: name=\"" << input_names_str_[i]
                      << "\", shape=[";
            for (size_t s = 0; s < shape.size(); ++s) {
                if (s > 0) std::cout << ",";
                std::cout << shape[s];
                // 打印各维度大小；动态维度（如 batch size）显示为 -1
            }
            std::cout << "], type=" << tensor_info.GetElementType() << std::endl;
            // GetElementType() 返回 ONNX 数据类型枚举值：
            //   1=float32, 2=uint8, 3=int8, 7=int64 等
        }
        std::cout << "  输出节点数: " << num_output_nodes << std::endl;
        for (size_t i = 0; i < num_output_nodes; ++i) {
            auto type_info = session_.GetOutputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            auto shape = tensor_info.GetShape();
            std::cout << "  输出[" << i << "]: name=\"" << output_names_str_[i]
                      << "\", shape=[";
            for (size_t s = 0; s < shape.size(); ++s) {
                if (s > 0) std::cout << ",";
                std::cout << shape[s];
            }
            std::cout << "], type=" << tensor_info.GetElementType() << std::endl;
        }
        std::cout << "================================================" << std::endl;
        // 分隔线，标识模型信息打印结束

        loaded_ = true;
        // 设置加载状态标志，允许后续 infer() 调用

        return ErrorCode::SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "ONNXModel load exception: " << e.what() << std::endl;
        // 捕获模型加载中的所有异常（文件不存在、格式错误、内存不足等）
        return ErrorCode::MODEL_LOAD_FAILED;
    }
}

ErrorCode AIModelFramework::ONNXModel::unload() {
    session_ = Ort::Session(nullptr);
    // 将 Ort::Session 替换为空 Session（构造 Ort::Session(nullptr) 表示无模型状态）；
    // 原有 session_ 对象被析构，释放 ONNX Runtime 持有的模型权重内存（通常数十 MB）

    loaded_ = false;
    // 重置加载状态标志，防止后续 infer() 使用空 Session

    return ErrorCode::SUCCESS;
}


ErrorCode AIModelFramework::ONNXModel::infer(const std::vector<float>& input, std::vector<float>& output) {
    // 执行 ONNX Runtime 单次前向推理，支持 float32/uint8/int8 三种输入类型

    if (!loaded_) return ErrorCode::MODEL_NOT_LOADED;
    // 前置检查：Session 未加载时提前返回

    try {
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        // 创建 CPU 内存信息描述符：
        //   OrtArenaAllocator：使用 ORT 的竞技场分配器（Arena Allocator），
        //     减少频繁 malloc/free 的开销（块级内存管理）
        //   OrtMemTypeDefault：默认 CPU 内存类型（非固定内存/非 GPU 内存）

        std::vector<int64_t> input_shape = {1, 1, 224, 224};
        // 固定输入 tensor 形状：batch=1，channel=1（灰度），H=224，W=224；
        // int64_t 类型是 ONNX Runtime 要求的形状维度类型

        // 获取模型期望的输入类型
        auto input_type_info = session_.GetInputTypeInfo(0);
        auto tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        auto input_type = tensor_info.GetElementType();
        // 动态查询模型第 0 个输入节点的元素数据类型（float32/uint8/int8）；
        // 不同量化级别的模型期望不同的输入类型，必须在运行时检测后构造对应的 Ort::Value

        std::cout << "【调试】模型输入类型: " << input_type << std::endl;
        // 打印输入类型枚举值（1=float32, 2=uint8, 3=int8），
        // 便于确认模型是否为量化版本

        Ort::Value input_tensor{nullptr};
        // 声明输入 tensor 对象，初始为空（null Value）；
        // 后续根据 input_type 分支赋值

        if (input_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
            // float32 模型：直接用原始数据
            input_tensor = Ort::Value::CreateTensor<float>(
                memory_info, const_cast<float*>(input.data()), input.size(),
                input_shape.data(), input_shape.size());
            // 从现有内存创建 float32 输入 tensor（零拷贝）；
            // const_cast<float*>：ORT API 需要非 const 指针，
            //   实际上不会修改数据，但 API 设计如此（安全强制转换）；
            // CreateTensor 不复制数据，tensor 持有对 input.data() 的引用，
            // 须确保 input 在 session_.Run() 期间保持有效

        } else if (input_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8) {
            // uint8 量化模型：把归一化后的 float 转回 uint8 (0~255)
            std::vector<uint8_t> uint8_input(input.size());
            for (size_t i = 0; i < input.size(); ++i) {
                // 反归一化: val * std + mean -> 原始像素值
                float pixel = input[i] * 78.795f + 33.405f;
                // 执行 Z-score 归一化的逆变换：pixel = normalized × std + mean；
                // 将已归一化的 float 值反推回 [0, 255] 范围的原始像素值

                pixel = std::max(0.0f, std::min(255.0f, pixel));
                // 裁剪到 [0, 255] 合法范围，防止浮点误差导致越界

                uint8_input[i] = static_cast<uint8_t>(pixel + 0.5f);
                // 四舍五入（+0.5 后截断）转换为 uint8；
                // 量化模型期望原始未归一化的 uint8 像素值作为输入
            }
            input_tensor = Ort::Value::CreateTensor<uint8_t>(
                memory_info, uint8_input.data(), uint8_input.size(),
                input_shape.data(), input_shape.size());
            // 从 uint8 数组创建输入 tensor（零拷贝，uint8_input 须在 Run 期间有效）

        } else if (input_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8) {
            // int8 量化模型
            std::vector<int8_t> int8_input(input.size());
            for (size_t i = 0; i < input.size(); ++i) {
                float pixel = input[i] * 78.795f + 33.405f;
                // 同上，反归一化到原始像素值范围

                float scaled = pixel - 128.0f;
                // int8 量化的零点偏移：将 [0, 255] 映射到 [-128, 127]；
                // 等价于 int8 = uint8 - 128（有符号/无符号转换）

                scaled = std::max(-128.0f, std::min(127.0f, scaled));
                // 裁剪到 int8 合法范围 [-128, 127]

                int8_input[i] = static_cast<int8_t>(scaled);
                // 截断转换为 int8（不四舍五入，与量化训练时的取整方式一致）
            }
            input_tensor = Ort::Value::CreateTensor<int8_t>(
                memory_info, int8_input.data(), int8_input.size(),
                input_shape.data(), input_shape.size());
            // 从 int8 数组创建输入 tensor

        } else {
            std::cerr << "不支持的输入类型: " << input_type << std::endl;
            return ErrorCode::MODEL_INFERENCE_FAILED;
            // 遇到未知输入类型时打印错误并返回失败；
            // 常见原因：模型使用了 float16 或其他非标准量化类型
        }

        // 推理
        auto output_tensors = session_.Run(Ort::RunOptions{nullptr},
                                           input_names_.data(), &input_tensor, 1,
                                           output_names_.data(), output_names_.size());
        // 执行 ONNX Runtime 前向传播：
        //   Ort::RunOptions{nullptr}：使用默认运行选项（无超时设置）
        //   input_names_.data()：输入节点名称数组指针（const char**）
        //   &input_tensor：输入 tensor 值数组（单个输入，取地址即为 Ort::Value* 数组）
        //   1：输入 tensor 数量
        //   output_names_.data()：输出节点名称数组指针
        //   output_names_.size()：输出 tensor 数量（通常为 1）
        // 返回 std::vector<Ort::Value>，每个元素对应一个输出 tensor

        // 获取输出类型，自动转 float
        auto out_type_info = output_tensors[0].GetTensorTypeAndShapeInfo();
        auto output_type = out_type_info.GetElementType();
        size_t output_size = out_type_info.GetElementCount();
        // 动态查询输出 tensor 的元素类型和总元素数量

        std::cout << "【调试5】输出类型: " << output_type
                  << ", 元素数: " << output_size << std::endl;
        // 【调试5】打印输出 tensor 的类型和大小：
        //   float32 分类模型：output_type=1（FLOAT），output_size=2（火焰）或 4（振动）；
        //   量化模型：output_type=2（UINT8）或 3（INT8）

        if (output_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
            float* floatarr = output_tensors[0].GetTensorMutableData<float>();
            output.assign(floatarr, floatarr + output_size);
            // 直接将输出 float 数组复制到 output vector；
            // assign(first, last) 以指针范围赋值，等效于 memcpy

        } else if (output_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8) {
            uint8_t* data = output_tensors[0].GetTensorMutableData<uint8_t>();
            output.resize(output_size);
            // 将 uint8 输出转为 float 概率 (除以 255)
            for (size_t i = 0; i < output_size; ++i) {
                output[i] = static_cast<float>(data[i]) / 255.0f;
                // 将 [0, 255] 的 uint8 量化输出线性映射到 [0.0, 1.0] 概率范围；
                // 此映射假设量化尺度为 1/255，可能与实际量化参数不完全一致
            }

        } else if (output_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8) {
            int8_t* data = output_tensors[0].GetTensorMutableData<int8_t>();
            output.resize(output_size);
            for (size_t i = 0; i < output_size; ++i) {
                output[i] = (static_cast<float>(data[i]) + 128.0f) / 255.0f;
                // 将 [-128, 127] 的 int8 量化输出偏移并线性映射到 [0.0, 1.0]；
                // +128 将 int8 范围平移到 [0, 255]，再除以 255 归一化到概率范围
            }
        } else {
            std::cerr << "不支持的输出类型: " << output_type << std::endl;
            return ErrorCode::MODEL_INFERENCE_FAILED;
        }

        std::cout << "【调试5】模型输出: ";
        for (size_t i = 0; i < output_size; ++i) {
            std::cout << output[i] << " ";
        }
        std::cout << std::endl;
        // 打印转换后的 float 输出值，便于验证 softmax 概率分布是否合理

        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "ONNXModel infer exception: " << e.what() << std::endl;
        return ErrorCode::MODEL_INFERENCE_FAILED;
    }
}

bool AIModelFramework::ONNXModel::is_loaded() const { return loaded_; }
// 返回内部加载状态标志 loaded_，const 修饰确保不修改对象状态

std::vector<int64_t> AIModelFramework::ONNXModel::get_input_shape() const {
    return {};
    // 桩实现：返回空 vector；
    // 完整实现应调用 session_.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape()
}

std::vector<int64_t> AIModelFramework::ONNXModel::get_output_shape() const {
    return {};
    // 桩实现：返回空 vector；同上
}

AIModelFramework::ONNXModel::~ONNXModel() = default;
// 使用编译器默认析构函数；
// Ort::Session 析构函数会释放 ONNX Runtime 模型权重内存；
// Ort::Env 析构函数会清理全局线程池资源

#ifdef USE_TENSORRT
AIModelFramework::TensorRTModel::TensorRTModel() {}
// 默认构造函数，无需初始化（engine_/context_ 均初始化为 nullptr）
AIModelFramework::TensorRTModel::~TensorRTModel() {}
// 默认析构函数；完整实现应调用 engine_->destroy() 和 context_->destroy()
ErrorCode AIModelFramework::TensorRTModel::build_from_onnx(const std::string&) { return ErrorCode::NOT_IMPLEMENTED; }
// 桩实现：从 ONNX 构建 TensorRT Engine（树莓派无 NVIDIA GPU，不适用）
ErrorCode AIModelFramework::TensorRTModel::load_engine(const std::string&) { return ErrorCode::NOT_IMPLEMENTED; }
// 桩实现：加载序列化的 TensorRT Engine 文件
ErrorCode AIModelFramework::TensorRTModel::infer(const std::vector<float>&, std::vector<float>&) { return ErrorCode::NOT_IMPLEMENTED; }
// 桩实现：TensorRT GPU 推理（不适用于树莓派）
bool AIModelFramework::TensorRTModel::is_ready() const { return false; }
// 返回 false 表示 TensorRT 未就绪（桩状态）
size_t AIModelFramework::TensorRTModel::get_workspace_size() const { return 0; }
// 桩实现：返回 0 字节工作区大小
#endif
// 结束 USE_TENSORRT 条件编译块

AIModelFramework::MultiTaskModel::MultiTaskModel() {}
// 默认构造函数，无实际初始化（完整实现须创建 SharedEncoder 和 TaskHead 实例）
AIModelFramework::MultiTaskModel::~MultiTaskModel() = default;
// 默认析构函数，unique_ptr<TaskHead> 自动释放各任务头实例
ErrorCode AIModelFramework::MultiTaskModel::initialize(const std::string&) { return ErrorCode::NOT_IMPLEMENTED; }
// 桩实现：多任务模型初始化
ErrorCode AIModelFramework::MultiTaskModel::infer(const ModelInput&, TaskOutput&) { return ErrorCode::NOT_IMPLEMENTED; }
// 桩实现：多任务推理（共享编码器 + 多任务头）
ErrorCode AIModelFramework::MultiTaskModel::update_task_weights(const std::vector<float>&) { return ErrorCode::NOT_IMPLEMENTED; }
// 桩实现：更新任务权重向量

AIModelFramework::InferenceCache::InferenceCache(size_t capacity) : capacity_(capacity) {}
// 构造函数：通过成员初始化列表设置缓存容量上限 capacity_；
// cache_ 向量初始为空，无需额外初始化

AIModelFramework::InferenceCache::~InferenceCache() {}
// 析构函数：cache_ 向量自动析构，释放所有 CacheEntry 的内存

bool AIModelFramework::InferenceCache::get(const std::vector<float>& input_hash, std::vector<float>& output) {
    // 线程安全缓存查询：在 cache_ 中线性搜索与 input_hash 匹配的条目

    std::lock_guard<std::mutex> lock(mutex_);
    // 加锁保护 cache_ 的并发读取

    for (const auto& entry : cache_) {
        // 遍历所有缓存条目（线性搜索，容量固定为 100 时 O(100) 可接受）

        if (entry.input_hash.size() == input_hash.size()) {
            // 首先比较长度，长度不同的 hash 向量直接跳过

            bool match = true;
            for (size_t i = 0; i < input_hash.size(); ++i) {
                if (std::abs(entry.input_hash[i] - input_hash[i]) > 1e-6) {
                    match = false;
                    break;
                    // 浮点数比对使用绝对差值容差 1e-6（约 0.000001）；
                    // 纯整数像素值经归一化后差异通常 > 1e-4，
                    // 1e-6 的容差可过滤浮点误差同时避免误匹配
                }
            }
            if (match) {
                output = entry.output;
                // 命中缓存：将存储的输出向量赋值给 output
                return true;
            }
        }
    }
    return false;
    // 未命中缓存，调用方须执行实际推理
}

void AIModelFramework::InferenceCache::put(const std::vector<float>& input_hash, const std::vector<float>& output) {
    // 线程安全缓存写入：添加新条目，必要时驱逐最旧的条目（FIFO 策略）

    std::lock_guard<std::mutex> lock(mutex_);
    // 加锁保护 cache_ 的并发写入

    if (cache_.size() >= capacity_) {
        cache_.erase(cache_.begin());
        // 缓存已满时删除最旧的条目（cache_ 首部，即最先插入的条目）；
        // 这是 FIFO（先入先出）驱逐策略，非严格 LRU（最近最少使用）；
        // erase(begin()) 在 vector 上是 O(N) 操作（需移动后续所有元素），
        // 但容量固定为 100 时性能可接受；改用 std::deque 可降至 O(1)
    }

    CacheEntry entry;
    // 创建新缓存条目

    entry.input_hash = input_hash;
    // 存储输入哈希向量（触发 std::vector<float> 的拷贝构造）

    entry.output = output;
    // 存储推理输出向量（同上，拷贝存储）

    entry.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    // 记录当前 Unix 时间戳（毫秒），用于 LRU 排序或超时清理；
    // 当前 FIFO 驱逐策略未使用此字段，保留供后续升级为真正 LRU 策略时使用

    cache_.push_back(entry);
    // 将新条目追加到缓存末尾（O(1) 均摊时间，vector 尾部插入）
}

void AIModelFramework::InferenceCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    // 加锁保护清空操作
    cache_.clear();
    // 清空所有缓存条目，释放 vector 内部存储的 CacheEntry 对象；
    // 通常在模型热替换、配置热更新或参数变更后调用，
    // 防止旧参数生成的缓存污染新模型的推理结果
}

} // namespace MultiProject
// 关闭命名空间 MultiProject