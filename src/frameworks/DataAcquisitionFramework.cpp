/**
  ******************************************************************************
  * @file    DataAcquisitionFramework.cpp
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
 * @brief   数据采集框架 — 实现文件
 * ----------------------------------------------------------------------------
 * @details 本文件实现了 DataAcquisitionFramework.hpp 中声明的全部类和方法，
 *          包含以下四个主要实现模块：
 *
 *          ── DataAcquisitionFramework 主框架类实现 ──
 *            构造/析构、initialize（硬件/模拟两条路径）、acquire_data、
 *            diagnose、start/stop_streaming、set_sample_rate/set_resolution、
 *            enableSimulation、openSimulationSource、readNextSimulationData
 *
 *          ── 火焰检测采集类实现（仅 FIRE_DETECTION_PROJECT）──
 *            FireDetectionAcquisition：三路 USB 摄像头并发采集
 *              findUsbCameraDevices（sysfs USB 路径过滤 + VIDIOC_QUERYCAP 验证）
 *              openCamera、camera_worker（帧采集+灰度化+入队）
 *              initialize、shutdown、capture_images
 *
 *          ── 轴承振动采集类实现（仅 BEARING_VIBRATION_PROJECT）──
 *            BearingVibrationAcquisition：AD4134 SPI 驱动三轴振动采集
 *              AD4134 引脚定义（含 H1→实物 Pin 翻转规律说明）
 *              AD4134Device：GPIO 初始化、SPI 配置、寄存器写入/回读、
 *              并行 3 通道 24 位数据读取
 *
 *          ── 注释掉的两版 findUsbCameraDevices 历史实现 ──
 *            v1：通过 /sys/class/video4linux/ * /name 匹配 "uvcvideo"
 *            v2：通过 sysfs 符号链接路径含 "/usb" 过滤
 *            v3（当前）：v2 基础上叠加 VIDIOC_QUERYCAP 能力过滤
 *
 * @note    模拟模式下：
 *            火焰检测：通过 source_name 携带 "FIRE_PARAM|..." 编码参数，
 *              供 DataProcessingFramework 写 PNG 时同步写 .txt 标注文件；
 *            振动监测：通过 source_name 携带 "BEARING_PARAM|..." 编码参数，
 *              含 CSV 文件名、类别标签和起始行号。
 * ============================================================================
 */

#include "DataAcquisitionFramework.hpp"
// 引入数据采集框架的完整类声明，含所有嵌套类、成员变量和方法签名

#include <chrono>
// 引入 std::chrono，用于采样时间戳和帧率计算

#include <iostream>
// 引入 std::cout / std::cerr，用于状态提示和错误信息打印

#include <thread>
// 引入 std::this_thread::sleep_for，用于摄像头工作线程的帧率控制

#include <fcntl.h>
// 引入 open()、O_RDWR、O_RDONLY、O_NONBLOCK 等文件控制接口；
// 用于打开 SPI 设备文件（/dev/spidev0.0）和摄像头 V4L2 设备文件（/dev/videoX）

#include <unistd.h>
// 引入 close()、usleep()、delayMicroseconds() 等 POSIX 接口；
// close() 用于关闭 SPI 文件描述符；usleep() 用于寄存器写入后的等待延迟

#include <sys/ioctl.h>
// 引入 ioctl() 系统调用；
// 用于 SPI 配置（SPI_IOC_WR_MODE/BITS/SPEED/MESSAGE）
// 和 V4L2 摄像头能力查询（VIDIOC_QUERYCAP）

#include <linux/spi/spidev.h>
// 引入 Linux SPI 设备驱动接口，提供：
//   SPI_MODE_3        — SPI 模式 3（CPOL=1, CPHA=1，AD4134 所需）
//   SPI_IOC_WR_MODE   — 设置 SPI 模式的 ioctl 命令码
//   SPI_IOC_WR_BITS_PER_WORD — 设置每字位数的 ioctl 命令码
//   SPI_IOC_WR_MAX_SPEED_HZ  — 设置时钟频率的 ioctl 命令码
//   SPI_IOC_MESSAGE(n) — 执行 n 条 SPI 全双工传输的 ioctl 命令码
//   struct spi_ioc_transfer — 单次 SPI 传输描述结构体

#include <cstring>
// 引入 memset()，用于 SPI 传输描述结构体（spi_ioc_transfer）的零初始化

#include <wiringPi.h>
// 引入 WiringPi 库，提供 GPIO 控制函数：
//   wiringPiSetupGpio() — 以 BCM 编号模式初始化 GPIO
//   pinMode()          — 设置引脚方向（INPUT/OUTPUT）
//   digitalWrite()     — 写引脚电平（HIGH/LOW）
//   digitalRead()      — 读引脚电平
//   delay()            — 毫秒级延时
//   delayMicroseconds() — 微秒级延时

#include <fstream>
// 引入 std::ifstream，用于振动模拟模式下读取 CSV 数据文件

#include <sstream>
// 引入 std::stringstream，用于 CSV 行解析（按分隔符分列）

#include <algorithm>
// 引入 std::sort、std::find、std::transform，用于设备路径数字排序、
// CSV 列名查找和字符串大小写转换

#include <array>
// 引入 std::array，用于振动模拟数据中三轴样本的存储
// （sim_state_->all_data 为 std::vector<std::array<float,3>>）

#include <atomic>
// 引入 std::atomic，用于跨线程安全读写的状态标志

#include <mutex>
// 引入 std::mutex 和 std::lock_guard，用于 status_mutex_ 的 RAII 加锁

#include <condition_variable>
// 引入 std::condition_variable（被 ConcurrentQueue 使用），
// 虽然本文件未直接声明 cv 成员，但编译 ConcurrentQueue.hpp 时需要此头文件

#include <dirent.h>
// 引入 POSIX 目录遍历接口（opendir/readdir/closedir），
// 备用方案；当前实现使用 std::filesystem 替代

#include <regex>
// 引入 std::regex，早期版本 findUsbCameraDevices 中用于提取设备编号数字；
// 当前 v3 版本改用 rfind+substr，但引入保留以备历史兼容

#include <linux/videodev2.h>
// 引入 V4L2 内核接口，提供：
//   struct v4l2_capability — 摄像头能力描述结构体
//   VIDIOC_QUERYCAP       — 查询摄像头能力的 ioctl 命令码
//   V4L2_CAP_VIDEO_CAPTURE — 视频采集能力标志位
//   V4L2_CAP_META_CAPTURE — 元数据采集能力标志位（过滤非图像节点）

namespace fs = std::filesystem;
// 为 std::filesystem 命名空间创建别名 fs，减少路径操作代码的书写长度

