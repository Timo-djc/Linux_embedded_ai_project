/**
  ******************************************************************************
  * @file    DataAcquisitionFramework.hpp
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
 * @brief   数据采集框架 — 多项目统一采集入口与硬件驱动层头文件
 * ----------------------------------------------------------------------------
 * @details 本文件定义了 MultiProject 框架数据采集层的全部类，
 *          覆盖从上层统一接口到底层硬件驱动的完整采集链路。
 *
 *          文件内容按以下层次组织：
 *
 *          ── 前向声明 ──
 *            FireDetectionAcquisition / BearingVibrationAcquisition
 *            解耦主框架类与具体采集类之间的头文件依赖
 *
 *          ── DataAcquisitionFramework（主框架类） ──
 *            继承 IDataAcquisition 接口，根据编译期激活的项目宏
 *            将采集请求分发至 FireDetectionAcquisition 或
 *            BearingVibrationAcquisition；
 *            支持真实硬件采集和模拟模式（Simulation Mode）两种运行路径；
 *            模拟模式下从本地文件系统读取预录数据，无需实物硬件即可调试
 *
 *          ── FireDetectionAcquisition（仅 FIRE_DETECTION_PROJECT） ──
 *            负责三路 USB 摄像头的并发帧采集；
 *            每路摄像头运行独立工作线程，通过 ConcurrentQueue<FrameData>
 *            与主线程解耦；支持自动识别 USB 摄像头设备路径
 *
 *          ── BearingVibrationAcquisition（仅 BEARING_VIBRATION_PROJECT） ──
 *            负责通过 SPI 总线驱动 AD4134 ADC 芯片采集三轴加速度数据；
 *            内嵌 AD4134Device 驱动类，封装寄存器读写、GPIO 初始化
 *            和三通道数据读取的全部 SPI 底层操作
 *
 * @note    本文件使用 #pragma once 防止重复包含。
 *          DataAcquisitionFramework 禁用拷贝构造和拷贝赋值（= delete），
 *          防止意外复制导致硬件资源双重管理。
 *          FireDetectionAcquisition 和 BearingVibrationAcquisition 类
 *          通过条件编译宏隔离，编译时仅生成与激活项目对应的代码。
 * ============================================================================
 */

#pragma once
// 替代传统 #ifndef 头文件保护的现代写法，保证本文件在同一编译单元中只被展开一次

#include "../common/UnifiedDataTypes.hpp"
// 引入统一数据类型头文件，提供 IDataAcquisition 接口、RawData、
// SystemStatus::HardwareStatus、ModelInput/ModelOutput 等跨框架共享数据结构；
// IDataAcquisition 是 DataAcquisitionFramework 的虚基类

#include "../common/ProjectDefines.hpp"
// 引入全局预处理宏入口，提供 FIRE_DETECTION_PROJECT / BEARING_VIBRATION_PROJECT
// 项目选择宏、SPI_CHANNEL / SPI_SPEED / CAMERA_RESOLUTION_WIDTH 等硬件参数宏，
// 以及通过 FrameworkBase.hpp 传递的 MultiProject 命名空间公共类型

#include "../common/ConcurrentQueue.hpp"
// 引入线程安全并发队列模板，用于 FireDetectionAcquisition 中
// 摄像头工作线程与主线程之间的帧数据传递（FrameQueue = ConcurrentQueue<FrameData>）

#include <vector>
// 引入 std::vector，用于存储摄像头图像列表、振动数据通道、
// 工作线程列表、设备路径列表及模拟数据集等动态数组

#include <memory>
// 引入 std::unique_ptr / std::shared_ptr，用于管理
// FireDetectionAcquisition、BearingVibrationAcquisition、
// AD4134Device、SimulationState 等子模块实例的所有权

#include <atomic>
// 引入 std::atomic，用于 initialized_、streaming_、simulation_mode_、
// running_、cameras_opened_ 等跨线程访问标志的无锁读写

