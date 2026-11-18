/**
  ******************************************************************************
  * @file    FrameworkBase.hpp
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
 * @brief   多项目框架基础层 — 公共类型、数据结构与抽象接口头文件
 * ----------------------------------------------------------------------------
 * @details 本文件是整个 MultiProject 框架的基础层头文件，集中定义了供
 *          轴承振动监测（BearingVibrationProject）及未来扩展项目（水质检测、气体检测等）
 *          共享使用的全部基础类型、枚举、数据结构和抽象接口。
 *
 *          文件内容按以下层次组织：
 *
 *          ── 前置声明（Forward Declaration） ──
 *            IDataAcquisition / IDataProcessor / IAIModel /
 *            IDisplayLogger / IExtendedFeatures
 *            解耦头文件间的循环包含依赖
 *
 *          ── 基础类型定义（enum class） ──
 *            ProjectType      — 项目类型枚举（5 个值）
 *            SystemState      — 系统状态机枚举（10 个状态）
 *            TaskType         — AI 任务类型枚举（7 个值）
 *            ErrorCode        — 错误码枚举（7 大分类，共 50+ 个值）
 *            SeverityLevel    — 日志/事件严重程度枚举（6 级）
 *            ProcessingMode   — 数据处理模式枚举（4 个值）
 *
 *          ── 基础数据结构（struct） ──
 *            Timestamp        — 纳秒精度时间戳，含 now()/toString()/toISO8601() 方法
 *            VersionInfo      — 语义化版本号，含 toString()/operator==()/operator<() 方法
 *            PerformanceMetrics — 四维性能指标聚合（时间/频率/资源/成功率）
 *            Event            — 系统事件记录，含线程安全的静态工厂方法 create()
 *
 *          ── 回调函数类型别名（using） ──
 *            ErrorCallback / ProgressCallback / DataCallback /
 *            StatusCallback / EventCallback
 *
 *          ── 抽象框架接口（纯虚类） ──
 *            IFramework       — 单个子框架的生命周期与能力接口（31 个纯虚方法）
 *            IFrameworkManager — 多框架统一管理接口（11 个纯虚方法）
 *            IFrameworkFactory — 各子模块的工厂创建接口（6 个纯虚方法）
 *
 *          ── 工具函数（inline） ──
 *            errorCodeToString() / severityToString() /
 *            projectTypeToString() / systemStateToString() / taskTypeToString()
 *            将枚举值转换为可读字符串，用于日志输出和诊断报告
 *
 * @note    所有定义均位于命名空间 MultiProject 内（前置声明除外）。
 *          本文件为 header-only 设计：工具函数以 inline 修饰，
 *          结构体方法均在类体内定义，无需独立的 .cpp 编译单元。
 * ============================================================================
 */

#ifndef FRAMEWORK_BASE_HPP
// 头文件保护宏：若本头文件已被包含（宏已定义），则跳过全部内容，防止重复包含
#define FRAMEWORK_BASE_HPP
// 定义头文件保护宏，与上方 #ifndef 配对使用

#include <cstdint>
// 引入固定宽度整数类型：uint8_t、uint16_t、uint32_t、uint64_t、int32_t 等；
// 保证在不同平台（32 位/64 位 ARM）上整数位宽完全一致，
// 避免 int/long 宽度因平台而异导致的序列化和比较错误

#include <string>
// 引入 std::string，用于版本名称、日志消息、配置字符串、诊断报告等文本成员

#include <vector>
// 引入 std::vector，用于事件历史列表、框架名称列表、支持项目类型列表等动态数组

#include <memory>
// 引入 std::shared_ptr / std::unique_ptr，
// 供 IFrameworkManager 和 IFrameworkFactory 接口方法的参数和返回值使用，
// 实现对象所有权的安全传递和自动内存管理

#include <functional>
// 引入 std::function，用于定义回调函数类型别名
// （ErrorCallback / ProgressCallback / DataCallback / StatusCallback / EventCallback）；
// std::function 可包装普通函数、lambda 表达式、成员函数指针等可调用对象

#include <chrono>
// 引入 std::chrono 时间库，供 Timestamp::now() 方法获取系统当前时间；
// 使用 std::chrono::system_clock 和 duration_cast 进行纳秒级精度的时间获取

#include <atomic>
// 引入 std::atomic，供 Event::create() 中的静态序列号生成器
// （static std::atomic<uint64_t> next_id）使用，
// 保证多线程并发创建事件时 ID 单调递增且无数据竞争

#include <mutex>
// 引入 std::mutex，为框架层可能的并发访问提供基础同步原语；
// 本文件中未直接声明 mutex 成员，但供包含本头文件的实现层使用

#include <iostream>
// 引入 std::cout / std::cerr，供实现层在调试或错误输出时使用；
// 本头文件本身不直接调用 std::cout，引入以便实现文件无需重复包含

// 前置声明
// 以下五个类在本头文件中仅被 IFrameworkFactory 的方法签名所引用，
// 其完整定义位于各自对应的头文件中（DataAcquisitionFramework.hpp 等）；
// 使用前置声明（而非完整 #include）可打破头文件间的循环依赖，
// 减少编译时头文件递归展开的深度，降低增量编译时间
class IDataAcquisition;
// 前置声明数据采集抽象接口类，由 DataAcquisitionFramework.hpp 完整定义；
// IFrameworkFactory::createDataAcquisition() 的返回类型为 shared_ptr<IDataAcquisition>

class IDataProcessor;
// 前置声明数据处理抽象接口类，由 DataProcessingFramework.hpp 完整定义；
// IFrameworkFactory::createDataProcessor() 的返回类型为 shared_ptr<IDataProcessor>

class IAIModel;
// 前置声明 AI 模型抽象接口类，由 AIModelFramework.hpp 完整定义；
// IFrameworkFactory::createAIModel() 的返回类型为 shared_ptr<IAIModel>

class IDisplayLogger;
// 前置声明显示日志抽象接口类，由 DisplayLoggingFramework.hpp 完整定义；
// IFrameworkFactory::createDisplayLogger() 的返回类型为 shared_ptr<IDisplayLogger>

class IExtendedFeatures;
// 前置声明扩展功能抽象接口类，由 ExtendedFeaturesFramework.hpp 完整定义；
// IFrameworkFactory::createExtendedFeatures() 的返回类型为 shared_ptr<IExtendedFeatures>