namespace MultiProject {
// 开启命名空间 MultiProject

// 去除绝对路径前缀，只保留 datas/... 相对路径
// 例：/home/pi/multi_project_system/datas/FireDetection/...
//   → datas/FireDetection/...
static std::string to_relative_path(const std::string& abs_path) {
    // 文件级静态工具函数：将绝对路径截取为从 "datas/" 开始的相对路径；
    // 用途：写入 source_name 时避免携带机器特定的绝对路径前缀，
    //       使生成的标注文件在不同部署路径下可重现
    const std::string marker = "datas/";
    auto pos = abs_path.find(marker);
    // 在绝对路径字符串中查找 "datas/" 子串的位置
    if (pos != std::string::npos)
        return abs_path.substr(pos);
        // 找到标记位置，截取从 "datas/" 开始的后缀字符串
    return abs_path;   // 找不到则原样返回
    // 若路径中不含 "datas/"（如测试路径或绝对路径），原样返回避免丢失信息
}

DataAcquisitionFramework::DataAcquisitionFramework() : project_type_(CURRENT_PROJECT) {}
// 构造函数：通过成员初始化列表将编译期宏 CURRENT_PROJECT 记录到 project_type_；
// 其余成员（initialized_/streaming_/simulation_mode_ 等）均在声明处初始化（= false/0等），
// 无需在构造函数体中额外赋值

DataAcquisitionFramework::~DataAcquisitionFramework() {
    stop_streaming();
    // 析构函数中调用 stop_streaming()，确保摄像头工作线程
    // 或 AD4134 连续采样在对象销毁前安全停止；
    // 防止工作线程在 unique_ptr 成员析构后继续访问已释放的内存
}

ErrorCode DataAcquisitionFramework::initialize() {
    // 执行硬件或模拟数据源的初始化；
    // 模拟模式路径：跳过所有硬件操作，直接置 initialized_=true；
    // 真实硬件路径：加锁后按项目宏分发至对应的私有初始化方法

    if (simulation_mode_) {
        initialized_ = true;
        return ErrorCode::SUCCESS;
        // 模拟模式下 openSimulationSource() 已在 enableSimulation() 中完成，
        // 此处直接标记为已初始化即可，无需任何硬件操作
    }

    std::lock_guard<std::mutex> lock(status_mutex_);
    // 加锁保护硬件初始化期间 initialized_ 等状态标志的并发访问；
    // lock_guard 在作用域结束时自动解锁（RAII 语义）

    try {
#ifdef BEARING_VIBRATION_PROJECT
        return initialize_bearing_vibration();
        // 振动监测路径：创建 BearingVibrationAcquisition 实例，
        // 触发 SPI 初始化和 AD4134 寄存器配置
#endif
    } catch (const std::exception& e) {
        error_message_ = e.what();
        // 将异常信息存入 error_message_，供 is_ready() 和错误查询接口使用
        last_error_ = ErrorCode::UNKNOWN_ERROR;
        return last_error_;
    }
}


ErrorCode DataAcquisitionFramework::initialize_bearing_vibration() {
    // 私有方法：轴承振动监测项目硬件初始化，仅在 BEARING_VIBRATION_PROJECT 宏下编译

#ifdef BEARING_VIBRATION_PROJECT
    vibration_acquisition_ = std::make_unique<BearingVibrationAcquisition>();
    // 在堆上构造 BearingVibrationAcquisition 实例

    auto status = vibration_acquisition_->initialize(SPI_CHANNEL, SPI_SPEED);
    // 触发 GPIO 引脚初始化、SPI 总线配置和 AD4134 寄存器写入序列；
    // SPI_CHANNEL 和 SPI_SPEED 来自 ProjectDefines.hpp 中的宏定义

    if (status != ErrorCode::SUCCESS) {
        std::cerr << "AD4134设备初始化失败" << std::endl;
        return status;
    }
    initialized_ = true;
    return ErrorCode::SUCCESS;
#else
    return ErrorCode::NOT_IMPLEMENTED;
#endif
}

ErrorCode DataAcquisitionFramework::acquire_data(RawData& data) {
    // 获取一帧/一批原始数据；
    // 支持真实硬件和模拟两条路径，并在每次成功采集后更新实时帧率统计

    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    // 前置检查：初始化未完成时返回错误

    if (simulation_mode_) {
        return readNextSimulationData(data);
        // 模拟模式：从预加载的图像三元组或 CSV 数据中读取下一批样本
    }

    data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    // 记录本次采集的 Unix 毫秒时间戳，用于下游框架的帧间延迟统计

    data.sequence_number = total_samples_++;
    // 递增采集序列号（原子操作），用于检测丢帧（序列号不连续）

#ifdef BEARING_VIBRATION_PROJECT
    if (vibration_acquisition_) {
        std::vector<float> x_data, y_data, z_data;
        auto status = vibration_acquisition_->read_vibration_data(x_data, y_data, z_data, 5000);
        // 从 AD4134 读取 5000 个三轴样本（@ 50 kSPS ≈ 100 ms 的时域窗口）
        if (status != ErrorCode::SUCCESS) return status;

        data.sensor_data.insert(data.sensor_data.end(), x_data.begin(), x_data.end());
        data.sensor_data.insert(data.sensor_data.end(), y_data.begin(), y_data.end());
        data.sensor_data.insert(data.sensor_data.end(), z_data.begin(), z_data.end());
        // 按 X、Y、Z 顺序将三轴数据拼接到 sensor_data 向量；
        // 总长度 = 5000 × 3 = 15000 个 float，约 60 KB

        data.metadata.sample_rate = SAMPLE_RATE_HZ;
        // 记录采样率元数据，供 DataProcessingFramework 计算 FFT 频率轴

        data.source_name = "AD4134Sensor";
        // 标识数据来源为 AD4134 SPI ADC（非模拟）
    }
#endif

    uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    // 记录采集完成后的当前时间戳（毫秒）

    if (last_sample_time_ > 0) {
        float interval = (current_time - last_sample_time_) / 1000.0f;
        // 计算两次采集之间的间隔（秒）
        if (interval > 0) {
            acquisition_fps_ = 0.9 * acquisition_fps_ + 0.1 * (1.0f / interval);
            // 指数移动平均（EMA）计算实时帧率：
            //   新帧率 = 0.9 × 旧帧率 + 0.1 × (1/间隔)；
            //   α=0.1 的低通滤波消除帧间抖动，反映平滑帧率趋势
        }
    }
    last_sample_time_ = current_time;
    // 更新上次采集时间戳，供下次调用计算间隔
    return ErrorCode::SUCCESS;
}

bool DataAcquisitionFramework::is_ready() const {
    return initialized_ && error_message_.empty();
    // 两个条件同时满足才视为就绪：
    //   initialized_=true 表示初始化已完成；
    //   error_message_.empty() 表示无待处理错误
}

ErrorCode DataAcquisitionFramework::configure(const std::string& config_str) {
    (void)config_str;
    // (void) 消除未使用参数的编译器警告
    return ErrorCode::SUCCESS;
    // 桩实现：配置参数解析功能尚未实现（当前通过编译期宏和默认值配置）
}

SystemStatus::HardwareStatus DataAcquisitionFramework::diagnose() const {
    // 执行硬件自检，返回各子系统健康状态报告

    SystemStatus::HardwareStatus status;
    // 创建默认状态对象（各健康标志初始为 false）

#ifdef BEARING_VIBRATION_PROJECT
    if (vibration_acquisition_) {
        status.sensors_healthy = vibration_acquisition_->is_sensor_connected();
        // 检查 AD4134 传感器是否已成功初始化（通过 initialized_ 标志）

        status.spi_healthy = status.sensors_healthy;
        // SPI 总线健康状态与传感器连接状态绑定：
        // 传感器正常则认为 SPI 通信正常，反之亦然
    }
#endif
    return status;
}

ErrorCode DataAcquisitionFramework::start_streaming() {
    streaming_ = true;
    // 将流式采集标志置为 true；
    // 摄像头工作线程已在 initialize() 时启动（不在 start_streaming() 中），
    // 此处仅更新 streaming_ 状态供上层查询使用
    return ErrorCode::SUCCESS;
}

ErrorCode DataAcquisitionFramework::stop_streaming() {
    streaming_ = false;
    // 将流式采集标志置为 false；
    // 实际的线程停止（camera_worker 退出循环）由 running_=false 触发，
    // 此处仅更新 streaming_ 状态标志
    return ErrorCode::SUCCESS;
}

ErrorCode DataAcquisitionFramework::set_sample_rate(float rate_hz) {
    config_.sample_rate_hz = rate_hz;
    // 更新 AcquisitionConfig 中的采样率配置参数；
    // 注意：对于 AD4134，真正的采样率变更需要调用
    // vibration_acquisition_->set_sample_rate() 写入硬件寄存器，
    // 此处仅更新软件层配置（可能与实际硬件采样率不同步）
    return ErrorCode::SUCCESS;
}

ErrorCode DataAcquisitionFramework::set_resolution(int width, int height) {
    config_.image_width = width;
    config_.image_height = height;
    // 更新图像分辨率配置参数；
    // 同样为软件层配置更新，摄像头工作线程已启动时不会自动应用新分辨率，
    // 需重启线程才能生效
    return ErrorCode::SUCCESS;
}

float DataAcquisitionFramework::get_acquisition_fps() const {
    return acquisition_fps_;
    // 返回当前实时采集帧率的 EMA 平滑值
}

uint64_t DataAcquisitionFramework::get_total_samples() const {
    return total_samples_;
    // 返回累计采集样本数（每次 acquire_data() 成功后递增）
}

bool DataAcquisitionFramework::is_streaming() const {
    return streaming_;
    // 返回当前流式采集状态标志
}

// 模拟模式
void DataAcquisitionFramework::enableSimulation(bool enable, const std::string& dataPath) {
    // 启用或禁用模拟模式；须在 initialize() 之前调用

    simulation_mode_ = enable;
    simulation_data_path_ = dataPath;
    // 记录模拟数据根目录路径，供 openSimulationSource() 使用

    if (enable) {
        if (openSimulationSource() != ErrorCode::SUCCESS) {
            std::cerr << "Failed to open simulation data source: " << dataPath << std::endl;
            simulation_mode_ = false;
            // 打开失败时回退到真实硬件模式，防止后续 acquire_data() 使用无效的 sim_state_
        }
    } else {
        sim_state_.reset();
        // 禁用模拟模式时释放已加载的模拟数据（释放 ImageTriplet 列表或 CSV 数据内存）
    }
}

ErrorCode DataAcquisitionFramework::openSimulationSource() {
    // 预加载模拟数据源，不执行任何硬件操作

    sim_state_ = std::make_unique<SimulationState>();
    // 创建模拟状态对象，各字段初始为默认值（空列表，current_index=0）

#ifdef BEARING_VIBRATION_PROJECT
    // 轴承振动模拟：读取 CSV 文件
    std::ifstream file(simulation_data_path_);
    if (!file.is_open()) {
        std::cerr << "无法打开模拟数据文件: " << simulation_data_path_ << std::endl;
        return ErrorCode::FILE_NOT_FOUND;
        // 文件不存在或权限不足时打印错误路径并返回
    }

    // 读取表头行
    std::string line;
    if (!std::getline(file, line)) {
        return ErrorCode::FILE_READ_ERROR;
        // 文件为空（无法读取第一行）时返回读取错误
    }

    // 去除 UTF-8 BOM（\xEF\xBB\xBF）
    const std::string bom = "\xEF\xBB\xBF";
    if (line.compare(0, bom.size(), bom) == 0) {
        line = line.substr(bom.size());
        // UTF-8 BOM 是 Windows Excel 保存 CSV 时自动添加的 3 字节标记（EF BB BF）；
        // 若不去除，第一列的列名会包含 BOM 字符，导致 find() 查找失败
    }

    // 去除可能的回车符
    if (!line.empty() && line.back() == '\r') line.pop_back();
    // Windows 换行符 \r\n 中的 \r（回车符）会残留在 getline 读取的行末；
    // pop_back() 删除末尾 \r，避免列名比较时出现不可见字符导致的匹配失败

    // 判断分隔符：尝试制表符和逗号
    std::vector<std::string> headers;
    char delimiter = '\t'; // 默认制表符
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, '\t')) {
        if (!cell.empty() && cell.back() == '\r') cell.pop_back();
        headers.push_back(cell);
    }
    // 先尝试制表符（\t）分割，适合从 MATLAB/Excel 导出的 TSV 格式

    if (headers.size() <= 1) {
        // 制表符分割不成功，尝试逗号
        // 若只解析出 ≤1 列，说明分隔符不是制表符，改用逗号重试
        headers.clear();
        ss.clear();
        ss.str(line);
        delimiter = ',';
        while (std::getline(ss, cell, ',')) {
            if (!cell.empty() && cell.back() == '\r') cell.pop_back();
            headers.push_back(cell);
        }
        // 重试逗号分隔，适合标准 CSV 格式
    }
    // 自动检测分隔符的策略：先尝试制表符（TSV），失败后回退到逗号（CSV）

    // 目标列名
    std::vector<std::string> target_cols = {"de_normal_X", "de_normal_Y", "de_normal_Z"};
    // 目标列名对应轴承振动数据集中正常（de_normal）状态的 X/Y/Z 三轴列；
    // 若数据集包含其他故障类别（de_ballfault 等），列名需对应调整

    std::vector<int> col_indices;
    for (const auto& col : target_cols) {
        auto it = std::find(headers.begin(), headers.end(), col);
        if (it == headers.end()) {
            std::cerr << "CSV 文件中未找到列: " << col << std::endl;
            std::cerr << "表头内容: ";
            for (const auto& h : headers) std::cerr << "'" << h << "' ";
            std::cerr << std::endl;
            // 打印完整的实际表头内容，帮助用户核对列名拼写和 BOM/回车符问题
            return ErrorCode::CONFIG_PARSE_ERROR;
            // 目标列缺失时返回配置解析错误（列名与实际文件不匹配）
        }
        col_indices.push_back(std::distance(headers.begin(), it));
        // 记录目标列在 headers 中的位置索引，用于后续数据行的列值提取
    }

    // 读取数据行，最多 5000 行
    const int max_rows = 5000;
    int row_count = 0;
    while (std::getline(file, line) && row_count < max_rows) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // 同样去除行末的 Windows 回车符

        std::stringstream data_stream(line);
        std::string value_str;
        std::vector<float> values;

        // 使用已确定的分隔符解析
        while (std::getline(data_stream, value_str, delimiter)) {
            if (!value_str.empty() && value_str.back() == '\r') value_str.pop_back();
            values.push_back(std::stof(value_str));
            // std::stof 将字符串转换为 float；
            // 若某列包含非数字字符（如空字符串、"NaN"），会抛出 std::invalid_argument，
            // 当前未单独捕获，依赖外层 try-catch
        }

        if (values.size() < headers.size()) continue;
        // 跳过列数不足的残缺行（如文件末尾的空行）

        float x = values[col_indices[0]];
        float y = values[col_indices[1]];
        float z = values[col_indices[2]];
        sim_state_->all_data.push_back({x, y, z});
        // 将三轴样本以 std::array<float, 3> 形式追加到全量数据列表
        row_count++;
    }
    file.close();

    if (sim_state_->all_data.empty()) {
        std::cerr << "CSV 文件中没有有效数据" << std::endl;
        return ErrorCode::FILE_READ_ERROR;
    }

    std::cout << "[BEARING] 已加载 " << sim_state_->all_data.size()
              << " 个样本，CSV: " << simulation_data_path_ << std::endl;
    // 打印加载成功的样本数量，便于验证 CSV 文件是否被完整读取

    sim_state_->current_index = 0;
    sim_state_->batch_size = 5000;
    // 初始化读取位置和批量大小；
    // batch_size=5000 对应 AD4134 @ 50 kSPS 约 100 ms 的采样窗口，
    // 与 acquire_data() 中的真实采集调用（samples=5000）保持一致
    return ErrorCode::SUCCESS;
