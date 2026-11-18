/**
  ******************************************************************************
  * @file    SystemConfig.hpp
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
 * @brief   多项目检测系统 — 全局系统配置聚合层头文件
 * ----------------------------------------------------------------------------
 * @details 本文件定义了 MultiProject 系统最顶层的配置体系，
 *          从抽象接口到具体实现、从硬件描述到应用编排，
 *          形成完整的四层配置架构：
 *
 *          ── 抽象层 ──
 *            ISystemConfig       — 系统配置的纯虚接口，规范配置的加载/保存/
 *                                  校验/克隆/合并等生命周期行为
 *
 *          ── 数据层（4 个聚合结构体） ──
 *            GeneralSystemConfig — 系统通用配置（系统信息、运行模式、日志、
 *                                  性能、网络、安全、存储、报警、维护、电源）
 *            HardwareConfig      — 硬件资源描述（CPU/GPU/内存/存储设备/
 *                                  网络接口/USB/GPIO/串口/I2C/SPI/摄像头/传感器）
 *            SoftwareConfig      — 软件运行环境描述（OS/运行时/系统服务/
 *                                  进程配置/环境变量）
 *            ApplicationConfig   — 应用编排配置（子项目注册/子框架注册/
 *                                  数据流水线/AI 模型目录/UI 配置）
 *
 *          ── 实现层 ──
 *            SystemConfig        — 实现 ISystemConfig 接口的具体类，
 *                                  聚合上述四个结构体并提供访问器方法
 *
 * @note    所有结构体均包含带默认值的构造函数，以树莓派 4B +
 *          Raspberry Pi OS（aarch64）为默认硬件平台预填充合理初始值，
 *          运行时可通过 SystemConfig::load() 从 JSON 文件覆盖。
 *
 * @par     命名空间：MultiProject
 * ============================================================================
 */

#ifndef SYSTEM_CONFIG_HPP
// 头文件保护宏：若本头文件已被包含（宏已定义），则跳过全部内容，防止重复包含
#define SYSTEM_CONFIG_HPP
// 定义头文件保护宏，与上方 #ifndef 配对使用

#include "FrameworkBase.hpp"
// 引入框架基础层头文件，获取 MultiProject 命名空间下的全部公共类型：
// ErrorCode、VersionInfo、ProjectType、ProcessingMode、SeverityLevel 等枚举和结构体；
// 本文件中 ISystemConfig 接口、GeneralSystemConfig 等多处直接引用上述类型

#include <string>
// 引入 std::string，用于系统名称、描述、路径、IP 地址、URL、密钥等文本字段

#include <vector>
// 引入 std::vector，用于网络接口列表、GPIO 引脚列表、报警级别列表、
// 服务列表、流水线列表、AI 模型列表等动态大小的序列成员

#include <map>
// 引入 std::map，用于 EnvironmentConfig::variables（环境变量名到值的有序映射）

#include <memory>
// 引入 std::shared_ptr / std::make_shared，
// 供 ISystemConfig::clone() 和 ISystemConfig::merge() 的返回类型使用

namespace MultiProject {
// 开启命名空间 MultiProject，将以下全部类型定义包裹其中，
// 避免与其他模块中同名的 SystemConfig、HardwareConfig 等类型产生符号冲突

class ISystemConfig {
// 系统配置的纯虚接口类（纯接口，Interface）；
// 规范所有具体系统配置实现必须提供的配置生命周期方法；
// 通过接口解耦调用方与具体配置实现，支持在不修改调用代码的前提下替换配置存储方案
public:
    virtual ~ISystemConfig() = default;
    // 虚析构函数：确保通过基类指针（std::shared_ptr<ISystemConfig>）
    // 销毁派生类对象时正确调用派生类析构函数，防止资源泄漏

    virtual ErrorCode load(const std::string& config_str) = 0;
    // 纯虚方法：从 JSON 字符串解析并加载配置到当前对象；
    // config_str 通常来自文件读取或网络下载；
    // 成功返回 ErrorCode::SUCCESS，JSON 格式错误返回 CONFIG_PARSE_ERROR，
    // 内容校验失败返回 CONFIG_VALIDATION_FAILED

    virtual std::string save() const = 0;
    // 纯虚方法：将当前全部配置序列化为 JSON 字符串并返回；
    // 用于配置持久化到文件或通过网络接口导出；
    // const 修饰表明不修改任何成员状态

    virtual std::string validate() const = 0;
    // 纯虚方法：校验当前配置的合法性，返回校验结果描述字符串；
    // 无错误时返回空字符串，存在问题时返回具体错误描述（可为多行文本）

    virtual VersionInfo getVersion() const = 0;
    // 纯虚方法：返回本配置对象对应的版本信息；
    // 用于加载时的版本兼容性检查（防止旧版配置被新版程序误解析）

    virtual std::string getType() const = 0;
    // 纯虚方法：返回配置类型的标识字符串（如 "SystemConfig"）；
    // 用于日志输出和多配置对象管理时的类型区分

    virtual std::string getDescription() const = 0;
    // 纯虚方法：返回本配置对象的人类可读描述字符串；
    // 用于诊断报告和配置管理 UI 的说明性文本

    virtual void resetToDefault() = 0;
    // 纯虚方法：将所有配置项恢复为编译期默认值；
    // 通常用于配置损坏、误操作后的恢复，或首次初始化时建立基准配置

    virtual std::shared_ptr<ISystemConfig> clone() const = 0;
    // 纯虚方法：创建并返回当前配置对象的深拷贝；
    // 返回 shared_ptr 以共享所有权；
    // 用于在修改配置前保存快照，实现配置回滚（Rollback）

    virtual std::shared_ptr<ISystemConfig> merge(const ISystemConfig& other) const = 0;
    // 纯虚方法：将 other 配置合并到当前配置并返回合并结果的新对象；
    // 合并策略由实现层决定（如 other 的非默认值覆盖当前值）；
    // 不修改当前对象（const），而是返回新对象

    virtual std::string toJson(bool pretty = true) const = 0;
    // 纯虚方法：将当前配置序列化为 JSON 字符串；
    // pretty=true 时输出格式化（带缩进）的可读 JSON，
    // pretty=false 时输出紧凑 JSON（节省网络传输带宽）

    virtual ErrorCode fromJson(const std::string& json_str) = 0;
    // 纯虚方法：从 JSON 字符串解析并更新当前配置；
    // 与 toJson() 互为逆操作；
    // 解析成功返回 ErrorCode::SUCCESS，失败返回对应错误码
};
// 结束 ISystemConfig 接口类定义

struct GeneralSystemConfig {
    // 系统通用配置结构体：涵盖与具体检测项目无关的系统级通用参数，
    // 包括系统身份信息、运行模式、日志策略、性能限制、
    // 网络接口、安全策略、存储路径、报警分级、维护计划和电源管理十个子域

    struct SystemInfo {
        // 子结构体 SystemInfo：系统身份与元数据信息
        std::string name;
        // 系统名称，如 "Multi-Project Detection System"；
        // 用于启动日志横幅、OLED 首屏和远程监控设备列表

        std::string id;
        // 系统唯一标识符，如 "MPDS-001"；
        // 用于多设备部署场景下区分不同监测节点

        std::string description;
        // 系统功能的简短文字描述，用于诊断报告和配置管理界面的说明文本

        VersionInfo version;
        // 系统软件版本号（VersionInfo 结构体，含 major/minor/patch/build 字段）；
        // 用于版本兼容性检查和升级记录

        std::string manufacturer;
        // 设备制造商名称，用于设备档案和售后服务追溯

        std::string model;
        // 设备型号字符串，如 "MPDS-1000"，用于硬件规格查询和备件采购

        std::string serial_number;
        // 设备序列号，用于唯一标识单台物理设备，与制造商数据库对应

        std::string installation_date;
        // 设备安装日期字符串（建议 ISO 8601 格式 "YYYY-MM-DD"），
        // 用于计算设备使用年限和维保到期提醒

        std::string location;
        // 设备安装位置描述，如 "Factory Floor"、"Server Room B-03"；
        // 用于物理巡检和故障定位
    } system_info;
    // 声明 system_info 成员变量

    struct OperationMode {
        // 子结构体 OperationMode：控制系统启动行为和异常恢复策略
        ProcessingMode processing_mode = ProcessingMode::REALTIME;
        // 数据处理模式，默认实时模式（REALTIME）；
        // 引用 FrameworkBase.hpp 中的 ProcessingMode 枚举

        bool enable_auto_start = true;
        // 为 true 时系统上电后自动执行 initialize() 并进入 RUNNING 状态，
        // 无需人工干预；适用于无人值守的工业边缘部署场景

        bool enable_auto_recovery = true;
        // 为 true 时检测到可恢复错误后自动执行 restart()；
        // 自动恢复次数受 max_restart_attempts 限制，超限后停止尝试并发送报警