namespace MultiProject {
// 开启命名空间 MultiProject，将以下所有类型定义包裹其中，
// 避免与标准库或第三方库中的同名枚举、结构体、类产生符号冲突

// ============================================================================
// 基础类型定义
// ============================================================================
// 分节注释：以下枚举类（enum class）定义本框架层使用的全部强类型分类标识

enum class ProjectType {
    // 强类型枚举：项目类型标识，供 IFrameworkFactory 的工厂方法按类型分派创建具体子模块；
    // enum class 比传统 enum 更安全：枚举值不会隐式转换为整数，
    // 不同枚举类的同名值也不会相互污染命名空间
    UNKNOWN = 0,           // 未知项目类型，作为未初始化或解析失败时的默认值
    FIRE_DETECTION = 1,    // 火焰检测项目（对应 FireDetectionProject）
    BEARING_VIBRATION = 2, // 轴承振动监测项目（对应 BearingVibrationProject）
    WATER_QUALITY = 3,     // 水质检测项目（预留，暂未实现）
    GAS_DETECTION = 4      // 气体检测项目（预留，暂未实现）
};

enum class SystemState {
    // 强类型枚举：系统状态机的全部合法状态；
    // 状态转换须通过 IFramework 的生命周期方法（initialize/shutdown/calibrate 等）触发，
    // 非法状态转换应返回 ErrorCode::INVALID_STATE_TRANSITION
    BOOTING = 0,        // 上电启动中：系统刚启动，硬件自检尚未完成
    INITIALIZING = 1,   // 初始化中：正在执行 initialize()，加载配置和模型
    READY = 2,          // 就绪：初始化完成，等待启动运行指令
    RUNNING = 3,        // 运行中：正常采集、推理、报警流程进行中
    PAUSED = 4,         // 已暂停：运行已临时挂起，可恢复到 RUNNING
    ERROR = 5,          // 错误：检测到不可自动恢复的故障，需人工干预
    CALIBRATING = 6,    // 校准中：正在执行 calibrate()（如传感器零点校准）
    UPDATING = 7,       // 更新中：正在进行固件或模型的在线更新
    SHUTTING_DOWN = 8,  // 关机中：正在执行 shutdown()，安全释放资源
    DIAGNOSING = 9      // 诊断中：正在执行 diagnose() 或 selfTest()，采集系统健康数据
};

enum class TaskType {
    // 强类型枚举：AI 推理任务的类型分类；
    // 供 IAIModel 实现层根据任务类型选择对应的后处理逻辑
    UNKNOWN = 0,           // 未知任务类型，作为默认/未配置状态
    CLASSIFICATION = 1,    // 图像分类：输出各类别 softmax 概率向量（如轴承故障类型）
    REGRESSION = 2,        // 回归：输出连续数值（如 RUL 剩余使用寿命天数）
    DETECTION = 3,         // 目标检测：输出边界框坐标和类别置信度
    SEGMENTATION = 4,      // 语义分割：输出像素级类别掩码（预留，计算量较大，树莓派慎用）
    ANOMALY_DETECTION = 5, // 异常检测：输出异常得分（如自编码器重构误差）
    MULTI_TASK = 6         // 多任务：单模型同时完成分类+检测等多个任务头的推理
};

enum class ErrorCode : int32_t {
    // 强类型枚举：全系统统一的错误码规范，底层类型声明为 int32_t；
    // 采用分段编码策略，按错误来源分为 7 大类别，便于快速定位故障模块：
    //   0       — 通用成功/错误（SUCCESS 及通用错误）
    //   1000+   — 硬件层错误（摄像头/SPI/I2C/GPIO/ADC/传感器）
    //   2000+   — 文件与存储层错误（文件/数据库/配置）
    //   3000+   — AI 模型层错误（加载/推理/版本/优化/量化/剪枝）
    //   4000+   — 网络层错误（连接/超时/认证/SSL/服务端）
    //   5000+   — 系统资源层错误（线程/互斥锁/条件变量/信号量/共享内存）
    //   6000+   — 业务逻辑层错误（状态转换/操作权限/数据校验/业务规则）
    //   9999    — 兜底未知错误

    // ---- 通用错误（0~99） ----
    SUCCESS = 0,                  // 操作成功，无错误
    UNKNOWN_ERROR = 1,            // 未知错误，无法归类时使用
    NOT_INITIALIZED = 2,          // 对象尚未初始化就调用了需要初始化完成的方法
    ALREADY_INITIALIZED = 3,      // 重复调用 initialize()（幂等保护）
    INVALID_PARAMETER = 4,        // 传入参数非法（空指针、越界值、格式错误等）
    OUT_OF_MEMORY = 5,            // 内存分配失败（malloc/new 返回 nullptr）
    TIMEOUT = 6,                  // 操作超时（等待响应超过预设时限）
    NOT_IMPLEMENTED = 7,          // 接口方法尚未实现（纯虚方法子类未覆盖时的占位返回）
    NOT_SUPPORTED = 8,            // 当前平台或配置不支持该操作

    // ---- 硬件层错误（1000~1999） ----
    HARDWARE_INIT_FAILED = 1000,  // 硬件初始化总体失败（细分原因见后续码）
    CAMERA_NOT_FOUND = 1001,      // 摄像头设备未找到（/dev/videoN 不存在）
    CAMERA_OPEN_FAILED = 1002,    // 摄像头设备文件打开失败（权限不足或驱动未加载）
    CAMERA_READ_FAILED = 1003,    // 从摄像头读取帧数据失败（V4L2 read/MMAP 错误）
    SPI_INIT_FAILED = 1004,       // SPI 总线初始化失败（/dev/spidevX.Y 无法打开）
    SPI_READ_FAILED = 1005,       // SPI 读取操作失败（ioctl SPI_IOC_MESSAGE 返回错误）
    SPI_WRITE_FAILED = 1006,      // SPI 写入操作失败
    I2C_INIT_FAILED = 1007,       // I2C 总线初始化失败（/dev/i2c-X 无法打开）
    I2C_READ_FAILED = 1008,       // I2C 读取操作失败（ioctl I2C_RDWR 返回错误）
    I2C_WRITE_FAILED = 1009,      // I2C 写入操作失败（如 OLED 指令写入失败）
    GPIO_INIT_FAILED = 1010,      // GPIO 初始化失败（/dev/gpiochip 无法访问）
    GPIO_READ_FAILED = 1011,      // GPIO 读取电平失败
    GPIO_WRITE_FAILED = 1012,     // GPIO 写入电平失败（如继电器控制引脚写入失败）
    ADC_INIT_FAILED = 1013,       // ADC（如 AD4134）初始化失败
    ADC_READ_FAILED = 1014,       // ADC 读取采样数据失败
    SENSOR_NOT_RESPONDING = 1015, // 传感器无响应（I2C/SPI 应答超时）
    SENSOR_CALIBRATION_FAILED = 1016, // 传感器校准失败（校准值超出合理范围）

    // ---- 文件与存储层错误（2000~2999） ----
    FILE_NOT_FOUND = 2000,            // 文件不存在（模型文件、配置文件路径错误）
    FILE_READ_ERROR = 2001,           // 文件读取失败（磁盘 I/O 错误）
    FILE_WRITE_ERROR = 2002,          // 文件写入失败（磁盘空间不足或只读文件系统）
    FILE_PERMISSION_DENIED = 2003,    // 文件权限不足（进程无读/写权限）
    DATABASE_ERROR = 2004,            // 数据库操作通用错误
    DATABASE_CONNECTION_FAILED = 2005,// 数据库连接失败
    CONFIG_PARSE_ERROR = 2006,        // 配置文件解析失败（JSON 格式错误等）
    CONFIG_VALIDATION_FAILED = 2007,  // 配置内容校验失败（参数值超出合法范围）

    // ---- AI 模型层错误（3000~3999） ----
    MODEL_LOAD_FAILED = 3000,         // 模型文件加载失败（路径错误、格式不兼容）
    MODEL_INFERENCE_FAILED = 3001,    // 推理执行失败（ONNX Runtime 内部错误）
    MODEL_NOT_LOADED = 3002,          // 尚未加载模型就调用推理接口
    MODEL_VERSION_MISMATCH = 3003,    // 模型版本与框架不兼容
    MODEL_OPTIMIZATION_FAILED = 3004, // 模型图优化（Graph Optimization）失败
    MODEL_QUANTIZATION_FAILED = 3005, // 模型量化（INT8/FP16 量化）失败
    MODEL_PRUNE_FAILED = 3006,        // 模型剪枝（Pruning）失败

