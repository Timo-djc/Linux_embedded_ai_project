/**
  ******************************************************************************
  * @file    AIModelFramework.hpp
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
 * @brief   AI 推理模型框架 — 多后端推理引擎与多任务模型管理头文件
 * ----------------------------------------------------------------------------
 * @details 本文件定义了 MultiProject 框架的 AI 推理层核心类 AIModelFramework，
 *          继承自 IAIModel 接口，负责统一管理火焰检测和轴承振动监测两个子项目
 *          的 AI 推理全生命周期（加载、推理、优化、量化、剪枝、卸载）。
 *
 *          类内按功能层次组织以下私有嵌套类（推理后端）：
 *
 *          1. ONNXModel          — 基于 ONNX Runtime 的 CPU 推理后端（常驻启用）
 *             封装 Ort::Session 的创建、输入输出名称管理及前向推理调用
 *
 *          2. TensorRTModel      — 基于 NVIDIA TensorRT 的 GPU 推理后端
 *             （条件编译，仅在定义 USE_TENSORRT 时启用）
 *             封装 ICudaEngine 构建与 IExecutionContext 推理
 *
 *          3. PythonModel        — 基于 CPython 嵌入的 Python 模型后端
 *             （条件编译，仅在定义 USE_PYTHON_INTEGRATION 时启用）
 *             支持调用 PyTorch/TensorFlow Python 代码进行推理和在线训练
 *
 *          4. MultiTaskModel     — 共享编码器 + 多任务头（分类/回归/检测/异常）
 *             的多任务推理后端，支持单次前向传播同时完成多个推理任务
 *
 *          5. InferenceCache     — LRU 风格的推理结果缓存
 *             注意：bearing_vibration_config 已将推理缓存修复为关闭（enable_cache=false）
 *
 *          对外公开接口按职能分为四组：
 *            模型生命周期管理：load_model / unload_model / save_model
 *            推理接口：infer / infer_batch
 *            模型优化：optimize_model / quantize_model / prune_model / fine_tune
 *            项目特化接口（条件编译）：
 *              FIRE_DETECTION_PROJECT    → detect_flames / estimate_flame_intensity
 *              BEARING_VIBRATION_PROJECT → analyze_vibration_pattern / predict_remaining_life
 *            状态查询：is_model_loaded / get_model_info / get_inference_time / get_model_size
 *
 * @note    本文件使用 #pragma once 防止重复包含。
 *          ONNX Runtime C++ API（onnxruntime_cxx_api.h）须在 CMakeLists.txt 中
 *          通过 find_package(onnxruntime) 或手动指定 include/lib 路径引入。
 *          TensorRT 和 Python 相关代码通过 USE_TENSORRT / USE_PYTHON_INTEGRATION
 *          宏进行条件编译，在树莓派 CPU 部署场景下两者均不定义。
 * ============================================================================
 */

#pragma once
// 替代传统 #ifndef 头文件保护的现代写法，
// 保证本文件在同一编译单元中只被展开一次

#include "../common/UnifiedDataTypes.hpp"
// 引入统一数据类型定义头文件，提供 ModelInput、ModelOutput、RawData、
// IAIModel 接口及其他跨框架共享的数据结构定义；
// 路径 "../common/" 表示相对本文件向上一级再进入 common 子目录

#include "../common/ProjectDefines.hpp"
// 引入全局预处理宏与公共头文件聚合入口，提供：
//   FIRE_DETECTION_PROJECT / BEARING_VIBRATION_PROJECT 项目选择宏
//   以及通过 FrameworkBase.hpp 传递的 MultiProject 命名空间公共类型
//   （ErrorCode、ProjectType 等）

#include <onnxruntime_cxx_api.h>
// 引入 ONNX Runtime C++ API 主头文件；
// 提供 Ort::Env（运行时环境）、Ort::Session（推理会话）、
// Ort::AllocatorWithDefaultOptions（默认分配器）、
// Ort::Value（张量值）等核心类型；
// 编译时须在 include 路径中包含 ONNX Runtime 的 include 目录

#include <onnxruntime_session_options_config_keys.h>
// 引入 ONNX Runtime Session Options 配置键常量；
// 提供如 kOrtSessionOptionsConfigDisableMemArena（禁用内存池）等字符串常量键，
// 供 Ort::SessionOptions 的 AddConfigEntry() 方法调用以精细控制推理行为

#ifdef USE_TENSORRT
// 条件编译块：仅在编译命令中定义了 USE_TENSORRT 宏时才引入 TensorRT 相关头文件；
// 树莓派 CPU 推理场景下不定义此宏，以避免引入 CUDA 依赖
#include <NvInfer.h>
// 引入 NVIDIA TensorRT 核心推理库头文件；
// 提供 nvinfer1::ICudaEngine（CUDA 推理引擎）、
// nvinfer1::IExecutionContext（推理上下文）、
// nvinfer1::ILogger（日志接口）等核心类型

#include <NvOnnxParser.h>
// 引入 TensorRT ONNX 解析器头文件；
// 提供 nvonnxparser::IParser，用于将 .onnx 模型文件解析并转换为
// TensorRT 的 INetworkDefinition，再构建优化后的 ICudaEngine
#endif
// 结束 USE_TENSORRT 条件编译块

#include <opencv2/dnn/dnn.hpp>
// 引入 OpenCV DNN 模块头文件；
// 提供 cv::dnn::Net（深度神经网络推理）等接口；
// 本文件中 cv::Mat 用于火焰检测的图像输入（detect_flames / preprocess_fire_image），
// OpenCV DNN 可作为 ONNX Runtime 的备用推理路径

