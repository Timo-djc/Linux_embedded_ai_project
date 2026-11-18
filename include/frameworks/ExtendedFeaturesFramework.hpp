/**
  ******************************************************************************
  * @file    ExtendedFeaturesFramework.hpp
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
 * @brief   扩展功能框架 — 外设驱动、OLED 菜单系统与报警输出层头文件
 * ----------------------------------------------------------------------------
 * @details 本文件定义了 MultiProject 框架扩展功能层的核心类
 *          ExtendedFeaturesFramework，继承自 IExtendedFeatures 接口，
 *          负责管理以下五类扩展硬件和功能：
 *
 *          1. PWR_OFF 掉电中断
 *             监听外部掉电信号引脚（pigpio alert），触发安全关机回调
 *
 *          2. RS485 模拟量输出（4~20 mA 电流环）
 *             通过 UART + RS485 收发器向 PLC/DCS 上位系统发送 AI 推理结果
 *             （4~8 mA 状态信号 + 8~20 mA 分类结果）
 *
 *          3. 继电器输出
 *             根据 AI 推理类别 ID 驱动继电器 K1（轴承故障）/ K2（火焰）
 *
 *          4. 74HC595D 移位寄存器 LED / 蜂鸣器驱动
 *             通过 GPIO 串行时序控制 8 路 LED 和 1 路蜂鸣器
 *
 *          5. OLED 三键菜单系统
 *             独立刷新线程 + pigpio 三键中断；NORMAL/VIEW/EDIT 三种交互模式；
 *             7 个菜单页面；50 ms 防抖 + 600/2000 ms 双级长按检测
 *
 *          对外公开的枚举类：LedChannel / MenuPage / SettingsItem / MenuMode
 * ============================================================================
 */

#pragma once

#include "../common/UnifiedDataTypes.hpp"
// 引入统一数据类型：IExtendedFeatures（虚基类）、ModelOutput、SystemStatus、
// MenuSnapshot、AbnormalRecord、ErrorCode、ProjectType 等

#include <string>
// std::string：短信/邮件内容、服务器 URL 等文本字段

#include <vector>
// std::vector：log_cache_（日志翻页缓冲）和 phone_numbers（短信接收列表）

#include <atomic>
// std::atomic：pwr_off_triggered_、menu_running_、按键时间戳等跨线程标志，
// 按键回调（pigpio ISR）和菜单主线程并发访问，须原子操作

#include <functional>
// std::function：shutdown_callback_ 类型，允许注册任意可调用对象作为关机回调

#include <array>
// std::array：hc595_shadow_（固定 8 元素），与 74HC595D 8 位移位寄存器宽度对应

#include <mutex>
// std::mutex：menu_mtx_ 保护菜单状态在菜单线程与按键回调线程之间的并发访问

#include <thread>
// std::thread：menu_thread_（OLED 菜单独立刷新线程）

#include <chrono>
// std::chrono：供 mono_ms() 获取单调时钟毫秒时间戳，用于防抖和长按计时

namespace MultiProject {

class DisplayLoggingFramework;
// 前向声明：ExtendedFeaturesFramework 持有 dl_ 裸指针（不转移所有权），
// 无需包含完整头文件，避免循环依赖

// ── 74HC595 通道索引 ─────────────────────────────────────────────────
enum class LedChannel : uint8_t {
// 强类型枚举：74HC595D 8 路输出通道的逻辑索引（Bit 0~7）；
// 通过 led_set(LedChannel::XXXX, true/false) 控制对应通道亮灭

    WORKING_NORMAL = 0,
    // Bit 0：系统正常运行指示灯；推理结果正常时点亮，任何报警触发后熄灭

    BEARING_OUTER  = 1,
    // Bit 1：轴承外圈故障指示灯；推理类别 "deouterrace" 时点亮

    FIRE_NORMAL    = 2,
    // Bit 2：火焰检测正常指示灯；推理类别 "normal"（无火焰）时点亮

    BEARING_CAGE   = 3,
    // Bit 3：轴承保持架故障指示灯；推理类别 "decagefault" 时点亮

    BEARING_BALL   = 4,
    // Bit 4：轴承滚动体故障指示灯；推理类别 "deballfault" 时点亮