    // ---- 网络层错误（4000~4999） ----
    NETWORK_ERROR = 4000,             // 网络通用错误
    CONNECTION_FAILED = 4001,         // 连接远程服务器失败（TCP 连接拒绝）
    CONNECTION_TIMEOUT = 4002,        // 连接超时（服务器无响应）
    AUTHENTICATION_FAILED = 4003,     // 身份认证失败（API Key 或密码错误）
    SSL_HANDSHAKE_FAILED = 4004,      // TLS/SSL 握手失败（证书验证错误）
    SERVER_ERROR = 4005,              // 远程服务器返回错误响应（HTTP 5xx 等）

    // ---- 系统资源层错误（5000~5999） ----
    SYSTEM_RESOURCE_EXHAUSTED = 5000,       // 系统资源耗尽（文件描述符、进程数等达到上限）
    THREAD_CREATION_FAILED = 5001,          // 线程创建失败（pthread_create 返回错误）
    THREAD_JOIN_FAILED = 5002,              // 线程 join 失败（线程已分离或 ID 无效）
    MUTEX_INIT_FAILED = 5003,               // 互斥锁初始化失败
    CONDITION_VARIABLE_INIT_FAILED = 5004,  // 条件变量初始化失败
    SEMAPHORE_INIT_FAILED = 5005,           // 信号量初始化失败
    SHARED_MEMORY_INIT_FAILED = 5006,       // 共享内存初始化失败

    // ---- 业务逻辑层错误（6000~6999） ----
    INVALID_STATE_TRANSITION = 6000, // 非法状态转换（如在 RUNNING 状态下再次调用 initialize()）
    OPERATION_NOT_ALLOWED = 6001,    // 当前状态下不允许执行该操作
    DATA_VALIDATION_FAILED = 6002,   // 数据内容校验失败（超出物理量程、NaN/Inf 等）
    BUSINESS_RULE_VIOLATION = 6003,  // 业务规则违反（如报警阈值设置逻辑冲突）

    // ---- 兜底错误 ----
    ERROR_UNKNOWN = 9999    // 未分类的未知错误，switch-case 遗漏时的兜底返回值
};

enum class SeverityLevel {
    // 强类型枚举：日志记录和事件报告的严重程度分级；
    // 级别从低到高，数值越大表示问题越严重，处理优先级越高
    DEBUG = 0,    // 调试级别：详细的内部状态信息，仅在开发调试阶段启用，生产环境通常过滤
    INFO = 1,     // 信息级别：正常运行状态通知（系统启动、任务完成等），用于运行日志审计
    WARNING = 2,  // 警告级别：潜在问题或性能下降，系统仍可继续运行，但需要关注
    ERROR = 3,    // 错误级别：操作失败，当前任务无法完成，系统可自动恢复或降级运行
    CRITICAL = 4, // 严重级别：严重错误，影响核心功能，需要人工干预处理
    FATAL = 5     // 致命级别：不可恢复的系统级错误，通常触发系统安全停机
};

enum class ProcessingMode {
    // 强类型枚举：数据处理模式分类；
    // 供 IDataProcessor 实现层根据模式选择对应的处理策略
    REALTIME = 0,  // 实时模式：每帧数据到达后立即处理，追求最低端到端延迟
    BATCH = 1,     // 批处理模式：积累一批数据后统一处理，吞吐量高但延迟较大
    STREAMING = 2, // 流式模式：持续的滑动窗口处理，平衡延迟和吞吐量
    HYBRID = 3     // 混合模式：根据系统负载动态在实时/批处理之间切换
};

// ============================================================================
// 基础数据结构
// ============================================================================
// 分节注释：以下结构体定义框架层共享的通用数据类型

struct Timestamp {
    // 纳秒精度的时间戳结构体，用于为采集数据、事件记录和性能计时打标记；
    // 采用 UNIX 纪元时间（1970-01-01T00:00:00Z）为起点，
    // 将时间拆分为整秒和纳秒余数两个字段以避免 uint64_t 纳秒值溢出（约 292 年后溢出）

    uint64_t seconds;
    // 自 UNIX 纪元起的完整秒数；
    // uint64_t 可表示约 5840 亿年，无溢出风险

    uint32_t nanoseconds;
    // 当前秒内的纳秒余数，范围 [0, 999,999,999]；
    // uint32_t 最大值 4,294,967,295 > 999,999,999，字段宽度足够

    uint32_t sequence;
    // 同一秒内的序列号，用于区分同一秒内创建的多个时间戳；
    // 默认值 0，若需要单调递增序列可由调用方维护和递增

    Timestamp() : seconds(0), nanoseconds(0), sequence(0) {}
    // 默认构造函数：将全部字段初始化为 0，
    // 表示 UNIX 纪元起点（1970-01-01T00:00:00.000000000Z），
    // 用于标识"尚未赋值"的时间戳

    static Timestamp now() {
        // 静态工厂方法：获取调用瞬间的系统当前时间并封装为 Timestamp 对象
        // 返回值为按值传递的 Timestamp 副本（RVO 优化可消除额外拷贝）
        Timestamp ts;
        // 创建本地 Timestamp 实例，各字段初始为 0
        auto now = std::chrono::system_clock::now();
        // 调用 std::chrono::system_clock::now() 获取当前挂钟时间点（wall-clock time point）；
        // system_clock 是与 UNIX 时间同步的时钟，适合用于时间戳记录；
        // 返回类型为 std::chrono::time_point<std::chrono::system_clock>
        auto duration = now.time_since_epoch();
        // 计算从 UNIX 纪元（epoch）至当前时间点的持续时长；
        // 返回类型为 std::chrono::nanoseconds（或更精细的 duration，取决于平台实现）
        ts.seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        // 将总持续时长截断为秒单位，赋值给 seconds 字段；
        // duration_cast 执行截断（向零取整）而非四舍五入
        ts.nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000;
        // 计算纳秒余数：先将总时长转换为纳秒数，再对 10^9 取模，
        // 得到当前秒内的纳秒偏移量（范围 0~999,999,999）
        return ts;
        // 返回填充完成的 Timestamp 对象；sequence 字段保持默认值 0
    }

    std::string toString() const {
        // 将时间戳格式化为 "秒.纳秒.序列号" 格式的字符串，用于日志文件和调试输出
        char buffer[64];
        // 固定大小的栈上字符缓冲区，64 字节足以容纳最长的时间戳字符串
        snprintf(buffer, sizeof(buffer), "%llu.%09u.%u",
                (unsigned long long)seconds, nanoseconds, sequence);
        // 格式化输出：
        //   %llu        → seconds（强制转换为 unsigned long long 以兼容 printf 格式规范）
        //   %09u        → nanoseconds（零填充至 9 位，保持纳秒部分固定宽度）
        //   %u          → sequence
        // 示例输出："1744000000.123456789.0"
        return std::string(buffer);
        // 将 C 风格字符数组转换为 std::string 返回
    }

    std::string toISO8601() const {
        // 将时间戳格式化为 ISO 8601 标准格式字符串，便于与外部系统（REST API、数据库）交互
        // 输出格式示例："2026-04-08T12:34:56.000000000Z"（UTC 时区，后缀 Z 表示 Zero offset）
        time_t time_sec = static_cast<time_t>(seconds);
        // 将 uint64_t 秒数转换为 time_t（POSIX 标准时间类型），供 gmtime_r 使用；
        // static_cast 比 C 风格强转更安全，且意图更明确
        struct tm tm_time;
        // 声明 tm 结构体用于存储分解后的 UTC 时间（年/月/日/时/分/秒）
        gmtime_r(&time_sec, &tm_time);
        // 将 UNIX 时间戳转换为 UTC 时间的 tm 结构体；
        // gmtime_r 是 gmtime 的线程安全版本（可重入），
        // 将结果写入调用方提供的 tm_time 缓冲区，避免静态全局缓冲区的竞态条件
        char buffer[64];
        // 临时缓冲区，存储 strftime 格式化的日期时间部分
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm_time);
        // 按 ISO 8601 格式格式化日期时间（不含毫秒/纳秒部分）；
        // 示例输出："2026-04-08T12:34:56"
        char result[128];
        // 最终结果缓冲区，存储完整的 ISO 8601 字符串（含纳秒和时区后缀）
        snprintf(result, sizeof(result), "%s.%09uZ", buffer, nanoseconds);
        // 拼接纳秒部分和 UTC 时区标识 'Z'；
        // %09u 确保纳秒值零填充至 9 位
        return std::string(result);
        // 返回完整的 ISO 8601 格式时间戳字符串
    }
};
// 结束 Timestamp 结构体定义