#endif
// 结束 BEARING_VIBRATION_PROJECT 条件编译块

    return ErrorCode::NOT_IMPLEMENTED;
    // 两个项目宏均未定义时的兜底返回（应由 ProjectDefines.hpp 中的 #error 阻止此情况）
}

ErrorCode DataAcquisitionFramework::readNextSimulationData(RawData& data) {
    // 从模拟数据源读取下一批数据并封装为 RawData；
    // 到达数据末尾时循环回头（环形读取，实现无限数据流模拟）

    if (!sim_state_) return ErrorCode::NOT_INITIALIZED;
    // 模拟状态未创建（openSimulationSource 未成功）时返回错误

    data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    data.sequence_number = total_samples_++;
    // 填充时间戳和序列号（与真实采集路径行为一致）

#ifdef BEARING_VIBRATION_PROJECT
    // 从 all_data 中读取一批样本
    size_t batch = sim_state_->batch_size;
    size_t total = sim_state_->all_data.size();
    if (total == 0) return ErrorCode::DATA_VALIDATION_FAILED;

    std::vector<float> x_batch, y_batch, z_batch;
    x_batch.reserve(batch);
    y_batch.reserve(batch);
    z_batch.reserve(batch);
    // 预分配容量，避免批量 push_back 中的多次内存重分配

    // 在循环前记录起始行号，保证准确性
    const size_t batch_start_row = sim_state_->current_index;
    // 记录本批次在 all_data 中的起始索引，写入 source_name 供标注文件溯源

    for (size_t i = 0; i < batch; ++i) {
        const auto& sample = sim_state_->all_data[sim_state_->current_index];
        x_batch.push_back(sample[0]);
        y_batch.push_back(sample[1]);
        z_batch.push_back(sample[2]);
        // 将当前样本的三轴值分别追加到对应批次向量

        sim_state_->current_index++;
        if (sim_state_->current_index >= total) {
            sim_state_->current_index = 0; // 循环
            // 到达数据末尾时回零，实现无限循环采样
        }
    }

    // 将参数编码到 source_name，由 DataProcessingFramework 在写 PNG 时同步写 txt
    // 格式: "BEARING_PARAM|csv=...|category=de_normal|start_row=N"
    // （原有注释说明了 source_name 的格式规范）

    // 按 X, Y, Z 顺序拼接
    data.sensor_data.clear();
    data.sensor_data.insert(data.sensor_data.end(), x_batch.begin(), x_batch.end());
    data.sensor_data.insert(data.sensor_data.end(), y_batch.begin(), y_batch.end());
    data.sensor_data.insert(data.sensor_data.end(), z_batch.begin(), z_batch.end());
    // 与 acquire_data() 真实采集路径相同的 X/Y/Z 顺序拼接，保持数据格式一致

    data.metadata.sample_rate = SAMPLE_RATE_HZ;
    // source_name 编码参数，供 DataProcessingFramework 写 PNG 后同步写 txt
    {
        std::string param = "BEARING_PARAM|csv=" + to_relative_path(simulation_data_path_)
            + "|category=de_normal|start_row=" + std::to_string(batch_start_row);
        // "BEARING_PARAM|" 前缀的参数编码，包含：
        //   csv：CSV 文件的相对路径（去除绝对前缀）
        //   category：数据集类别标签（此处固定为 "de_normal"，实际应从文件名推断）
        //   start_row：本批次在 CSV 文件中的起始行号，用于精确溯源
        data.source_name = param;
    }
    return ErrorCode::SUCCESS;
#endif
// 结束 BEARING_VIBRATION_PROJECT 条件编译块

    return ErrorCode::NOT_IMPLEMENTED;
}