    FIRE_ALARM     = 5,
    // Bit 5：火焰报警指示灯；推理类别 "fire" 且置信度超阈值时点亮并触发蜂鸣器

    DEVICE_ERROR   = 6,
    // Bit 6：设备故障指示灯；硬件自检失败或推理引擎初始化失败时点亮

    BUZZER         = 7,
    // Bit 7：蜂鸣器控制位；置 1 时驱动蜂鸣器发声，与报警 LED 同步触发
};

// ── OLED 菜单页枚举 ──────────────────────────────────────────────────
enum class MenuPage : uint8_t {
// 强类型枚举：OLED 菜单系统的页面标识；
// cur_page_ 记录当前页，menu_loop() 根据此值调用对应 render_* 函数

    STANDBY        = 0,  // 待机界面（系统名称 + 操作提示）
    // 默认待机画面；显示系统名称、版本和"按 MODE 键进入菜单"提示

    PROJECT_STATUS = 1,  // 项目运行状态
    // 实时显示 AI 推理快照（故障类别、置信度、健康指数等）

    LOG_BEARING    = 2,  // 轴承异常日志（近一周）
    // 近 7 天轴承振动异常记录列表；VIEW 模式下 LEFT/RIGHT 翻页浏览

    LOG_FIRE       = 3,  // 火焰异常日志（近一周）
    // 近 7 天火焰检测异常记录列表；VIEW 模式下 LEFT/RIGHT 翻页浏览

    EXPORT_LOG     = 4,  // 导出日志
    // 触发 export_week_logs() 将近 7 天 CSV 和截图导出到目标目录

    SHUTDOWN       = 5,  // 关机确认
    // 二次确认关机页面；shutdown_confirm_ 控制是否已进入二次确认

    SETTINGS       = 6,  // 设置菜单（长按 2s 进入）
    // EDIT 模式下显示并调整置信度阈值和报警延迟两个参数；
    // 须长按 MODE 2s（LONG_PRESS_EDIT）才能进入，防止意外修改

    PAGE_COUNT     = 7,
    // 页面总数标记；有效索引范围 [0, 6]；
    // 用于 page_next()/page_prev() 的循环边界，
    // 以及 log_cache_page_ 初始化为无效状态
};

// ── 设置菜单项枚举 ───────────────────────────────────────────────────
enum class SettingsItem : uint8_t {
// 强类型枚举：SETTINGS 页面中可调节参数项；settings_item_ 记录当前光标所在项

    CONF_THRESHOLD = 0,  // 置信度阈值（0.10~0.95，步进0.05）
    // 运行时通过 get_conf_threshold() 供推理模块读取

    ALARM_DELAY    = 1,  // 报警延迟秒数（0~60，步进5）
    // 运行时通过 get_alarm_delay() 供报警模块读取

    ITEM_COUNT     = 2,
    // 菜单项总数；on_key_mode_short() 在 EDIT 模式下循环切换时的边界
};

// ── 菜单交互模式 ─────────────────────────────────────────────────────
// NORMAL   : LEFT/RIGHT 切换页面，MODE 进入查看模式
// VIEW     : 已进入某页查看（Log翻页）；MODE 短按退出；长按2s进入 EDIT
// EDIT     : 设置模式，光标闪烁；LEFT/RIGHT 调值；MODE 短按切换项，长按退出
enum class MenuMode : uint8_t {
// 强类型枚举：菜单交互模式；menu_mode_ 与 cur_page_ 共同决定按键响应逻辑

    NORMAL = 0,
    // 正常导航：LEFT/RIGHT 切换页面；MODE 短按进 VIEW；MODE 超长按进 EDIT

    VIEW   = 1,
    // 页面查看：LOG 页 LEFT/RIGHT 翻页；MODE 短按退出；MODE 长按进 EDIT

    EDIT   = 2,
    // 参数编辑（仅 SETTINGS 页）：光标闪烁；LEFT/RIGHT 调值；
    // MODE 短按切换参数项；MODE 长按退出 EDIT
};

// ────────────────────────────────────────────────────────────────────
class ExtendedFeaturesFramework : public IExtendedFeatures {
// 扩展功能框架具体实现类；继承 IExtendedFeatures，集中管理五类外设功能；
// 所有 GPIO/UART 访问通过 pigpio 库完成，须在调用本类方法前完成 gpioInitialise()

public:
    ExtendedFeaturesFramework();
    // 构造：初始化所有成员为安全默认值；调用 gpioInitialise() 记录结果到 pigpio_initialized_；
    // 不执行硬件操作，具体初始化由各 initialize_*() 方法触发