struct VersionInfo {
    // 语义化版本号结构体，遵循 Semantic Versioning 2.0.0 规范（semver.org）；
    // 格式：Major.Minor.Patch.Build，各字段均为无符号 8 位整数（0~255）

    uint8_t major;
    // 主版本号：不向后兼容的 API 变更时递增（Breaking Change）
    uint8_t minor;
    // 次版本号：向后兼容的新功能添加时递增（New Feature）
    uint8_t patch;
    // 修订版本号：向后兼容的 bug 修复时递增（Bug Fix）
    uint8_t build;
    // 构建号：每次 CI/CD 流水线构建时递增，用于区分相同版本的不同构建产物
    std::string name;
    // 版本代号或发行名称（可选），如 "alpha"、"release" 等
    std::string date;
    // 版本发布日期字符串，格式建议为 ISO 8601（"2026-04-08"）
    std::string commit;
    // Git 提交哈希值（可选），用于精确追溯代码版本，
    // 通常取前 7~40 位十六进制字符串

    VersionInfo() : major(1), minor(0), patch(0), build(0) {}
    // 默认构造函数：将版本号初始化为 1.0.0.0，
    // 对应项目第一个正式发布版本；name/date/commit 默认为空字符串

    std::string toString() const {
        // 将版本号格式化为 "Major.Minor.Patch.Build" 格式的字符串
        char buffer[128];
        // 固定大小栈上缓冲区，128 字节足以容纳版本字符串（最长约 15 个字符）
        snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", major, minor, patch, build);
        // 格式化四段版本号，各字段以 '.' 分隔；
        // %u 对应 uint8_t，无符号 8 位整数的合法打印格式
        return std::string(buffer);
        // 返回版本号字符串，如 "1.0.0.0"
    }

    bool operator==(const VersionInfo& other) const {
        // 相等运算符重载：仅比较四个版本号字段，忽略 name/date/commit；
        // 两个 VersionInfo 对象的四段版本号完全一致时返回 true
        return major == other.major && minor == other.minor && patch == other.patch && build == other.build;
        // 短路求值：若 major 不等则立即返回 false，无需比较后续字段
    }

    bool operator<(const VersionInfo& other) const {
        // 小于运算符重载：实现版本号的字典序比较（先比 major，再比 minor，以此类推）；
        // 支持标准库排序算法（std::sort）和有序容器（std::set/std::map）的使用
        if (major != other.major) return major < other.major;
        // 主版本号不同时直接返回大小比较结果；
        // 主版本号更大的版本更新，例如 2.x > 1.x
        if (minor != other.minor) return minor < other.minor;
        // 主版本号相同时比较次版本号
        if (patch != other.patch) return patch < other.patch;
        // 前两段相同时比较修订号
        return build < other.build;
        // 前三段均相同时以构建号作为最终比较依据
    }
};
// 结束 VersionInfo 结构体定义

struct PerformanceMetrics {
    // 性能指标聚合结构体：按四个维度统计系统各处理阶段的性能数据，
    // 供 IFramework::getPerformanceMetrics() 接口返回，用于实时监控和性能分析

    struct TimeMetrics {
        // 子结构体 TimeMetrics：记录各处理阶段的耗时（单位：毫秒）
        double acquisition_time_ms = 0.0;
        // 数据采集阶段耗时：从触发采集指令到获得原始数据所经过的时间（ms）；
        // 对应摄像头帧读取或 ADC 数据传输的时间

        double preprocessing_time_ms = 0.0;
        // 数据预处理阶段耗时：包括缩放、灰度化、归一化等图像预处理操作（ms）

        double inference_time_ms = 0.0;
        // AI 模型推理阶段耗时：ONNX Runtime 执行前向传播的时间（ms）；
        // 树莓派 CPU 推理的关键性能指标，通常为总耗时的最大占比

        double postprocessing_time_ms = 0.0;
        // 后处理阶段耗时：包括 argmax、NMS、阈值判断、特征提取等操作（ms）

        double display_time_ms = 0.0;
        // 显示与日志阶段耗时：OLED 刷新、视频叠加绘制、文件写入等（ms）

        double total_time_ms = 0.0;
        // 端到端总耗时（ms）：从采集开始到显示输出完成的全链路延迟；
        // 理论上 total = acquisition + preprocessing + inference + postprocessing + display，
        // 但在多线程流水线下各阶段并行执行，total 取决于关键路径（瓶颈阶段）
    } time;
    // 声明 time 成员变量

    struct FrequencyMetrics {
        // 子结构体 FrequencyMetrics：记录各处理阶段的吞吐率（帧/秒，fps）
        double acquisition_fps = 0.0;
        // 数据采集阶段的帧率（fps）：传感器实际输出的数据帧频率

        double processing_fps = 0.0;
        // 数据处理（预处理+后处理）阶段的帧率（fps）

        double inference_fps = 0.0;
        // AI 模型推理阶段的帧率（fps）：= 1000.0 / inference_time_ms

        double overall_fps = 0.0;
        // 系统综合帧率（fps）：由整个流水线的最慢阶段（瓶颈）决定；
        // 通常 overall_fps ≤ min(acquisition_fps, inference_fps)
    } frequency;
    // 声明 frequency 成员变量

    struct ResourceMetrics {
        // 子结构体 ResourceMetrics：记录系统资源占用情况
        float cpu_usage_percent = 0.0f;
        // CPU 使用率（%），范围 0~100（单核）或 0~400（树莓派 4B 四核）；
        // 通过读取 /proc/stat 计算得到

        float memory_usage_mb = 0.0f;
        // 物理内存使用量（MB）；
        // 通过读取 /proc/self/status 中的 VmRSS 字段获取

        float gpu_usage_percent = 0.0f;
        // GPU 使用率（%）；树莓派无独立 GPU，此字段通常为 0.0；
        // 预留供 NVIDIA Jetson 等带 GPU 的嵌入式平台使用

        float gpu_memory_mb = 0.0f;
        // GPU 显存占用（MB）；树莓派场景下通常为 0.0

        float disk_usage_percent = 0.0f;
        // 磁盘使用率（%）：日志目录所在分区的使用率；
        // 接近 100% 时应触发日志清理或停止写入，防止根分区填满导致系统崩溃

        float network_usage_kbps = 0.0f;
        // 网络带宽占用（kbps）：远程监控数据上传消耗的带宽；
        // 用于评估上传间隔设置是否合理
    } resources;
    // 声明 resources 成员变量

    struct SuccessMetrics {
        // 子结构体 SuccessMetrics：统计帧处理的成功率
        uint64_t total_frames = 0;
        // 自上次 reset() 以来处理的总帧数（含成功和失败帧）

        uint64_t success_frames = 0;
        // 自上次 reset() 以来处理成功的帧数（推理完成且结果有效）