#include <mutex>
// 引入 std::mutex，用于 status_mutex_（保护状态数据的并发访问）
// 和 InferenceCache::mutex_（缓存的线程安全）

#include <thread>
// 引入 std::thread，用于 FireDetectionAcquisition 中
// 每路摄像头独立工作线程（workers_ 列表）的创建和管理

#include <condition_variable>
// 引入 std::condition_variable，用于工作线程与主线程之间的
// 条件通知（配合 ConcurrentQueue 中的 cond_.wait() 实现）

#include <fstream>
// 引入 std::ifstream / std::ofstream，用于模拟模式下读取本地文件
// （如振动数据 CSV 文件、图像文件路径列表等）

#include <filesystem>
// 引入 std::filesystem（C++17），用于模拟模式中遍历数据目录
// （如 datas/FireDetection/fire_images/{left,center,right}/），
// 自动发现并枚举图像文件路径

#include <string>
// 引入 std::string，用于模型路径、设备路径、错误信息等文本字段

#include <array>
// 引入 std::array，用于 SimulationState::all_data 中以
// std::array<float, 3> 表示单个三轴振动样本（X/Y/Z 加速度值）

#include <opencv2/opencv.hpp>
// 引入 OpenCV 完整功能头文件；
// 提供 cv::Mat（图像矩阵）、cv::VideoCapture（摄像头采集）、
// cv::resize()、cv::cvtColor() 等图像处理接口；
// 编译时须链接 OpenCV 相关库（-lopencv_core -lopencv_videoio 等）

namespace MultiProject {
// 开启命名空间 MultiProject

// 前向声明
// 以下两个类在 DataAcquisitionFramework 的成员声明中仅用于 unique_ptr，
// 无需完整类型定义，使用前向声明可避免循环头文件包含并加快编译速度

#ifdef BEARING_VIBRATION_PROJECT
class BearingVibrationAcquisition;
// 前向声明轴承振动采集类；完整定义位于本文件后续的 #ifdef BEARING_VIBRATION_PROJECT 区域；
// DataAcquisitionFramework::vibration_acquisition_（unique_ptr）依赖本前向声明
#endif

class DataAcquisitionFramework : public IDataAcquisition {
// DataAcquisitionFramework：数据采集框架的统一入口类；
// 继承 IDataAcquisition 抽象接口（定义于 UnifiedDataTypes.hpp），
// 根据编译期激活的项目宏将采集请求分发至对应的硬件采集子类；
// 同时提供模拟模式，无需实物硬件即可从本地文件读取预录数据进行调试

public:
    DataAcquisitionFramework();
    // 构造函数：从 ProjectDefines.hpp 中的 CURRENT_PROJECT 宏推断 project_type_；
    // 创建对应的采集子类实例（fire_acquisition_ 或 vibration_acquisition_）；
    // 初始化 AcquisitionConfig 默认参数，不执行实际硬件初始化

    ~DataAcquisitionFramework();
    // 析构函数：若 streaming_=true 则先调用 stop_streaming() 安全停止工作线程；
    // 通过 unique_ptr 自动销毁采集子类实例，释放 SPI 文件描述符和摄像头句柄

    DataAcquisitionFramework(const DataAcquisitionFramework&) = delete;
    // 禁用拷贝构造函数：DataAcquisitionFramework 持有硬件资源（摄像头/SPI 设备文件）
    // 和线程资源（workers_），不允许浅拷贝；= delete 使编译器在拷贝尝试时报错

    DataAcquisitionFramework& operator=(const DataAcquisitionFramework&) = delete;
    // 禁用拷贝赋值运算符：与拷贝构造函数同理，防止资源双重所有权

    ErrorCode initialize() override;
    // 覆盖 IDataAcquisition::initialize()：执行硬件初始化；
    // 根据 project_type_ 分发至 initialize_fire_detection() 或
    // initialize_bearing_vibration() 私有方法；
    // 初始化成功后设置 initialized_=true；
    // 模拟模式下（simulation_mode_=true）跳过硬件初始化，改为调用 openSimulationSource()