        bool enable_safe_mode = false;
        // 为 true 时以安全模式启动：禁用非关键功能（如远程监控、数据压缩），
        // 优先保障核心检测功能的稳定运行；适用于硬件故障或资源不足时的降级运行

        int max_restart_attempts = 3;
        // 自动恢复的最大重启次数，默认 3 次；
        // 超过此次数后系统停止自动重启，触发 CRITICAL 级别报警通知人工介入

        int restart_delay_seconds = 10;
        // 相邻两次自动重启之间的等待时间（秒），默认 10 秒；
        // 延迟可避免因快速连续重启加剧硬件损耗或掩盖根本原因
    } operation_mode;
    // 声明 operation_mode 成员变量

    struct LoggingConfig {
        // 子结构体 LoggingConfig：全局日志策略配置
        bool enable_logging = true;
        // 总开关：为 false 时完全禁用日志文件写入，降低磁盘 I/O 和 CPU 开销

        std::string log_level = "INFO";
        // 日志过滤级别字符串，与 SeverityLevel 枚举对应：
        // "DEBUG"、"INFO"（默认）、"WARNING"、"ERROR"、"CRITICAL"、"FATAL"；
        // 低于此级别的日志条目将被过滤，不写入文件

        std::string log_directory = "/var/log/multi_project";
        // 日志文件根目录路径；符合 Linux FHS 标准；
        // 系统启动时若目录不存在应自动创建

        int max_log_files = 10;
        // 日志滚动（Rotation）时保留的最大文件数量；
        // 超出后按时间顺序自动删除最旧文件

        int max_log_size_mb = 100;
        // 单个日志文件的最大大小（MB），达到上限后触发滚动；
        // 与 max_log_files 共同控制磁盘总占用上限（最多 10 × 100 MB = 1 GB）

        bool enable_rotation = true;
        // 为 true 时启用日志滚动；设为 false 时单个文件无限增长（调试场景可用）

        std::vector<std::string> log_targets = {"file", "console"};
        // 日志输出目标列表，默认同时输出到文件和控制台（stdout/stderr）；
        // 可选值包括："file"、"console"、"syslog"、"network" 等

        bool enable_structured_logging = true;
        // 为 true 时以结构化格式（JSON）写入日志，便于 ELK/Splunk 等日志分析平台解析；
        // 为 false 时以纯文本格式写入，可读性更好但不易程序化处理
    } logging;
    // 声明 logging 成员变量

    struct PerformanceConfig {
        // 子结构体 PerformanceConfig：全局性能参数与资源限制配置
        int target_fps = 30;
        // 系统目标帧率（fps），与 ProjectDefines.hpp 中 MAX_FPS 宏保持一致；
        // 主循环依据此值计算帧间休眠时间，防止 CPU 空转

        int buffer_size = 10;
        // 线程间 ConcurrentQueue 帧缓冲区深度，与 BUFFER_SIZE 宏保持一致；
        // 较大缓冲区提高稳定性，但增加端到端延时

        int max_threads = 4;
        // 系统允许创建的最大工作线程数，对应树莓派 4B 的四核 Cortex-A72；
        // 超过 CPU 核心数的线程将产生上下文切换开销

        bool enable_performance_monitoring = true;
        // 为 true 时定期采集 CPU/内存/温度等性能指标并更新 PerformanceMetrics；
        // 性能监控本身约消耗 1~3% CPU，可在极端资源受限时关闭

        int monitoring_interval_seconds = 60;
        // 性能指标采集的时间间隔（秒），默认 60 秒；
        // 较短间隔提高监控精度，但增加 I/O 和 CPU 开销

        float cpu_usage_limit = 80.0f;
        // CPU 使用率上限（%），超过时触发性能警告；
        // 80% 为经验阈值，预留 20% 余量应对突发计算需求

        float memory_usage_limit = 80.0f;
        // 物理内存使用率上限（%），超过时触发内存警告；
        // 树莓派 4B（4 GB）超过 80% 时剩余约 800 MB，
        // 需警惕 OOM Kill 导致进程被强制终止

        float temperature_limit = 85.0f;
        // CPU 结温上限（℃），与 SelfCheck::Thresholds::cpu_temperature 保持一致；
        // 超过 85 ℃ 时树莓派 CPU 开始热节流降频，检测帧率将随之下降
    } performance;
    // 声明 performance 成员变量

    struct NetworkConfig {
        // 子结构体 NetworkConfig：系统网络接口与远程监控配置
        struct InterfaceConfig {
            // 单个网络接口的配置描述
            std::string name;
            // 接口名称，如 "eth0"（有线）、"wlan0"（无线）

            std::string type;
            // 接口类型字符串，如 "ethernet"、"wifi"

            std::string ip_address;
            // 静态 IP 地址（dhcp_enabled=false 时生效），如 "192.168.1.50"

            std::string netmask;
            // 子网掩码，如 "255.255.255.0"，与 ip_address 配合确定网段

            std::string gateway;
            // 默认网关地址，用于路由跨网段数据包

            std::string dns1;
            // 首选 DNS 服务器地址

            std::string dns2;
            // 备选 DNS 服务器地址

            bool dhcp_enabled = true;
            // 为 true 时通过 DHCP 自动获取 IP 地址；
            // 工业部署建议设为 false 并手动配置静态 IP，
            // 避免 DHCP 地址变化导致上位机连接中断
        };
        std::vector<InterfaceConfig> interfaces;
        // 系统所有网络接口的配置列表；
        // 树莓派 4B 通常含 eth0（千兆以太网）和 wlan0（WiFi）两个接口

        bool enable_remote_access = false;
        // 为 true 时启用远程数据上传和监控访问；默认关闭

        std::string remote_server_url;
        // 远程监控服务器的 URL，如 "http://192.168.1.100"；
        // enable_remote_access=true 时须填写有效地址

        int remote_port = 8080;
        // 远程监控服务器的 HTTP 端口，与各项目配置中的 server_port 保持一致

        std::string remote_username;
        // 远程服务器的认证用户名（enable_authentication=true 时使用）

        std::string remote_password;
        // 远程服务器的认证密码；
        // 生产环境建议使用加密存储或环境变量传入，避免明文写入配置文件

        bool enable_ssl = false;
        // 为 true 时使用 HTTPS/TLS 加密传输，防止数据在网络传输过程中被截获；
        // 需配置服务器证书，否则 SSL 握手失败

        int heartbeat_interval = 30;
        // 向远程服务器发送心跳包的时间间隔（秒），与各项目配置中的值保持一致

        int timeout_seconds = 30;
        // 远程通信的超时时间（秒）；超时后连接视为失败，记录 CONNECTION_TIMEOUT 错误
    } network;
    // 声明 network 成员变量

    struct SecurityConfig {
        // 子结构体 SecurityConfig：系统访问控制与数据安全配置
        bool enable_authentication = false;
        // 为 true 时启用用户身份认证（本地或远程访问均需验证），默认关闭；
        // 工业内网环境通常依赖网络隔离而非应用层认证

        std::string admin_username;
        // 管理员用户名，enable_authentication=true 时用于访问控制

        std::string admin_password;
        // 管理员密码（明文），建议生产环境改用哈希存储（如 SHA-256+Salt）

        bool enable_encryption = false;
        // 为 true 时对日志文件和配置文件执行加密存储，防止数据泄露

        std::string encryption_key;
        // 对称加密密钥字符串；enable_encryption=true 时使用；
        // 应通过安全密钥管理机制注入，不应硬编码在配置文件中

        bool enable_audit_log = false;
        // 为 true 时记录所有管理操作（配置修改、手动报警确认等）的审计日志；
        // 用于合规审查和安全事件追溯

        bool enable_firewall = false;
        // 为 true 时激活基于 iptables/nftables 的软件防火墙规则；
        // 配合 allowed_ips 白名单限制外部访问来源

        std::vector<std::string> allowed_ips;
        // IP 地址白名单列表，enable_firewall=true 时仅允许列表内的 IP 访问；
        // 空列表通常表示不限制（需结合防火墙规则确认语义）

        bool enable_auto_lock = false;
        // 为 true 时在 auto_lock_timeout 秒无操作后自动锁定系统访问权限

        int auto_lock_timeout = 300;
        // 自动锁定超时时间（秒），默认 300 秒（5 分钟）；
        // enable_auto_lock=false 时此字段不生效
    } security;
    // 声明 security 成员变量

    struct StorageConfig {
        // 子结构体 StorageConfig：系统数据、配置和模型文件的存储路径及策略
        std::string data_directory = "/var/data/multi_project";
        // 系统运行数据（采集原始数据、处理结果、特征文件）的根目录；
        // 符合 Linux FHS 的 /var/data 约定

        std::string config_directory = "/etc/multi_project";
        // 系统配置文件目录；符合 Linux FHS 的 /etc 约定；
        // 各子项目的 JSON 配置文件（fire_detection.json 等）存放于此