        uint64_t failed_frames = 0;
        // 自上次 reset() 以来处理失败的帧数（推理错误、数据损坏、超时等）

        float success_rate = 0.0f;
        // 处理成功率：success_frames / total_frames，范围 [0.0, 1.0]；
        // 低成功率（< 0.95）通常表明系统存在性能瓶颈或硬件故障

        void update(bool success) {
            // 更新成功率统计：每处理一帧后调用一次
            total_frames++;
            // 总帧计数加一，uint64_t 溢出需约 5.8×10^11 帧，正常运营场景无溢出风险
            if (success) success_frames++; else failed_frames++;
            // 根据本帧处理结果更新成功或失败计数；
            // 三目条件表达式等价于 if-else，编译器可生成等效代码
            success_rate = static_cast<float>(success_frames) / total_frames;
            // 重新计算成功率；
            // static_cast<float> 将 uint64_t 转换为 float 后执行浮点除法，
            // 避免整数除法截断（若不转型则结果恒为 0 或 1）
        }
    } success;
    // 声明 success 成员变量

    void reset() {
        // 重置所有性能指标为初始零值，通常在开始新一轮性能测试周期前调用
        time = TimeMetrics();
        // 以默认构造的 TimeMetrics 对象替换当前值，各耗时字段归零
        frequency = FrequencyMetrics();
        // 以默认构造的 FrequencyMetrics 对象替换当前值，各帧率字段归零
        resources = ResourceMetrics();
        // 以默认构造的 ResourceMetrics 对象替换当前值，各资源占用字段归零
        success = SuccessMetrics();
        // 以默认构造的 SuccessMetrics 对象替换当前值，帧计数和成功率归零
    }

    std::string toString() const {
        // 将全部性能指标格式化为单行可读字符串，用于日志输出和 OLED/终端状态显示
        char buffer[512];
        // 栈上固定缓冲区，512 字节足以容纳完整的格式化性能字符串
        snprintf(buffer, sizeof(buffer),
                "Time: %.2fms (A:%.2f P:%.2f I:%.2f D:%.2f) | "
                // 总耗时及各阶段耗时（A=采集, P=预处理, I=推理, D=显示），精确到 0.01 ms
                "FPS: %.2f (A:%.2f P:%.2f I:%.2f) | "
                // 综合帧率及各阶段帧率，精确到 0.01 fps
                "Resources: CPU:%.1f%% Mem:%.1fMB GPU:%.1f%% | "
                // CPU 使用率、内存用量、GPU 使用率，精确到 0.1
                "Success: %.1f%% (%llu/%llu)",
                // 成功率（百分比形式）及成功/总帧数
                time.total_time_ms,
                time.acquisition_time_ms,
                time.preprocessing_time_ms,
                time.inference_time_ms,
                time.display_time_ms,
                frequency.overall_fps,
                frequency.acquisition_fps,
                frequency.processing_fps,
                frequency.inference_fps,
                resources.cpu_usage_percent,
                resources.memory_usage_mb,
                resources.gpu_usage_percent,
                success.success_rate * 100.0f,
                // 成功率乘以 100 转换为百分比显示
                (unsigned long long)success.success_frames,
                // uint64_t 强转为 unsigned long long 以匹配 %llu 格式规范
                (unsigned long long)success.total_frames);
        return std::string(buffer);
        // 返回格式化后的性能指标字符串
    }
};
// 结束 PerformanceMetrics 结构体定义

struct Event {
    // 系统事件记录结构体：封装一次系统事件的全部上下文信息，
    // 用于事件历史追溯、故障诊断和报警通知

    uint64_t id;
    // 事件唯一 ID，由 create() 工厂方法通过原子自增生成，全局单调递增；
    // 便于按 ID 排序或查询特定事件

    Timestamp timestamp;
    // 事件发生时刻的时间戳，由 create() 调用 Timestamp::now() 自动填充

    SeverityLevel severity;
    // 事件严重程度，决定日志记录级别和是否触发报警通知

    ErrorCode error_code;
    // 与本事件关联的错误码；对于纯信息性事件（如系统启动），填写 ErrorCode::SUCCESS

    std::string source;
    // 事件来源模块的标识字符串，例如 "FireDetectionProject"、"AD4134Driver" 等；
    // 用于快速定位产生事件的代码模块

    std::string message;
    // 事件的简短描述文本（一行以内），用于日志摘要和 OLED 状态显示

    std::string details;
    // 事件的详细附加信息（可多行），用于完整诊断报告；
    // 默认为空字符串，create() 工厂方法不自动填充此字段，需调用方手动赋值

    Event() : id(0), severity(SeverityLevel::INFO), error_code(ErrorCode::SUCCESS) {}
    // 默认构造函数：id 初始化为 0（未分配 ID），严重程度默认 INFO，错误码默认 SUCCESS；
    // timestamp/source/message/details 字段使用其默认构造（Timestamp 归零，string 为空）

    static Event create(SeverityLevel level, const std::string& src,
                       const std::string& msg, ErrorCode code = ErrorCode::SUCCESS) {
        // 静态工厂方法：创建一个已填充 ID、时间戳、严重程度、来源、消息和错误码的事件对象；
        // 使用工厂方法而非构造函数的优势：可在方法体内维护线程安全的全局状态（如 next_id）
        static std::atomic<uint64_t> next_id{1};
        // 静态局部变量：线程安全的全局事件 ID 计数器，初始值为 1（0 保留给默认构造的未分配状态）；
        // std::atomic<uint64_t> 保证多线程并发调用 create() 时 ID 单调递增且无数据竞争；
        // static 保证计数器在程序整个生命周期内唯一存在（跨多次 create() 调用持久保持）
        Event event;
        // 创建本地 Event 对象，通过默认构造函数初始化各字段
        event.id = next_id++;
        // 以原子后置自增方式为本事件分配唯一 ID；
        // next_id++ 的原子操作等价于"读取当前值→赋给 event.id→计数器+1"，
        // 整个过程不可被其他线程插入（无锁线程安全）
        event.timestamp = Timestamp::now();
        // 记录事件创建瞬间的系统时间作为事件时间戳
        event.severity = level;
        // 设置事件严重程度（由调用方传入）
        event.source = src;
        // 设置事件来源模块名称（由调用方传入）
        event.message = msg;
        // 设置事件简短描述（由调用方传入）
        event.error_code = code;
        // 设置关联错误码，默认值 ErrorCode::SUCCESS（通过默认参数实现）
        return event;
        // 返回填充完成的 Event 对象；details 字段保持空字符串，需调用方按需填充
    }
};
// 结束 Event 结构体定义

using ErrorCallback = std::function<void(ErrorCode, const std::string&)>;
// 错误回调函数类型别名：
// 当框架内发生错误时调用，参数为错误码和错误描述字符串；
// 调用方通过 IFramework::setErrorCallback() 注册具体的处理函数（如日志记录、上报服务端）

using ProgressCallback = std::function<void(int, int, const std::string&)>;
// 进度回调函数类型别名：
// 参数依次为：当前步骤编号、总步骤数、当前步骤描述字符串；
// 用于长时操作（模型加载、系统初始化、批量处理）的进度反馈，
// 供上层 UI 更新进度条或日志输出进度信息

using DataCallback = std::function<void(const void*, size_t)>;
// 原始数据回调函数类型别名：
// 参数依次为：指向数据缓冲区的 void 指针、缓冲区字节数；
// 用于将原始采集数据（如 ADC 字节流、压缩图像帧）透传给外部订阅者，
// 调用方负责根据已知数据格式正确解释 void* 指针