    ErrorCode acquire_data(RawData& data) override;
    // 覆盖 IDataAcquisition::acquire_data()：从硬件或模拟数据源获取一帧/一批原始数据；
    // 真实模式：从 FireDetectionAcquisition 的帧队列弹出或从 AD4134 读取振动数据；
    // 模拟模式：调用 readNextSimulationData(data)，从预录数据文件中读取下一批样本；
    // 须在 initialized_=true 时调用，否则返回 NOT_INITIALIZED

    bool is_ready() const override;
    // 覆盖 IDataAcquisition::is_ready()：返回采集框架是否处于可采集状态；
    // 条件：initialized_=true 且（真实模式下硬件已就绪 / 模拟模式下文件已打开）；
    // const 修饰确保本方法不修改对象状态

    ErrorCode configure(const std::string& config_str) override;
    // 覆盖 IDataAcquisition::configure()：从 JSON 字符串解析并更新 AcquisitionConfig；
    // 支持运行时热更新采样率、分辨率、曝光等参数（无需重新初始化）；
    // 解析失败返回 CONFIG_PARSE_ERROR

    SystemStatus::HardwareStatus diagnose() const override;
    // 覆盖 IDataAcquisition::diagnose()：执行硬件自检并返回状态报告；
    // 火焰检测：检查各摄像头设备文件是否可访问、帧率是否满足要求；
    // 振动监测：检查 SPI 通信响应、AD4134 寄存器读回值是否正常；
    // const 修饰确保诊断过程不修改框架状态


#ifdef BEARING_VIBRATION_PROJECT
    // 以下方法仅在 BEARING_VIBRATION_PROJECT 编译条件下生效

    ErrorCode read_vibration_data(std::vector<float>& x_data,
                                 std::vector<float>& y_data,
                                 std::vector<float>& z_data);
    // 轴承振动监测专用接口：从 AD4134 读取三轴振动加速度数据；
    // 参数 x_data/y_data/z_data：各轴输出的采样序列（单位 g，长度由采样率和读取窗口决定）；
    // 内部委托 vibration_acquisition_->read_vibration_data() 执行 SPI 数据读取；
    // 采样数量由上层调用方按需指定

    ErrorCode calibrate_ad4134();
    // 轴承振动监测专用接口：触发 AD4134 传感器校准流程；
    // 执行零点偏移（Offset）和增益（Gain）的自动校准；
    // 校准期间传感器须处于静止（无振动）状态；
    // 内部委托 vibration_acquisition_->calibrate_sensor()
#endif
// 结束 BEARING_VIBRATION_PROJECT 条件编译块

    ErrorCode start_streaming();
    // 启动连续数据流采集模式：
    //   火焰检测：启动三路摄像头工作线程（FireDetectionAcquisition 的 workers_）；
    //   振动监测：启动 AD4134 连续采样循环；
    // 成功后设置 streaming_=true；
    // 须在 initialized_=true 之后调用

    ErrorCode stop_streaming();
    // 停止连续数据流采集模式：
    //   向 ConcurrentQueue 发送停止信号（queue.stop()），使工作线程退出阻塞；
    //   调用 thread::join() 等待所有工作线程安全退出；
    // 成功后设置 streaming_=false

    ErrorCode set_sample_rate(float rate_hz);
    // 设置数据采集频率：
    //   火焰检测：更新目标帧率（fps_），通过 V4L2 VIDIOC_S_PARM 写入摄像头；
    //   振动监测：更新 AD4134 的 OSR 参数以调整每通道采样率；
    // 参数 rate_hz：目标采样频率（Hz），振动监测有效范围通常为 1k~50k Hz

    ErrorCode set_resolution(int width, int height);
    // 设置图像采集分辨率（仅对火焰检测项目有效）：
    //   通过 cv::VideoCapture::set(CAP_PROP_FRAME_WIDTH/HEIGHT) 更新摄像头分辨率；
    //   同时更新 config_.image_width / config_.image_height；
    //   振动监测项目调用此方法将被忽略或返回 NOT_SUPPORTED