    ~ExtendedFeaturesFramework() override;
    // 析构：shutdown_menu() → relay_reset_all() → led_all_off() →
    // 关闭 serial_fd_ → gpioTerminate()（若 pigpio 已初始化）

    // ── PWR_OFF ────────────────────────────────────────────────────
    void      set_shutdown_callback(std::function<void()> cb);
    // 注册掉电关机回调（须在 initialize_pwr_off() 之前调用）；
    // PWR_OFF 引脚检测到掉电信号后由 on_pwr_off() 调用此回调

    ErrorCode initialize_pwr_off();
    // 初始化 PWR_OFF 掉电检测引脚：注册 pwr_off_alert 静态回调 + 配置上拉输入；
    // 掉电信号为低电平（下降沿）触发

    // ── 4-8mA 状态信号 ─────────────────────────────────────────────
    ErrorCode output_standby();
    // 输出待机状态电流（4 mA，code=0）；4 mA 为电流环零点，表示"在线但待机"

    ErrorCode output_infer_failed();
    // 输出推理失败状态电流（约 5.3 mA，code=1）；PLC 可据此判断设备需维护

    ErrorCode output_reserved_6();
    // 输出预留状态信号 6（约 6.67 mA，code=2）；预留给未来扩展

    ErrorCode output_reserved_7();
    // 输出预留状态信号 7（约 8 mA，code=3）；4~8 mA 范围上限

    // ── 8-20mA 分类结果 ────────────────────────────────────────────
    ErrorCode output_ai_result_current(int class_id, float confidence);
    // 根据推理类别 ID 输出 8~20 mA 电流；8 mA=正常，20 mA=最严重故障；
    // 委托 rs485_send_frame() 打包发送 RS485 帧

    ErrorCode output_ai_result_current(const ModelOutput& output);
    // 重载：直接接受 ModelOutput，提取 class_id 和 confidence 后调用上方方法

    // ── 继电器 ─────────────────────────────────────────────────────
    ErrorCode initialize_relay();
    // 初始化 BCM17（K1 轴承）和 BCM27（K2 火焰）继电器控制引脚为输出低电平

    ErrorCode relay_set(unsigned int gpio, bool on);
    // 设置指定 BCM 引脚继电器通断（true=吸合/高电平，false=断开/低电平）

    ErrorCode relay_reset_all();
    // 复位全部继电器到安全断开状态；系统关机或推理停止时调用

    // ── 74HC595D LED / 蜂鸣器 ──────────────────────────────────────
    ErrorCode initialize_hc595();
    // 初始化 74HC595D 的 SH_CP/ST_CP/DS GPIO 引脚；全熄初始状态输出到硬件

    ErrorCode led_set(LedChannel ch, bool on);
    // 更新 hc595_shadow_[channel] 后调用 hc595_flush() 整体输出到硬件

    ErrorCode led_all_off();
    // hc595_shadow_ 全零后 hc595_flush()；系统关机/复位/省电模式时调用

    ErrorCode led_write_raw(uint8_t state);
    // 直接写入 8 位原始状态字节（原子更新全部 8 路通道）；用于开机自检流水灯

    // ── OLED 菜单系统 ──────────────────────────────────────────────

    /**
     * @brief 初始化菜单：配置三个按键、启动刷新线程
     *
     * 调用后 OLED 显示 STANDBY 画面，等待用户按键进入菜单。
     * 必须在 pigpio 初始化后调用。
     *
     * @param dl  DisplayLoggingFramework 指针（不转移所有权）
     */
    ErrorCode initialize_menu(DisplayLoggingFramework* dl);
    // 保存 dl 指针（不持有所有权）→ 注册三键中断回调 →
    // 启动 menu_thread_（menu_running_=true）→ 显示 STANDBY → menu_inited_=true