using StatusCallback = std::function<void(const std::string&)>;
// 状态通知回调函数类型别名：
// 参数为状态描述字符串；
// 用于向 UI 层或日志层推送简短的系统状态变更通知，
// 比 EventCallback 更轻量，适合高频状态刷新场景

using EventCallback = std::function<void(const Event&)>;
// 事件回调函数类型别名：
// 参数为 const Event& 引用，避免 Event 对象的拷贝开销；
// 通过 IFramework::registerEventCallback() 注册，
// 每当框架产生新事件（如检测到轴承报警、系统状态切换）时触发

// ============================================================================
// 抽象框架基类
// ============================================================================
// 分节注释：以下三个纯虚类（接口类）定义框架层的核心抽象契约

class IFramework {
// 单个子框架的抽象接口基类（纯接口，Interface）；
// 定义了一个具体监测项目框架（如 BearingVibrationProject、FireDetectionProject）
// 必须实现的全部生命周期、诊断、配置、性能监控和事件管理方法；
// 采用纯虚函数（= 0）强制子类实现，不允许直接实例化本接口
public:
    virtual ~IFramework() = default;
    // 虚析构函数：确保通过基类指针（std::shared_ptr<IFramework>）销毁派生类对象时，
    // 派生类析构函数被正确调用，防止资源泄漏（内存、文件描述符、线程等）；
    // = default 指示编译器自动生成默认析构函数实现

    virtual ErrorCode initialize() = 0;
    // 纯虚方法：执行框架初始化（硬件接口打开、模型加载、配置解析等）；
    // 成功返回 ErrorCode::SUCCESS，失败返回对应的错误码；
    // 初始化完成后系统状态应转换为 SystemState::READY

    virtual ErrorCode shutdown() = 0;
    // 纯虚方法：安全关闭框架（停止工作线程、释放硬件资源、关闭文件描述符）；
    // 应确保调用后所有子线程已退出，资源已完全释放；
    // 成功返回 ErrorCode::SUCCESS

    virtual ErrorCode restart() {
        // 非纯虚方法：提供默认的重启实现（先关闭再初始化），子类可选择覆盖；
        // 若 shutdown() 失败则中止重启流程并返回错误码，不继续执行 initialize()
        ErrorCode err = shutdown();
        // 调用虚方法 shutdown()，执行当前对象的具体关闭实现
        if (err != ErrorCode::SUCCESS) return err;
        // 若关闭过程出现错误，提前返回错误码，避免在未完全清理的状态下重新初始化
        return initialize();
        // 关闭成功后调用虚方法 initialize()，重新执行初始化流程
    }

    virtual std::string getName() const = 0;
    // 纯虚方法：返回本框架实例的唯一名称字符串（如 "BearingVibrationProject"）；
    // 用于 IFrameworkManager 按名称查找和区分多个框架实例

    virtual VersionInfo getVersion() const = 0;
    // 纯虚方法：返回本框架实现的版本信息；
    // 供 IFrameworkManager 进行版本兼容性检查

    virtual SystemState getState() const = 0;
    // 纯虚方法：返回框架当前的系统状态机状态；
    // 调用方可据此判断是否允许执行特定操作

    virtual bool isInitialized() const = 0;
    // 纯虚方法：返回框架是否已完成初始化（即状态 ≥ READY）；
    // 比 getState() == READY 更简洁的二值查询

    virtual bool isRunning() const = 0;
    // 纯虚方法：返回框架是否处于运行状态（SystemState::RUNNING）；
    // 用于快速判断是否正在执行采集/推理/报警流程

    virtual std::string getErrorString() const = 0;
    // 纯虚方法：返回最近一次错误的人类可读描述字符串；
    // 配合 getLastError() 提供完整的错误上下文

    virtual ErrorCode getLastError() const = 0;
    // 纯虚方法：返回最近一次操作产生的错误码；
    // 返回 ErrorCode::SUCCESS 表示无待处理错误

    virtual void clearError() = 0;
    // 纯虚方法：清除已记录的错误状态，使框架从 ERROR 状态恢复到可操作状态；
    // 通常在人工确认错误后调用

    virtual ErrorCode diagnose(int level = 0) = 0;
    // 纯虚方法：执行指定深度级别的系统诊断；
    // level=0 为快速诊断（仅检查关键状态），level 越大诊断越深入但耗时越长；
    // 诊断期间系统状态应切换为 SystemState::DIAGNOSING

    virtual std::string getDiagnosticReport() const = 0;
    // 纯虚方法：返回最近一次 diagnose() 调用生成的诊断报告字符串；
    // 报告应包含各子系统状态、性能指标和建议措施

    virtual ErrorCode selfTest() = 0;
    // 纯虚方法：执行自检程序（传感器连通性、内存、温度等）；
    // 自检通过返回 ErrorCode::SUCCESS，任一项失败返回对应错误码

    virtual ErrorCode calibrate() = 0;
    // 纯虚方法：执行传感器校准程序（如 ADC 零点/增益校准）；
    // 校准期间系统状态应切换为 SystemState::CALIBRATING

    virtual ErrorCode loadConfig(const std::string& config_str) = 0;
    // 纯虚方法：从 JSON 字符串加载并应用配置；
    // 通常在 initialize() 内部调用，也可在运行时热更新配置

    virtual std::string saveConfig() const = 0;
    // 纯虚方法：将当前配置序列化为 JSON 字符串并返回；
    // 用于配置持久化或通过网络接口导出当前配置

    virtual std::string validateConfig() const = 0;
    // 纯虚方法：校验当前配置的合法性，返回校验结果描述字符串；
    // 无错误时返回空字符串或 "OK"，存在问题时返回具体错误描述

    virtual ErrorCode resetConfig() = 0;
    // 纯虚方法：将所有配置项恢复为编译期默认值；
    // 通常用于配置损坏或误操作后的恢复

    virtual PerformanceMetrics getPerformanceMetrics() const = 0;
    // 纯虚方法：返回当前性能指标快照（PerformanceMetrics 结构体副本）；
    // 调用方可据此判断系统是否满足实时性要求

    virtual void resetPerformanceMetrics() = 0;
    // 纯虚方法：清零性能指标计数器，开始新一轮统计周期

    virtual void setPerformanceMonitoring(bool enabled) = 0;
    // 纯虚方法：启用或禁用性能监控数据的实时采集；
    // 禁用时可减少性能统计的 CPU 开销（约 1~3%）

    virtual void registerEventCallback(EventCallback callback) = 0;
    // 纯虚方法：注册事件回调函数；
    // 每当框架产生新事件时调用 callback，参数为 const Event& 引用

    virtual void unregisterEventCallback() = 0;
    // 纯虚方法：注销已注册的事件回调函数；
    // 通常在框架销毁前调用，防止回调指向已释放的对象（悬空回调）

    virtual std::vector<Event> getEventHistory(int max_events = 100) const = 0;
    // 纯虚方法：返回最近 max_events 条事件记录的副本；
    // 默认返回最近 100 条，实现层应按时间倒序排列（最新事件在最前）

    virtual void clearEventHistory() = 0;
    // 纯虚方法：清空事件历史缓冲区；
    // 通常在磁盘持久化事件后调用以释放内存

    virtual void setErrorCallback(ErrorCallback callback) = 0;
    // 纯虚方法：注册错误回调函数；
    // 当框架内发生错误时调用 callback(error_code, error_message)

    virtual void setProgressCallback(ProgressCallback callback) = 0;
    // 纯虚方法：注册进度回调函数；
    // 在长时操作（初始化、模型加载）期间周期性调用以报告进度