    float get_acquisition_fps() const;
    // 返回当前实际采集帧率（帧/秒）；
    // 由每次 acquire_data() 调用后根据时间差更新 acquisition_fps_；
    // 用于与 PerformanceMetrics::FrequencyMetrics::acquisition_fps 对接

    uint64_t get_total_samples() const;
    // 返回自系统启动以来的累计采集样本数；
    // 每次 acquire_data() 成功后递增 total_samples_；
    // 用于长时间运行监控和吞吐量统计

    bool is_streaming() const;
    // 返回当前是否处于连续流采集状态（读取 streaming_ 原子变量）；
    // 用于上层框架判断是否需要调用 start_streaming()

    // 模拟模式接口
    void enableSimulation(bool enable, const std::string& dataPath = "");
    // 启用或禁用模拟模式：
    //   enable=true 时设置 simulation_mode_=true，记录 simulation_data_path_；
    //   enable=false 时重置为真实硬件采集模式；
    // 须在 initialize() 之前调用，使初始化流程选择正确的数据源；
    // dataPath：预录数据文件的根目录路径，如 "datas/FireDetection"；
    // 默认为空字符串（使用编译期默认数据路径）

    bool isSimulationEnabled() const { return simulation_mode_; }
    // 内联方法：返回当前是否处于模拟模式（读取 simulation_mode_ 原子变量）；
    // const 修饰确保不修改对象状态

private:
// 私有区域：以下方法和成员仅供类内部使用

    ErrorCode initialize_fire_detection();
    // 私有方法：火焰检测项目的硬件初始化流程；
    // 调用 FireDetectionAcquisition::initialize()，触发 USB 摄像头设备发现
    // 和 V4L2 流初始化；由 initialize() 在 project_type_==FIRE_DETECTION 时调用

    ErrorCode initialize_bearing_vibration();
    // 私有方法：轴承振动监测项目的硬件初始化流程；
    // 调用 BearingVibrationAcquisition::initialize()，触发 SPI 总线初始化
    // 和 AD4134 寄存器配置；由 initialize() 在 project_type_==BEARING_VIBRATION 时调用

    ProjectType project_type_;
    // 当前框架实例的项目类型（FIRE_DETECTION 或 BEARING_VIBRATION）；
    // 由构造函数从 CURRENT_PROJECT 宏读取并固化，
    // 用于各公共方法中的项目类型分发

    std::atomic<bool> initialized_{false};
    // 硬件初始化完成标志（原子变量），初始为 false；
    // initialize() 成功后置 true，是 acquire_data() 和 start_streaming() 的前置条件

    std::atomic<bool> streaming_{false};
    // 连续流采集状态标志（原子变量），初始为 false；
    // start_streaming() 成功后置 true，stop_streaming() 后复位为 false；
    // 多线程场景下（工作线程读、主线程写）需原子访问

    uint64_t total_samples_{0};
    // 累计采集样本数，由 acquire_data() 在主线程递增；
    // 单线程访问（主线程），无需原子保护

    uint64_t last_sample_time_{0};
    // 最近一次采样完成的时间戳（Unix 毫秒），用于计算实时采集帧率；
    // acquisition_fps_ = 1000.0f / (current_time - last_sample_time_)

    float acquisition_fps_{0.0f};
    // 实时采集帧率（帧/秒），由每次 acquire_data() 调用后更新；
    // 初始为 0.0f，表示尚未进行任何采集

    ErrorCode last_error_{ErrorCode::SUCCESS};
    // 最近一次操作产生的错误码，初始为 SUCCESS；
    // 各公共方法失败时更新，由上层框架通过 IDataAcquisition 的错误查询接口读取

    std::string error_message_;
    // 最近一次错误的文本描述，与 last_error_ 配合提供完整错误上下文