        std::string model_directory = "/opt/models/multi_project";
        // ONNX 模型文件目录；符合 Linux FHS 的 /opt 约定（第三方软件包）；
        // 注意：与 ProjectDefines.hpp 中 MODEL_PATH 宏的 "../models/..." 相对路径不同，
        // 两者的参照基准目录不同（此处为绝对路径，宏为相对于可执行文件的路径）

        int max_storage_days = 90;
        // 历史数据最大保留天数，超过 90 天的数据将被自动清理

        bool enable_compression = true;
        // 为 true 时对日志和数据文件执行 gzip 压缩后存储，
        // 通常可节省 60~80% 的磁盘空间

        std::string compression_level = "medium";
        // 压缩级别字符串，可选 "fast"（压缩率低，速度快）、
        // "medium"（均衡，默认）、"best"（压缩率高，CPU 开销大）

        bool enable_backup = true;
        // 为 true 时启用定期配置和数据备份功能

        int backup_interval_hours = 24;
        // 备份执行间隔（小时），默认每 24 小时执行一次全量备份

        std::string backup_directory = "/var/backup/multi_project";
        // 备份文件存放目录；应与主数据目录位于不同磁盘分区，
        // 防止主存储故障时备份文件同时丢失

        int max_backup_files = 30;
        // 保留的最大备份文件数量，超出后自动删除最旧备份；
        // 配合 backup_interval_hours=24，约保留最近 30 天的备份
    } storage;
    // 声明 storage 成员变量

    struct AlarmConfig {
        // 子结构体 AlarmConfig：报警分级定义与通知策略配置
        struct AlarmLevel {
            // 单个报警级别的属性描述
            std::string name;
            // 报警级别名称字符串，如 "INFO"、"WARNING"、"ERROR"、"CRITICAL"

            int priority;
            // 报警优先级数值（0 为最低），数值越大优先级越高；
            // 用于多个报警同时触发时的处理顺序排队

            std::string color;
            // 报警对应的 UI 显示颜色字符串（如 "green"、"yellow"、"orange"、"red"）；
            // 用于 OLED 状态显示和 Web 监控界面的视觉区分

            bool require_acknowledge;
            // 为 true 时此级别报警触发后须人工确认（Acknowledge）才能复位；
            // 通常 ERROR 及以上级别需要人工确认，INFO/WARNING 可自动复位

            int auto_reset_seconds;
            // 报警触发后自动复位的等待时间（秒）；
            // 0 表示不自动复位（必须人工确认），非零值表示等待指定秒数后自动清除
        };
        std::vector<AlarmLevel> alarm_levels;
        // 系统支持的全部报警级别列表，由构造函数预填充四个标准级别

        bool enable_visual_alarm = true;
        // 为 true 时通过 GPIO 驱动 LED 指示灯提供本地视觉报警

        bool enable_audible_alarm = true;
        // 为 true 时通过 GPIO 驱动蜂鸣器提供本地声音报警

        bool enable_sms_alarm = false;
        // 为 true 时通过短信网关向 alarm_recipients 列表发送报警通知；
        // 默认关闭，需配置短信服务商 API 密钥后启用

        bool enable_email_alarm = false;
        // 为 true 时通过 SMTP 向 alarm_recipients 列表发送邮件报警通知；
        // 默认关闭，需配置邮件服务器信息后启用

        std::vector<std::string> alarm_recipients;
        // 报警通知接收人列表，存储手机号或邮件地址字符串；
        // 与 enable_sms_alarm / enable_email_alarm 配合使用

        int alarm_delay_seconds = 0;
        // 报警触发到实际发出通知的延迟时间（秒），默认 0（立即发出）；
        // 适当延迟可过滤短暂瞬态干扰，减少误报次数

        int repeat_interval_seconds = 300;
        // 持续性报警的重复通知间隔（秒），默认 300 秒（5 分钟）；
        // 防止同一报警在持续存在期间不断发送通知骚扰运维人员
    } alarm;
    // 声明 alarm 成员变量

    struct MaintenanceConfig {
        // 子结构体 MaintenanceConfig：系统自动维护任务调度配置
        bool enable_auto_maintenance = true;
        // 为 true 时系统按 schedules 列表中的 cron 表达式自动执行维护任务

        struct MaintenanceSchedule {
            // 单个维护任务的调度描述
            std::string task;
            // 维护任务名称，如 "DATA_CLEANUP"、"SYSTEM_CHECK"、"BACKUP"

            std::string schedule;
            // cron 表达式（5 字段格式：分 时 日 月 周），例如：
            // "0 2 * * *" 表示每天凌晨 2:00 执行

            bool enabled;
            // 为 true 时本任务参与调度；设为 false 可临时禁用单个任务
        };
        std::vector<MaintenanceSchedule> schedules;
        // 维护任务调度列表，由构造函数预填充三个默认任务

        bool enable_self_diagnostic = true;
        // 为 true 时系统按 diagnostic_interval_minutes 周期自动执行自检

        int diagnostic_interval_minutes = 60;
        // 自检执行间隔（分钟），默认每 60 分钟执行一次

        bool enable_auto_update = false;
        // 为 true 时系统按 update_check_interval_hours 周期检查并自动下载安装更新；
        // 工业生产环境建议关闭自动更新，改为人工审核后手动升级

        std::string update_server_url;
        // 固件/软件更新服务器的 URL；enable_auto_update=true 时使用

        int update_check_interval_hours = 24;
        // 检查更新的时间间隔（小时），默认每 24 小时检查一次
    } maintenance;
    // 声明 maintenance 成员变量

    struct PowerManagementConfig {
        // 子结构体 PowerManagementConfig：电源管理与节能策略配置
        bool enable_power_saving = true;
        // 为 true 时允许系统根据负载自动切换电源模式；
        // 与 GeneralSystemConfig 相对，此处为更细粒度的 CPU/GPU 频率调节配置

        struct PowerMode {
            // 单个电源工作模式的参数描述
            std::string name;
            // 模式名称字符串，如 "HIGH_PERFORMANCE"、"BALANCED"、
            // "POWER_SAVING"、"SLEEP"

            int cpu_frequency_mhz;
            // 该模式下 CPU 的目标运行频率（MHz）；
            // 通过 Linux cpufreq 子系统（/sys/devices/system/cpu/cpu0/cpufreq/）设置

            int gpu_frequency_mhz;
            // 该模式下 GPU（VideoCore VI）的目标运行频率（MHz）；
            // 通过 /boot/config.txt 的 core_freq 参数或运行时 vcgencmd 命令设置

            float voltage;
            // 该模式下 CPU 的核心电压（V）；
            // 低电压可降低功耗，但须在稳定性边界内（树莓派 4B 最低约 0.85 V）

            bool enabled;
            // 为 true 时本模式可被自适应调度选用；
            // SLEEP 模式通常仅在 enable_sleep_mode=true 时开放
        };
        std::vector<PowerMode> power_modes;
        // 系统支持的全部电源模式列表，由构造函数预填充四个标准模式

        int sleep_timeout_minutes = 30;
        // 连续空闲超过此时长（分钟）后进入睡眠模式，默认 30 分钟；
        

        bool enable_auto_wakeup = false;
        // 为 true 时系统在 wakeup_schedule 指定的时刻自动从睡眠模式唤醒

        std::string wakeup_schedule;
        // 自动唤醒的 cron 表达式，enable_auto_wakeup=true 时使用；
        // 例如 "0 6 * * *" 表示每天早上 6:00 自动唤醒

        bool enable_battery_monitoring = false;
        // 为 true 时启用电池电量监测（适用于 UPS 或移动供电场景）；
        // 树莓派通常使用 5V 市电供电，默认关闭

        float low_battery_threshold = 20.0f;
        // 低电量警告阈值（%），低于此值触发节电模式切换

        float critical_battery_threshold = 10.0f;
        // 临界低电量阈值（%），低于此值触发安全关机以防止数据损坏
    } power_management;
    // 声明 power_management 成员变量