    /** 停止菜单刷新线程并释放按键中断 */
    void shutdown_menu();
    // menu_running_=false → join menu_thread_ → 注销三键回调 → menu_inited_=false

    /**
     * @brief 更新 Project Status 快照（每次 AI 推理后调用，线程安全）
     * @param snap  最新状态快照
     */
    void update_menu_snapshot(const MenuSnapshot& snap);
    // 加锁（menu_mtx_）覆盖 snap_ 成员（非阻塞写，加锁时间极短）；
    // menu_loop() 定期加锁读取 snap_ 刷新 OLED 的 PROJECT_STATUS 页面

    // ── 设置值读取接口（供推理模块使用）──────────────────────────
    float get_conf_threshold() const;   // 当前置信度阈值
    // 返回 conf_threshold_（范围 [0.10, 0.95]，初始 0.50f）；
    // AIModelFramework 应使用此值替代配置文件中的静态阈值

    int   get_alarm_delay()    const;   // 当前报警延迟（秒）
    // 返回 alarm_delay_（范围 [0, 60]，初始 0）；
    // 报警模块在触发继电器/蜂鸣器前应等待此秒数

    // ── 原有接口 ───────────────────────────────────────────────────
    ErrorCode check_for_updates()  override;
    // 连接远程更新服务器，检查是否有新版本固件或模型可用

    ErrorCode perform_update()     override;
    // 下载并安装更新（固件/模型）；更新期间系统状态切换为 UPDATING

    ErrorCode switch_to_bootloader()  override;
    // 切换到 Bootloader 模式（重启进入 OTA 更新引导模式）

    ErrorCode switch_to_application() override;
    // 从 Bootloader 切换回应用程序模式

    ErrorCode verify_checksum()    override;
    // 验证固件/模型文件的 CRC 校验和，与 ENABLE_CRC_CHECK 宏配合

    ErrorCode send_sms_alert(const std::string& message,
                             const std::vector<std::string>& phone_numbers) override;
    // 通过短信网关 API 向 phone_numbers 发送报警短信；
    // 与 AlarmConfig::Methods::enable_sms 及 alarm_recipients 配置对应

    ErrorCode send_email_alert(const std::string& subject,
                               const std::string& message) override;
    // 通过 SMTP 发送报警邮件；邮件服务器配置从 SystemConfig::NetworkConfig 读取

    ErrorCode connect_to_server(const std::string& server_url) override;
    // 建立到远程监控服务器的 TCP/HTTP 连接；失败返回 CONNECTION_FAILED

    ErrorCode send_diagnostic_data(const SystemStatus& status) override;
    // 将系统健康状态序列化为 JSON 后 HTTP POST 上传到远程服务器

    ErrorCode auto_calibrate()     override;
    // 触发全系统自动校准（振动零偏校准 + 摄像头白平衡）；
    // 校准期间系统状态切换为 CALIBRATING

    ErrorCode set_power_mode(int mode) override;
    // 设置电源工作模式（0~3 = HIGH_PERFORMANCE/BALANCED/POWER_SAVING/SLEEP）；
    // 与 PowerManagementConfig::PowerMode 编码一致

    float     get_battery_level()  override;
    // 返回电池电量百分比（0.0~100.0）；
    // enable_battery_monitoring=false 时返回 -1.0f

private:
    // ── pigpio ─────────────────────────────────────────────────────
    bool pigpio_initialized_{false};
    // pigpio 全局初始化状态标志；构造函数 gpioInitialise() 成功后置 true；
    // 所有依赖 pigpio 的方法须先检查此标志

    // ── PWR_OFF ────────────────────────────────────────────────────
    std::function<void()> shutdown_callback_;
    // 掉电关机回调；set_shutdown_callback() 写入；on_pwr_off() 调用

    std::atomic<bool>     pwr_off_triggered_{false};
    // 掉电信号已触发标志（原子变量）；防止重复触发关机回调（边沿检测）

    static void pwr_off_alert(int gpio, int level, uint32_t tick, void* ud);
    // pigpio alert 静态回调：PWR_OFF 引脚电平变化时触发；
    // level=0（低电平=掉电信号）时经 ud 指针调用 on_pwr_off()；
    // static 是 pigpio C API 要求的回调签名形式