    mutable std::mutex status_mutex_;
    // 可变互斥锁（mutable 允许在 const 方法如 is_ready()、diagnose() 中加锁）；
    // 保护 acquisition_fps_、total_samples_ 等状态数据的并发读写

    struct AcquisitionConfig {
    // 采集参数配置内嵌结构体：存储运行时可调整的采集行为参数
        float sample_rate_hz = 30.0f;
        // 采集频率（Hz），默认 30 Hz；
        //   火焰检测：对应摄像头目标帧率（fps_）
        //   振动监测：此字段由 SPI_CHANNEL 和 AD4134 OSR 参数决定，默认 50000 Hz；
        //   注意此处默认值 30.0f 仅适用于火焰检测，
        //   振动监测初始化时会覆盖为 SAMPLE_RATE_HZ（50000）

        int image_width = 320;
        // 采集图像宽度（像素），默认 320；
        // 注意：此默认值（320）低于 CameraConfig 中的 640 和模型输入 224，
        // 实际部署时应与 CAMERA_RESOLUTION_WIDTH（640）保持一致

        int image_height = 320;
        // 采集图像高度（像素），默认 320

        int buffer_capacity = 100;
        // 采集缓冲区容量（帧数或样本数），默认 100；
        // 用于初始化 ConcurrentQueue 的缓冲深度上限，
        // 防止生产速度长期超过消费速度导致内存无限增长

        bool auto_exposure = true;
        // 为 true 时摄像头启用自动曝光（AE）；
        // 与 CameraConfig::CameraParams::exposure = -1.0f 的语义一致

        bool auto_white_balance = true;
        // 为 true 时摄像头启用自动白平衡（AWB）；
        // 灰度推理模型对白平衡不敏感，但保持 AWB 有助于提高图像质量
    } config_;
    // 声明 config_ 成员变量，值初始化采用上方默认值

#ifdef BEARING_VIBRATION_PROJECT
    std::unique_ptr<BearingVibrationAcquisition> vibration_acquisition_;
    // 轴承振动采集子类实例（条件编译）；
    // 由 initialize_bearing_vibration() 创建，管理 AD4134 SPI 驱动和三轴数据读取；
    // unique_ptr 确保析构时自动调用 BearingVibrationAcquisition::shutdown()
#endif

    // 模拟模式相关
    std::atomic<bool> simulation_mode_{false};
    // 模拟模式总开关（原子变量），初始为 false；
    // enableSimulation(true) 后置 true，initialize() 将跳过硬件初始化
    // 改为调用 openSimulationSource() 打开预录数据文件

    std::string simulation_data_path_;
    // 模拟数据文件的根目录路径，由 enableSimulation() 写入；
    // 如 "datas/FireDetection" 或 "datas/BearingVibration"；
    // openSimulationSource() 基于此路径枚举具体的数据文件

    struct SimulationState {
    // 模拟模式运行状态内嵌结构体：维护模拟数据的读取进度

#ifdef BEARING_VIBRATION_PROJECT
        std::vector<std::array<float, 3>> all_data;
        // 预录的全部振动数据点列表；
        // 每个元素为 std::array<float, 3>，按 {x, y, z} 顺序存储三轴加速度值（单位 g）；
        // openSimulationSource() 从 CSV 文件逐行解析并填充此列表

        size_t current_index = 0;
        // 当前读取位置指针，初始为 0；
        // 每次 readNextSimulationData() 读取 batch_size 个点后递增 batch_size；
        // 到达 all_data.size() 后循环回零

        size_t batch_size = 5000;
        // 每次 readNextSimulationData() 调用返回的数据点数量，默认 5000；
        // 对应 AD4134 @ 50 kSPS 时约 100 ms 的采样窗口；
        // 与 BearingVibrationAcquisition::read_vibration_data() 的默认 samples=5000 一致
#endif
// 结束 BEARING_VIBRATION_PROJECT 条件编译块（SimulationState 内）

    };
    std::unique_ptr<SimulationState> sim_state_;
    // 模拟模式运行状态实例；
    // 由 openSimulationSource() 在 simulation_mode_=true 时创建；
    // unique_ptr 保证在框架析构时自动释放所有预加载的模拟数据内存