#include <memory>
// 引入智能指针：std::unique_ptr、std::shared_ptr；
// 用于管理 ONNXModel、TensorRTModel、PythonModel、
// MultiTaskModel、InferenceCache 等嵌套类实例的所有权

#include <vector>
// 引入 std::vector，用于存储推理输入/输出张量数据、
// 输入输出名称列表、推理时间统计、缓存条目等动态数组

#include <string>
// 引入 std::string，用于模型路径、模型类型、故障类型、模型信息等文本字段

#include <atomic>
// 引入 std::atomic，用于 model_loaded_ 成员（std::atomic<bool>）；
// 保证多线程环境下加载状态的读写操作无数据竞争

#include <mutex>
// 引入 std::mutex，用于 InferenceCache 内部的并发访问保护；
// InferenceCache::mutex_ 声明为 mutable，允许在 const 成员函数 get() 中加锁

#include <unordered_map>
// 引入 std::unordered_map，提供 O(1) 平均查找的哈希映射容器；
// 可用于模型参数索引或后续扩展的模型注册表

#ifdef USE_PYTHON_INTEGRATION
// 条件编译块：仅在定义了 USE_PYTHON_INTEGRATION 宏时引入 CPython 嵌入头文件；
// 树莓派 CPU 推理场景下通常不定义此宏
#include <Python.h>
// 引入 CPython C API 主头文件；
// 提供 PyObject*（Python 对象句柄）、Py_Initialize()/Py_Finalize()（解释器生命周期）、
// PyImport_ImportModule()（导入 Python 模块）等接口；
// 编译时须在 include 路径中包含 Python 开发头文件目录，并链接 -lpython3.x
#endif
// 结束 USE_PYTHON_INTEGRATION 条件编译块

namespace MultiProject {
// 开启命名空间 MultiProject，与其他框架模块共享同一命名空间

class AIModelFramework : public IAIModel {
// AIModelFramework：AI 推理模型框架的具体实现类；
// 继承 IAIModel 抽象接口（定义于 UnifiedDataTypes.hpp），
// 提供面向两个子项目（火焰检测、轴承振动监测）的统一推理管理能力；
// 采用多后端设计：根据编译宏和运行时配置自动选择
// ONNX Runtime / TensorRT / Python / MultiTask 中最合适的推理路径

public:
    AIModelFramework(ProjectType project_type);
    // 构造函数：根据项目类型（FIRE_DETECTION 或 BEARING_VIBRATION）
    // 初始化 AIModelFramework 实例；
    // 创建对应的模型配置（ModelConfig）、推理缓存（InferenceCache）
    // 及 ONNXModel/MultiTaskModel 嵌套类实例；
    // 不执行模型文件加载，模型加载由 load_model() 触发

    ~AIModelFramework();
    // 析构函数：安全释放所有推理后端资源；
    // 若 model_loaded_=true，先调用 unload_model() 确保 ONNX Session 被关闭、
    // TensorRT Engine 被销毁、Python 解释器状态被清理，
    // 再释放各 unique_ptr 成员

    ErrorCode load_model(const std::string& model_path) override;
    // 覆盖 IAIModel::load_model()：从指定路径加载 ONNX 模型文件；
    // 根据 project_type_ 分发至 load_fire_detection_model() 或
    // load_bearing_vibration_model() 私有方法；
    // 成功后将 model_loaded_ 置为 true；
    // 返回 ErrorCode::SUCCESS 或 MODEL_LOAD_FAILED / FILE_NOT_FOUND

    ErrorCode unload_model() override;
    // 覆盖 IAIModel::unload_model()：释放当前已加载的模型及推理会话；
    // 清空输入输出名称缓冲区，将 model_loaded_ 置为 false；
    // 通常在系统关机或模型热替换前调用

    ErrorCode infer(const ModelInput& input, ModelOutput& output) override;
    // 覆盖 IAIModel::infer()：对单个输入样本执行前向推理；
    // 内部流程：预处理（preprocess_input）→ ONNXModel::infer() → 后处理（postprocess_output）；
    // 每次推理更新 total_inferences_ 和 avg_inference_time_ms_；
    // 若 InferenceCache 命中则跳过推理直接返回缓存结果；
    // 须在 model_loaded_=true 时调用，否则返回 MODEL_NOT_LOADED

    ErrorCode infer_batch(const std::vector<ModelInput>& inputs,
                         std::vector<ModelOutput>& outputs) override;
    // 覆盖 IAIModel::infer_batch()：对一批输入样本执行批量推理；
    // 参数 inputs：输入样本列表（批量大小由调用方控制，不超过 ModelConfig::max_batch_size）；
    // 参数 outputs：推理结果列表，与 inputs 一一对应；
    // 在 CPU 推理场景下批量推理不能显著提升吞吐量，与循环调用 infer() 效果相近；
    // GPU/TensorRT 场景下批量推理可充分利用并行计算能力

    ErrorCode optimize_model() override;
    // 覆盖 IAIModel::optimize_model()：对已加载模型执行图级优化；
    // 包括算子融合（Operator Fusion）、常量折叠（Constant Folding）、
    // 冗余节点消除等 ONNX Runtime Graph Optimization；
    // 系统状态应在优化期间切换为 SystemState::UPDATING；
    // 成功返回 ErrorCode::SUCCESS，失败返回 MODEL_OPTIMIZATION_FAILED