    void        on_pwr_off();
    // 掉电事件处理：pwr_off_triggered_.exchange(true) 防重复；
    // 若 shutdown_callback_ 非空则调用之

    // ── RS485 ──────────────────────────────────────────────────────
    int  serial_fd_{-1};
    // RS485 串口文件描述符，初始 -1（未打开）；
    // initialize_rs485() 通过 open("/dev/ttyAMA0", O_RDWR) 赋值

    bool rs485_initialized_{false};
    // RS485 初始化完成标志

    ErrorCode initialize_rs485();
    // 打开串口设备文件 → 配置波特率/8N1/原始模式 → 配置 DE/RE 引脚

    void      rs485_send_enable();
    // DE 引脚置高，切换 RS485 收发器到发送模式；write() 之前调用

    void      rs485_receive_enable();
    // DE 引脚置低，切换回接收模式；发送完成后立即调用，防止总线冲突

    ErrorCode rs485_send_frame(uint8_t type, uint8_t class_id,
                               float conf, float ma);
    // 构造并发送 RS485 帧：[0xAA][type][class_id][conf(4B)][ma(4B)][CRC16]；
    // 流程：rs485_send_enable() → write() → tcdrain() → rs485_receive_enable()

    ErrorCode output_status_signal(float ma, uint8_t code);
    // 内部辅助：向 RS485 输出指定电流值和状态码；
    // 由 output_standby() / output_infer_failed() 等方法委托

    // ── 继电器 ─────────────────────────────────────────────────────
    bool relay_initialized_{false};
    // 继电器控制引脚初始化标志

    void relay_update(int class_id);
    // 根据 AI 推理类别 ID 更新继电器状态：
    //   振动 class_id>0 → BCM17/K1 吸合；火焰 class_id=1 → BCM27/K2 吸合；
    //   其余复位

    // ── 74HC595D ───────────────────────────────────────────────────
    bool hc595_initialized_{false};
    // 74HC595D GPIO 引脚初始化标志

    std::array<bool, 8> hc595_shadow_{};
    // 8 路输出通道软件影像（Shadow Register），索引对应 LedChannel 枚举值；
    // led_set() 更新后调用 hc595_flush() 写入硬件；默认全零（全熄）

    void hc595_shift_bit(bool data);
    // 向 74HC595D 逐位移入一位：写 DS 引脚 → SH_CP 产生上升沿（数据移入）

    void hc595_flush();
    // 8 次 hc595_shift_bit() → ST_CP 锁存脉冲（8 路输出同步更新）；
    // 锁存脉冲确保各通道原子更新，避免中间闪烁

    void led_update(int class_id);
    // 根据推理类别 ID 自动更新 LED 和蜂鸣器：
    //   class_id=0（正常）：WORKING_NORMAL/FIRE_NORMAL 亮，其余熄灭；
    //   轴承故障：对应 BEARING_* 亮 + BUZZER；火焰：FIRE_ALARM 亮 + BUZZER；
    //   同时调用 relay_update()

    // ── OLED 菜单状态 ──────────────────────────────────────────────
    DisplayLoggingFramework* dl_{nullptr};
    // DisplayLoggingFramework 裸指针（不持有所有权）；
    // initialize_menu() 写入；用于读取/导出异常日志；
    // 调用方须确保 dl_ 所指对象生命周期长于本对象

    mutable std::mutex  menu_mtx_;
    // 菜单状态互斥锁（mutable 允许在 const 方法中加锁）；
    // 保护：cur_page_、menu_mode_、snap_、log_cache_、log_cache_page_、
    // shutdown_confirm_、settings_item_、conf_threshold_、alarm_delay_、cursor_visible_

    MenuPage            cur_page_{MenuPage::STANDBY};
    // 当前 OLED 菜单页面，初始 STANDBY；
    // page_next()/page_prev() 更新；menu_loop() 据此调用对应 render_* 函数

    MenuMode            menu_mode_{MenuMode::NORMAL};  // 当前交互模式
    // 当前菜单交互模式，初始 NORMAL；与 cur_page_ 共同决定按键响应行为

    bool                menu_inited_{false};
    // 菜单系统初始化完成标志；防止未初始化时执行菜单操作