    ErrorCode openSimulationSource();
    // 私有方法：打开并预加载模拟数据源；
    //   火焰检测：使用 std::filesystem 遍历 simulation_data_path_ 目录，
    //             构建 ImageTriplet 列表（不预加载图像数据，仅记录路径）；
    //   振动监测：读取 simulation_data_path_ 下的 CSV 文件，
    //             解析所有数据行并填充 SimulationState::all_data；
    // 成功后创建 sim_state_，失败返回 FILE_NOT_FOUND 或 FILE_READ_ERROR

    ErrorCode readNextSimulationData(RawData& data);
    // 私有方法：从模拟数据源读取下一批数据并封装为 RawData；
    //   火焰检测：按 current_index 读取三幅图像文件（cv::imread），
    //             组装为包含三路摄像头帧的 RawData；
    //   振动监测：从 all_data 中截取 batch_size 个三轴数据点，
    //             分离为三个浮点数组后封装到 RawData；
    // 每次调用后递增 current_index（到末尾时回零循环）
};
// 结束 DataAcquisitionFramework 类定义


// ============================================================================
// 轴承振动采集类（保持不变）
// ============================================================================
// 分节注释：BearingVibrationAcquisition 负责通过 SPI 总线驱动 AD4134 ADC 芯片
// 采集三轴加速度数据，内嵌 AD4134Device 类封装全部 SPI 底层操作

#ifdef BEARING_VIBRATION_PROJECT
class BearingVibrationAcquisition {
// 轴承振动监测专用采集类（条件编译）；
// 通过 AD4134Device 嵌套类驱动 AD4134 四通道 ADC 芯片，
// 以 50 kSPS 采样率同时采集三轴（X/Y/Z）加速度信号
public:
    BearingVibrationAcquisition();
    // 构造函数：创建 AD4134Device 实例，初始化各成员为默认状态

    ~BearingVibrationAcquisition();
    // 析构函数：若 initialized_=true，调用 shutdown() 安全关闭 SPI 连接
    // 并释放 GPIO 资源

    ErrorCode initialize(int spi_channel = SPI_CHANNEL, int spi_speed = SPI_SPEED);
    // 初始化轴承振动采集：
    //   1. 调用 AD4134Device::initialize(spi_channel, spi_speed)
    //      → 打开 /dev/spidev{spi_channel}.0，配置 SPI 模式/速率/位宽
    //   2. 调用 AD4134Device::init_gpio_pins()
    //      → 配置 BCM 引脚方向和初始电平（CS、DOUT、CLK 等）
    //   3. 调用 AD4134Device::configure_registers()
    //      → 按 bearing_vibration_config.hpp 中的寄存器默认值写入 AD4134
    // 参数均使用 ProjectDefines.hpp 中的宏作为默认值，无需调用方显式传参；
    // 成功后设置 initialized_=true

    ErrorCode shutdown();
    // 关闭 SPI 连接，释放文件描述符；
    // 将 BCM 引脚恢复至安全输入状态（避免上电时误触发继电器）；
    // 设置 initialized_=false

    ErrorCode read_vibration_data(std::vector<float>& x_data,
                                 std::vector<float>& y_data,
                                 std::vector<float>& z_data,
                                 int samples = 5000);
    // 从 AD4134 三个通道读取指定数量的振动加速度样本；
    // 参数 x_data/y_data/z_data：各轴输出的采样序列（单位 g）；
    // 参数 samples：每通道采样点数，默认 5000；
    //   @ 50 kSPS 对应约 100 ms 的时域窗口（满足 1024 点 FFT 的需求）；
    // 内部委托 AD4134Device::read_channels() 通过 SPI 批量读取并转换为物理单位