    ErrorCode quantize_model(bool use_int8 = true) override;
    // 覆盖 IAIModel::quantize_model()：对模型执行后训练量化（PTQ）；
    // use_int8=true 时执行 INT8 量化（精度损失较小，推理速度提升最大）；
    // use_int8=false 时执行 FP16 量化（精度损失极小，速度提升适中）；
    // 树莓派 CPU 仅支持 FP32/INT8，FP16 加速需 ARM Cortex-A 特定指令集支持；
    // 内部调用 quantize_with_pytorch() 私有方法；
    // 失败返回 MODEL_QUANTIZATION_FAILED

    ErrorCode prune_model(float sparsity = 0.5f) override;
    // 覆盖 IAIModel::prune_model()：对模型执行权重剪枝（Pruning）；
    // sparsity：目标稀疏率（0.0~1.0），默认 0.5f 表示剪除 50% 的权重；
    // 剪枝后需重新微调（fine_tune）以恢复精度损失；
    // 内部调用 prune_with_tfmot() 私有方法（TF Model Optimization Toolkit）；
    // 失败返回 MODEL_PRUNE_FAILED

    ErrorCode fine_tune(const std::vector<ModelInput>& inputs,
                       const std::vector<ModelOutput>& targets) override;
    // 覆盖 IAIModel::fine_tune()：使用新的标注数据对模型进行在线微调；
    // inputs：微调输入样本列表；targets：对应的标注目标输出列表；
    // 通常在剪枝或量化后调用以恢复精度，或在部署后收集新数据时进行领域适应；
    // 在 USE_PYTHON_INTEGRATION 编译条件下可通过 PythonModel::train() 执行


#ifdef BEARING_VIBRATION_PROJECT
    // 以下方法仅在编译 BEARING_VIBRATION_PROJECT 时生效

    ErrorCode analyze_vibration_pattern(const std::vector<float>& vibration_data,
                                       std::string& fault_type,
                                       float& severity);
    // 轴承振动监测项目专用接口：分析振动信号并识别故障类型；
    // 参数 vibration_data：单通道（X/Y/Z 轴之一）的振动加速度时间序列（单位 g）；
    // 参数 fault_type：输出识别到的故障类型字符串（如 "denormal"/"deballfault" 等）；
    // 参数 severity：输出故障严重程度归一化值（范围 0.0~1.0，0 表示正常）；
    // 内部流程：preprocess_vibration_data() → infer() → postprocess_vibration_output()

    ErrorCode predict_remaining_life(const std::vector<std::vector<float>>& historical_data,
                                    float& remaining_life_hours);
    // 轴承振动监测项目专用接口：基于历史振动数据预测轴承剩余使用寿命（RUL）；
    // 参数 historical_data：多个时间点的振动特征向量历史序列（外层为时间步，内层为特征维度）；
    // 参数 remaining_life_hours：输出预测的剩余使用寿命（小时）；
    // 与 PredictiveMaintenanceConfig::RULPrediction 中的指数退化模型互补，
    // 此处通过 AI 模型提供数据驱动的 RUL 估计
#endif
// 结束 BEARING_VIBRATION_PROJECT 条件编译块

    ErrorCode preprocess_input(const RawData& raw_data, ModelInput& model_input);
    // 通用预处理接口：将原始采集数据（RawData）转换为模型输入张量（ModelInput）；
    // 根据 project_type_ 自动分发至对应的预处理私有方法：
    //   FIRE_DETECTION    → preprocess_fire_image()（灰度化 + resize + 归一化）
    //   BEARING_VIBRATION → preprocess_vibration_data()（CWT 变换 + resize + 归一化）

    ErrorCode postprocess_output(const std::vector<float>& raw_output, ModelOutput& model_output);
    // 通用后处理接口：将模型原始输出浮点向量转换为结构化 ModelOutput；
    // 根据 project_type_ 自动分发：
    //   FIRE_DETECTION    → postprocess_fire_output()（softmax + 阈值过滤 + 边界框解码）
    //   BEARING_VIBRATION → postprocess_vibration_output()（argmax + 置信度计算 + 健康指数）

    ErrorCode save_model(const std::string& path);
    // 将当前（可能已量化/剪枝/微调）的模型序列化并保存到指定路径；
    // 用于保存优化后的模型以供下次部署直接加载，避免重复优化

#ifdef USE_TENSORRT
    ErrorCode export_to_tensorrt(const std::string& output_path);
    // TensorRT 加速条件下的模型导出接口：
    // 将已加载的 ONNX 模型通过 NvOnnxParser 解析并构建优化后的 TensorRT Engine，
    // 序列化保存到 output_path（通常为 .trt 或 .engine 文件）；
    // 导出的 Engine 绑定特定 GPU 架构，不可跨设备迁移
#endif
// 结束 USE_TENSORRT 条件编译块

    ErrorCode update_model_parameters(const std::vector<float>& new_params);
    // 在不重新加载模型文件的情况下，直接更新模型中的指定参数（如偏置、量化缩放因子）；
    // 用于在线学习（Online Learning）场景下的参数微调

    bool is_model_loaded() const;
    // 线程安全地返回模型是否已成功加载（读取 std::atomic<bool> model_loaded_）；
    // 调用方在执行推理前应先调用本方法校验状态，避免在未加载时调用 infer()

    std::string get_model_info() const;
    // 返回当前已加载模型的元信息字符串，内容包括：
    //   模型类型、输入/输出形状、参数量、量化状态、版本信息等；
    // 用于诊断报告和管理界面的模型状态展示

    float get_inference_time() const;
    // 返回最近一次推理或历史推理的平均耗时（毫秒）；
    // 对应 avg_inference_time_ms_ 成员，由每次 infer() 调用后更新；
    // 可与 PerformanceMetrics::TimeMetrics::inference_time_ms 对接