    GeneralSystemConfig() {
        // 构造函数：为 system_info、alarm.alarm_levels、
        // power_management.power_modes 和 maintenance.schedules
        // 填充合理的默认初始值

        system_info.name = "Multi-Project Detection System";
        // 系统名称默认值，反映多项目通用框架的定位
        system_info.id = "MPDS-001";
        // 系统 ID 默认值，部署多台设备时应将此字段更新为唯一值
        system_info.description = "Multi-Project Industrial Detection System";
        // 系统功能描述
        system_info.version = VersionInfo{2, 0, 0, 1};
        // 系统版本默认值 2.0.0.1，使用聚合初始化
        system_info.manufacturer = "Your Company";
        // 制造商名称占位符，实际部署前应替换为真实公司名称
        system_info.model = "MPDS-1000";
        // 设备型号占位符
        system_info.serial_number = "SN001";
        // 序列号占位符，批量生产时应由制造流程注入唯一序列号
        system_info.installation_date = "2024-01-01";
        // 安装日期占位符，部署时应更新为实际安装日期
        system_info.location = "Factory Floor";
        // 安装位置默认描述

        alarm.alarm_levels = {
            {"INFO",     0, "green",  false, 60},
            // INFO 级别：优先级 0，绿色，无需确认，60 秒后自动复位；
            // 对应系统正常操作通知（如启动完成、任务成功）
            {"WARNING",  1, "yellow", false, 300},
            // WARNING 级别：优先级 1，黄色，无需确认，300 秒后自动复位；
            // 对应轻微异常（振动略高、帧率下降等）
            {"ERROR",    2, "orange", true,  600},
            // ERROR 级别：优先级 2，橙色，需人工确认，600 秒后自动复位；
            // 对应需要关注但系统可继续运行的故障
            {"CRITICAL", 3, "red",    true,  0}
            // CRITICAL 级别：优先级 3，红色，需人工确认，auto_reset_seconds=0（不自动复位）；
            // 对应需要立即人工介入的严重故障（如火焰检测到、轴承临界故障）
        };

        power_management.power_modes = {
            {"HIGH_PERFORMANCE", 1500, 500, 1.35f, true},
            // 高性能模式：CPU 1500 MHz、GPU 500 MHz、电压 1.35 V；
            // 对应树莓派 4B 的最高运行频率，在报警状态时使用
            {"BALANCED",         1000, 300, 1.20f, true},
            // 均衡模式：CPU 1000 MHz、GPU 300 MHz、电压 1.20 V；
            // 日常正常监测时的默认电源模式
            {"POWER_SAVING",      600, 200, 1.05f, true},
            // 节能模式：CPU 600 MHz、GPU 200 MHz、电压 1.05 V；
            // 空闲或低负载时降频节能
            {"SLEEP",             200, 100, 0.90f, false}
            // 睡眠模式：CPU 200 MHz、GPU 100 MHz、电压 0.90 V；
            // enabled=false 表示该模式默认不参与自适应调度
        };

        maintenance.schedules = {
            {"DATA_CLEANUP", "0 2 * * *", true},
            // 数据清理任务：每天凌晨 2:00 执行，删除超过 max_storage_days 的旧数据
            {"SYSTEM_CHECK", "0 * * * *", true},
            // 系统自检任务：每小时整点执行一次系统健康检查
            {"BACKUP",       "0 3 * * *", true}
            // 数据备份任务：每天凌晨 3:00 执行，避免与数据清理（2:00）同时运行
        };
    }
};
// 结束 GeneralSystemConfig 结构体定义

struct HardwareConfig {
    // 硬件资源描述结构体：以树莓派 4B 为默认目标平台，
    // 完整描述系统运行所依赖的全部硬件组件规格和配置

    struct CPUConfig {
        // 子结构体 CPUConfig：CPU 处理器配置与热保护参数
        std::string architecture;
        // CPU 指令集架构字符串，如 "ARMv8"（AArch64）；
        // 用于确认编译目标架构与运行平台一致

        int cores;
        // CPU 物理核心数，树莓派 4B 为 4 核（Cortex-A72）；
        // 用于确定线程池大小和并行度上限

        int frequency_mhz;
        // CPU 额定运行频率（MHz），树莓派 4B 默认 1500 MHz；
        // 实际频率可能因热节流降至 600 MHz

        bool enable_turbo_boost = true;
        // 为 true 时允许在散热条件满足时超频（Turbo Boost）；
        // 树莓派的超频通过 /boot/config.txt 的 arm_freq 参数实现

        int thermal_threshold = 85;
        // CPU 热节流触发温度阈值（℃），超过后内核自动降频；
        // 与 PerformanceConfig::temperature_limit 保持一致
    } cpu;
    // 声明 cpu 成员变量

    struct GPUConfig {
        // 子结构体 GPUConfig：GPU 图形处理器配置参数
        std::string model;
        // GPU 型号字符串，树莓派 4B 使用 "VideoCore VI"

        int memory_mb;
        // GPU 共享内存大小（MB），树莓派 4B 默认 128 MB；
        // 通过 /boot/config.txt 的 gpu_mem 参数配置，范围 64~512 MB

        int frequency_mhz;
        // GPU 核心频率（MHz），树莓派 4B 默认 500 MHz

        bool enable_cuda = false;
        // 为 true 时尝试启用 CUDA 加速（树莓派无 NVIDIA GPU，此字段恒为 false）；
        // 保留字段供 Jetson 等平台使用

        bool enable_opengl = true;
        // 为 true 时启用 OpenGL ES 硬件加速（VideoCore VI 支持 OpenGL ES 3.1）；
        // 用于视频显示叠加绘制（边界框、置信度标注等）
    } gpu;
    // 声明 gpu 成员变量

    struct MemoryConfig {
        // 子结构体 MemoryConfig：物理内存与 Swap 配置
        int total_mb;
        // 系统物理内存总量（MB），树莓派 4B 最大 4096 MB（4 GB）

        int swap_mb;
        // Swap 交换分区大小（MB），树莓派默认 100 MB；
        // 内存不足时内核将部分内存页换出至 Swap，但 SD 卡 I/O 速度远低于内存，
        // 频繁 Swap 会导致系统响应迟缓

        bool enable_memory_compression = false;
        // 为 true 时启用 zram（内存压缩）以增加有效可用内存；
        // 在 CPU 资源充裕而内存紧张时有效，否则压缩/解压缩开销得不偿失

        int memory_threshold_percent = 80;
        // 内存使用率告警阈值（%），与 PerformanceConfig::memory_usage_limit 保持一致
    } memory;
    // 声明 memory 成员变量

    struct StorageDeviceConfig {
        // 单个存储设备的规格描述
        std::string device;
        // 设备文件路径，如 "/dev/mmcblk0p1"（SD 卡分区）或 "/dev/sda1"（USB 存储）

        std::string type;
        // 存储介质类型字符串，如 "SD Card"、"USB Flash"、"SSD"

        std::string filesystem;
        // 文件系统类型字符串，如 "ext4"（Linux 标准）、"FAT32"（兼容性好）

        int capacity_gb;
        // 存储设备容量（GB）

        bool read_only = false;
        // 为 true 时以只读方式挂载（防止意外写入），默认可读写
    };
    std::vector<StorageDeviceConfig> storage_devices;
    // 系统所有存储设备的描述列表，由构造函数预填充 SD 卡默认配置

    struct NetworkInterfaceConfig {
        // 单个网络硬件接口的规格描述（区别于 GeneralSystemConfig 中的软件层接口配置）
        std::string name;
        // 接口标识名称，如 "eth0"、"wlan0"

        std::string mac_address;
        // 网卡 MAC 地址字符串（格式 "XX:XX:XX:XX:XX:XX"），全球唯一

        std::string type;
        // 接口硬件类型，如 "eth"（以太网）、"wifi"（无线）

        int speed_mbps;
        // 接口额定传输速率（Mbps），树莓派 4B 的 eth0 为千兆（1000 Mbps）

        bool promiscuous_mode = false;
        // 为 true 时网卡进入混杂模式，接收所有经过的网络帧（含非本机 MAC 的帧）；
        // 用于网络抓包调试，生产环境应关闭
    };
    std::vector<NetworkInterfaceConfig> network_interfaces;
    // 系统所有网络硬件接口的描述列表

    struct USBConfig {
        // 子结构体 USBConfig：USB 控制器配置
        bool enable_usb = true;
        // 为 true 时启用 USB 控制器，挂载 USB 摄像头等外设时须保持启用

        int version = 3;
        // USB 协议版本号，树莓派 4B 支持 USB 3.0（SuperSpeed，5 Gbps）；
        // USB 摄像头通常使用 USB 2.0，可在 USB 3.0 端口向下兼容

        int ports = 4;
        // USB 物理端口数量，树莓派 4B 有 2 个 USB 3.0 和 2 个 USB 2.0，共 4 个

        bool enable_power_delivery = false;
        // 为 true 时启用 USB Power Delivery（USB PD）功能，
        // 允许通过 USB 端口向外部设备供电

        float power_limit_watts = 2.5f;
        // 单个 USB 端口的最大供电功率（W），默认 2.5 W（标准 USB 2.0 规范 500 mA × 5 V）；
        // 树莓派官方建议所有 USB 设备总功耗不超过 1.2 A
    } usb;
    // 声明 usb 成员变量

    struct GPIOConfig {
        // 子结构体 GPIOConfig：GPIO 引脚配置
        struct PinConfig {
            // 单个 GPIO 引脚的配置描述
            int number;
            // BCM GPIO 编号（BCM 模式，非物理引脚号）

            std::string name;
            // 引脚功能名称字符串，如 "SDA1"（I2C 数据）、"GPIO17"（继电器控制）