    ErrorCode calibrate_sensor();
    // 执行 AD4134 传感器零偏校准：
    //   1. 采集 1 秒静态数据（无振动条件下）
    //   2. 计算三轴均值（零偏偏移量）
    //   3. 通过 write_register() 将偏移量写入 AD4134 偏置补偿寄存器；
    // 须在安装完成、设备静止时调用，以消除安装偏角和重力分量的影响

    ErrorCode set_sample_rate(float rate_hz);
    // 动态调整 AD4134 的每通道采样率；
    // 通过修改过采样率（OSR）寄存器实现：
    //   rate_hz = 外部时钟（6.4 MHz）/ OSR / 通道数；
    //   可用采样率取决于 AD4134 支持的 OSR 档位（2、4、8、16、32...）；
    // 委托 AD4134Device::set_sample_rate() 计算并写入对应 OSR 值

    bool is_sensor_connected() const;
    // 返回 AD4134 传感器是否已成功初始化并可正常通信；
    // 通过 AD4134Device::is_initialized() 和 SPI 读寄存器应答校验

    float get_current_sample_rate() const;
    // 返回当前 AD4134 实际每通道采样率（Hz）；
    // 对应 current_sample_rate_ 成员值（初始为 SAMPLE_RATE_HZ=50000）

private:
    class AD4134Device {
    // 私有嵌套类：AD4134 四通道 ADC 芯片的底层 SPI 驱动；
    // 封装全部寄存器读写、GPIO 初始化和三通道 DMA/FIFO 数据读取操作；
    // BearingVibrationAcquisition 通过此类与硬件交互，上层无需了解 SPI 细节
    public:
        AD4134Device();
        // 构造函数：将 spi_fd_ 初始化为 -1（未打开），initialized_ 为 false

        ~AD4134Device();
        // 析构函数：若 spi_fd_ != -1，调用 close(spi_fd_) 释放文件描述符

        ErrorCode initialize(int spi_channel, int speed);
        // 初始化 SPI 总线连接：
        //   1. open("/dev/spidev{spi_channel}.0", O_RDWR) 打开 SPI 设备文件
        //   2. ioctl(spi_fd_, SPI_IOC_WR_MODE, &mode) 设置 SPI 模式 3（CPOL=1, CPHA=1）
        //   3. ioctl(spi_fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed) 设置时钟频率
        //   4. ioctl(spi_fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) 设置 8 位字长
        // 成功后设置 initialized_=true

        ErrorCode shutdown();
        // 关闭 SPI 文件描述符：close(spi_fd_)，设置 spi_fd_=-1 和 initialized_=false

        ErrorCode read_channels(std::vector<float>& ch1_data,
                               std::vector<float>& ch2_data,
                               std::vector<float>& ch3_data,
                               int samples);
        // 从 AD4134 连续读取 samples 个三通道数据点：
        //   1. 构造 SPI 读取指令帧（依据 AD4134 数据手册的数据包格式）
        //   2. 通过 ioctl(SPI_IOC_MESSAGE) 执行多次 SPI 全双工传输
        //   3. 解析返回的 24 位 ADC 原始码（二补数）并转换为加速度值（g）：
        //      g = raw_code / (2^23) × full_scale / pga_gain
        //   4. 将转换结果追加到 ch1_data/ch2_data/ch3_data（对应 X/Y/Z 轴）

        ErrorCode init_gpio_pins();
        // 初始化 AD4134 控制所需的 GPIO 引脚（BCM 编号）：
        //   片选（CS）→ 输出，初始高电平（不选中）
        //   同步（SYNC）→ 输出，初始高电平
        //   复位（RESET）→ 输出，初始高电平（低电平复位）
        // 通过 /dev/gpiochip 或 WiringPi 的 ioctl 接口设置引脚方向和初始电平