    size_t get_model_size() const;
    // 返回已加载模型的内存占用大小（字节）；
    // 用于内存使用率监控，与 PerformanceConfig::memory_usage_limit 配合
    // 判断模型是否超出内存预算

private:
// 私有区域：以下嵌套类和成员变量仅供 AIModelFramework 内部使用

    class ONNXModel {
    // 私有嵌套类：ONNX Runtime CPU 推理后端；
    // 封装 Ort::Session 的完整生命周期（创建→推理→销毁）和
    // 输入输出节点名称的安全管理
    public:
        ONNXModel();
        // 默认构造函数：初始化 Ort::Env（ONNX Runtime 运行时环境）；
        // Ort::Env 内部管理线程池和全局内存分配器，每个进程通常只创建一个

        ~ONNXModel();
        // 析构函数：确保 Ort::Session 被正确销毁，释放模型文件占用的内存

        ErrorCode load(const std::string& model_path);
        // 从指定路径加载 .onnx 模型文件并创建推理会话（Ort::Session）；
        // 加载时读取模型的输入输出节点名称，存储到 input_names_str_ 和 output_names_str_，
        // 并将原始 C 字符串指针存入 input_names_/output_names_ 供推理调用；
        // 成功后设置 loaded_=true

        ErrorCode unload();
        // 重置 Ort::Session 为 nullptr 状态，释放模型占用的内存；
        // 清空 input_names_/output_names_ 及其字符串缓冲区；
        // 设置 loaded_=false

        ErrorCode infer(const std::vector<float>& input, std::vector<float>& output);
        // 执行单次前向推理：
        //   1. 将 input 浮点向量封装为 Ort::Value（CPU 张量）
        //   2. 调用 session_.Run() 执行前向传播
        //   3. 从输出 Ort::Value 中提取数据复制到 output 向量
        // 须在 loaded_=true 时调用

        bool is_loaded() const;
        // 返回当前 ONNX 模型是否已成功加载（返回 loaded_ 成员值）

        std::vector<int64_t> get_input_shape() const;
        // 返回模型第一个输入节点的形状（如 {1, 1, 224, 224} 表示 NCHW 格式）；
        // 供预处理层校验输入张量形状是否与模型期望一致

        std::vector<int64_t> get_output_shape() const;
        // 返回模型第一个输出节点的形状（如 {1, 4} 表示 batch=1、4 个类别的 logits）；
        // 供后处理层确定输出向量的解析方式

    private:
        Ort::Env env_;
        // ONNX Runtime 全局运行时环境；
        // 参数通常为日志级别（如 ORT_LOGGING_LEVEL_WARNING）和环境名称字符串；
        // Ort::Env 的生命周期须覆盖所有 Ort::Session 的生命周期

        Ort::Session session_{nullptr};
        // ONNX Runtime 推理会话，初始化为 nullptr（未加载状态）；
        // load() 成功后持有已加载模型的推理图和权重；
        // 线程安全性：单个 Session 不支持多线程并发调用 Run()，
        // 若需并发推理应创建多个 Session 实例或使用外部互斥锁

        Ort::AllocatorWithDefaultOptions allocator_;
        // ONNX Runtime 默认内存分配器；
        // 用于调用 session_.GetInputNameAllocated()/GetOutputNameAllocated()
        // 动态分配输入输出节点名称字符串的内存

        std::vector<const char*> input_names_;
        // 模型输入节点名称的 C 字符串指针数组；
        // 指向 input_names_str_ 中各字符串的 .c_str() 指针；
        // session_.Run() 需要此格式的数组，不可持有悬空指针

        std::vector<const char*> output_names_;
        // 模型输出节点名称的 C 字符串指针数组；
        // 指向 output_names_str_ 中各字符串的 .c_str() 指针

        // 新增：存储字符串，确保指针有效
        std::vector<std::string> input_names_str_;
        // 输入节点名称的 std::string 存储容器（新增字段）；
        // 此字段是关键：input_names_ 中的 const char* 指针指向这些字符串内部缓冲区；
        // 若直接将 GetInputNameAllocated() 返回的临时字符串赋值给 const char* 则产生悬空指针；
        // 通过先存入 std::string 再调用 .c_str() 可保证指针生命周期与 session_ 一致

        std::vector<std::string> output_names_str_;
        // 输出节点名称的 std::string 存储容器（新增字段）；
        // 与 input_names_str_ 同理，确保 output_names_ 中的 C 字符串指针不悬空

        bool loaded_{false};
        // ONNX 模型加载状态标志，初始为 false；
        // load() 成功后置 true，unload() 后重置为 false
    };
    // 结束 ONNXModel 嵌套类定义

#ifdef USE_TENSORRT
    class TensorRTModel {
    // 私有嵌套类：NVIDIA TensorRT GPU 推理后端（条件编译）；
    // 封装 TensorRT Engine 的构建（从 ONNX 解析）、加载（从序列化 Engine 文件）
    // 和推理执行（通过 IExecutionContext）
    public:
        TensorRTModel();
        // 构造函数：初始化 Logger 和内部状态

        ~TensorRTModel();
        // 析构函数：销毁 IExecutionContext（context_）和 ICudaEngine（engine_），
        // 并释放 CUDA 内存缓冲区

        ErrorCode build_from_onnx(const std::string& onnx_path);
        // 从 ONNX 文件构建优化后的 TensorRT Engine：
        //   1. 调用 nvonnxparser::IParser 解析 .onnx 文件
        //   2. 通过 TensorRT Builder 执行层融合、精度校准等优化
        //   3. 构建 ICudaEngine 并序列化保存（.trt 文件）
        // 构建过程较耗时（分钟级），通常在首次部署时执行一次