#ifdef BEARING_VIBRATION_PROJECT
ErrorCode DataAcquisitionFramework::calibrate_ad4134() {
    if (vibration_acquisition_) {
        return vibration_acquisition_->calibrate_sensor();
        // 委托 BearingVibrationAcquisition 执行传感器校准（当前为桩实现）
    }
    return ErrorCode::NOT_INITIALIZED;
    // vibration_acquisition_ 为空（未初始化）时返回错误
}
#endif
// 结束 calibrate_ad4134 的 BEARING_VIBRATION_PROJECT 条件编译块

// ============================================================================
// 轴承振动采集类实现
// ============================================================================
#ifdef BEARING_VIBRATION_PROJECT

// ── AD4134 引脚定义（BCM编号，已按原理图H1镜像翻转规律核查）──────────
// 翻转规律：H1奇数Pin → 实物偶数Pin(+1)；H1偶数Pin → 实物奇数Pin(-1)
//
//  信号        H1 Pin  实物 Pin  BCM
//  AD_MOSI     Pin20   Pin19     BCM10  （SPI0 MOSI）
//  AD_MISO     Pin22   Pin21     BCM9   （SPI0 MISO）
//  AD_CS       Pin23   Pin24     BCM8   （SPI0 CE0，片选低有效）
//  AD_SCLK     Pin24   Pin23     BCM11  （SPI0 SCLK）
//  AD_ODR      Pin27   Pin28     BCM1   （数据就绪中断，输入）
//  AD_DCLK     Pin28   Pin27     BCM0   （并行数据同步时钟，输出）
//  AD_RESET    Pin30   Pin29     BCM5   （硬件复位，低有效，输出）
//  AD_PDN      Pin31   Pin32     BCM12  （掉电控制，低有效，输出）
//  AD_DOUT0    Pin32   Pin31     BCM6   （X轴数据，输入）
//  AD_DOUT1    Pin34   Pin33     BCM13  （Y轴数据，输入）
//  AD_DOUT2    Pin36   Pin35     BCM19  （Z轴数据，输入）
//
// 注：H1 Pin21 标注 × = NC（未连接），非 AD_CS。
//     AD_CS 在 H1 Pin23 → 实物 Pin24 → BCM8 = SPI0 CE0（标准片选）。
//     使用 /dev/spidev0.0 时内核自动控制 CE0（BCM8），无需软件手动操作。
//
// 以上为原有引脚映射表（保留），H1→实物 Pin 翻转规律：
//   奇数 H1 Pin N → 实物 Pin N+1；偶数 H1 Pin N → 实物 Pin N-1；
//   此镜像规律适用于本项目所用 40Pin 扩展板接口的物理布局