    virtual std::string getStatistics() const = 0;
    // 纯虚方法：返回框架统计信息的格式化字符串（如累计处理帧数、报警次数等）；
    // 与 getPerformanceMetrics() 的区别：统计信息侧重业务层面，性能指标侧重系统资源层面

    virtual void resetStatistics() = 0;
    // 纯虚方法：重置业务统计计数器（如报警次数、异常检测次数等）

    virtual uint64_t getUptime() const = 0;
    // 纯虚方法：返回框架自上次 initialize() 完成以来的运行时长（单位：秒）；
    // 用于 SLA（服务级别协议）监控和故障频率分析
};
// 结束 IFramework 接口类定义

class IFrameworkManager {
// 多框架统一管理的抽象接口基类；
// 负责管理多个 IFramework 实例的注册、查找和统一生命周期控制；
// 在本项目中用于同时管理轴承监测和火焰检测两个子框架的协同运行
public:
    virtual ~IFrameworkManager() = default;
    // 虚析构函数：确保通过基类指针销毁派生类时正确调用派生类析构函数

    virtual bool addFramework(std::shared_ptr<IFramework> framework) = 0;
    // 纯虚方法：向管理器注册一个框架实例；
    // 以 shared_ptr 传入，管理器持有共享所有权；
    // 同名框架（getName() 相同）通常拒绝重复注册，返回 false

    virtual bool removeFramework(const std::string& name) = 0;
    // 纯虚方法：按名称从管理器中注销并移除框架实例；
    // 移除前应先调用该框架的 shutdown()；
    // 名称不存在时返回 false

    virtual std::shared_ptr<IFramework> getFramework(const std::string& name) = 0;
    // 纯虚方法：按名称查找并返回框架实例的 shared_ptr；
    // 名称不存在时返回 nullptr；调用方应检查返回值是否为空

    virtual ErrorCode initializeAll() = 0;
    // 纯虚方法：依次调用所有已注册框架的 initialize()；
    // 任一框架初始化失败时，行为由实现层决定（继续初始化其余框架，或中止并返回错误）

    virtual ErrorCode shutdownAll() = 0;
    // 纯虚方法：依次调用所有已注册框架的 shutdown()；
    // 通常按注册顺序的逆序执行，确保依赖关系正确处理

    virtual ErrorCode restartAll() = 0;
    // 纯虚方法：依次重启所有已注册框架（先 shutdownAll 再 initializeAll）

    virtual SystemState getSystemState() const = 0;
    // 纯虚方法：返回所有框架中最严重的系统状态；
    // 若任一框架处于 ERROR 状态，则整体系统状态应反映为 ERROR

    virtual std::string getSystemHealth() const = 0;
    // 纯虚方法：返回系统整体健康状态的可读描述字符串；
    // 汇总各子框架的状态、性能指标和告警信息

    virtual std::string runSystemDiagnostic() = 0;
    // 纯虚方法：对所有已注册框架执行诊断并返回综合诊断报告；
    // 报告应清晰标识各子框架的诊断结果

    virtual std::vector<std::string> getFrameworkList() const = 0;
    // 纯虚方法：返回当前已注册的所有框架名称列表；
    // 用于枚举和调试管理器中的框架集合
};
// 结束 IFrameworkManager 接口类定义

class IFrameworkFactory {
// 框架各子模块的工厂抽象接口基类（工厂方法模式）；
// 负责根据项目类型（ProjectType）创建对应的数据采集、处理、AI 推理、
// 显示日志和扩展功能子模块实例；
// 将对象创建逻辑与使用逻辑解耦，支持在不修改调用代码的前提下替换具体实现
public:
    virtual ~IFrameworkFactory() = default;
    // 虚析构函数：确保工厂对象通过基类指针销毁时正确清理资源

    virtual std::shared_ptr<IDataAcquisition> createDataAcquisition(ProjectType project_type) = 0;
    // 纯虚工厂方法：根据项目类型创建数据采集模块实例；
    // FIRE_DETECTION → USB 摄像头采集模块；
    // BEARING_VIBRATION → AD4134 SPI ADC 采集模块；
    // 返回 shared_ptr，调用方与工厂共享所有权

    virtual std::shared_ptr<IDataProcessor> createDataProcessor(ProjectType project_type) = 0;
    // 纯虚工厂方法：根据项目类型创建数据处理模块实例；
    // FIRE_DETECTION → 图像预处理（灰度化/resize/归一化）；
    // BEARING_VIBRATION → 振动信号处理（时频分析/特征提取）

    virtual std::shared_ptr<IAIModel> createAIModel(ProjectType project_type) = 0;
    // 纯虚工厂方法：根据项目类型创建 AI 推理模块实例；
    // 两个项目均使用 ONNX Runtime，但模型路径和输入规格不同

    virtual std::shared_ptr<IDisplayLogger> createDisplayLogger(ProjectType project_type) = 0;
    // 纯虚工厂方法：根据项目类型创建显示与日志模块实例；
    // 控制 OLED 显示内容格式和本地日志文件策略

    virtual std::shared_ptr<IExtendedFeatures> createExtendedFeatures(ProjectType project_type) = 0;
    // 纯虚工厂方法：根据项目类型创建扩展功能模块实例；
    // 包含报警输出、RUL 预测、远程监控等项目特定扩展功能

    virtual std::vector<ProjectType> getSupportedProjects() const = 0;
    // 纯虚方法：返回当前工厂实现支持创建的项目类型列表；
    // 调用方可据此在运行时查询工厂能力，避免传入不支持的 project_type
};
// 结束 IFrameworkFactory 接口类定义

// 工具函数
// 以下五个内联函数将枚举值转换为可读字符串，
// 声明为 inline 避免在多个编译单元包含本头文件时产生函数定义重复链接错误（ODR 违规）