        ErrorCode load_engine(const std::string& engine_path);
        // 从序列化的 TensorRT Engine 文件（.trt/.engine）加载推理引擎；
        // 比 build_from_onnx() 快得多，适合每次启动时使用

        ErrorCode infer(const std::vector<float>& input, std::vector<float>& output);
        // 使用 IExecutionContext 执行 GPU 推理：
        //   1. 将 input 数据复制到 CUDA 设备内存（cudaMemcpy Host→Device）
        //   2. 调用 context_->executeV2()（或 enqueueV2()）执行推理
        //   3. 将输出数据从 CUDA 设备内存复制回主机内存（cudaMemcpy Device→Host）

        bool is_ready() const;
        // 返回 TensorRT Engine 是否已构建/加载完毕（返回 ready_ 成员值）

        size_t get_workspace_size() const;
        // 返回 TensorRT 推理所需的工作区内存大小（字节）；
        // 用于监控 GPU 显存占用

    private:
        class Logger : public nvinfer1::ILogger {
        // 内嵌类：实现 TensorRT 的日志接口 nvinfer1::ILogger；
        // TensorRT API 要求调用方提供 ILogger 实现以接收内部日志消息
            void log(Severity severity, const char* msg) override {}
            // 覆盖 log() 方法：当前实现为空函数体，丢弃所有 TensorRT 内部日志；
            // 若需要输出 TensorRT 诊断信息，可替换为 std::cout/std::cerr 输出；
            // 空实现可降低日志 I/O 对推理延迟的影响
        } logger_;
        // logger_ 成员：TensorRT Logger 实例，在 Builder/Parser 初始化时传入

        nvinfer1::ICudaEngine* engine_{nullptr};
        // TensorRT CUDA 推理引擎指针，初始化为 nullptr；
        // 通过 build_from_onnx() 或 load_engine() 赋值；
        // 注意：ICudaEngine 须通过 engine_->destroy() 显式销毁（非 delete），
        // 析构函数中须调用此方法

        nvinfer1::IExecutionContext* context_{nullptr};
        // TensorRT 推理执行上下文指针；
        // 从 engine_->createExecutionContext() 创建；
        // 同一 Engine 可创建多个 Context 以支持并发推理（每个 Context 独立维护中间激活内存）

        bool ready_{false};
        // TensorRT Engine 就绪状态标志，初始为 false；
        // build_from_onnx() 或 load_engine() 成功后置 true
    };
#endif
// 结束 USE_TENSORRT 条件编译块

#ifdef USE_PYTHON_INTEGRATION
    class PythonModel {
    // 私有嵌套类：CPython 嵌入式推理后端（条件编译）；
    // 允许在 C++ 应用中嵌入 Python 解释器并调用 PyTorch/TensorFlow 模型，
    // 适用于模型结构较复杂、难以直接转换为 ONNX 的场景
    public:
        PythonModel();
        // 构造函数：初始化内部 Python 对象句柄为 nullptr

        ~PythonModel();
        // 析构函数：调用 Py_XDECREF 减少 py_model_/py_module_ 的引用计数，
        // 防止 Python 对象内存泄漏；
        // 注意：Py_Finalize() 通常在程序退出时由上层调用，而非在此析构

        ErrorCode initialize();
        // 初始化 CPython 解释器（Py_Initialize）并导入必要的 Python 模块；
        // 须在调用 load_model() 之前执行；
        // 初始化成功后设置 initialized_=true

        ErrorCode load_model(const std::string& model_path);
        // 通过 Python C API 加载指定路径的 Python 模型文件；
        // 内部执行等价于 Python 代码 "model = load_model(model_path)"；
        // 将 Python 模型对象句柄存储在 py_model_ 中

        ErrorCode infer(const std::vector<float>& input, std::vector<float>& output);
        // 通过 Python C API 调用 Python 模型的 predict() 方法执行推理；
        // 将 C++ std::vector<float> 转换为 Python numpy.ndarray，
        // 调用后将结果转换回 std::vector<float>；
        // CPython 全局解释器锁（GIL）须在调用前后正确管理

        ErrorCode train(const std::vector<std::vector<float>>& inputs,
                       const std::vector<std::vector<float>>& targets);
        // 通过 Python C API 调用 Python 模型的 train() 方法执行在线训练；
        // 参数 inputs：训练样本特征矩阵（外层为样本数，内层为特征维度）；
        // 参数 targets：对应的训练目标输出矩阵；
        // 适用于 fine_tune() 在 USE_PYTHON_INTEGRATION 编译条件下的实现

    private:
        PyObject* py_module_{nullptr};
        // Python 模块对象句柄（对应 import 语句加载的模块）；
        // 初始化为 nullptr；initialize() 成功后指向有效的 PyObject

        PyObject* py_model_{nullptr};
        // Python 模型对象句柄（对应 Python 中的模型实例）；
        // load_model() 成功后指向有效的 PyObject；
        // 析构时须调用 Py_XDECREF 减少引用计数

        bool initialized_{false};
        // CPython 解释器初始化状态标志；
        // initialize() 成功后置 true，防止重复初始化
    };
#endif
// 结束 USE_PYTHON_INTEGRATION 条件编译块

    class MultiTaskModel {
    // 私有嵌套类：多任务推理后端；
    // 采用"共享编码器 + 多任务头"架构（Shared Encoder + Multi-Task Heads）：
    //   SharedEncoder 将输入映射为公共特征向量；
    //   各 TaskHead 从公共特征出发独立完成各自任务的预测；
    // 相比多个独立模型，共享编码器大幅减少重复的特征提取计算量
    public:
        struct TaskOutput {
        // 多任务输出聚合结构体：存储各任务头的推理结果
            std::vector<float> classification_logits;
            // 分类任务 logits 向量（softmax 前的原始输出）；
            // argmax 后得到预测类别 ID