        ErrorCode configure_registers();
        // 按 bearing_vibration_config.hpp 中 AD4134Config::Registers 的默认值
        // 配置 AD4134 各功能寄存器：
        //   device_config（0x01）：使能器件，选择连续转换模式
        //   data_packet_config（0x00）：默认数据包格式
        //   filter_config（0x00）：配置 Sinc5+Sinc1 低通滤波器
        //   gain_config（0x00）：配置 PGA 增益
        // 每个寄存器通过 write_register() 写入

        ErrorCode set_sample_rate(float rate_hz);
        // 根据目标采样率计算对应的 OSR 值并写入 AD4134 过采样率寄存器：
        //   OSR = 外部时钟（6.4 MHz）/ (rate_hz × 通道数)
        // 支持的 OSR 档位需查阅 AD4134 数据手册，就近选取合法值

        bool is_initialized() const;
        // 返回 AD4134 是否已完成初始化（spi_fd_ != -1 且 initialized_=true）

    private:
        int spi_fd_{-1};
        // SPI 设备文件描述符，初始为 -1（未打开状态）；
        // initialize() 成功后由 open() 赋值为非负整数；
        // 用于后续所有 ioctl 和 read/write 系统调用

        int spi_channel_{0};
        // SPI 总线通道编号（对应 /dev/spidev{spi_channel_}.0），
        // 由 initialize() 参数传入并存储；
        // 与 ProjectDefines.hpp 中 SPI_CHANNEL（=0）的默认值一致

        int spi_speed_{1000000};
        // SPI 时钟频率（Hz），默认 1,000,000 Hz（1 MHz）；
        // 注意：BearingVibrationAcquisition::initialize() 的默认参数
        // SPI_SPEED=2000000（2 MHz），与此处字段默认值（1 MHz）不同；
        // 实际使用时以 initialize() 传入的 spi_speed 参数为准，
        // 此字段在 initialize() 中被覆盖

        bool initialized_{false};
        // AD4134 设备初始化状态标志，初始为 false；
        // initialize() 成功后置 true，shutdown() 后复位为 false

        ErrorCode read_register(uint8_t reg, uint8_t& value);
        // 通过 SPI 读取 AD4134 指定寄存器的值：
        //   1. 构造读命令字节：reg | 0x80（最高位置 1 表示读操作）
        //   2. 通过 SPI_IOC_MESSAGE 发送命令并接收 1 字节响应
        //   3. 将响应值写入 value 参数
        // 用于寄存器校验和自检

        ErrorCode write_register(uint8_t reg, uint8_t value);
        // 通过 SPI 向 AD4134 指定寄存器写入值：
        //   1. 构造写命令字节：reg & 0x7F（最高位清零表示写操作）
        //   2. 通过 SPI_IOC_MESSAGE 发送命令字节和数据字节
        // 用于初始化配置和运行时参数更新
    };
    // 结束 AD4134Device 嵌套类定义

    std::unique_ptr<AD4134Device> ad4134_device_;
    // AD4134 设备驱动实例，由构造函数 make_unique 创建；
    // unique_ptr 确保在 BearingVibrationAcquisition 析构时
    // 自动调用 AD4134Device 析构函数关闭 SPI 文件描述符

    std::atomic<bool> initialized_{false};
    // 振动采集层初始化状态标志（原子变量），初始为 false；
    // initialize() 成功后置 true，shutdown() 后复位；
    // 与 AD4134Device::initialized_ 的区别：
    //   AD4134Device::initialized_ 跟踪 SPI 硬件层状态；
    //   此处 initialized_ 跟踪 BearingVibrationAcquisition 整体逻辑层状态

    float current_sample_rate_{SAMPLE_RATE_HZ};
    // 当前配置的每通道采样率（Hz），初始为 SAMPLE_RATE_HZ（50000）；
    // set_sample_rate() 成功后更新此字段；
    // get_current_sample_rate() 返回此值
};
#endif // BEARING_VIBRATION_PROJECT
// 结束 BearingVibrationAcquisition 条件编译块

} // namespace MultiProject
// 关闭命名空间 MultiProject