inline std::string errorCodeToString(ErrorCode code) {
    // 将 ErrorCode 枚举值转换为对应的名称字符串，用于日志输出和诊断报告；
    // 使用 switch-case 而非 std::map 以避免运行时哈希查找开销，编译器可优化为跳转表
    switch (code) {
        case ErrorCode::SUCCESS: return "SUCCESS";
        case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
        case ErrorCode::NOT_INITIALIZED: return "NOT_INITIALIZED";
        case ErrorCode::ALREADY_INITIALIZED: return "ALREADY_INITIALIZED";
        case ErrorCode::INVALID_PARAMETER: return "INVALID_PARAMETER";
        case ErrorCode::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case ErrorCode::TIMEOUT: return "TIMEOUT";
        case ErrorCode::NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case ErrorCode::NOT_SUPPORTED: return "NOT_SUPPORTED";
        case ErrorCode::HARDWARE_INIT_FAILED: return "HARDWARE_INIT_FAILED";
        case ErrorCode::CAMERA_NOT_FOUND: return "CAMERA_NOT_FOUND";
        case ErrorCode::CAMERA_OPEN_FAILED: return "CAMERA_OPEN_FAILED";
        case ErrorCode::CAMERA_READ_FAILED: return "CAMERA_READ_FAILED";
        case ErrorCode::SPI_INIT_FAILED: return "SPI_INIT_FAILED";
        case ErrorCode::SPI_READ_FAILED: return "SPI_READ_FAILED";
        case ErrorCode::SPI_WRITE_FAILED: return "SPI_WRITE_FAILED";
        case ErrorCode::I2C_INIT_FAILED: return "I2C_INIT_FAILED";
        case ErrorCode::I2C_READ_FAILED: return "I2C_READ_FAILED";
        case ErrorCode::I2C_WRITE_FAILED: return "I2C_WRITE_FAILED";
        case ErrorCode::GPIO_INIT_FAILED: return "GPIO_INIT_FAILED";
        case ErrorCode::GPIO_READ_FAILED: return "GPIO_READ_FAILED";
        case ErrorCode::GPIO_WRITE_FAILED: return "GPIO_WRITE_FAILED";
        case ErrorCode::ADC_INIT_FAILED: return "ADC_INIT_FAILED";
        case ErrorCode::ADC_READ_FAILED: return "ADC_READ_FAILED";
        case ErrorCode::SENSOR_NOT_RESPONDING: return "SENSOR_NOT_RESPONDING";
        case ErrorCode::SENSOR_CALIBRATION_FAILED: return "SENSOR_CALIBRATION_FAILED";
        case ErrorCode::FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case ErrorCode::FILE_READ_ERROR: return "FILE_READ_ERROR";
        case ErrorCode::FILE_WRITE_ERROR: return "FILE_WRITE_ERROR";
        case ErrorCode::FILE_PERMISSION_DENIED: return "FILE_PERMISSION_DENIED";
        case ErrorCode::DATABASE_ERROR: return "DATABASE_ERROR";
        case ErrorCode::DATABASE_CONNECTION_FAILED: return "DATABASE_CONNECTION_FAILED";
        case ErrorCode::CONFIG_PARSE_ERROR: return "CONFIG_PARSE_ERROR";
        case ErrorCode::CONFIG_VALIDATION_FAILED: return "CONFIG_VALIDATION_FAILED";
        case ErrorCode::MODEL_LOAD_FAILED: return "MODEL_LOAD_FAILED";
        case ErrorCode::MODEL_INFERENCE_FAILED: return "MODEL_INFERENCE_FAILED";
        case ErrorCode::MODEL_NOT_LOADED: return "MODEL_NOT_LOADED";
        case ErrorCode::MODEL_VERSION_MISMATCH: return "MODEL_VERSION_MISMATCH";
        case ErrorCode::MODEL_OPTIMIZATION_FAILED: return "MODEL_OPTIMIZATION_FAILED";
        case ErrorCode::MODEL_QUANTIZATION_FAILED: return "MODEL_QUANTIZATION_FAILED";
        case ErrorCode::MODEL_PRUNE_FAILED: return "MODEL_PRUNE_FAILED";
        case ErrorCode::NETWORK_ERROR: return "NETWORK_ERROR";
        case ErrorCode::CONNECTION_FAILED: return "CONNECTION_FAILED";
        case ErrorCode::CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";
        case ErrorCode::AUTHENTICATION_FAILED: return "AUTHENTICATION_FAILED";
        case ErrorCode::SSL_HANDSHAKE_FAILED: return "SSL_HANDSHAKE_FAILED";
        case ErrorCode::SERVER_ERROR: return "SERVER_ERROR";
        case ErrorCode::SYSTEM_RESOURCE_EXHAUSTED: return "SYSTEM_RESOURCE_EXHAUSTED";
        case ErrorCode::THREAD_CREATION_FAILED: return "THREAD_CREATION_FAILED";
        case ErrorCode::THREAD_JOIN_FAILED: return "THREAD_JOIN_FAILED";
        case ErrorCode::MUTEX_INIT_FAILED: return "MUTEX_INIT_FAILED";
        case ErrorCode::CONDITION_VARIABLE_INIT_FAILED: return "CONDITION_VARIABLE_INIT_FAILED";
        case ErrorCode::SEMAPHORE_INIT_FAILED: return "SEMAPHORE_INIT_FAILED";
        case ErrorCode::SHARED_MEMORY_INIT_FAILED: return "SHARED_MEMORY_INIT_FAILED";
        case ErrorCode::INVALID_STATE_TRANSITION: return "INVALID_STATE_TRANSITION";
        case ErrorCode::OPERATION_NOT_ALLOWED: return "OPERATION_NOT_ALLOWED";
        case ErrorCode::DATA_VALIDATION_FAILED: return "DATA_VALIDATION_FAILED";
        case ErrorCode::BUSINESS_RULE_VIOLATION: return "BUSINESS_RULE_VIOLATION";
        default: return "UNKNOWN_ERROR_CODE";
        // default 分支处理 ErrorCode::ERROR_UNKNOWN 及任何未来新增但未在此列出的枚举值，
        // 防止编译器警告（switch 未覆盖所有枚举值）并保证运行时不返回空字符串
    }
}

inline std::string severityToString(SeverityLevel level) {
    // 将 SeverityLevel 枚举值转换为对应的名称字符串，用于日志文件格式化输出
    switch (level) {
        case SeverityLevel::DEBUG: return "DEBUG";
        case SeverityLevel::INFO: return "INFO";
        case SeverityLevel::WARNING: return "WARNING";
        case SeverityLevel::ERROR: return "ERROR";
        case SeverityLevel::CRITICAL: return "CRITICAL";
        case SeverityLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
        // 兜底返回，防止枚举值扩展后函数返回空字符串
    }
}

inline std::string projectTypeToString(ProjectType type) {
    // 将 ProjectType 枚举值转换为对应的名称字符串，
    // 用于 IFrameworkManager 的框架列表输出和诊断报告的项目标识
    switch (type) {
        case ProjectType::UNKNOWN: return "UNKNOWN";
        case ProjectType::FIRE_DETECTION: return "FIRE_DETECTION";
        case ProjectType::BEARING_VIBRATION: return "BEARING_VIBRATION";
        case ProjectType::WATER_QUALITY: return "WATER_QUALITY";
        case ProjectType::GAS_DETECTION: return "GAS_DETECTION";
        default: return "UNKNOWN_PROJECT";
        // 兜底返回，防止枚举值扩展后函数返回空字符串
    }
}

inline std::string systemStateToString(SystemState state) {
    // 将 SystemState 枚举值转换为对应的名称字符串，
    // 用于 OLED 状态显示、日志记录和远程监控状态上报
    switch (state) {
        case SystemState::BOOTING: return "BOOTING";
        case SystemState::INITIALIZING: return "INITIALIZING";
        case SystemState::READY: return "READY";
        case SystemState::RUNNING: return "RUNNING";
        case SystemState::PAUSED: return "PAUSED";
        case SystemState::ERROR: return "ERROR";
        case SystemState::CALIBRATING: return "CALIBRATING";
        case SystemState::UPDATING: return "UPDATING";
        case SystemState::SHUTTING_DOWN: return "SHUTTING_DOWN";
        case SystemState::DIAGNOSING: return "DIAGNOSING";
        default: return "UNKNOWN_STATE";
        // 兜底返回，防止枚举值扩展后函数返回空字符串
    }
}

inline std::string taskTypeToString(TaskType type) {
    // 将 TaskType 枚举值转换为对应的名称字符串，
    // 用于 AI 模型推理日志和诊断报告中标识任务类型
    switch (type) {
        case TaskType::UNKNOWN: return "UNKNOWN";
        case TaskType::CLASSIFICATION: return "CLASSIFICATION";
        case TaskType::REGRESSION: return "REGRESSION";
        case TaskType::DETECTION: return "DETECTION";
        case TaskType::SEGMENTATION: return "SEGMENTATION";
        case TaskType::ANOMALY_DETECTION: return "ANOMALY_DETECTION";
        case TaskType::MULTI_TASK: return "MULTI_TASK";
        default: return "UNKNOWN_TASK";
        // 兜底返回，防止枚举值扩展后函数返回空字符串
    }
}

} // namespace MultiProject
// 关闭命名空间 MultiProject

#endif // FRAMEWORK_BASE_HPP
// 结束头文件保护宏条件编译块，与文件开头的 #ifndef FRAMEWORK_BASE_HPP 配对