            std::vector<float> regression_values;
            // 回归任务输出值向量，如 RUL 预测的天数估计

            std::vector<float> detection_boxes;
            // 目标检测任务的边界框坐标序列（格式：[x1,y1,x2,y2,score, ...]）

            std::vector<float> anomaly_scores;
            // 异常检测任务的异常得分向量；值越高表示越异常

            std::vector<float> task_weights;
            // 各任务头的动态权重向量（归一化，总和为 1.0）；
            // 高权重任务的输出对最终决策影响更大；
            // 可通过 update_task_weights() 在运行时动态调整
        };

        MultiTaskModel();
        // 构造函数：初始化共享编码器和任务头列表；
        // 任务头实例通过 unique_ptr 管理，在 initialize() 中按配置动态创建

        ~MultiTaskModel();
        // 析构函数：通过 unique_ptr 的析构自动释放所有 TaskHead 实例

        ErrorCode initialize(const std::string& config_path);
        // 从配置文件路径加载多任务模型的拓扑配置；
        // 根据配置创建对应的 TaskHead 子类实例（ClassificationHead、
        // RegressionHead、DetectionHead、AnomalyHead）并追加到 task_heads_；
        // 同时初始化 task_weights_ 为均匀分布（1/n）

        ErrorCode infer(const ModelInput& input, TaskOutput& output);
        // 执行多任务前向推理：
        //   1. encoder_.encode(input) → 公共特征向量 features
        //   2. 对 task_heads_ 中每个 TaskHead 调用 forward(features, output.xxx)
        // 所有任务头共享同一组特征，计算效率高于独立推理

        ErrorCode update_task_weights(const std::vector<float>& new_weights);
        // 更新各任务头的动态权重向量；
        // new_weights 须与 task_heads_.size() 等长且各元素非负；
        // 归一化处理（各权重除以总和）在方法内部执行

    private:
        class SharedEncoder {
        // 共享编码器嵌套类：将任意格式的模型输入（ModelInput）
        // 映射为固定维度的公共特征向量，供各任务头使用
        public:
            ErrorCode encode(const ModelInput& input, std::vector<float>& features);
            // 执行编码前向传播：将 ModelInput 通过多层神经网络变换为特征向量；
            // features 的维度由 encoder_layers_ 的最后一层输出维度决定

        private:
            std::vector<std::vector<float>> encoder_layers_;
            // 编码器各层的权重矩阵（简化表示，实际应为 Eigen 矩阵或张量）；
            // 每个元素对应一层的权重展平向量
        };
        // 结束 SharedEncoder 嵌套类定义

        class TaskHead {
        // 任务头抽象基类：定义各具体任务头（分类/回归/检测/异常）的统一接口
        public:
            virtual ErrorCode forward(const std::vector<float>& features, std::vector<float>& output) = 0;
            // 纯虚方法：从共享特征向量计算本任务头的输出；
            // 各子类实现特定任务的预测逻辑（如分类头应用 softmax，回归头直接输出）

            virtual ~TaskHead() = default;
            // 虚析构函数：确保通过基类指针（unique_ptr<TaskHead>）
            // 销毁子类对象时子类析构函数被正确调用
        };
        // 结束 TaskHead 抽象基类定义

        class ClassificationHead : public TaskHead {
        // 分类任务头：从特征向量计算各类别的 logits；
        // 对应火焰检测（2 分类）和轴承故障诊断（4 分类）的输出层
            ErrorCode forward(const std::vector<float>& features, std::vector<float>& output) override;
            // 计算分类 logits，存入 output；
            // 后处理层（postprocess_output）负责应用 softmax 和阈值过滤
        };

        class RegressionHead : public TaskHead {
        // 回归任务头：从特征向量输出连续数值，如 RUL 预测天数
            ErrorCode forward(const std::vector<float>& features, std::vector<float>& output) override;
            // 计算回归输出，结果经反归一化后对应物理量（天数、毫米/秒等）
        };

        class DetectionHead : public TaskHead {
        // 目标检测任务头：从特征向量解码边界框坐标和目标置信度；
        // 对应火焰区域定位任务
            ErrorCode forward(const std::vector<float>& features, std::vector<float>& output) override;
            // 输出格式：[x1, y1, x2, y2, confidence, class_id, ...] 序列
        };

        class AnomalyHead : public TaskHead {
        // 异常检测任务头：从特征向量计算重构误差或异常得分；
        // 可基于自编码器（Autoencoder）或单类分类（One-Class Classification）
            ErrorCode forward(const std::vector<float>& features, std::vector<float>& output) override;
            // 输出异常得分向量；得分越高表示输入样本越偏离训练分布
        };

        SharedEncoder encoder_;
        // 共享编码器实例，所有任务头共用此编码器的特征提取结果

        std::vector<std::unique_ptr<TaskHead>> task_heads_;
        // 任务头指针列表，使用 unique_ptr 确保自动内存管理；
        // 通过多态（虚函数 forward()）统一调用不同类型的任务头；
        // initialize() 根据配置动态创建并追加各类型任务头实例

        std::vector<float> task_weights_;
        // 各任务头的动态权重向量，与 task_heads_ 等长；
        // 初始为均匀分布（1/n），可通过 update_task_weights() 调整
    };
    // 结束 MultiTaskModel 嵌套类定义

    ProjectType project_type_;
    // 当前框架实例的项目类型（FIRE_DETECTION 或 BEARING_VIBRATION）；
    // 由构造函数赋值，用于在 load_model()、preprocess_input()、
    // postprocess_output() 等方法中分发至对应的项目特化逻辑