    std::atomic<bool>   menu_running_{false};
    // 菜单刷新线程运行标志（原子变量）；
    // initialize_menu() 置 true；shutdown_menu() 置 false 触发 menu_loop() 退出

    std::thread         menu_thread_;
    // OLED 菜单刷新线程，运行 menu_loop()；shutdown_menu() 中须 join()

    // Project Status 快照
    MenuSnapshot snap_;
    // AI 推理状态快照（受 menu_mtx_ 保护）；
    // update_menu_snapshot() 在推理线程中写，render_project_status() 在菜单线程中读

    // 日志翻页（VIEW 模式下生效）
    int                         log_idx_{0};
    // 当前日志页面显示的条目索引，初始 0；
    // VIEW 模式 LEFT 递减，RIGHT 递增；范围 [0, log_cache_.size()-1]

    std::vector<AbnormalRecord> log_cache_;      // 真实缓存（受 menu_mtx_ 保护）
    // 当前日志页（LOG_BEARING/LOG_FIRE）的异常记录缓存；
    // reload_log_cache() 从 dl_ 读取填充；render_log_page() 按 log_idx_ 显示

    MenuPage                    log_cache_page_{MenuPage::PAGE_COUNT};
    // log_cache_ 当前缓存的页面标识，初始 PAGE_COUNT（无效）；
    // 切换日志页时据此判断是否需要重新调用 reload_log_cache()

    // 关机二次确认
    bool shutdown_confirm_{false};
    // 关机二次确认标志；MODE 短按首次置 true；再次短按执行关机；
    // 其他任意按键清除（取消关机）

    // 设置菜单（EDIT 模式）
    SettingsItem settings_item_{SettingsItem::CONF_THRESHOLD};
    // EDIT 模式下当前光标所在参数项，初始 CONF_THRESHOLD；
    // on_key_mode_short() 在 EDIT 模式下循环切换

    float        conf_threshold_{0.50f};      // 置信度阈值（0.10~0.95）
    // 当前置信度阈值，初始 0.50f；EDIT 模式 LEFT/RIGHT 以 CONF_STEP 调整；
    // 边界 [CONF_MIN, CONF_MAX]

    int          alarm_delay_{0};             // 报警延迟（0~60s）
    // 当前报警延迟秒数，初始 0；EDIT 模式 LEFT/RIGHT 以 DELAY_STEP 调整；
    // 边界 [DELAY_MIN, DELAY_MAX]

    static constexpr float CONF_STEP  = 0.05f;
    // 置信度步进，编译期常量；20 步覆盖 [0.10, 0.95] 全范围

    static constexpr float CONF_MIN   = 0.10f;
    // 置信度最小值；低于此值几乎所有推理都触发报警

    static constexpr float CONF_MAX   = 0.95f;
    // 置信度最大值；高于此值可能造成漏报

    static constexpr int   DELAY_STEP = 5;
    // 报警延迟步进（秒）；12 步覆盖 [0, 60] 全范围

    static constexpr int   DELAY_MIN  = 0;
    // 报警延迟最小值（0 秒，无延迟）

    static constexpr int   DELAY_MAX  = 60;
    // 报警延迟最大值（60 秒）；超过 60 秒在安全监测场景中可能延误响应

    // 光标闪烁（EDIT 模式，menu_loop 每 500ms 翻转）
    bool cursor_visible_{true};
    // EDIT 模式光标可见状态；menu_loop() 每 BLINK_PERIOD_MS 翻转；
    // render_settings() 据此绘制光标符号

    uint32_t last_blink_ms_{0};
    // 上次光标翻转时刻（单调时钟毫秒）；
    // menu_loop() 通过 mono_ms() - last_blink_ms_ >= BLINK_PERIOD_MS 判断是否翻转

    static constexpr uint32_t BLINK_PERIOD_MS = 500;
    // 光标闪烁周期 500 ms（1 Hz），编译期常量；视觉清晰且不易引起眩晕

    // ── 按键防抖 & 长按检测 ────────────────────────────────────────
    static constexpr uint32_t DEBOUNCE_MS      = 50;    // 去抖时间
    // 按键消抖阈值（50 ms），编译期常量；50 ms 内的重复 alert 视为机械抖动被忽略