            std::string direction;
            // 引脚方向："in"（输入）或 "out"（输出）

            std::string pull;
            // 引脚内部上下拉配置："up"（上拉）、"down"（下拉）、"none"（浮空）

            int initial_value = 0;
            // 输出引脚的初始电平，0 = 低电平（Low），1 = 高电平（High）
        };
        std::vector<PinConfig> pins;
        // 系统使用的全部 GPIO 引脚配置列表，由构造函数预填充关键引脚

        std::string mode = "bcm";
        // GPIO 编号模式："bcm"（BCM 编号，推荐）或 "board"（物理引脚编号）；
        // BCM 模式与芯片数据手册一致，是 Linux 下 GPIO 操作的标准

        bool enable_pwm = false;
        // 为 true 时启用硬件 PWM（树莓派 BCM2711 提供 2 路硬件 PWM：GPIO12/GPIO18）

        int pwm_frequency = 1000;
        // PWM 信号频率（Hz），enable_pwm=true 时生效；
        // 1000 Hz（1 kHz）适用于 LED 亮度调节和简单电机控制
    } gpio;
    // 声明 gpio 成员变量

    struct SerialConfig {
        // 子结构体 SerialConfig：串口（UART）通信配置
        struct SerialPortConfig {
            // 单个串口的配置参数
            std::string device;
            // 串口设备文件路径，如 "/dev/ttyAMA0"（树莓派主 UART）

            int baud_rate;
            // 波特率（bps），常用值：9600、115200、921600 等；
            // 115200 bps 是工业传感器通信的常用波特率

            int data_bits;
            // 数据位数，通常为 8（8N1 最常见配置）

            std::string parity;
            // 校验位配置："none"（无校验）、"even"（偶校验）、"odd"（奇校验）

            int stop_bits;
            // 停止位数，通常为 1；部分旧式设备使用 2 位停止位

            bool flow_control = false;
            // 为 true 时启用硬件流控（RTS/CTS）；
            // 大多数嵌入式传感器通信不需要流控，默认关闭
        };
        std::vector<SerialPortConfig> ports;
        // 系统所有串口的配置列表
    } serial;
    // 声明 serial 成员变量

    struct I2CConfig {
        // 子结构体 I2CConfig：I2C 总线配置
        bool enable_i2c = true;
        // 为 true 时启用 I2C 总线（树莓派通过 /boot/config.txt 的 dtparam=i2c_arm=on 开启）

        int bus_number = 1;
        // I2C 总线编号，树莓派 4B 默认使用总线 1（/dev/i2c-1，对应 BCM GPIO2/3）

        int speed_hz = 100000;
        // I2C 总线时钟频率（Hz），默认 100 kHz（标准模式 Standard Mode）；
        // 可提高至 400 kHz（快速模式 Fast Mode）以加快 OLED 刷新速率

        std::vector<uint8_t> device_addresses;
        // I2C 总线上已知从机设备的地址列表；
        // 用于系统启动时的 I2C 总线扫描和设备连通性自检（如检查 OLED 0x3C 是否响应）
    } i2c;
    // 声明 i2c 成员变量

    struct SPIConfig {
        // 子结构体 SPIConfig：SPI 总线配置
        bool enable_spi = true;
        // 为 true 时启用 SPI 总线（通过 /boot/config.txt 的 dtparam=spi=on 开启）

        struct SPIBusConfig {
            // 单个 SPI 总线的参数描述
            int bus_number;
            // SPI 总线编号，对应 /dev/spidevN.x 中的 N

            int speed_hz;
            // SPI 时钟频率（Hz），AD4134 最高支持 80 MHz，
            // 实际配置见 AD4134Config::SPIInterface::speed_hz

            int mode;
            // SPI 工作模式（0~3，由 CPOL/CPHA 决定），AD4134 使用模式 3

            int bits_per_word;
            // 每次 SPI 传输的数据位宽，标准 8 位

            bool cs_high = false;
            // 为 false 时片选信号低电平有效（标准），为 true 时高电平有效（非标准）
        };
        std::vector<SPIBusConfig> buses;
        // 系统所有 SPI 总线的配置列表
    } spi;
    // 声明 spi 成员变量

    struct CameraConfig {
        // 子结构体 CameraConfig：摄像头硬件设备配置
        struct CameraDeviceConfig {
            // 单个摄像头设备的完整硬件规格描述
            int id;
            // 摄像头 V4L2 设备 ID（对应 /dev/videoN）

            std::string name;
            // 摄像头标识名称，如 "USB Camera"

            std::string type;
            // 摄像头连接方式，如 "usb"（USB 摄像头）、"csi"（CSI 接口摄像头）

            std::string driver;
            // 摄像头驱动框架，如 "v4l2"（Video for Linux 2）

            int width;
            // 采集分辨率宽度（像素）

            int height;
            // 采集分辨率高度（像素）

            int fps;
            // 采集帧率（fps）

            std::string format;
            // 像素格式字符串，如 "MJPG"（JPEG 压缩）、"YUYV"（YUV 原始格式）；
            // MJPG 带宽更低，YUYV 无需解压但带宽消耗较大

            bool auto_exposure = true;
            // 为 true 时启用自动曝光（AE），与 CameraParams::exposure=-1.0f 语义一致

            bool auto_white_balance = true;
            // 为 true 时启用自动白平衡（AWB），在彩色图像处理时有效；
            // 灰度图像转换前的白平衡调整对最终结果影响有限
        };
        std::vector<CameraDeviceConfig> cameras;
        // 系统所有摄像头设备的配置列表
    } camera;
    // 声明 camera 成员变量

    struct SensorConfig {
        // 子结构体 SensorConfig：通用传感器设备配置
        struct SensorDeviceConfig {
            // 单个传感器设备的规格描述
            std::string name;
            // 传感器标识名称，如 "AD4134_Accelerometer"

            std::string type;
            // 传感器物理量类型，如 "accelerometer"、"temperature"、"gas"

            std::string interface;
            // 传感器通信接口类型，如 "SPI"、"I2C"、"UART"、"ADC"

            uint8_t address;
            // 传感器 I2C/SPI 地址（uint8_t 对应 8 位地址空间）

            float range_min;
            // 传感器可测量范围下限（物理量单位，如 g、℃、ppm）

            float range_max;
            // 传感器可测量范围上限

            float accuracy;
            // 传感器测量精度（物理量单位），如加速度计 ±0.001 g

            int sampling_rate_hz;
            // 传感器采样率（Hz），对应 AD4134 的 50 kSPS 等
        };
        std::vector<SensorDeviceConfig> sensors;
        // 系统所有传感器设备的配置列表
    } sensor;
    // 声明 sensor 成员变量

    HardwareConfig() {
        // 构造函数：以树莓派 4B 为默认目标平台填充合理初始值

        cpu.architecture = "ARMv8";
        // Cortex-A72 实现 ARMv8-A 指令集（64 位 AArch64）
        cpu.cores = 4;
        // 树莓派 4B 搭载四核 Cortex-A72 处理器
        cpu.frequency_mhz = 1500;
        // 树莓派 4B 默认最高频率 1.5 GHz（超频可至 2.0 GHz）

        gpu.model = "VideoCore VI";
        // 树莓派 4B 集成 Broadcom VideoCore VI GPU
        gpu.memory_mb = 128;
        // GPU 共享内存默认 128 MB，可通过 /boot/config.txt 调整
        gpu.frequency_mhz = 500;
        // VideoCore VI 默认运行频率 500 MHz

        memory.total_mb = 4096;
        // 树莓派 4B 最大配置 4 GB（4096 MB）LPDDR4X 内存
        memory.swap_mb = 1024;
        // 默认 Swap 大小 1 GB，通过 /etc/dphys-swapfile 配置

        storage_devices.push_back({"/dev/mmcblk0p1", "SD Card", "ext4", 32, false});
        // 添加默认存储设备：32 GB SD 卡，ext4 文件系统，可读写挂载；
        // /dev/mmcblk0p1 为 SD 卡第一分区（通常为根分区）

        network_interfaces.push_back({"eth0", "00:11:22:33:44:55", "eth", 1000, false});
        // 添加默认网络接口：千兆以太网 eth0，MAC 地址为占位符，非混杂模式；
        // 实际 MAC 地址由树莓派 OTP 寄存器固化，此处为示例值

        gpio.pins = {
            {2,  "SDA1",   "in",  "up",   0},
            // BCM2：I2C1 数据线（SDA），输入方向，上拉电阻使能；
            // 连接 OLED 的 SDA 引脚

            {3,  "SCL1",   "in",  "up",   0},
            // BCM3：I2C1 时钟线（SCL），输入方向，上拉电阻使能；
            // 连接 OLED 的 SCL 引脚

            {4,  "GPIO4",  "out", "none", 0},
            // BCM4：通用输出引脚，无上下拉，初始低电平

            {17, "GPIO17", "out", "none", 0},
            // BCM17：轴承监测继电器 K1 控制引脚，输出，初始低电平（继电器断开）；
            // 与 AlarmConfig::RelayControl::bearing_relay_pin=17 一致

            {27, "GPIO27", "in",  "down", 0},
            // BCM27：火焰检测继电器 K2 控制引脚，此处配置为输入+下拉；
            // 实际用途需与 fire_detection_config.hpp 的继电器配置对照

            {22, "GPIO22", "out", "none", 0}
            // BCM22：通用输出引脚，如 LED 指示灯或蜂鸣器控制
        };

        serial.ports.push_back({"/dev/ttyAMA0", 115200, 8, "none", 1, false});
        // 添加默认串口：树莓派主 UART（/dev/ttyAMA0），
        // 波特率 115200，8N1（8 数据位，无校验，1 停止位），无硬件流控

        camera.cameras.push_back({0, "USB Camera", "usb", "v4l2", 640, 480, 30, "MJPG", true, true});
        // 添加默认摄像头：/dev/video0，USB 类型，V4L2 驱动，
        // 640×480 @ 30fps，MJPEG 格式，自动曝光，自动白平衡
    }
};
// 结束 HardwareConfig 结构体定义

struct SoftwareConfig {
    // 软件运行环境描述结构体：记录系统运行所依赖的操作系统、
    // 编译器运行时、系统服务、进程参数和环境变量配置