    std::atomic<bool> model_loaded_{false};
    // 模型加载状态原子布尔标志，初始为 false；
    // std::atomic<bool> 保证多线程读写无数据竞争：
    //   写操作：load_model()（成功后置 true）/ unload_model()（置 false）
    //   读操作：is_model_loaded() / infer() 的前置检查

    std::unique_ptr<ONNXModel> onnx_model_;
    // ONNX Runtime 推理后端实例，由构造函数创建；
    // 常驻启用（不受条件编译控制），是树莓派 CPU 推理场景的主要推理路径

#ifdef USE_TENSORRT
    std::unique_ptr<TensorRTModel> trt_model_;
    // TensorRT GPU 推理后端实例（条件编译）；
    // 在 NVIDIA 平台（如 Jetson）部署时可显著提升推理吞吐量
#endif
// 结束 USE_TENSORRT 条件编译块

#ifdef USE_PYTHON_INTEGRATION
    std::unique_ptr<PythonModel> python_model_;
    // CPython 嵌入式推理后端实例（条件编译）；
    // 用于调用无法直接转换为 ONNX 的 PyTorch 自定义算子模型
#endif
// 结束 USE_PYTHON_INTEGRATION 条件编译块

    std::unique_ptr<MultiTaskModel> multi_task_model_;
    // 多任务推理后端实例，常驻创建；
    // 在 onnx_model_ 完成基础推理后，multi_task_model_ 可进一步执行
    // 回归/检测/异常等扩展任务头的推理

    struct ModelConfig {
    // 模型配置内嵌结构体：存储从配置文件读取的推理相关超参数；
    // 与项目级 AIModelConfig（bearing_vibration_config.hpp 和
    // fire_detection_config.hpp 中的同名结构体）对应，
    // 本处为运行时内存中的工作副本
        std::string model_type;
        // 模型任务类型字符串，如 "classification"（分类）

        std::vector<int> input_shape;
        // 模型输入张量形状，如 {1, 1, 224, 224}（NCHW 格式）

        std::vector<int> output_shape;
        // 模型输出张量形状，如 {1, 4}（4 分类 logits）

        float mean[3];
        // 输入图像各通道的归一化均值；灰度模型仅使用 mean[0]=33.405f；
        // 数组长度固定为 3 以兼容 RGB 三通道模型的未来扩展

        float std[3];
        // 输入图像各通道的归一化标准差；灰度模型仅使用 std[0]=78.795f

        bool use_fp16;
        // 为 true 时使用 FP16 半精度推理；
        // 树莓派 CPU 不支持 FP16 硬件加速，此字段通常为 false

        int max_batch_size;
        // 单次推理允许的最大批量大小；CPU 推理场景通常为 1


#ifdef BEARING_VIBRATION_PROJECT
        struct VibrationConfig {
        // 轴承振动监测项目专用配置内嵌结构体（条件编译）
            float normal_vibration_limit;
            // 正常振动加速度上限（g），超过则触发告警

            float warning_vibration_limit;
            // 警告振动加速度阈值（g）

            float critical_vibration_limit;
            // 危险振动加速度阈值（g），超过须立即停机

            std::vector<std::string> fault_types;
            // 故障类型名称列表，与模型 argmax 输出索引对应：
            // {"denormal", "deballfault", "decagefault", "deouterrace"}

            bool use_frequency_analysis;
            // 为 true 时在预处理阶段执行 CWT 时频分析生成图像输入；
            // 为 false 时直接使用时域特征向量（跳过 CWT 计算）
        } vibration_config;
#endif
// 结束 BEARING_VIBRATION_PROJECT 条件编译块

    } config_;
    // config_ 成员变量：运行时模型配置实例，由 load_model() 从配置文件解析填充

    uint64_t total_inferences_{0};
    // 自系统启动以来的累计推理次数，由每次 infer() 调用递增；
    // 用于性能统计和推理频率监控

    float avg_inference_time_ms_{0.0f};
    // 历史推理的指数移动平均耗时（毫秒），由每次 infer() 调用后更新；
    // 公式：avg = alpha * last_time + (1-alpha) * avg（alpha 为平滑系数）

    std::vector<uint64_t> inference_times_;
    // 历史推理耗时记录列表（微秒），用于统计方差、最大值和分位数；
    // 列表长度须有上限（如最近 1000 次），防止内存无限增长

    class InferenceCache {
    // 私有嵌套类：推理结果 LRU 缓存；
    // 对相同输入的重复推理请求直接返回缓存结果，节省 CPU 时间；
    // 重要提示：bearing_vibration_config 中已将 enable_cache 修复为 false，
    // 因为缓存首次错误结果将被永久保留，难以排查；
    // 本类设计保留以备 TensorRT 等低延迟场景使用
    public:
        InferenceCache(size_t capacity);
        // 构造函数：初始化缓存容量上限 capacity_；
        // 当缓存条目数超过 capacity_ 时驱逐最旧的条目（LRU 策略）

        ~InferenceCache();
        // 析构函数：清空 cache_ 向量，释放所有缓存的输入哈希和输出数据

        bool get(const std::vector<float>& input_hash, std::vector<float>& output);
        // 查询缓存：若 input_hash 命中缓存则将对应输出写入 output 并返回 true；
        // 未命中则返回 false；
        // 此方法内部加锁（mutex_），支持多线程并发调用

        void put(const std::vector<float>& input_hash, const std::vector<float>& output);
        // 写入缓存：将 (input_hash, output) 键值对存入缓存，并更新时间戳；
        // 若缓存已满则删除最旧条目（timestamp 最小的 CacheEntry）；
        // 内部加锁保证线程安全