    static constexpr uint32_t LONG_PRESS_MS    = 600;   // 普通长按（关机）
    // 普通长按阈值（600 ms），编译期常量；
    // MODE 键持续 ≥ 600 ms 且 < 2000 ms 判定为普通长按，触发 on_key_mode_long()

    static constexpr uint32_t LONG_PRESS_EDIT  = 2000;  // 长按2s进入EDIT模式
    // 超长按阈值（2000 ms），编译期常量；
    // MODE 键持续 ≥ 2000 ms 判定为超长按，触发 on_key_mode_very_long()；
    // 2s 阈值防止日常操作误触发进入设置模式

    // KEY_MODE：下降沿记录按下时刻，上升沿判定短/长按
    std::atomic<uint32_t> key_mode_press_ms_{0};
    // MODE 键按下时刻（单调时钟毫秒，原子变量）；
    // 下降沿记录 mono_ms()；上升沿计算时长后分发到三个响应函数

    std::atomic<uint32_t> key_mode_last_ms_{0};
    // MODE 键上次触发时刻（防抖用，原子变量）；
    // 触发间隔 < DEBOUNCE_MS 时忽略

    // KEY_LEFT / KEY_RIGHT：下降沿触发
    std::atomic<uint32_t> key_left_last_ms_{0};
    // LEFT 键上次触发时刻（防抖用）；防抖通过后调用 on_key_left()

    std::atomic<uint32_t> key_right_last_ms_{0};
    // RIGHT 键上次触发时刻（防抖用）；防抖通过后调用 on_key_right()

    // ── 菜单内部方法 ───────────────────────────────────────────────
    void menu_loop();
    // 菜单刷新线程主函数（menu_thread_ 中运行）：
    // 循环（menu_running_=true）：加锁复制菜单状态本地副本 + 光标翻转 →
    // 解锁 → render_page_unlocked()（传入本地副本刷新 OLED）→ sleep；
    // 锁内只做状态复制，锁外做 OLED I/O，最小化锁持有时间

    void render_page();
    // 加锁读取菜单状态后委托 render_page_unlocked()；
    // 用于初始化后立即显示首帧（不经 menu_loop() 等待延迟）

    // 渲染函数
    void render_standby();
    // 渲染待机页：Line1=系统名称，Line2=版本号，Line3=操作提示

    void render_project_status();
    // 渲染项目状态页（从 snap_ 读取）：
    //   振动：故障类别 + 置信度/健康指数 + RUL；火焰："Fire!" 或 "Normal" + 置信度 + FPS

    void render_log_page(MenuPage page);
    // 渲染日志页（LOG_BEARING/LOG_FIRE）：
    //   log_cache_ 为空则显示"No records"；否则 4 行模式显示 log_idx_ 条记录

    void render_export_page();
    // 渲染导出页；VIEW 模式进入后调用 do_export() 执行实际导出并显示结果

    void render_shutdown_page();
    // 渲染关机确认页：一次/二次确认两种显示状态

    void render_settings();           // EDIT 模式下渲染设置页（含光标闪烁）
    // 渲染设置页（4 行模式）：标题 + CONF_THRESHOLD 行 + ALARM_DELAY 行；
    // 当前选中项旁根据 cursor_visible_ 绘制光标

    // 锁外渲染（menu_loop 使用，所有状态通过参数传入，无竞态）
    void render_page_unlocked(MenuPage page, MenuMode mode,
                              const MenuSnapshot& snap,
                              bool cursor_visible, bool shutdown_confirm,
                              int log_idx,
                              SettingsItem settings_item,
                              float conf_threshold, int alarm_delay,
                              const std::vector<AbnormalRecord>& log_cache);
    // 核心渲染分发函数（锁外版本）：menu_loop() 释放锁后调用，
    // 所有菜单状态通过参数传入（本地副本），无需访问被锁保护的成员；
    // 此设计消除"持锁执行 OLED I2C 写入（约 1~10 ms）"的性能问题

    // 日志操作（reload_log_cache 改为仅在锁外调用）
    void reload_log_cache(MenuPage page);
    // 重新加载日志页数据（须在 menu_mtx_ 外调用，磁盘 I/O 耗时）：
    // dl_->load_week_abnormal() 读取近 7 天记录 → 加锁更新 log_cache_ 和 log_cache_page_