static constexpr int AD4134_CS_PIN       = 8;    // SPI0 CE0，内核自动控制
// BCM8 对应 SPI0 的片选引脚 CE0，由内核 SPI 驱动在 /dev/spidev0.0 操作时自动管理，
// 不需要（也不应该）手动调用 digitalWrite 控制此引脚

static constexpr int AD4134_RESET_PIN    = 5;
// BCM5：AD4134 硬件复位引脚（RESET），低电平有效（拉低复位）；
// 对应 H1 Pin30 → 实物 Pin29

static constexpr int AD4134_PDN_PIN      = 12;
// BCM12：AD4134 掉电控制引脚（PDN，Power Down Negative），低电平使芯片掉电；
// 置高（HIGH）= ADC 正常工作；对应 H1 Pin31 → 实物 Pin32

static constexpr int AD4134_DOUT0_PIN    = 6;
// BCM6：AD4134 通道 0（X 轴）并行数据输出引脚（DOUT0），输入方向；
// 对应 H1 Pin32 → 实物 Pin31

static constexpr int AD4134_DOUT1_PIN    = 13;
// BCM13：AD4134 通道 1（Y 轴）并行数据输出引脚（DOUT1），输入方向；
// 对应 H1 Pin34 → 实物 Pin33

static constexpr int AD4134_DOUT2_PIN    = 19;
// BCM19：AD4134 通道 2（Z 轴）并行数据输出引脚（DOUT2），输入方向；
// 对应 H1 Pin36 → 实物 Pin35

static constexpr int AD4134_ODR_PIN      = 1;
// BCM1：AD4134 数据就绪引脚（ODR，Output Data Ready），输入方向；
// 上升沿表示一个新的采样周期开始（所有通道数据已就绪，可以读取）；
// 对应 H1 Pin27 → 实物 Pin28

static constexpr int AD4134_DCLK_PIN     = 0;
// BCM0：AD4134 并行数据时钟引脚（DCLK），输出方向；
// 主机（树莓派）生成 DCLK 脉冲，AD4134 在每个上升沿将 1 位数据移出到 DOUT 引脚；
// 每帧 24 个 DCLK 脉冲读取 3 通道各 24 位数据；
// 对应 H1 Pin28 → 实物 Pin27

static constexpr int AD4134_DCLK_HALF_US = 0;
// DCLK 半周期延时（微秒），0 表示不插入额外延时（速度最快）；
// 若信号完整性问题导致读取错误，可将此值增大到 1~5 μs

static constexpr float AD4134_VREF  = 4.096f;
// AD4134 参考电压（V）：4.096 V；
// 与 bearing_vibration_config.hpp 中 AD4134Config::Gain::reference_voltage=2.5f 不同，
// 此处使用实际硬件接线的参考电压值（以电路原理图为准）

static constexpr float AD4134_FS24  = 8388608.0f;
// AD4134 24 位满量程值：2^23 = 8,388,608；
// 24 位有符号整数（二补数），正满量程 = 2^23 - 1，
// 换算公式：电压(V) = raw_code × VREF / FS24

BearingVibrationAcquisition::BearingVibrationAcquisition() {}
// 默认构造函数：ad4134_device_ unique_ptr 初始化为空，initialized_=false

BearingVibrationAcquisition::~BearingVibrationAcquisition() { shutdown(); }
// 析构函数：调用 shutdown() 关闭 SPI 连接，
// 避免 spi_fd_ 在未关闭状态下被析构导致文件描述符泄漏

ErrorCode BearingVibrationAcquisition::initialize(int spi_channel, int spi_speed) {
    // 初始化振动采集系统：创建 AD4134Device 并触发 GPIO/SPI/寄存器初始化

    try {
        ad4134_device_ = std::make_unique<AD4134Device>();
        // 在堆上构造 AD4134Device 实例

        auto status = ad4134_device_->initialize(spi_channel, spi_speed);
        // 触发 GPIO 引脚配置 → SPI 设备文件打开 → 寄存器写入序列
        if (status != ErrorCode::SUCCESS) {
            std::cerr << "AD4134设备初始化失败" << std::endl;
            return status;
        }
        initialized_ = true;
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "振动采集初始化异常: " << e.what() << std::endl;
        return ErrorCode::SPI_INIT_FAILED;
        // 捕获所有标准异常（如 bad_alloc），返回 SPI 初始化失败码
    }
}

ErrorCode BearingVibrationAcquisition::shutdown() {
    ad4134_device_.reset();
    // 重置 unique_ptr，触发 AD4134Device 析构函数：
    // AD4134Device::~AD4134Device() 调用 shutdown() → close(spi_fd_)
    initialized_ = false;
    return ErrorCode::SUCCESS;
}

ErrorCode BearingVibrationAcquisition::read_vibration_data(
    std::vector<float>& x_data,
    std::vector<float>& y_data,
    std::vector<float>& z_data,
    int samples) {
    // 读取指定数量的三轴振动加速度样本

    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    return ad4134_device_->read_channels(x_data, y_data, z_data, samples);
    // 委托 AD4134Device::read_channels() 执行实际的 SPI/GPIO 数据读取
}

ErrorCode BearingVibrationAcquisition::calibrate_sensor() {
    return ErrorCode::SUCCESS;
    // 桩实现：传感器校准功能尚未实现（完整实现需采集静态数据计算零偏并写入寄存器）
}

ErrorCode BearingVibrationAcquisition::set_sample_rate(float rate_hz) {
    current_sample_rate_ = rate_hz;
    // 仅更新软件层记录值，不实际修改 AD4134 硬件寄存器；
    // 真实采样率调整需要修改 OSR 寄存器（通过 AD4134Device::set_sample_rate()）
    return ErrorCode::SUCCESS;
}

bool BearingVibrationAcquisition::is_sensor_connected() const {
    return initialized_;
    // 简化判断：以初始化状态标志代替实时 SPI 通信探测
}

float BearingVibrationAcquisition::get_current_sample_rate() const {
    return current_sample_rate_;
    // 返回当前记录的采样率，初始为 SAMPLE_RATE_HZ（50000 Hz）
}

// AD4134Device 实现
BearingVibrationAcquisition::AD4134Device::AD4134Device() {}
// 默认构造函数：spi_fd_=-1（未打开），initialized_=false（来自声明处初始化）