    struct OSConfig {
        // 子结构体 OSConfig：操作系统信息与更新策略
        std::string name;
        // OS 名称字符串，如 "Linux"

        std::string version;
        // OS 发行版版本号，如 "5.10"（内核主版本）

        std::string kernel;
        // 完整内核版本字符串，如 "5.10.103-v8+"（树莓派 64 位内核）；
        // 通过 uname -r 命令获取

        std::string architecture;
        // 系统架构字符串，如 "aarch64"（64 位 ARM）

        std::string distribution;
        // Linux 发行版名称，如 "Raspberry Pi OS"（原 Raspbian）

        bool enable_updates = true;
        // 为 true 时允许系统自动执行 apt update/upgrade；
        // 工业生产环境建议关闭自动更新，改为人工审核后手动升级

        std::string update_schedule = "0 4 * * *";
        // 系统更新执行时间的 cron 表达式，默认每天凌晨 4:00；
        // 错开数据清理（2:00）和备份（3:00）任务的执行时间
    } os;
    // 声明 os 成员变量

    struct RuntimeConfig {
        // 子结构体 RuntimeConfig：C++ 编译器和运行时库信息
        std::string language = "C++";
        // 编程语言标识字符串，固定为 "C++"

        std::string compiler;
        // 编译器名称，如 "g++"（GNU C++ 编译器）

        std::string compiler_version;
        // 编译器版本字符串，如 "10.2.1"（g++ 10.x 系列）；
        // 通过 g++ --version 命令获取

        std::string std_library;
        // C++ 标准库实现名称，如 "libstdc++"（GNU 标准库）

        std::vector<std::string> libraries;
        // 系统依赖的第三方动态链接库列表；
        // 用于启动前的依赖检查（通过 ldd 命令验证库文件是否存在）

        bool enable_debug = false;
        // 为 true 时以调试模式运行（启用 -g 符号表，性能下降）；
        // 生产环境应保持 false（Release 模式）

        std::string optimization_level = "-O2";
        // 编译优化级别字符串，用于记录编译时的优化设置；
        // -O2 是生产环境的推荐级别（平衡编译速度与运行性能）
    } runtime;
    // 声明 runtime 成员变量

    struct ServiceConfig {
        // 子结构体 ServiceConfig：systemd 系统服务配置
        struct ServiceItem {
            // 单个系统服务的配置描述
            std::string name;
            // 服务名称，如 "multi_project_system"

            std::string description;
            // 服务功能描述，用于 systemd unit 文件的 Description 字段

            bool enabled;
            // 为 true 时服务参与 systemd enable（开机自启动）

            bool auto_start;
            // 为 true 时在框架初始化时自动触发启动命令

            std::string start_command;
            // 服务启动命令字符串，如 "/usr/bin/multi_project_system --daemon"

            std::string stop_command;
            // 服务停止命令字符串，如 "pkill -f multi_project_system"

            std::string restart_command;
            // 服务重启命令字符串，如 "systemctl restart multi_project_system"
        };
        std::vector<ServiceItem> services;
        // 系统管理的所有服务配置列表，由 SoftwareConfig 构造函数预填充两个默认服务
    } service;
    // 声明 service 成员变量

    struct ProcessConfig {
        // 子结构体 ProcessConfig：进程级资源限制参数
        int max_processes = 100;
        // 系统允许的最大进程数；通过 ulimit -u 或 /etc/security/limits.conf 设置

        int max_threads_per_process = 20;
        // 每个进程允许的最大线程数；
        // 默认 20 远超框架实际使用的 4 个工作线程，留有充足余量

        bool enable_core_dump = false;
        // 为 true 时在进程崩溃时生成 core dump 文件，用于离线调试（gdb）；
        // 生产环境通常关闭（core 文件可能包含敏感数据且占用大量磁盘空间）

        std::string core_dump_path = "/var/core";
        // core dump 文件存放路径；enable_core_dump=true 时使用

        int core_dump_size_limit_mb = 100;
        // 单个 core dump 文件的最大大小（MB），防止 core 文件填满磁盘
    } process;
    // 声明 process 成员变量

    struct EnvironmentConfig {
        // 子结构体 EnvironmentConfig：进程运行环境变量与路径配置
        std::map<std::string, std::string> variables;
        // 环境变量名到值的有序映射，由构造函数预填充三个关键变量；
        // 在进程启动时通过 setenv() 注入

        std::vector<std::string> paths;
        // PATH 环境变量的目录列表，按优先级从高到低排列；
        // 系统将在这些目录中依次搜索可执行文件

        std::string working_directory;
        // 进程工作目录（Current Working Directory），影响相对路径的解析

        std::string home_directory;
        // 用户 HOME 目录，影响 ~ 路径展开和用户配置文件查找

        std::string temp_directory;
        // 临时文件目录，用于存放运行时中间文件（如模型加载时的临时缓存）
    } environment;
    // 声明 environment 成员变量

    SoftwareConfig() {
        // 构造函数：以树莓派 4B + Raspberry Pi OS 为默认平台填充软件环境初始值

        os.name = "Linux";
        os.version = "5.10";
        // 树莓派官方 Raspberry Pi OS（Bullseye 版本）的内核主版本
        os.kernel = "5.10.103-v8+";
        // 树莓派 64 位（v8+）内核的完整版本号
        os.architecture = "aarch64";
        // 64 位 ARM 架构标识
        os.distribution = "Raspberry Pi OS";
        // 发行版名称

        runtime.compiler = "g++";
        // GNU C++ 编译器
        runtime.compiler_version = "10.2.1";
        // Raspberry Pi OS Bullseye 随附的 GCC 版本
        runtime.std_library = "libstdc++";
        // GNU 标准 C++ 库
        runtime.libraries = {"OpenCV", "ONNX Runtime", "wiringPi", "libusb-1.0", "libcurl"};
        // 系统依赖的关键第三方库：
        //   OpenCV：图像采集和预处理（resize、灰度化、颜色转换等）
        //   ONNX Runtime：AI 模型推理引擎
        //   wiringPi：树莓派 GPIO 控制（继电器、蜂鸣器、LED）
        //   libusb-1.0：USB 设备底层访问（部分摄像头驱动依赖）
        //   libcurl：HTTP/HTTPS 网络请求（远程监控数据上传）

        service.services = {
            {"multi_project_system", "Multi-Project Detection System", true, true,
             "/usr/bin/multi_project_system --daemon", "pkill -f multi_project_system",
             "systemctl restart multi_project_system"},
            // 主检测系统服务：开机自启（enabled=true），自动启动（auto_start=true），
            // 以守护进程模式运行（--daemon），通过 pkill 停止，支持 systemctl 重启

            {"network_manager", "Network Manager Service", true, true,
             "systemctl start NetworkManager", "systemctl stop NetworkManager",
             "systemctl restart NetworkManager"}
            // NetworkManager 服务：管理以太网和 WiFi 连接，
            // 使用 systemctl 命令管理服务生命周期
        };

        environment.variables = {
            {"LD_LIBRARY_PATH", "/usr/local/lib:/opt/vc/lib"},
            // 动态链接库搜索路径：/usr/local/lib 为本地安装库（ONNX Runtime 等），
            // /opt/vc/lib 为树莓派 VideoCore 视频核心库

            {"PYTHONPATH", "/usr/local/lib/python3.9/dist-packages"},
            // Python 模块搜索路径（若系统中有 Python 相关工具或脚本使用）

            {"OPENCV_LOG_LEVEL", "ERROR"}
            // 将 OpenCV 内部日志级别设为 ERROR，
            // 屏蔽 INFO/WARNING 级别的冗余调试输出，保持终端输出整洁
        };
        environment.paths = {"/usr/local/bin", "/usr/bin", "/bin", "/usr/local/sbin", "/usr/sbin", "/sbin"};
        // 标准 Linux PATH 目录列表，按优先级从高到低排列；
        // /usr/local/bin 优先于 /usr/bin，确保本地安装的工具覆盖系统版本
        environment.working_directory = "/home/pi";
        // 树莓派默认用户 pi 的家目录，作为进程工作目录；
        // 相对路径（如 MODEL_PATH 宏的 "../models/..."）以此为基准
        environment.home_directory = "/home/pi";
        // 用户 HOME 目录，与 working_directory 保持一致
        environment.temp_directory = "/tmp";
        // Linux 标准临时文件目录，内存文件系统（tmpfs），重启后清空
    }
};
// 结束 SoftwareConfig 结构体定义

struct ApplicationConfig {
    // 应用编排配置结构体：从应用层视角描述系统中各子项目、
    // 子框架、数据流水线、AI 模型和 UI 界面的注册与运行参数