        void clear();
        // 清空全部缓存条目；
        // 在模型热替换或参数更新后应调用本方法，防止旧模型结果污染新模型的推理

    private:
        struct CacheEntry {
        // 缓存条目结构体：存储单次推理的输入哈希、输出结果和时间戳
            std::vector<float> input_hash;
            // 输入张量的哈希向量（可为输入数据的降采样或特征摘要），
            // 用于快速比对是否为相同输入

            std::vector<float> output;
            // 对应的模型推理输出向量（softmax 前的 logits 或最终概率）

            uint64_t timestamp;
            // 条目创建或最近访问的时间戳（Unix 纳秒时间）；
            // 用于 LRU 驱逐策略的排序依据
        };

        std::vector<CacheEntry> cache_;
        // 缓存条目线性存储容器；
        // 使用 vector 而非 unordered_map，适合容量较小（≤100）的缓存场景

        size_t capacity_;
        // 缓存最大条目数上限，由构造函数参数传入

        mutable std::mutex mutex_;
        // 可变互斥锁（mutable 修饰允许在 const 成员函数 get() 中加锁）；
        // 保护 cache_ 的并发读写操作
    };
    // 结束 InferenceCache 嵌套类定义

    std::unique_ptr<InferenceCache> inference_cache_;
    // 推理结果缓存实例；
    // 由构造函数创建，容量由 ModelConfig::max_batch_size 等参数决定；
    // 注意：在轴承振动监测项目中应通过 enable_cache=false 禁用其使用，
    // 防止首次错误推理结果被永久缓存

    ErrorCode load_fire_detection_model(const std::string& model_path);
    // 私有方法：火焰检测项目专用的模型加载流程；
    // 从 model_path 加载 fire_detection_model.onnx，
    // 读取并填充 config_.fire_config（火焰阈值、类别名称等）；
    // 由 load_model() 在 project_type_==FIRE_DETECTION 时调用

    ErrorCode load_bearing_vibration_model(const std::string& model_path);
    // 私有方法：轴承振动监测项目专用的模型加载流程；
    // 从 model_path 加载 bearing_vibration_model.onnx，
    // 读取并填充 config_.vibration_config（振动阈值、故障类型列表等）；
    // 由 load_model() 在 project_type_==BEARING_VIBRATION 时调用

    ErrorCode preprocess_fire_image(const cv::Mat& image, std::vector<float>& tensor);
    // 私有方法：火焰检测专用图像预处理流程；
    // 执行步骤：
    //   1. cv::cvtColor(BGR→GRAY)（若 config_.fire_config.use_color_features=false）
    //   2. cv::resize(640×480 → 224×224)
    //   3. Z-score 归一化：(pixel - mean[0]) / std[0] = (pixel - 33.405) / 78.795
    //   4. HWC → NCHW 张量格式转换，存入 tensor

    ErrorCode preprocess_vibration_data(const std::vector<float>& data, std::vector<float>& tensor);
    // 私有方法：振动数据专用预处理流程；
    // 执行步骤：
    //   1. 对时域振动信号执行连续小波变换（CWT）生成时频图
    //   2. 将时频图 resize 到 224×224 灰度图
    //   3. Z-score 归一化：(pixel - 33.405) / 78.795
    //   4. 转换为 NCHW 张量格式存入 tensor

    ErrorCode postprocess_fire_output(const std::vector<float>& output, ModelOutput& result);
    // 私有方法：火焰检测专用后处理流程；
    // 执行步骤：
    //   1. softmax(output) 计算各类别概率
    //   2. 取 class_id=1（fire）的概率与 config_.fire_config.flame_threshold 比较
    //   3. 若超过阈值，填充 result 的类别、置信度和边界框字段
    //   4. 更新 InferenceCache（若 enable_cache=true）

    ErrorCode postprocess_vibration_output(const std::vector<float>& output, ModelOutput& result);
    // 私有方法：振动数据专用后处理流程；
    // 执行步骤：
    //   1. softmax(output) 计算 4 类故障概率
    //   2. argmax 确定预测类别（denormal/deballfault/decagefault/deouterrace）
    //   3. 从 config_.vibration_config.fault_types 映射类别 ID 到类别名称
    //   4. 填充 result 的故障类型、置信度和健康指数字段

#ifdef USE_TENSORRT
    ErrorCode optimize_with_tensorrt();
    // 私有方法：调用 TensorRT 执行模型图级优化；
    // 将当前 ONNX 模型转换为 TensorRT Engine（通过 TensorRTModel::build_from_onnx()）；
    // 由 optimize_model() 在 USE_TENSORRT 编译条件下调用
#endif
// 结束 USE_TENSORRT 条件编译块

    ErrorCode quantize_with_pytorch();
    // 私有方法：调用 PyTorch 后训练量化（Post-Training Quantization, PTQ）；
    // 在 USE_PYTHON_INTEGRATION 编译条件下通过 PythonModel::train() 接口
    // 调用 Python 侧的 torch.quantization API；
    // 由 quantize_model() 调用

    ErrorCode prune_with_tfmot();
    // 私有方法：调用 TensorFlow Model Optimization Toolkit（TFMOT）执行权重剪枝；
    // 在 USE_PYTHON_INTEGRATION 编译条件下通过 PythonModel 接口
    // 调用 Python 侧的 tensorflow_model_optimization.sparsity API；
    // 由 prune_model() 调用
};
// 结束 AIModelFramework 类定义

} // namespace MultiProject
// 关闭命名空间 MultiProject