BearingVibrationAcquisition::AD4134Device::~AD4134Device() { shutdown(); }
// 析构函数：调用 shutdown() 关闭 SPI 文件描述符

static inline void ad4134_dclk_delay_half() {
    if (AD4134_DCLK_HALF_US > 0) delayMicroseconds(AD4134_DCLK_HALF_US);
    // 内联工具函数：在 DCLK 半周期插入微秒级延时；
    // AD4134_DCLK_HALF_US=0 时函数体为空操作（编译器会内联优化掉），
    // 不产生任何指令；若需要减慢 DCLK 速率以改善信号完整性，设置非零值
}

static inline int32_t ad4134_sign_extend_24(uint32_t x) {
    // 内联工具函数：对 24 位无符号整数执行符号扩展，转换为 32 位有符号整数
    if (x & 0x800000) return (int32_t)(x | 0xFF000000);
    // bit23（0x800000）为 1 表示负数（24 位二补数的符号位）；
    // 用 0xFF000000 将高 8 位全部置 1（符号扩展），得到 32 位有符号负数
    return (int32_t)x;
    // 正数（bit23=0）直接转型即可，高 8 位自然为 0
}

struct AD4134Frame3 {
    // 本地结构体：存储一个采样周期内三个通道的 24 位有符号 ADC 码
    int32_t ch0;  // 通道 0（X 轴）的符号扩展后的 24 位 ADC 码
    int32_t ch1;  // 通道 1（Y 轴）的符号扩展后的 24 位 ADC 码
    int32_t ch2;  // 通道 2（Z 轴）的符号扩展后的 24 位 ADC 码
};

static AD4134Frame3 ad4134_read_frame_parallel_3ch_24b() {
    // 核心数据采集函数：通过 GPIO bit-banging 从 AD4134 并行读取一帧三通道 24 位数据；
    // 此函数在每次 AD4134 数据就绪时被调用，是整个采集系统的时序关键路径

    while (digitalRead(AD4134_ODR_PIN) == HIGH) {}
    // 等待 ODR（数据就绪）引脚从高电平降为低电平（ODR 低电平表示新数据周期开始）；
    // 忙等待（busy-wait）：此期间 CPU 100% 占用，但延迟最低（无调度抖动）

    while (digitalRead(AD4134_ODR_PIN) == LOW)  {}
    // 等待 ODR 从低电平上升为高电平（上升沿触发数据读取开始）；
    // ODR 上升后，AD4134 已将本采样周期的三通道数据准备在 DOUT 引脚上

    uint32_t a = 0, b = 0, c = 0;
    // 三个通道的原始数据累积变量（无符号 32 位，最终只使用低 24 位）

    for (int i = 0; i < 24; ++i) {
        // 连续产生 24 个 DCLK 脉冲，每脉冲读取三通道各 1 位（MSB 先传）

        digitalWrite(AD4134_DCLK_PIN, HIGH);
        // DCLK 上升沿：AD4134 将当前 bit 移出到 DOUT 引脚（串行移位，MSB 先出）
        ad4134_dclk_delay_half();
        // 等待 DCLK 高电平半周期（确保 DOUT 数据在 DCLK 上升后稳定）

        a = (a << 1) | (digitalRead(AD4134_DOUT0_PIN) & 1);
        // 读取 X 轴数据的当前 bit（DOUT0），左移累积（MSB 先入）
        b = (b << 1) | (digitalRead(AD4134_DOUT1_PIN) & 1);
        // 读取 Y 轴数据的当前 bit（DOUT1）
        c = (c << 1) | (digitalRead(AD4134_DOUT2_PIN) & 1);
        // 读取 Z 轴数据的当前 bit（DOUT2）

        digitalWrite(AD4134_DCLK_PIN, LOW);
        // DCLK 下降沿：准备下一个 bit 的传输
        ad4134_dclk_delay_half();
        // 等待 DCLK 低电平半周期（保持时序对称）
    }
    // 循环结束后，a/b/c 各自包含完整的 24 位 ADC 码（bit23~bit0）

    AD4134Frame3 f;
    f.ch0 = ad4134_sign_extend_24(a);
    // 对 X 轴数据执行符号扩展（24 位→32 位有符号整数）
    f.ch1 = ad4134_sign_extend_24(b);
    // 对 Y 轴数据执行符号扩展
    f.ch2 = ad4134_sign_extend_24(c);
    // 对 Z 轴数据执行符号扩展
    return f;
    // 返回包含三通道有符号 ADC 码的帧结构体
}