    struct ProjectConfig {
        // 单个子项目的注册配置描述
        ProjectType type;
        // 项目类型枚举值，引用 FrameworkBase.hpp 中的 ProjectType

        std::string name;
        // 项目名称字符串，如 "Fire Detection System"

        std::string description;
        // 项目功能的简短描述，用于管理界面和诊断报告

        bool enabled;
        // 为 true 时本项目参与系统启动初始化和运行调度

        int priority;
        // 项目调度优先级（数值越小优先级越高）；
        // 多个项目同时运行时，优先级高的项目优先分配 CPU 资源

        std::string config_file;
        // 本项目专属配置文件的绝对路径，
        // 如 "/etc/multi_project/fire_detection.json"
    };
    std::vector<ProjectConfig> projects;
    // 系统注册的全部子项目列表，由构造函数预填充两个默认项目

    struct FrameworkConfig {
        // 单个子框架模块的注册配置描述
        std::string name;
        // 框架模块名称，如 "DataAcquisitionFramework"

        std::string version;
        // 框架模块版本号字符串，用于兼容性检查

        bool enabled;
        // 为 true 时本框架模块参与初始化和运行

        std::string config;
        // 本框架模块专属配置文件的绝对路径
    };
    std::vector<FrameworkConfig> frameworks;
    // 系统注册的全部子框架模块列表，由构造函数预填充五个标准框架

    struct DataflowConfig {
        // 子结构体 DataflowConfig：数据处理流水线配置
        struct PipelineConfig {
            // 单条流水线的配置描述
            std::string name;
            // 流水线名称，如 "FireDetectionPipeline"

            std::vector<std::string> stages;
            // 流水线各阶段的名称列表，按执行顺序排列；
            // 如 {"Acquisition", "Preprocessing", "Inference", "Postprocessing", "Display"}

            bool parallel_processing;
            // 为 true 时各阶段在独立线程中并行执行（流水线模式）；
            // 为 false 时各阶段在同一线程中串行执行

            int buffer_size;
            // 本流水线相邻阶段间 ConcurrentQueue 的深度

            bool enable_cache;
            // 为 true 时在推理阶段启用结果缓存；
            // 注意：AIModelConfig 中已关闭推理缓存以避免错误结果被永久缓存
        };
        std::vector<PipelineConfig> pipelines;
        // 系统中全部数据流水线的配置列表
    } dataflow;
    // 声明 dataflow 成员变量

    struct AIModelConfig {
        // 子结构体 AIModelConfig：系统级 AI 模型目录配置
        struct ModelConfig {
            // 单个 AI 模型的注册配置描述
            std::string name;
            // 模型标识名称，如 "VariableMobileNet_Fire"

            std::string path;
            // 模型文件的绝对路径，如 "/opt/models/fire_detection/fire_detection_model.onnx"；
            // 注意：此处使用绝对路径（/opt/models/...），
            // 与 ProjectDefines.hpp 中 MODEL_PATH 宏的相对路径（../models/...）不同

            std::string type;
            // 任务类型字符串，如 "Classification"，与 TaskType 枚举语义对应

            std::string framework;
            // 推理框架名称，如 "ONNX"（ONNX Runtime）

            bool enabled;
            // 为 true 时本模型参与加载和推理调度

            float confidence_threshold;
            // 模型输出置信度的判决阈值，高于此值才视为有效检测

            int input_width;
            // 模型输入张量宽度（像素），两个模型均为 224

            int input_height;
            // 模型输入张量高度（像素），两个模型均为 224
        };
        std::vector<ModelConfig> models;
        // 系统注册的全部 AI 模型列表，由构造函数预填充两个默认模型
    } ai_model;
    // 声明 ai_model 成员变量

    struct UIConfig {
        // 子结构体 UIConfig：用户界面显示配置
        bool enable_gui = false;
        // 为 true 时启用图形用户界面（GUI）；
        // 无头部署（headless）模式下应设为 false 以节省 GPU 和内存资源

        std::string theme = "dark";
        // UI 主题风格字符串，"dark"（深色）或 "light"（浅色）；
        // 工业监控屏幕通常使用深色主题以减少眩光

        int screen_width = 800;
        // UI 显示窗口宽度（像素），与 DisplayConfig::VideoDisplay::display_width 一致

        int screen_height = 600;
        // UI 显示窗口高度（像素），与 DisplayConfig::VideoDisplay::display_height 一致

        bool fullscreen = false;
        // 为 true 时以全屏模式运行 GUI；
        // 内嵌触摸屏工控机通常使用全屏模式

        int refresh_rate = 30;
        // UI 刷新率（fps），与 target_fps 保持一致

        std::vector<std::string> languages = {"en", "zh"};
        // 支持的 UI 语言列表：英文（"en"）和中文（"zh"）

        std::string default_language = "en";
        // 默认 UI 语言，未检测到用户语言偏好时使用英文
    } ui;
    // 声明 ui 成员变量

    ApplicationConfig() {
        // 构造函数：预填充两个子项目、五个子框架、两条流水线和两个 AI 模型的默认配置

        projects = {
            {ProjectType::FIRE_DETECTION, "Fire Detection System",
             "Real-time flame detection using computer vision", true, 1,
             "/etc/multi_project/fire_detection.json"},
            // 火焰检测项目：启用，优先级 1（最高），配置文件位于 /etc/multi_project/

            {ProjectType::BEARING_VIBRATION, "Bearing Vibration Monitoring",
             "Predictive maintenance using vibration analysis", true, 2,
             "/etc/multi_project/bearing_vibration.json"}
            // 轴承振动监测项目：启用，优先级 2，配置文件位于 /etc/multi_project/
        };

        frameworks = {
            {"DataAcquisitionFramework",  "1.0.0", true, "/etc/multi_project/data_acquisition.json"},
            // 数据采集框架：负责摄像头帧读取和 ADC 采样
            {"DataProcessingFramework",   "1.0.0", true, "/etc/multi_project/data_processing.json"},
            // 数据处理框架：负责图像预处理和振动信号分析
            {"AIModelFramework",          "1.0.0", true, "/etc/multi_project/ai_model.json"},
            // AI 推理框架：负责 ONNX 模型加载和推理执行
            {"DisplayLoggingFramework",   "1.0.0", true, "/etc/multi_project/display_logging.json"},
            // 显示日志框架：负责 OLED 刷新、视频叠加和日志写入
            {"ExtendedFeaturesFramework", "1.0.0", true, "/etc/multi_project/extended_features.json"}
            // 扩展功能框架：负责报警输出、RUL 预测、远程监控等扩展功能
        };

        dataflow.pipelines = {
            {"FireDetectionPipeline",
             {"Acquisition", "Preprocessing", "Inference", "Postprocessing", "Display"},
             true, 10, true},
            // 火焰检测流水线：5 个阶段，并行处理（true），缓冲区 10 帧，启用缓存；
            // 注意：enable_cache=true 与 AIModelConfig 中关闭推理缓存的修复存在潜在矛盾，
            // 此处 enable_cache 为流水线级别的结果缓存，与模型推理级别缓存不同

            {"VibrationAnalysisPipeline",
             {"Acquisition", "FFT", "FeatureExtraction", "Classification", "Logging"},
             false, 20, false}
            // 振动分析流水线：5 个阶段，串行处理（false），缓冲区 20 帧，关闭缓存；
            // 振动信号的 FFT 和特征提取须按时序严格串行执行，不宜并行
        };

        ai_model.models = {
            {"VariableMobileNet_Fire",
            "/opt/models/fire_detection/fire_detection_model.onnx",
            "Classification", "ONNX", true, 0.7f, 224, 224},
            // 火焰检测模型：VariableMobileNet 架构，ONNX 格式，
            // 置信度阈值 0.7（高于 fire_detection_config 中的 0.5，此处为系统级缺省值），
            // 输入 224×224 单通道灰度图

            {"VariableMobileNet_Bearing",
            "/opt/models/bearing_vibration/bearing_vibration_model.onnx",
            "Classification", "ONNX", true, 0.7f, 224, 224}
            // 轴承振动分类模型：相同架构和输入规格，置信度阈值 0.7
        };
    }
};
// 结束 ApplicationConfig 结构体定义

class SystemConfig : public ISystemConfig {
// SystemConfig：ISystemConfig 接口的具体实现类；
// 聚合 GeneralSystemConfig、HardwareConfig、SoftwareConfig、ApplicationConfig
// 四个配置域，提供统一的配置加载/保存/克隆/访问接口；
// 本类为框架层对外暴露的主要配置管理入口
public:
    SystemConfig() {
        // 构造函数：初始化版本信息字段
        version_info.major = 2;
        // 主版本号 2，对应 GeneralSystemConfig::version 中的 major=2
        version_info.minor = 0;
        // 次版本号 0
        version_info.patch = 0;
        // 修订号 0
        version_info.build = 1;
        // 构建号 1
        version_info.name = "Multi-Project System Config";
        // 版本代号描述字符串
        version_info.date = __DATE__;
        // 编译日期字符串：__DATE__ 是编译器预定义宏，展开为编译时的日期
        // 格式为 "Mmm DD YYYY"，如 "Apr  8 2026"；
        // 每次重新编译都会自动更新，无需手动维护
        version_info.commit = "GIT_COMMIT_HASH";
        // Git 提交哈希占位符；实际工程中应通过 CMake 或脚本
        // 在编译时将真实哈希值注入（如 -DGIT_COMMIT_HASH=$(git rev-parse --short HEAD)）
    }