    void do_export();
    // 触发日志导出（须在 menu_mtx_ 外调用）：
    // 调用 dl_->export_week_logs() 导出 CSV 和截图

    // 按键响应（重构后的三键逻辑）
    void on_key_mode_short();         // MODE 短按
    // NORMAL → 进 VIEW；VIEW(SHUTDOWN) → shutdown_confirm_ 切换；
    // VIEW(其他) → 返 NORMAL；EDIT → settings_item_ 切换下一项

    void on_key_mode_long();          // MODE 长按（≥600ms，关机确认/切换项）
    // EDIT → 退出 EDIT 返 VIEW；
    // VIEW(SHUTDOWN 已二次确认) → 执行关机

    void on_key_mode_very_long();     // MODE 超长按（≥2000ms，进入/退出EDIT）
    // NORMAL/VIEW → 进入 SETTINGS 页的 EDIT 模式；EDIT → 退出返 NORMAL

    void on_key_left();               // LEFT：上一页 / 上一条记录 / 减小值
    // NORMAL → page_prev()；VIEW(LOG) → log_idx_--；
    // EDIT(CONF) → conf_threshold_ -= CONF_STEP；EDIT(DELAY) → alarm_delay_ -= DELAY_STEP

    void on_key_right();              // RIGHT：下一页 / 下一条记录 / 增大值
    // NORMAL → page_next()；VIEW(LOG) → log_idx_++；
    // EDIT(CONF) → conf_threshold_ += CONF_STEP；EDIT(DELAY) → alarm_delay_ += DELAY_STEP

    // 页面切换（NORMAL 模式）
    void page_next();
    // cur_page_ = (cur_page_ + 1) % PAGE_COUNT，然后 render_page()

    void page_prev();
    // cur_page_ = (cur_page_ + PAGE_COUNT - 1) % PAGE_COUNT，然后 render_page()

    // pigpio alert 静态回调
    static void cb_key_mode (int gpio, int level, uint32_t tick, void* ud);
    // MODE 键 alert 回调：下降沿记录 key_mode_press_ms_；
    // 上升沿根据持续时长分发到 short/long/very_long；
    // key_mode_last_ms_ + DEBOUNCE_MS 防抖

    static void cb_key_left (int gpio, int level, uint32_t tick, void* ud);
    // LEFT 键 alert 回调：仅响应下降沿；key_left_last_ms_ 防抖后调用 on_key_left()

    static void cb_key_right(int gpio, int level, uint32_t tick, void* ud);
    // RIGHT 键 alert 回调：仅响应下降沿；key_right_last_ms_ 防抖后调用 on_key_right()

    // OLED 工具
    static void oled2(const std::string& l1, bool cn1,
                      const std::string& l2, bool cn2);
    // 封装 drawOLED() 显示两行文本；cn1/cn2 = true 使用 wqy16 中文字体

    static void oled3(const std::string& l1, bool cn1,
                      const std::string& l2, bool cn2,
                      const std::string& l3, bool cn3);
    // 封装 drawOLED3() 显示三行文本；9x18B 大字体，适合远距离观察

    static void oled4(const std::string& l1, bool cn1,
                      const std::string& l2, bool cn2,
                      const std::string& l3, bool cn3,
                      const std::string& l4, bool cn4);
    // 封装 drawOLED4() 显示四行文本；6x13 小字体（约 21 英文字符/行），
    // 适合日志记录详情和设置菜单等信息密集场景

    static std::string fmt_conf(float c);
    // 置信度格式化：c=0.923f → "0.92"（保留两位小数，固定宽度）

    static std::string fmt_ma(float ma);
    // 电流值格式化（含单位）：ma=12.5f → "12.5mA"

    static uint32_t    mono_ms();
    // 返回单调时钟当前时刻（毫秒，uint32_t）；
    // 使用 std::chrono::steady_clock 或 gpioTick()（微秒→毫秒）；
    // uint32_t 约 49 天溢出，对系统运行时间影响可忽略；
    // 为防抖、长按检测和光标闪烁提供统一时间基准
};

} // namespace MultiProject