ErrorCode BearingVibrationAcquisition::AD4134Device::initialize(int spi_channel, int speed) {
    // 初始化 AD4134 设备：GPIO 配置 → SPI 设备打开 → SPI 参数设置 → 寄存器配置

    auto status = init_gpio_pins();
    if (status != ErrorCode::SUCCESS)
        return status;
    // 优先初始化 GPIO：配置 DOUT/ODR/DCLK/PDN/RESET 引脚方向和初始电平，
    // 并执行硬件复位序列（RESET 低→等待→高）

    // AD_CS 接 BCM8（SPI0 CE0，H1 Pin23→实物Pin24）
    // 使用 /dev/spidev0.0，内核 SPI 驱动自动控制 CE0(BCM8)
    std::string spi_device = "/dev/spidev0." + std::to_string(spi_channel);
    // 构造 SPI 设备文件路径：spi_channel=0 → "/dev/spidev0.0"
    spi_fd_ = open(spi_device.c_str(), O_RDWR);
    // 以读写模式打开 SPI 设备文件
    if (spi_fd_ < 0) {
        perror("open spi");
        // perror() 打印 "open spi: <errno 对应的系统错误描述>"，如 "Permission denied"
        return ErrorCode::SPI_INIT_FAILED;
    }

    // SPI Mode3（CPOL=1 CPHA=1），由内核自动管理 CE0 片选
    uint8_t mode = SPI_MODE_3;
    if (ioctl(spi_fd_, SPI_IOC_WR_MODE, &mode) < 0) {
        // 设置 SPI 工作模式为模式 3（时钟空闲高电平，数据在第二个时钟边沿采样）；
        // AD4134 数据手册要求使用模式 3
        close(spi_fd_);
        return ErrorCode::SPI_INIT_FAILED;
    }

    uint8_t bits = 8;
    if (ioctl(spi_fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        // 设置每次 SPI 传输的数据位宽为 8 位（1 字节）
        close(spi_fd_);
        return ErrorCode::SPI_INIT_FAILED;
    }

    uint32_t speed_hz = speed;
    if (ioctl(spi_fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0) {
        // 设置 SPI 时钟频率（Hz）；与 SPI_SPEED 宏定义的值对应（2 MHz）
        close(spi_fd_);
        return ErrorCode::SPI_INIT_FAILED;
    }

    spi_channel_ = spi_channel;
    spi_speed_   = speed;
    // 保存 SPI 配置参数供后续 read/write_register 使用

    auto cfg = configure_registers();
    if (cfg != ErrorCode::SUCCESS) {
        close(spi_fd_);
        return cfg;
        // 寄存器配置失败时关闭 SPI 设备文件并返回错误
    }

    initialized_ = true;
    return ErrorCode::SUCCESS;
}

ErrorCode BearingVibrationAcquisition::AD4134Device::shutdown() {
    if (spi_fd_ >= 0) {
        close(spi_fd_);
        // 关闭 SPI 设备文件描述符，释放内核资源
        spi_fd_ = -1;
        // 将文件描述符置为无效值（-1），防止重复 close 导致未定义行为
    }
    initialized_ = false;
    return ErrorCode::SUCCESS;
}

ErrorCode BearingVibrationAcquisition::AD4134Device::init_gpio_pins() {
    // 配置 AD4134 所有控制引脚的方向和初始电平，并执行硬件复位序列

    if (wiringPiSetupGpio() == -1)
        return ErrorCode::GPIO_INIT_FAILED;
    // 以 BCM GPIO 编号模式初始化 WiringPi 库（若已初始化则返回 0，不重复初始化）

    // ── 数据输入引脚 ─────────────────────────────────────────
    pinMode(AD4134_DOUT0_PIN, INPUT);   // BCM6  X轴
    // DOUT0：X 轴并行数据输出（从 AD4134 角度是输出，从树莓派角度是输入）
    pinMode(AD4134_DOUT1_PIN, INPUT);   // BCM13 Y轴
    // DOUT1：Y 轴并行数据输出（输入方向）
    pinMode(AD4134_DOUT2_PIN, INPUT);   // BCM19 Z轴
    // DOUT2：Z 轴并行数据输出（输入方向）
    pinMode(AD4134_ODR_PIN,   INPUT);   // BCM1  数据就绪
    // ODR：数据就绪信号（输入方向），上升沿触发数据读取

    // ── 数据时钟输出 ─────────────────────────────────────────
    pinMode(AD4134_DCLK_PIN, OUTPUT);   // BCM0
    // DCLK：并行数据同步时钟（输出方向），由树莓派驱动
    digitalWrite(AD4134_DCLK_PIN, LOW);
    // 初始电平置低，确保 DCLK 空闲时为低电平（与 AD4134 时序要求一致）

    // ── AD_CS = BCM8（SPI0 CE0）由 /dev/spidev0.0 内核驱动自动控制 ──
    // 无需手动 pinMode，SPI open 后内核接管该引脚
    // 原有注释说明：打开 /dev/spidev0.0 后，内核 SPI 驱动自动控制 BCM8 的方向和电平，
    // 若软件手动调用 pinMode(8, OUTPUT)，可能与内核驱动产生冲突

    // ── AD_PDN 掉电控制（BCM12，H1 Pin31→实物Pin32）─────────
    // 低有效，上拉电阻已保持高电平；软件确认置高=使能ADC
    pinMode(AD4134_PDN_PIN, OUTPUT);    // BCM12
    // PDN（Power Down Negative）：低电平使 AD4134 进入掉电节能模式；
    // 原有注释说明：硬件上拉电阻已保证上电后 PDN 为高（ADC 正常工作），
    // 软件此处再次显式置高确认
    digitalWrite(AD4134_PDN_PIN, HIGH); // HIGH = ADC正常工作
    delay(5);
    // 等待 5ms，确保 ADC 从掉电状态恢复到正常工作状态（上电稳定时间）

    // ── AD_RESET 硬件复位（BCM5，H1 Pin30→实物Pin29）────────
    // 低有效：拉低→等待→拉高 完成复位序列
    // 注意：init_gpio_pins 中 RESET_PIN 未执行 pinMode，
    // 可能是因为 ADC 上电时 RESET 已由外部上拉保持高电平，
    // 这里直接驱动 HIGH/LOW 需确保 BCM5 已被配置为输出模式
    delay(10);
    // 等待 10ms 确保 ADC 上电稳定后再执行复位
    digitalWrite(AD4134_RESET_PIN, LOW);
    // 拉低 RESET 引脚，触发 AD4134 内部复位（寄存器恢复出厂默认值）
    delay(10);
    // 保持低电平 10ms（AD4134 要求的最小复位脉冲宽度）
    digitalWrite(AD4134_RESET_PIN, HIGH);
    // 释放 RESET，AD4134 退出复位状态，开始按默认配置工作
    delay(20);
    // 等待 20ms，AD4134 完成内部复位后的初始化序列

    return ErrorCode::SUCCESS;
}

ErrorCode BearingVibrationAcquisition::AD4134Device::configure_registers() {
    // 按 bearing_vibration_config.hpp 中的寄存器默认值配置 AD4134 各功能寄存器；
    // 每次写入后立即回读校验，确认寄存器值已正确写入（抗 SPI 通信干扰）

    ErrorCode status;

    status = write_register(0x00, 0x80);
    // 写 DEVICE_CONFIG 寄存器（地址 0x00），值 0x80；
    // 0x80 通常触发软件复位（bit7=1=SOFT_RESET），使寄存器恢复出厂值
    if (status != ErrorCode::SUCCESS) return status;
    usleep(10000);
    // 软件复位后等待 10ms，确保 AD4134 完成内部复位序列

    status = write_register(0x02, 0x01);
    // 写地址 0x02，值 0x01；
    // 根据 AD4134 数据手册，此寄存器可能为数据包配置（DATA_PACKET_CONFIG），
    // 0x01 启用三通道数据输出格式
    if (status != ErrorCode::SUCCESS) return status;
    {
        uint8_t val;
        status = read_register(0x02, val);
        if (status != ErrorCode::SUCCESS || val != 0x01)
            return ErrorCode::SPI_WRITE_FAILED;
        // 回读校验：若读回值与写入值不一致，说明 SPI 通信存在问题
    }

    status = write_register(0x11, 0x20);
    // 写地址 0x11，值 0x20；
    // 可能为滤波器配置寄存器，0x20 选择特定的数字滤波器类型和 OSR
    if (status != ErrorCode::SUCCESS) return status;
    {
        uint8_t val;
        status = read_register(0x11, val);
        if (status != ErrorCode::SUCCESS || val != 0x20)
            return ErrorCode::SPI_WRITE_FAILED;
    }

    status = write_register(0x12, 0x02);
    // 写地址 0x12，值 0x02；
    // 可能为增益/PGA 配置寄存器
    if (status != ErrorCode::SUCCESS) return status;
    {
        uint8_t val;
        status = read_register(0x12, val);
        if (status != ErrorCode::SUCCESS || (val & 0x03) != 0x02)
            return ErrorCode::SPI_WRITE_FAILED;
        // 掩码校验（& 0x03）：只检查低 2 位，高位可能为只读状态位
    }

    status = write_register(0x1E, 0x2A);
    // 写地址 0x1E，值 0x2A（二进制 00101010）；
    // 可能为模拟前端配置寄存器，配置电源管理或基准电压选择
    if (status != ErrorCode::SUCCESS) return status;
    {
        uint8_t val;
        status = read_register(0x1E, val);
        if (status != ErrorCode::SUCCESS || val != 0x2A)
            return ErrorCode::SPI_WRITE_FAILED;
    }

    status = write_register(0x1D, 0x54);
    // 写地址 0x1D，值 0x54（二进制 01010100）；
    // 可能为通道使能或输出格式配置寄存器
    if (status != ErrorCode::SUCCESS) return status;
    {
        uint8_t val;
        status = read_register(0x1D, val);
        if (status != ErrorCode::SUCCESS || val != 0x54)
            return ErrorCode::SPI_WRITE_FAILED;
    }

    status = write_register(0x13, 0x08);
    // 写地址 0x13，值 0x08；
    // 最后一个配置寄存器，写入后 AD4134 进入工作状态并开始连续转换
    if (status != ErrorCode::SUCCESS) return status;
    {
        uint8_t val;
        status = read_register(0x13, val);
        if (status != ErrorCode::SUCCESS || val != 0x08)
            return ErrorCode::SPI_WRITE_FAILED;
    }

    usleep(20000);
    // 最后等待 20ms，确保 AD4134 完成最终配置并进入稳定的连续转换模式
    return ErrorCode::SUCCESS;
}

ErrorCode BearingVibrationAcquisition::AD4134Device::read_channels(
    std::vector<float>& ch1_data,
    std::vector<float>& ch2_data,
    std::vector<float>& ch3_data,
    int samples) {
    // 循环采集 samples 个三通道振动加速度样本

    if (!initialized_) return ErrorCode::NOT_INITIALIZED;

    ch1_data.clear();
    ch2_data.clear();
    ch3_data.clear();
    ch1_data.reserve(samples);
    ch2_data.reserve(samples);
    ch3_data.reserve(samples);
    // 清空并预分配容量，避免循环中 push_back 的多次内存重分配

    for (int n = 0; n < samples; ++n) {
        AD4134Frame3 f = ad4134_read_frame_parallel_3ch_24b();
        // 等待 ODR 上升沿并通过 DCLK bit-banging 读取一帧三通道 24 位数据

        float v0 = (float)f.ch0 * AD4134_VREF / AD4134_FS24;
        float v1 = (float)f.ch1 * AD4134_VREF / AD4134_FS24;
        float v2 = (float)f.ch2 * AD4134_VREF / AD4134_FS24;
        // 将 24 位有符号 ADC 码转换为实际电压值（单位：V）；
        // 公式：V = raw_code × Vref / (2^23)
        // 例：raw_code = 4194304（正满量程一半）→ V = 4194304 × 4.096 / 8388608 = 2.048 V；
        // 注意：此处输出为电压值（V），而非加速度值（g）；
        // 将电压转换为加速度需要传感器的灵敏度系数（mV/g），由外部处理层完成

        ch1_data.push_back(v0);
        ch2_data.push_back(v1);
        ch3_data.push_back(v2);
        // 将各通道电压值追加到对应输出列表
    }

    return ErrorCode::SUCCESS;
}

ErrorCode BearingVibrationAcquisition::AD4134Device::set_sample_rate(float rate_hz) {
    (void)rate_hz;
    // 消除未使用参数警告
    return ErrorCode::SUCCESS;
    // 桩实现：AD4134 采样率动态调整（修改 OSR 寄存器）尚未实现
}

bool BearingVibrationAcquisition::AD4134Device::is_initialized() const {
    return initialized_;
    // 返回设备初始化状态
}

ErrorCode BearingVibrationAcquisition::AD4134Device::write_register(uint8_t reg, uint8_t value) {
    // 通过 SPI 向 AD4134 指定寄存器写入单字节值

    uint8_t tx[2] = {static_cast<uint8_t>(reg & 0x3F), value};
    // tx[0]：寄存器地址字节，高 2 位（bit7, bit6）清零；
    //   bit7=0 表示写操作（AD4134 协议），bit6=0 为寄存器地址高位（6 位地址空间）；
    //   reg & 0x3F：保留低 6 位地址，高 2 位强制为 0（写命令）
    // tx[1]：待写入的数据字节

    uint8_t rx[2];
    // 接收缓冲区（SPI 全双工同步传输时 rx 会收到从机回应，写操作时可忽略）

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    // 零初始化传输描述结构体，清除所有可选字段（避免使用未初始化内存）

    tr.tx_buf        = (unsigned long)tx;
    // 发送缓冲区地址（转换为 unsigned long 是 spi_ioc_transfer 的接口要求）
    tr.rx_buf        = (unsigned long)rx;
    // 接收缓冲区地址
    tr.len           = 2;
    // 本次传输的字节数（2 字节：1 字节地址 + 1 字节数据）
    tr.speed_hz      = (uint32_t)spi_speed_;
    // 使用配置的 SPI 时钟频率（覆盖 SPI 设备文件的默认频率）
    tr.bits_per_word = 8;
    // 每字 8 位

    // CE0(BCM8) 由内核 SPI 驱动自动控制，无需手动操作
    // 原有注释说明：ioctl SPI_IOC_MESSAGE 执行期间，内核驱动自动拉低 CE0，
    // 传输完成后自动释放 CE0 为高电平

    if (ioctl(spi_fd_, SPI_IOC_MESSAGE(1), &tr) < 0)
        return ErrorCode::SPI_WRITE_FAILED;
        // SPI_IOC_MESSAGE(1) 表示发送 1 条传输描述符；ioctl 返回 < 0 表示失败
    return ErrorCode::SUCCESS;
}

ErrorCode BearingVibrationAcquisition::AD4134Device::read_register(uint8_t reg, uint8_t& value) {
    // 通过 SPI 从 AD4134 指定寄存器读取单字节值

    uint8_t tx[2] = {static_cast<uint8_t>(0x40 | (reg & 0x3F)), 0x00};
    // tx[0]：寄存器地址字节，bit6=1 表示读操作（AD4134 协议：0x40 = 0100 0000）；
    //   (reg & 0x3F) 保留低 6 位寄存器地址；
    //   OR 0x40 将 bit6 置 1（读命令标志）
    // tx[1]：发送哑字节（dummy byte）0x00，在读取时钟期间驱动 MOSI 为低电平

    uint8_t rx[2];
    // rx[0]：读命令字节期间从机的响应（通常无效或为状态字节）
    // rx[1]：从机在哑字节时钟期间返回的寄存器数据

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf        = (unsigned long)tx;
    tr.rx_buf        = (unsigned long)rx;
    tr.len           = 2;
    tr.speed_hz      = (uint32_t)spi_speed_;
    tr.bits_per_word = 8;

    if (ioctl(spi_fd_, SPI_IOC_MESSAGE(1), &tr) < 0)
        return ErrorCode::SPI_READ_FAILED;
        // ioctl 失败（SPI 通信错误）时返回 SPI 读取失败错误码

    value = rx[1];
    // rx[1] 是从机在第 2 个字节时钟期间返回的寄存器内容；
    // AD4134 的 SPI 读操作：第 1 个字节发送读命令，第 2 个字节读取寄存器数据
    return ErrorCode::SUCCESS;
}

#endif // BEARING_VIBRATION_PROJECT
// 结束 BearingVibrationAcquisition 实现的条件编译块

} // namespace MultiProject
// 关闭命名空间 MultiProject