    ErrorCode load(const std::string& config_str) override { return ErrorCode::SUCCESS; }
    // 覆盖 ISystemConfig::load()：当前为桩实现（Stub），直接返回 SUCCESS；
    // 完整实现应解析 config_str 中的 JSON 并更新 general/hardware/software/application 各域

    std::string save() const override { return "{}"; }
    // 覆盖 ISystemConfig::save()：当前为桩实现，返回空 JSON 对象 "{}"；
    // 完整实现应将全部配置域序列化为嵌套 JSON 字符串

    std::string validate() const override { return ""; }
    // 覆盖 ISystemConfig::validate()：当前为桩实现，返回空字符串（表示无错误）；
    // 完整实现应检查各配置域的约束（如路径存在性、数值范围等）并返回错误描述

    VersionInfo getVersion() const override { return version_info; }
    // 覆盖 ISystemConfig::getVersion()：返回 version_info 成员的副本

    std::string getType() const override { return "SystemConfig"; }
    // 覆盖 ISystemConfig::getType()：返回固定字符串 "SystemConfig" 作为类型标识

    std::string getDescription() const override { return "Complete system configuration"; }
    // 覆盖 ISystemConfig::getDescription()：返回固定的配置描述字符串

    void resetToDefault() override {
        // 覆盖 ISystemConfig::resetToDefault()：
        // 通过重新构造各配置域对象，将所有字段恢复为编译期默认值
        general = GeneralSystemConfig();
        // 重新构造 GeneralSystemConfig，触发其构造函数重新填充默认值
        hardware = HardwareConfig();
        // 重新构造 HardwareConfig，重置树莓派硬件默认参数
        software = SoftwareConfig();
        // 重新构造 SoftwareConfig，重置软件环境默认参数
        application = ApplicationConfig();
        // 重新构造 ApplicationConfig，重置项目注册和流水线默认配置
        last_error = ErrorCode::SUCCESS;
        // 清除错误状态码
        error_message.clear();
        // 清空错误消息字符串
    }

    std::shared_ptr<ISystemConfig> clone() const override {
        // 覆盖 ISystemConfig::clone()：创建当前 SystemConfig 对象的深拷贝
        auto config = std::make_shared<SystemConfig>();
        // 通过 make_shared 创建新的 SystemConfig 实例，
        // 调用默认构造函数（version_info 被初始化）
        config->general = this->general;
        // 将当前 general 配置域深拷贝到新对象（结构体赋值触发成员逐一拷贝）
        config->hardware = this->hardware;
        // 深拷贝 hardware 配置域
        config->software = this->software;
        // 深拷贝 software 配置域
        config->application = this->application;
        // 深拷贝 application 配置域
        config->version_info = this->version_info;
        // 深拷贝版本信息
        return config;
        // 返回 shared_ptr，调用方与本函数共享新对象的所有权
    }

    std::shared_ptr<ISystemConfig> merge(const ISystemConfig& other) const override {
        // 覆盖 ISystemConfig::merge()：当前为简化实现，仅返回当前对象的克隆；
        // 完整实现应将 other 中非默认值的字段覆盖到克隆对象中，
        // 实现"other 配置补丁到当前配置"的合并语义；
        // other 参数当前未使用，编译器可能产生 unused parameter 警告
        return clone();
        // 返回当前配置的克隆，忽略 other 的内容（桩实现）
    }

    std::string toJson(bool pretty = true) const override {
        // 覆盖 ISystemConfig::toJson()：当前为桩实现，返回空 JSON 对象；
        // 参数 pretty 当前未使用；
        // 完整实现应调用 JSON 序列化库（如 nlohmann/json）将全部配置域输出为 JSON
        return "{}";
    }

    ErrorCode fromJson(const std::string& json_str) override { return ErrorCode::SUCCESS; }
    // 覆盖 ISystemConfig::fromJson()：当前为桩实现，直接返回 SUCCESS；
    // 参数 json_str 当前未使用；
    // 完整实现应解析 JSON 并更新各配置域成员

    GeneralSystemConfig& getGeneralConfig() { return general; }
    // 非 const 访问器：返回 general 配置域的可修改引用，供调用方直接修改字段

    const GeneralSystemConfig& getGeneralConfig() const { return general; }
    // const 访问器：返回 general 配置域的只读引用，供 const 调用方安全读取

    HardwareConfig& getHardwareConfig() { return hardware; }
    // 非 const 访问器：返回 hardware 配置域的可修改引用

    const HardwareConfig& getHardwareConfig() const { return hardware; }
    // const 访问器：返回 hardware 配置域的只读引用

    SoftwareConfig& getSoftwareConfig() { return software; }
    // 非 const 访问器：返回 software 配置域的可修改引用

    const SoftwareConfig& getSoftwareConfig() const { return software; }
    // const 访问器：返回 software 配置域的只读引用

    ApplicationConfig& getApplicationConfig() { return application; }
    // 非 const 访问器：返回 application 配置域的可修改引用

    const ApplicationConfig& getApplicationConfig() const { return application; }
    // const 访问器：返回 application 配置域的只读引用

    ErrorCode getLastError() const { return last_error; }
    // 返回最近一次操作产生的错误码；
    // 非虚方法（相较于 ISystemConfig 中的虚函数声明），
    // SystemConfig 类独有的扩展接口

    std::string getErrorString() const { return error_message; }
    // 返回最近一次错误的人类可读描述字符串；
    // SystemConfig 类独有的扩展接口

    void clearError() { last_error = ErrorCode::SUCCESS; error_message.clear(); }
    // 清除错误状态：将 last_error 重置为 SUCCESS，清空 error_message；
    // 在人工确认错误后调用，使对象恢复到可正常操作的状态

private:
// 私有成员区域：以下数据成员仅供类内部方法和友元访问

    GeneralSystemConfig general;
    // 系统通用配置域实例（系统信息、运行模式、日志、性能等十个子域）

    HardwareConfig hardware;
    // 硬件资源描述配置域实例（CPU/GPU/内存/GPIO 等十二个子域）

    SoftwareConfig software;
    // 软件运行环境配置域实例（OS/运行时/服务/进程/环境变量五个子域）

    ApplicationConfig application;
    // 应用编排配置域实例（子项目/框架/流水线/AI 模型/UI 五个子域）

    VersionInfo version_info;
    // 本 SystemConfig 对象的版本信息，由构造函数初始化为 2.0.0.1

    ErrorCode last_error = ErrorCode::SUCCESS;
    // 最近一次操作的错误码，默认 SUCCESS（无错误）；
    // 通过 getLastError() 和 clearError() 对外访问和清除

    std::string error_message;
    // 最近一次错误的人类可读描述字符串，默认空字符串；
    // 通过 getErrorString() 对外读取
};
// 结束 SystemConfig 类定义

} // namespace MultiProject
// 关闭命名空间 MultiProject

#endif // SYSTEM_CONFIG_HPP
// 结束头文件保护宏条件编译块，与文件开头的 #ifndef SYSTEM_CONFIG_HPP 配对