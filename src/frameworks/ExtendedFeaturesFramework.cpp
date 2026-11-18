/**
  ******************************************************************************
  * @file    ExtendedFeaturesFramework.cpp
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
 * @brief   扩展功能框架 — 实现文件【第一部分：第1~460行原码】
 * ----------------------------------------------------------------------------
 * 本文件实现 ExtendedFeaturesFramework.hpp 中声明的全部方法，涵盖以下模块：
 *
 *  常量 / GPIO 定义
 *    PWM（BCM18，1 kHz 硬件 PWM，4~20 mA 电流环输出）
 *    4~20 mA 电流值分配表（待机/推理失败/各故障类别/火焰检测）
 *    RS485（/dev/ttyAMA0，9600 baud，DE控制 BCM24）
 *    继电器（K1=BCM17 轴承项目，K2=BCM27 火焰项目）
 *    74HC595D 串行 LED 驱动（DS=BCM23，STCP=BCM26，SHCP=BCM22）
 *    按键（BCM5 MODE，BCM6 LEFT，BCM13 RIGHT）
 *    菜单刷新间隔（100 ms）
 *
 *  PWM 工具（ma_to_duty / pwm_output）
 *    将 mA 值线性换算为 pigpio 占空比（1,000,000 分辨率）
 *
 *  构造 / 析构
 *    构造：pigpio 初始化 → PWM 待机 → RS485/继电器/HC595 初始化
 *    析构：停止菜单线程 → 关所有 LED → 复位继电器 → PWM 待机 → 关闭串口
 *
 *  RS485 通信
 *    initialize_rs485（termios 配置：9600,8E1,VTIME=10）
 *    rs485_send_enable / rs485_receive_enable（DE 引脚半双工切换）
 *    rs485_send_frame（8字节帧：帧头+类型+类别+置信度×10000+mA整数+mA小数）
 *
 *  继电器（initialize_relay / relay_set / relay_reset_all / relay_update）
 *
 *  74HC595D LED 驱动
 *    initialize_hc595 / hc595_shift_bit / hc595_flush（SHCP+STCP 时序）
 *    led_set / led_all_off / led_write_raw / led_update
 *
 *  状态信号输出
 *    output_status_signal / output_standby / output_infer_failed
 *    output_ai_result_current（支持 class_id+conf 及 ModelOutput 两种重载）
 *
 *  PWR_OFF 断电保护（BCM4 电平监听 → pigpio alert 回调）
 *
 *  设置值对外接口（get_conf_threshold / get_alarm_delay）
 *
 *  OLED 菜单工具（mono_ms / fmt_conf / fmt_ma / oled2 / oled3 / oled4）
 *
 *  initialize_menu / shutdown_menu / update_menu_snapshot
 *
 * @note  菜单刷新间隔由 500 ms 改为 100 ms，保证按键响应及时和光标闪烁流畅
 * ============================================================================
 */

#include "ExtendedFeaturesFramework.hpp"  // 引入头文件
// 引入扩展功能框架完整声明，含 MenuPage/MenuMode/MenuSnapshot/LedChannel 等枚举
// AbnormalRecord、SettingsItem 等结构体，及所有方法签名

#include "DisplayLoggingFramework.hpp"  // 引入头文件
// 引入显示与日志框架声明，用于菜单内调用 load_week_abnormal() 和 export_week_logs()

#include "../common/ProjectDefines.hpp"  // 引入头文件
// 引入公共项目定义：ProjectType 枚举（BEARING_VIBRATION / FIRE_DETECTION）
// 和 CURRENT_PROJECT 宏，供 relay_update() / led_update() 的条件编译分支使用

#include <iostream>  // 引入头文件
// std::cout / std::cerr：构造初始化进度打印、PWM 状态打印和错误信息输出

#include <cmath>  // 引入头文件
// std::max / std::min / std::floor / std::round：ma_to_duty 中的数值裁剪和取整

#include <algorithm>  // 引入头文件
// std::max / std::min（与 <cmath> 协同）；字符串截断用 std::min<size_t>

#include <cstring>  // 引入头文件
// memset / strlen 等 C 字符串函数；termios 结构体初始化和串口操作预留

#include <cstdio>  // 引入头文件
// FILE* / fopen / fprintf / fclose / snprintf：日志写入和格式化字符串

#include <ctime>  // 引入头文件
// std::time / std::localtime / std::strftime：on_pwr_off() 中记录断电时间戳

#include <sstream>  // 引入头文件
// std::ostringstream：字符串格式化拼接（render_* 函数中的 UI 文本构造预留）

#include <iomanip>  // 引入头文件
// std::setw / std::setprecision：格式化输出宽度和精度（与 sstream 协同预留）

#include <fcntl.h>  // 引入头文件
// O_RDWR / O_NOCTTY / O_NDELAY：initialize_rs485() 中以非阻塞读写模式打开串口

#include <unistd.h>  // 引入头文件
// close() / write() / tcdrain() / tcflush() / tcgetattr() / tcsetattr()：
// 串口文件描述符操作和 POSIX termios 配置

#include <termios.h>  // 引入头文件
// struct termios / cfsetispeed / cfsetospeed：RS485 串口波特率、格式和模式配置

#include <sys/ioctl.h>  // 引入头文件
// ioctl()：串口扩展控制（RS485 方向控制的底层接口，部分实现可能通过 ioctl 实现）

extern "C" {  // 以 C 链接方式引入 C 库（禁止 C++ 名称修饰）
// extern "C" 块：禁止 C++ 名称修饰（name mangling），以 C 链接方式引入以下两个 C 库
#include <pigpio.h>  // 引入头文件
// 引入 pigpio 库：gpioInitialise / gpioTerminate / gpioSetMode / gpioWrite / gpioRead
// gpioHardwarePWM / gpioSetAlertFuncEx / gpioSetPullUpDown 等 GPIO 控制函数
#include "oled.h"  // 引入头文件
// 引入 OLED 驱动 C 接口：drawOLED / drawOLED3 / drawOLED4
// 对应 u8g2 Full Buffer 模式的四行显示封装
}  // 块结束

namespace MultiProject {  // 开启命名空间
// 开启项目命名空间 MultiProject

// ============================================================================
// 常量 / GPIO 定义
// ============================================================================

static constexpr unsigned int PWM_PIN      = 18;  // 编译期常量定义
// PWM 输出引脚：BCM18（H1 Pin35→实物Pin36），支持硬件 PWM（pigpio gpioHardwarePWM）
static constexpr unsigned int PWM_FREQ_HZ  = 1000;  // 编译期常量定义
// PWM 频率：1 kHz；工业 4~20 mA 电流环的标准调制频率
static constexpr float        LINEAR_K     = 50.580f;  // 编译期常量定义
// 线性变换系数 K：电流环 D/A 转换的斜率参数（mA → 占空比公式）
static constexpr float        LINEAR_B     = -0.0153f;  // 编译期常量定义
// 线性变换截距 B：补偿电流环零点偏移的修正量
static constexpr float        MA_MIN       = 4.0f;  // 编译期常量定义
// 电流环最小值：4 mA（IEC 61131-2 标准的活跃范围下限，低于此值表示断线）
static constexpr float        MA_MAX       = 20.0f;  // 编译期常量定义
// 电流环最大值：20 mA（满量程输出，对应最大故障严重程度或火焰确认）

static constexpr float MA_STANDBY          = 4.0f;  // 编译期常量定义
// 待机电流：4 mA（系统上电但未开始推理，对应电流环下限）
static constexpr float MA_INFER_FAIL       = 5.0f;  // 编译期常量定义
// 推理失败电流：5 mA（模型加载失败或推理异常，与待机区分）
static constexpr float MA_RESERVED_6       = 6.0f;  // 编译期常量定义
// 预留状态6：6 mA（保留给未来功能扩展）
static constexpr float MA_RESERVED_7       = 7.0f;  // 编译期常量定义
// 预留状态7：7 mA（保留给未来功能扩展）
static constexpr float MA_CLASS_BASE       = 8.0f;  // 编译期常量定义
// 类别电流基准：8 mA（AI 推理结果输出的起始电流，各类别在此基础上线性插值）

static constexpr float BEARING_DENORMAL    = 11.0f;  // 编译期常量定义
// 轴承正常（de_normal）对应满置信度电流：11 mA
static constexpr float BEARING_BALLFAULT   = 14.0f;  // 编译期常量定义
// 轴承滚珠故障（de_ballfault）满置信度电流：14 mA
static constexpr float BEARING_CAGEFAULT   = 17.0f;  // 编译期常量定义
// 轴承保持架故障（de_cagefault）满置信度电流：17 mA
static constexpr float BEARING_OUTERRACE   = 20.0f;  // 编译期常量定义
// 轴承外圈故障（de_outerrace）满置信度电流：20 mA（满量程，最严重故障）
static constexpr float FIRE_NORMAL_MA      = 14.0f;  // 编译期常量定义
// 火焰检测正常（normal）满置信度电流：14 mA
static constexpr float FIRE_FIRE_MA        = 20.0f;  // 编译期常量定义
// 火焰检测告警（fire）满置信度电流：20 mA（满量程，触发消防联动）

static constexpr unsigned int PWR_OFF_PIN  = 4;  // 编译期常量定义
// 断电检测引脚：BCM4，监听 UPS 电源状态；边沿中断触发安全关机流程

static constexpr unsigned int RS485_DE_PIN = 24;  // 编译期常量定义
// RS485 方向控制引脚：BCM24（DE/RE 引脚），HIGH=发送，LOW=接收（半双工切换）
static constexpr const char*  RS485_PORT   = "/dev/ttyAMA0";  // 编译期常量定义
// RS485 对应的 Linux 串口设备文件（树莓派 UART0，需在 /boot/config.txt 启用）
static constexpr int          RS485_BAUD   = B9600;  // 编译期常量定义
// RS485 波特率：9600 bps（termios B9600 常量），工业现场的标准通信速率
static constexpr uint8_t      FRAME_H1     = 0x55;  // 编译期常量定义
// RS485 帧头字节 1：0x55（01010101，便于起始检测和同步）
static constexpr uint8_t      FRAME_H2     = 0xAA;  // 编译期常量定义
// RS485 帧头字节 2：0xAA（10101010，与 H1 互补，降低误帧率）
static constexpr uint8_t      FRAME_STATUS = 0x01;  // 编译期常量定义
// 帧类型：状态帧（待机/失败等系统状态，无 AI 推理结果）
static constexpr uint8_t      FRAME_RESULT = 0x02;  // 编译期常量定义
// 帧类型：结果帧（AI 推理成功后携带类别 ID 和置信度的数据帧）
static constexpr size_t       FRAME_LEN    = 8;  // 编译期常量定义
// RS485 数据帧总长度：8 字节（H1+H2+Type+ClassID+Conf_H+Conf_L+mA_Int+mA_Dec）

static constexpr unsigned int RELAY_BEARING_PIN = 17;  // 编译期常量定义
// 继电器 K1 控制引脚：BCM17，轴承振动项目的报警继电器（故障时闭合）
static constexpr unsigned int RELAY_FIRE_PIN    = 27;  // 编译期常量定义
// 继电器 K2 控制引脚：BCM27，火焰检测项目的消防联动继电器（检测到火焰时闭合）

static constexpr unsigned int HC595_DS   = 23;  // 编译期常量定义
// 74HC595D 串行数据引脚：BCM23（DS，Serial Data Input）
static constexpr unsigned int HC595_STCP = 26;  // 编译期常量定义
// 74HC595D 存储时钟引脚：BCM26（STCP，Storage Clock，上升沿将移位寄存器→输出锁存）
static constexpr unsigned int HC595_SHCP = 22;  // 编译期常量定义
// 74HC595D 移位时钟引脚：BCM22（SHCP，Shift Clock，上升沿将 DS 数据移入移位寄存器）
static constexpr uint8_t CH_WORK    = 0;  // 编译期常量定义
// LED 通道 0：CH_WORK（系统工作指示灯，正常运行时点亮）
static constexpr uint8_t CH_B_OUTER = 1;  // 编译期常量定义
// LED 通道 1：CH_B_OUTER（轴承外圈故障指示灯）
static constexpr uint8_t CH_F_OK    = 2;  // 编译期常量定义
// LED 通道 2：CH_F_OK（火焰检测正常状态指示灯）
static constexpr uint8_t CH_B_CAGE  = 3;  // 编译期常量定义
// LED 通道 3：CH_B_CAGE（轴承保持架故障指示灯）
static constexpr uint8_t CH_B_BALL  = 4;  // 编译期常量定义
// LED 通道 4：CH_B_BALL（轴承滚珠故障指示灯）
static constexpr uint8_t CH_F_ALARM = 5;  // 编译期常量定义
// LED 通道 5：CH_F_ALARM（火焰报警指示灯，火焰检测触发时点亮）
static constexpr uint8_t CH_DEV_ERR = 6;  // 编译期常量定义
// LED 通道 6：CH_DEV_ERR（设备错误/故障总指示灯，任何异常时点亮）
static constexpr uint8_t CH_BEE     = 7;  // 编译期常量定义
// LED 通道 7：CH_BEE（蜂鸣器驱动通道，异常时通过 HC595 驱动蜂鸣器）

static constexpr unsigned int KEY_MODE_PIN  = 16;  // 编译期常量定义
// MODE 按键引脚：BCM16（内部上拉，低电平有效），支持短按/长按/超长按三级判定
static constexpr unsigned int KEY_LEFT_PIN  = 20;  // 编译期常量定义
// LEFT 按键引脚：BCM20（内部上拉，低电平有效），下降沿触发向前翻页/减小设置值
static constexpr unsigned int KEY_RIGHT_PIN = 21;  // 编译期常量定义
// RIGHT 按键引脚：BCM21（内部上拉，低电平有效），下降沿触发向后翻页/增大设置值

// 刷新间隔：100ms，保证按键响应及时、光标闪烁流畅
// （原500ms会让RIGHT键感觉"无反应"）
static constexpr int MENU_PERIOD_MS = 100;  // 编译期常量定义
// 菜单后台刷新线程的休眠间隔：100 ms
// 从 500 ms 缩短至 100 ms 的原因：500 ms 时 RIGHT 键按下到响应最长需等 500 ms，
// 用户感知"无反应"；100 ms 使最大响应延迟降至 100 ms，光标闪烁也更流畅

// ============================================================================
// PWM 工具
// ============================================================================

static unsigned int ma_to_duty(float ma) {  // 代码行
// 静态工具函数：将 4~20 mA 电流值转换为 pigpio 硬件 PWM 占空比（0~1,000,000 分辨率）
    ma  = std::max(MA_MIN, std::min(MA_MAX, ma));  // 代码行
    // 先将 ma 裁剪到 [4.0, 20.0] 的合法范围，防止越界输出
    float pct = 100.0f - (150.0f / 120.0f) * 2.0f * (ma - LINEAR_B) / LINEAR_K * 100.0f;  // 代码行
    // 反推占空比百分比：基于线性变换公式 ma = LINEAR_K × duty% + LINEAR_B 的逆变换
    // 100.0f - (...) 是因为 PWM 低占空比对应高电流（反向驱动的 D/A 转换器特性）
    pct = std::max(0.0f, std::min(100.0f, pct));  // 代码行
    // 将百分比裁剪到 [0, 100]，防止线性外推产生负值或超过满量程
    unsigned int d = static_cast<unsigned int>(pct * 10000.0f + 0.5f);  // 代码行
    // 将百分比转换为 pigpio 占空比整数（范围 0~1,000,000）：×10000 + 0.5 四舍五入
    return d > 1000000u ? 1000000u : d;  // 返回
    // 最终限幅：pigpio 硬件 PWM 的占空比上限为 1,000,000（100.0000%）
}  // 块结束

static bool pwm_output(float ma) {  // 代码行
// 静态函数：通过 pigpio 硬件 PWM 输出指定 mA 电流，成功返回 true
    int r = gpioHardwarePWM(PWM_PIN, PWM_FREQ_HZ, ma_to_duty(ma));  // 代码行
    // 调用 pigpio 硬件 PWM 接口：PIN=BCM18，频率=1 kHz，占空比由 ma_to_duty 换算
    if (r != 0) { std::cerr << "[PWM] 失败 ma=" << ma << " ret=" << r << std::endl; return false; }  // 条件判断
    // 非零返回值表示 pigpio 错误（GPIO 不支持硬件 PWM 或 pigpio 未初始化）
    std::cout << "[PWM] " << ma << "mA" << std::endl;  // 打印信息
    // 打印当前 PWM 输出电流值，便于调试和运行日志
    return true;  // 返回
    // 输出成功
}  // 块结束

// ============================================================================
// 构造 / 析构
// ============================================================================

ExtendedFeaturesFramework::ExtendedFeaturesFramework() {  // 代码行
// 构造函数：依次初始化 pigpio、PWM 待机、RS485、继电器、HC595 LED 驱动
    if (gpioInitialise() < 0) {  // 条件判断
    // 初始化 pigpio 库（必须最先调用，否则所有 GPIO 操作均无效）
        std::cerr << "[EXT] pigpio 初始化失败" << std::endl;  // 打印错误
        // 初始化失败（可能是权限不足或已有其他进程占用）
        pigpio_initialized_ = false;  // 代码行
        // 标记 pigpio 未就绪，后续所有 GPIO 依赖操作将跳过
    } else {  // else 分支
        pigpio_initialized_ = true;  // 代码行
        // 标记 pigpio 就绪
        pwm_output(MA_STANDBY);  // 代码行
        // 上电后立即输出待机电流 4 mA，告知上位机系统已上电但尚未推理
    }  // 块结束
    if (initialize_rs485()  == ErrorCode::SUCCESS) std::cout << "[RS485] 就绪" << std::endl;  // 条件判断
    // 初始化 RS485 串口（termios 配置），成功则打印就绪信息
    if (initialize_relay()  == ErrorCode::SUCCESS) std::cout << "[RELAY] 就绪" << std::endl;  // 条件判断
    // 初始化继电器 GPIO 引脚，成功则打印就绪信息
    if (initialize_hc595()  == ErrorCode::SUCCESS) std::cout << "[HC595] 就绪" << std::endl;  // 条件判断
    // 初始化 74HC595D LED 驱动链路，成功则打印就绪信息
}  // 块结束

ExtendedFeaturesFramework::~ExtendedFeaturesFramework() {  // 代码行
// 析构函数：安全关闭所有外设，确保系统下电状态清洁
    shutdown_menu();  // 代码行
    // 停止 OLED 菜单后台刷新线程，等待 menu_thread_ join()
    led_all_off();  // 代码行
    // 关闭所有 LED 和蜂鸣器（将 HC595 shadow 全清零并刷新）
    relay_reset_all();  // 代码行
    // 复位所有继电器（断开 K1 和 K2 的触点）
    if (pigpio_initialized_) { pwm_output(MA_STANDBY); gpioTerminate(); }  // 条件判断
    // pigpio 已初始化：先将 PWM 恢复待机电流 4 mA，再调用 gpioTerminate() 释放 GPIO 资源
    if (serial_fd_ >= 0) { rs485_receive_enable(); close(serial_fd_); }  // 条件判断
    // 串口已打开：先将 DE 置低（切换到接收模式，释放总线控制权），再关闭文件描述符
}  // 块结束

// ============================================================================
// RS485
// ============================================================================

ErrorCode ExtendedFeaturesFramework::initialize_rs485() {  // 代码行
// 初始化 RS485 半双工串行通信接口
    if (pigpio_initialized_) {  // 条件判断
    // pigpio 已就绪时才能操作 GPIO
        gpioSetMode(RS485_DE_PIN, PI_OUTPUT);  // 代码行
        // 将 BCM24（DE/RE 控制引脚）设置为输出模式
        gpioWrite(RS485_DE_PIN, 0);  // 代码行
        // 默认置低（接收使能）：上电时 RS485 总线处于接收状态，不主动发送
    }  // 块结束
    serial_fd_ = open(RS485_PORT, O_RDWR | O_NOCTTY | O_NDELAY);  // 代码行
    // 以读写、非控制终端、非阻塞模式打开串口设备文件 /dev/ttyAMA0
    // O_NOCTTY：不将此串口设为进程的控制终端（防止接收到终端信号）
    // O_NDELAY：非阻塞打开（不等待载波信号 DCD）
    if (serial_fd_ < 0) return ErrorCode::HARDWARE_INIT_FAILED;  // 条件判断
    // 打开失败（设备不存在、权限不足或 UART 未启用）

    struct termios opt{};  // 代码行
    // 声明 termios 结构体并初始化为全零（{} 零初始化）
    tcgetattr(serial_fd_, &opt);  // 代码行
    // 读取当前串口的 termios 配置（在此基础上修改，而非从空配置开始）
    cfsetispeed(&opt, RS485_BAUD);  cfsetospeed(&opt, RS485_BAUD);  // 代码行
    // 设置输入和输出波特率均为 B9600（9600 bps）
    opt.c_cflag &= ~CSIZE;  opt.c_cflag |= CS8;  // 代码行
    // 清除字长位后设置为 8 位字长（CS8）
    opt.c_cflag |= PARENB;  // 代码行
    // 启用奇偶校验（PARENB：Parity Enable）
    opt.c_cflag &= ~CSTOPB; opt.c_cflag |= CREAD | CLOCAL;  // 代码行
    // 清除 CSTOPB 设置 1 位停止位；CREAD 启用接收；CLOCAL 忽略载波信号
    opt.c_iflag &= ~(IXON | IXOFF | IXANY); opt.c_iflag |= INPCK;  // 代码行
    // 关闭软件流控（XON/XOFF），启用奇偶校验错误检测（INPCK）
    opt.c_oflag = 0; opt.c_lflag = 0;  // 代码行
    // 输出标志和本地标志全清零：raw 模式（无输出处理，无回显，无特殊字符）
    opt.c_cc[VMIN] = 0; opt.c_cc[VTIME] = 10;  // 代码行
    // VMIN=0：非阻塞读（即使无数据也立即返回）
    // VTIME=10：读超时 10×0.1s = 1.0 秒（配合 VMIN=0 形成超时读）
    tcflush(serial_fd_, TCIFLUSH);  // 代码行
    // 清空接收缓冲区中残留的历史数据（TCIFLUSH 仅清空输入缓冲）
    if (tcsetattr(serial_fd_, TCSANOW, &opt) != 0) {  // 条件判断
    // 立即应用新的 termios 配置（TCSANOW：无延迟立即生效）
        close(serial_fd_); serial_fd_ = -1;  // 代码行
        // 配置失败时关闭文件描述符并重置为 -1
        return ErrorCode::HARDWARE_INIT_FAILED;  // 返回
        // 返回硬件初始化失败错误码
    }  // 块结束
    rs485_initialized_ = true;  // 代码行
    // 标记 RS485 初始化成功
    return ErrorCode::SUCCESS;  // 返回
    // 初始化完成
}  // 块结束

void ExtendedFeaturesFramework::rs485_send_enable() {  // 代码行
// 切换 RS485 到发送模式（DE 置高，禁止远端发送，允许本端发送）
    if (pigpio_initialized_) gpioWrite(RS485_DE_PIN, 1);  // 条件判断
    // DE=HIGH：使能驱动器（Driver Enable），禁止接收器（/RE=HIGH）
    if (serial_fd_ >= 0) tcdrain(serial_fd_);  // 条件判断
    // 等待发送缓冲区清空（tcdrain 阻塞直到所有已写数据发送完毕）
    // 此处调用是为了确保切换发送方向前上次发送已经完成
}  // 块结束
void ExtendedFeaturesFramework::rs485_receive_enable() {  // 代码行
// 切换 RS485 到接收模式（DE 置低，允许接收，禁止本端驱动总线）
    if (pigpio_initialized_) gpioWrite(RS485_DE_PIN, 0);  // 条件判断
    // DE=LOW：禁用驱动器，启用接收器（/RE=LOW）
}  // 块结束

ErrorCode ExtendedFeaturesFramework::rs485_send_frame(uint8_t type, uint8_t class_id,  // 代码行
                                                        float conf, float ma) {  // 代码行
// 构造并发送一帧 RS485 数据（8 字节固定格式）
    if (!rs485_initialized_ || serial_fd_ < 0) return ErrorCode::NOT_INITIALIZED;  // 条件判断
    // 前置检查：RS485 未初始化或文件描述符无效时返回错误
    uint16_t cf = static_cast<uint16_t>(  // 代码行
    // 将置信度 [0.0, 1.0] 转换为 uint16_t 整数（乘以 10000 放大为 [0, 10000]）
        std::max(0.0f, std::min(1.0f, conf)) * 10000.0f + 0.5f);  // 代码行
        // 先裁剪到 [0,1] 防止越界，×10000 将精度保持到 0.01%，+0.5 四舍五入
    uint8_t frame[FRAME_LEN] = {  // 代码行
    // 构造 8 字节固定格式帧
        FRAME_H1, FRAME_H2, type, class_id,  // 代码行
        // 字节 0-1：帧头（0x55, 0xAA）；字节 2：帧类型；字节 3：AI 类别 ID
        static_cast<uint8_t>(cf >> 8), static_cast<uint8_t>(cf & 0xFF),  // 代码行
        // 字节 4-5：置信度高字节和低字节（大端序，×10000 后的 uint16）
        static_cast<uint8_t>(std::floor(ma)),  // 代码行
        // 字节 6：mA 电流值的整数部分（floor 取下整）
        static_cast<uint8_t>(std::round((ma - std::floor(ma)) * 100.0f))  // 代码行
        // 字节 7：mA 电流值的小数部分（小数×100 四舍五入，精度 0.01 mA）
    };  // 代码行
    rs485_send_enable();  // 代码行
    // 切换到发送模式（DE=HIGH）
    ssize_t w = write(serial_fd_, frame, FRAME_LEN);  // 代码行
    // 将 8 字节帧写入串口发送缓冲区
    tcdrain(serial_fd_);  // 代码行
    // 等待发送缓冲区实际清空（确保数据已在总线上传输完毕）
    rs485_receive_enable();  // 代码行
    // 发送完成后立即切换回接收模式（DE=LOW），释放总线控制权
    return (w == static_cast<ssize_t>(FRAME_LEN)) ? ErrorCode::SUCCESS  // 返回
                                                   : ErrorCode::GPIO_WRITE_FAILED;  // 代码行
    // 写入字节数等于 FRAME_LEN(8) 表示成功；否则返回写入失败
}  // 块结束

// ============================================================================
// 继电器
// ============================================================================

ErrorCode ExtendedFeaturesFramework::initialize_relay() {  // 代码行
// 初始化两路继电器控制引脚（K1=BCM17 轴承，K2=BCM27 火焰）
    if (!pigpio_initialized_) return ErrorCode::GPIO_INIT_FAILED;  // 条件判断
    // pigpio 未就绪时无法操作 GPIO
    gpioSetMode(RELAY_BEARING_PIN, PI_OUTPUT); gpioWrite(RELAY_BEARING_PIN, 0);  // 代码行
    // BCM17 设为输出，初始低电平（继电器断开）
    gpioSetMode(RELAY_FIRE_PIN,    PI_OUTPUT); gpioWrite(RELAY_FIRE_PIN, 0);  // 代码行
    // BCM27 设为输出，初始低电平（继电器断开）
    relay_initialized_ = true;  // 代码行
    // 标记继电器初始化成功
    return ErrorCode::SUCCESS;  // 返回
    // 初始化完成
}  // 块结束
ErrorCode ExtendedFeaturesFramework::relay_set(unsigned int gpio, bool on) {  // 代码行
// 设置单路继电器状态（on=true 闭合，on=false 断开）
    if (!relay_initialized_) return ErrorCode::NOT_INITIALIZED;  // 条件判断
    // 继电器未初始化时返回错误
    return gpioWrite(gpio, on ? 1 : 0) == 0 ? ErrorCode::SUCCESS : ErrorCode::GPIO_WRITE_FAILED;  // 返回
    // gpioWrite 返回 0 表示成功；非 0 表示 GPIO 写入失败（引脚编号不合法等）
}  // 块结束
ErrorCode ExtendedFeaturesFramework::relay_reset_all() {  // 代码行
// 复位所有继电器（两路全部断开）
    if (!pigpio_initialized_) return ErrorCode::GPIO_INIT_FAILED;  // 条件判断
    gpioWrite(RELAY_BEARING_PIN, 0);  // 代码行
    // K1（轴承继电器）断开
    gpioWrite(RELAY_FIRE_PIN, 0);  // 代码行
    // K2（火焰继电器）断开
    return ErrorCode::SUCCESS;  // 返回
    // 复位完成
}  // 块结束
void ExtendedFeaturesFramework::relay_update(int class_id) {  // 代码行
// 根据 AI 推理类别 ID 更新继电器状态（故障/火焰时闭合，正常时断开）
    if (!relay_initialized_) return;  // 条件判断
    // 继电器未初始化时直接返回
#ifdef BEARING_VIBRATION_PROJECT  // 条件编译
    gpioWrite(RELAY_BEARING_PIN, class_id != 0 ? 1 : 0);  // 代码行
    // 轴承项目：class_id=0 为正常（denormal），非 0 为故障，故障时闭合 K1
#endif  // 条件编译
}  // 块结束

// ============================================================================
// 74HC595D
// ============================================================================

ErrorCode ExtendedFeaturesFramework::initialize_hc595() {  // 代码行
// 初始化 74HC595D 串行 LED 驱动电路
    if (!pigpio_initialized_) return ErrorCode::GPIO_INIT_FAILED;  // 条件判断
    // pigpio 未就绪时无法操作 GPIO
    gpioSetMode(HC595_DS,   PI_OUTPUT); gpioWrite(HC595_DS,   0);  // 代码行
    // DS（BCM23）设为输出，初始低电平
    gpioSetMode(HC595_STCP, PI_OUTPUT); gpioWrite(HC595_STCP, 0);  // 代码行
    // STCP（BCM26，存储时钟）设为输出，初始低电平
    gpioSetMode(HC595_SHCP, PI_OUTPUT); gpioWrite(HC595_SHCP, 0);  // 代码行
    // SHCP（BCM22，移位时钟）设为输出，初始低电平
    hc595_shadow_.fill(false);  // 代码行
    // 将软件镜像（shadow）全部清零（8 个 LED/蜂鸣器通道全部关闭）
    hc595_initialized_ = true;  // 代码行
    // 标记 HC595 初始化成功
    hc595_flush();  // 代码行
    // 立即刷新：将全零状态写入 HC595 移位寄存器，确保上电时所有输出均为低电平
    return ErrorCode::SUCCESS;  // 返回
    // 初始化完成
}  // 块结束
void ExtendedFeaturesFramework::hc595_shift_bit(bool data) {  // 代码行
// 向 74HC595D 移位寄存器串行输入 1 位数据（MSB 先输入）
    gpioWrite(HC595_DS,   data ? 1 : 0);  // 代码行
    // 在 SHCP 上升沿之前稳定 DS 数据（建立时间要求）
    gpioWrite(HC595_SHCP, 0);  // 代码行
    // SHCP 下降沿（准备上升沿）
    gpioWrite(HC595_SHCP, 1);  // 代码行
    // SHCP 上升沿：74HC595D 在此时刻将 DS 的值移入移位寄存器的最低位
}  // 块结束
void ExtendedFeaturesFramework::hc595_flush() {  // 代码行
// 将 hc595_shadow_ 中的 8 位状态刷新输出到 74HC595D 的锁存输出引脚
    if (!hc595_initialized_ && !pigpio_initialized_) return;  // 条件判断
    // 双重检查：HC595 未初始化且 pigpio 未就绪时跳过
    for (int i = 0; i < 8; ++i) hc595_shift_bit(!hc595_shadow_[i]);  // 循环
    // 将 shadow 的 8 位数据依次移入 HC595（取反是因为 LED 低电平有效或驱动逻辑反转）
    gpioWrite(HC595_STCP, 1);  // 代码行
    // STCP 上升沿：将移位寄存器的 8 位数据同时锁存到输出寄存器（Q0~Q7 立即更新）
    gpioWrite(HC595_STCP, 0);  // 代码行
    // STCP 恢复低电平，准备下次锁存操作
}  // 块结束
ErrorCode ExtendedFeaturesFramework::led_set(LedChannel ch, bool on) {  // 代码行
// 设置单个 LED 通道的开关状态并立即刷新到硬件
    if (!hc595_initialized_) return ErrorCode::NOT_INITIALIZED;  // 条件判断
    // HC595 未初始化时返回错误
    uint8_t idx = static_cast<uint8_t>(ch);  // 代码行
    // 将 LedChannel 枚举转换为数组下标（0~7）
    if (idx >= 8) return ErrorCode::INVALID_PARAMETER;  // 条件判断
    // 下标越界（枚举值超出 0~7 范围）时返回无效参数错误
    hc595_shadow_[idx] = on;  // 代码行
    // 更新软件镜像中对应通道的状态
    hc595_flush();  // 代码行
    // 将更新后的镜像刷新输出到硬件
    return ErrorCode::SUCCESS;  // 返回
    // 设置完成
}  // 块结束
ErrorCode ExtendedFeaturesFramework::led_all_off() {  // 代码行
// 关闭所有 LED 和蜂鸣器（shadow 全清零后刷新）
    if (!pigpio_initialized_) return ErrorCode::GPIO_INIT_FAILED;  // 条件判断
    hc595_shadow_.fill(false);  // 代码行
    // 将 8 个通道的 shadow 全部置 false
    hc595_flush();  // 代码行
    // 刷新到硬件（关闭所有输出）
    return ErrorCode::SUCCESS;  // 返回
    // 关闭完成
}  // 块结束
ErrorCode ExtendedFeaturesFramework::led_write_raw(uint8_t state) {  // 代码行
// 直接以字节位图方式写入所有 LED 通道状态（bit7 对应 ch0，bit0 对应 ch7）
    if (!hc595_initialized_) return ErrorCode::NOT_INITIALIZED;  // 条件判断
    for (int i = 0; i < 8; ++i) hc595_shadow_[i] = (state >> (7 - i)) & 0x01;  // 循环
    // 将 state 的每一位映射到 hc595_shadow_：bit(7-i) 对应通道 i
    hc595_flush();  // 代码行
    // 刷新到硬件
    return ErrorCode::SUCCESS;  // 返回
    // 写入完成
}  // 块结束
void ExtendedFeaturesFramework::led_update(int class_id) {  // 代码行
// 根据 AI 推理类别 ID 更新 LED 和蜂鸣器状态
    if (!hc595_initialized_) return;  // 条件判断
    // HC595 未初始化时直接返回
    hc595_shadow_.fill(false);  // 代码行
    // 先将所有通道清零（每次更新从空白状态开始，避免上次状态残留）
#ifdef BEARING_VIBRATION_PROJECT  // 条件编译
    switch (class_id) {  // switch 分支选择
    // 轴承振动项目：根据四分类结果点亮对应 LED
        case 0: hc595_shadow_[CH_WORK]    = true; break;  // case 分支
        // class_id=0（denormal 正常）：仅点亮工作指示灯
        case 1: hc595_shadow_[CH_DEV_ERR] = true; hc595_shadow_[CH_B_BALL]  = true; hc595_shadow_[CH_BEE] = true; break;  // case 分支
        // class_id=1（滚珠故障）：错误灯+滚珠故障灯+蜂鸣器同时激活
        case 2: hc595_shadow_[CH_DEV_ERR] = true; hc595_shadow_[CH_B_CAGE] = true; hc595_shadow_[CH_BEE] = true; break;  // case 分支
        // class_id=2（保持架故障）：错误灯+保持架故障灯+蜂鸣器同时激活
        case 3: hc595_shadow_[CH_DEV_ERR] = true; hc595_shadow_[CH_B_OUTER] = true; hc595_shadow_[CH_BEE] = true; break;  // case 分支
        // class_id=3（外圈故障）：错误灯+外圈故障灯+蜂鸣器同时激活
        default: break;  // 代码行
        // 未知类别不操作任何 LED
    }  // 块结束
#endif  // 条件编译
    hc595_flush();  // 代码行
    // 将更新后的 shadow 刷新到 74HC595D 硬件输出
}  // 块结束

// ============================================================================
// 状态信号
// ============================================================================

ErrorCode ExtendedFeaturesFramework::output_status_signal(float ma, uint8_t code) {  // 代码行
// 输出系统状态信号：设置指定 mA 电流、复位所有继电器、关所有 LED、发送状态帧
    ErrorCode r = ErrorCode::SUCCESS;  // 代码行
    if (pigpio_initialized_ && !pwm_output(ma)) r = ErrorCode::GPIO_WRITE_FAILED;  // 条件判断
    // pigpio 就绪时输出指定电流；pwm_output 失败则记录错误码
    if (relay_initialized_) { gpioWrite(RELAY_BEARING_PIN, 0); gpioWrite(RELAY_FIRE_PIN, 0); }  // 条件判断
    // 继电器已初始化时复位两路继电器（断开 K1 和 K2）
    led_all_off();  // 代码行
    // 关闭所有 LED 和蜂鸣器
    rs485_send_frame(FRAME_STATUS, code, 0.0f, ma);  // 代码行
    // 发送状态帧：类型=FRAME_STATUS，类别码=code，置信度=0，电流=ma
    return r;  // 返回
    // 返回 PWM 操作结果（成功或写入失败）
}  // 块结束
ErrorCode ExtendedFeaturesFramework::output_standby()     { if (!pigpio_initialized_) return ErrorCode::GPIO_INIT_FAILED; return output_status_signal(MA_STANDBY,    0x04); }  // 代码行
// 输出待机状态：4 mA + 状态码 0x04（系统待机，未推理）
ErrorCode ExtendedFeaturesFramework::output_infer_failed(){ if (!pigpio_initialized_) return ErrorCode::GPIO_INIT_FAILED; return output_status_signal(MA_INFER_FAIL, 0x05); }  // 代码行
// 输出推理失败状态：5 mA + 状态码 0x05（模型推理异常）
ErrorCode ExtendedFeaturesFramework::output_reserved_6()  { if (!pigpio_initialized_) return ErrorCode::GPIO_INIT_FAILED; return output_status_signal(MA_RESERVED_6, 0x06); }  // 代码行
// 输出预留状态6：6 mA + 状态码 0x06（保留扩展用途）
ErrorCode ExtendedFeaturesFramework::output_reserved_7()  { if (!pigpio_initialized_) return ErrorCode::GPIO_INIT_FAILED; return output_status_signal(MA_RESERVED_7, 0x07); }  // 代码行
// 输出预留状态7：7 mA + 状态码 0x07（保留扩展用途）

ErrorCode ExtendedFeaturesFramework::output_ai_result_current(int class_id, float conf) {  // 代码行
// 根据 AI 推理结果（类别 ID + 置信度）输出完整状态：PWM 电流 + 继电器 + LED + RS485 帧
    if (!pigpio_initialized_) return ErrorCode::GPIO_INIT_FAILED;  // 条件判断
    // pigpio 未就绪时无法输出电流
    conf = std::max(0.0f, std::min(1.0f, conf));  // 代码行
    // 置信度裁剪到 [0.0, 1.0]，防止无效输入
    float rated = MA_CLASS_BASE;  // 代码行
    // 初始化额定电流为类别基准 8 mA（未匹配任何类别时的默认值）
#ifdef BEARING_VIBRATION_PROJECT  // 条件编译
    switch (class_id) {  // switch 分支选择
    // 轴承振动项目：四分类对应四档额定电流
        case 0: rated = BEARING_DENORMAL;  break;  // case 分支
        // denormal（正常）额定电流：11 mA
        case 1: rated = BEARING_BALLFAULT; break;  // case 分支
        // ballfault（滚珠故障）额定电流：14 mA
        case 2: rated = BEARING_CAGEFAULT; break;  // case 分支
        // cagefault（保持架故障）额定电流：17 mA
        case 3: rated = BEARING_OUTERRACE; break;  // case 分支
        // outerrace（外圈故障）额定电流：20 mA
        default: return output_infer_failed();  // 代码行
        // 未知类别 ID：输出推理失败状态（5 mA）
    }  // 块结束
#else  // 条件编译
    return output_standby();  // 返回
    // 未定义项目宏时：输出待机状态（兜底保护）
#endif  // 条件编译
    float ma = MA_CLASS_BASE + (rated - MA_CLASS_BASE) * conf;  // 代码行
    // 实际输出电流 = 基准电流 + (额定电流 - 基准电流) × 置信度
    // 置信度越高，输出电流越接近额定值；低置信度时接近基准电流 8 mA
    bool ok = pwm_output(ma);  // 代码行
    // 通过硬件 PWM 输出计算得到的实际电流
    relay_update(class_id);  // 代码行
    // 根据类别 ID 更新继电器状态（故障/火焰时闭合对应继电器）
    led_update(class_id);  // 代码行
    // 根据类别 ID 更新 LED 和蜂鸣器状态
    rs485_send_frame(FRAME_RESULT, static_cast<uint8_t>(class_id), conf, ma);  // 代码行
    // 发送结果帧：类型=FRAME_RESULT，类别 ID，置信度×10000，实际电流
    return ok ? ErrorCode::SUCCESS : ErrorCode::GPIO_WRITE_FAILED;  // 返回
    // PWM 输出成功则返回 SUCCESS；失败则返回 GPIO 写入错误
}  // 块结束
ErrorCode ExtendedFeaturesFramework::output_ai_result_current(const ModelOutput& o) {  // 代码行
// ModelOutput 重载：从 ModelOutput 结构体中提取 class_id 和 confidence 后转调
    return output_ai_result_current(o.classification.class_id, o.classification.confidence);  // 返回
    // 委托给 (int, float) 版本处理，统一实现逻辑
}  // 块结束

// ============================================================================
// PWR_OFF
// ============================================================================

void ExtendedFeaturesFramework::set_shutdown_callback(std::function<void()> cb) {  // 代码行
// 注册断电安全关机回调函数（由上层在 main 中注册，触发时执行资源清理）
    shutdown_callback_ = std::move(cb);  // 代码行
    // 移动语义存储回调函数，避免 std::function 的复制开销
}  // 块结束
ErrorCode ExtendedFeaturesFramework::initialize_pwr_off() {  // 代码行
// 初始化断电检测（BCM4 电平监听，上升沿触发安全关机）
    if (!pigpio_initialized_) return ErrorCode::GPIO_INIT_FAILED;  // 条件判断
    // pigpio 未就绪时无法注册 alert 回调
    gpioSetMode(PWR_OFF_PIN, PI_INPUT);  // 代码行
    // BCM4 设为输入模式，监听 UPS 电源模块的断电信号
    if (gpioSetAlertFuncEx(PWR_OFF_PIN, pwr_off_alert, this) != 0)  // 条件判断
    // 注册 GPIO 边沿中断回调（任何电平变化都调用 pwr_off_alert）
        return ErrorCode::GPIO_INIT_FAILED;  // 返回
        // 注册失败时返回 GPIO 初始化失败
    std::cout << "[PWR_OFF] BCM4 监听已启动" << std::endl;  // 打印信息
    // 打印确认信息
    return ErrorCode::SUCCESS;  // 返回
    // 初始化完成
}  // 块结束
void ExtendedFeaturesFramework::pwr_off_alert(int, int level, uint32_t, void* ud) {  // 代码行
// pigpio alert 回调：BCM4 电平变化时由 pigpio 线程调用
    if (level == 1) static_cast<ExtendedFeaturesFramework*>(ud)->on_pwr_off();  // 条件判断
    // 仅在上升沿（level=1）时触发关机；下降沿（level=0）忽略（避免误触）
}  // 块结束
void ExtendedFeaturesFramework::on_pwr_off() {  // 代码行
// 断电安全关机处理：仅执行一次（原子标志保护），快速安全地关闭所有外设后关机
    bool expected = false;  // 代码行
    if (!pwr_off_triggered_.compare_exchange_strong(expected, true)) return;  // 条件判断
    // compare_exchange_strong：若 pwr_off_triggered_ 当前为 false，则设为 true 并继续
    // 若已为 true（已触发过），直接返回（幂等保护，防止重复关机）
    std::cerr << "[PWR_OFF] 意外断电，执行安全关机..." << std::endl;  // 打印错误
    // 向 stderr 输出断电警告（stderr 通常不缓冲，紧急情况下更可靠）
    if (pigpio_initialized_) pwm_output(MA_STANDBY);  // 条件判断
    // 将电流环恢复到 4 mA 待机状态，通知上位机系统即将下线
    led_all_off();  // 代码行
    // 关闭所有 LED 和蜂鸣器
    relay_reset_all();  // 代码行
    // 复位所有继电器
    rs485_send_frame(FRAME_STATUS, 0xFF, 0.0f, MA_STANDBY);  // 代码行
    // 发送断电状态帧：类别码 0xFF 表示断电（非正常关机标识）
    {  // 块开始
    // 内层作用域：限制 now/buf/f 的生命周期
        std::time_t now = std::time(nullptr);  // 代码行
        // 获取当前 Unix 时间戳
        char buf[32];  // 代码行
        // 声明时间字符串缓冲区
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));  // 代码行
        // 格式化当前时间为 "YYYY-MM-DD HH:MM:SS" 字符串
        FILE* f = fopen("../log/system.log", "a");  // 代码行
        // 以追加模式打开系统日志文件
        if (f) { fprintf(f, "%s Power-off protection triggered!\n", buf); fclose(f); }  // 条件判断
        // 若文件打开成功，写入断电时间记录后关闭文件
    }  // 块结束
    if (shutdown_callback_) shutdown_callback_();  // 条件判断
    // 若已注册关机回调，执行它（上层资源清理，如保存未写入的日志）
    std::system("sudo shutdown now");  // 代码行
    // 调用系统命令立即关机（需要 sudoers 中配置 pi 用户无需密码执行 shutdown）
}  // 块结束

// ============================================================================
// 设置值对外接口
// ============================================================================

float ExtendedFeaturesFramework::get_conf_threshold() const {  // 代码行
// 线程安全地获取当前置信度阈值（加锁保护，防止读取时菜单线程正在修改）
    std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
    // 获取菜单互斥锁
    return conf_threshold_;  // 返回
    // 返回置信度阈值（当前值由用户通过 EDIT 模式设置）
}  // 块结束
int ExtendedFeaturesFramework::get_alarm_delay() const {  // 代码行
// 线程安全地获取当前报警延迟时间（秒）
    std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
    // 获取菜单互斥锁
    return alarm_delay_;  // 返回
    // 返回报警延迟（秒），防止抖动触发误报警
}  // 块结束

// ============================================================================
// OLED 菜单工具
// ============================================================================

uint32_t ExtendedFeaturesFramework::mono_ms() {  // 代码行
// 静态工具函数：获取单调时钟的当前毫秒值（uint32_t，约 49.7 天溢出一次）
    using namespace std::chrono;  // 代码行
    // 引入 chrono 命名空间，简化后续代码
    return static_cast<uint32_t>(  // 返回
    // 将时间段转换为 uint32_t（溢出时自动回绕，差值计算仍正确）
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());  // 代码行
        // steady_clock：单调时钟，不受系统时间调整影响，适合测量持续时长
}  // 块结束
std::string ExtendedFeaturesFramework::fmt_conf(float c) {  // 代码行
// 将置信度 [0,1] 格式化为 "XX.X%" 字符串（保留一位小数）
    char buf[10]; snprintf(buf, sizeof(buf), "%.1f%%", c * 100.0f); return buf;  // 代码行
    // 乘以 100 转为百分比，snprintf 防止缓冲区溢出，返回 std::string
}  // 块结束
std::string ExtendedFeaturesFramework::fmt_ma(float ma) {  // 代码行
// 将电流值格式化为 "XX.XXmA" 字符串（保留两位小数）
    char buf[12]; snprintf(buf, sizeof(buf), "%.2fmA", ma); return buf;  // 代码行
    // 返回带单位的电流字符串，如 "12.43mA"
}  // 块结束
void ExtendedFeaturesFramework::oled2(const std::string& l1, bool cn1,  // 代码行
// 封装 drawOLED（两行显示）：l1/l2 为显示文本，cn1/cn2 为是否使用中文字体
                                      const std::string& l2, bool cn2) {  // 代码行
    drawOLED(l1.c_str(), cn1, l2.c_str(), cn2);  // 代码行
    // 调用 oled.h 中的 C 函数，传入 C 风格字符串指针
}  // 块结束
void ExtendedFeaturesFramework::oled3(const std::string& l1, bool cn1,  // 代码行
// 封装 drawOLED3（三行显示）
                                      const std::string& l2, bool cn2,  // 代码行
                                      const std::string& l3, bool cn3) {  // 代码行
    drawOLED3(l1.c_str(), cn1, l2.c_str(), cn2, l3.c_str(), cn3);  // 代码行
    // 调用 oled.h 中的三行显示 C 函数
}  // 块结束
void ExtendedFeaturesFramework::oled4(const std::string& l1, bool cn1,  // 代码行
// 封装 drawOLED4（四行显示，OLED 的主要使用模式）
                                      const std::string& l2, bool cn2,  // 代码行
                                      const std::string& l3, bool cn3,  // 代码行
                                      const std::string& l4, bool cn4) {  // 代码行
    drawOLED4(l1.c_str(), cn1, l2.c_str(), cn2, l3.c_str(), cn3, l4.c_str(), cn4);  // 代码行
    // 调用 oled.h 中的四行显示 C 函数（最常用，对应 128×64 像素的四行布局）
}  // 块结束

// ============================================================================
// initialize_menu / shutdown_menu / update_menu_snapshot
// ============================================================================

ErrorCode ExtendedFeaturesFramework::initialize_menu(DisplayLoggingFramework* dl) {  // 代码行
// 初始化 OLED 菜单系统：配置按键 GPIO、注册中断回调、启动菜单刷新线程
    if (!pigpio_initialized_) {  // 条件判断
    // pigpio 未就绪时菜单不可用（按键和 OLED 均需 GPIO）
        std::cerr << "[Menu] pigpio 未初始化，菜单不可用" << std::endl;  // 打印错误
        return ErrorCode::GPIO_INIT_FAILED;  // 返回
        // 返回 GPIO 初始化失败错误码
    }  // 块结束
    dl_ = dl;  // 代码行
    // 保存 DisplayLoggingFramework 指针，供日志加载和导出功能使用

    for (unsigned int pin : {KEY_MODE_PIN, KEY_LEFT_PIN, KEY_RIGHT_PIN}) {  // 循环
    // 遍历三个按键引脚，统一配置
        gpioSetMode(pin, PI_INPUT);  // 代码行
        // 设置为输入模式（接收外部按键信号）
        gpioSetPullUpDown(pin, PI_PUD_UP);  // 代码行
        // 启用内部上拉电阻：未按键时 GPIO 保持高电平，按下时拉低（低电平有效）
    }  // 块结束
    gpioSetAlertFuncEx(KEY_MODE_PIN,  cb_key_mode,  this);  // 代码行
    // 为 MODE 键（BCM5）注册电平变化中断回调，传入 this 指针作为用户数据
    gpioSetAlertFuncEx(KEY_LEFT_PIN,  cb_key_left,  this);  // 代码行
    // 为 LEFT 键（BCM6）注册中断回调
    gpioSetAlertFuncEx(KEY_RIGHT_PIN, cb_key_right, this);  // 代码行
    // 为 RIGHT 键（BCM13）注册中断回调

    cur_page_         = MenuPage::STANDBY;  // 代码行
    // 初始页面设为待机页（STANDBY）
    menu_mode_        = MenuMode::NORMAL;  // 代码行
    // 初始模式设为普通模式（NORMAL）
    shutdown_confirm_ = false;  // 代码行
    // 关机确认标志初始为 false
    log_idx_          = 0;  // 代码行
    // 日志浏览索引初始为 0
    log_cache_page_   = MenuPage::PAGE_COUNT;  // 代码行
    // log_cache_page_ 设为 PAGE_COUNT（无效值），强制首次进入日志页时重载
    cursor_visible_   = true;  // 代码行
    // EDIT 模式光标初始可见
    last_blink_ms_    = 0;  // 代码行
    // 上次光标闪烁时刻初始为 0

    menu_inited_  = true;  // 代码行
    // 标记菜单已初始化
    menu_running_.store(true);  // 代码行
    // 原子写入：通知菜单刷新线程开始运行
    menu_thread_  = std::thread([this]{ menu_loop(); });  // 代码行
    // 启动菜单后台刷新线程，执行 menu_loop() 函数

    std::cout << "[Menu] 初始化完成  MODE=BCM" << KEY_MODE_PIN  // 打印信息
              << "  LEFT=BCM" << KEY_LEFT_PIN  // 代码行
              << "  RIGHT=BCM" << KEY_RIGHT_PIN << std::endl;  // 代码行
    // 打印初始化成功信息及各按键的 BCM 引脚编号
    return ErrorCode::SUCCESS;  // 返回
    // 菜单系统初始化完成
}  // 块结束

void ExtendedFeaturesFramework::shutdown_menu() {  // 代码行
// 安全停止菜单系统（停止刷新线程，注销按键中断回调）
    if (!menu_inited_) return;  // 条件判断
    // 若菜单未初始化，幂等返回
    menu_running_.store(false);  // 代码行
    // 通知菜单刷新线程退出循环（menu_loop 中的 while(menu_running_.load()) 检测此标志）
    if (menu_thread_.joinable()) menu_thread_.join();  // 条件判断
    // 等待菜单刷新线程安全退出（join 确保线程已完全停止后才继续）
    if (pigpio_initialized_) {  // 条件判断
    // pigpio 已就绪时注销按键中断回调
        gpioSetAlertFuncEx(KEY_MODE_PIN,  nullptr, nullptr);  // 代码行
        // 取消 MODE 键的 alert 回调（传入 nullptr 注销）
        gpioSetAlertFuncEx(KEY_LEFT_PIN,  nullptr, nullptr);  // 代码行
        // 取消 LEFT 键的 alert 回调
        gpioSetAlertFuncEx(KEY_RIGHT_PIN, nullptr, nullptr);  // 代码行
        // 取消 RIGHT 键的 alert 回调
    }  // 块结束
    menu_inited_ = false;  // 代码行
    // 标记菜单已停止
    std::cout << "[Menu] 已停止" << std::endl;  // 打印信息
    // 打印停止确认信息
}  // 块结束

void ExtendedFeaturesFramework::update_menu_snapshot(const MenuSnapshot& snap) {  // 代码行
// 线程安全地更新菜单快照（AI 推理结果、继电器状态、电流值等）
    std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
    // 加锁保护 snap_ 的写入（防止菜单刷新线程同时读取导致数据竞争）
    snap_ = snap;  // 代码行
    // 更新成员变量 snap_，菜单刷新线程在下一个 100 ms 周期时读取并渲染
}  // 块结束

// ============================================================================
// 菜单后台刷新线程
// ============================================================================

void ExtendedFeaturesFramework::menu_loop() {  // 代码行
// 菜单后台刷新线程主函数：每 100 ms 读取状态快照并驱动 OLED 渲染
// 三步骤设计保证线程安全：①锁内快速复制状态 ②锁外文件IO ③锁外OLED渲染
    while (menu_running_.load()) {  // 循环
    // 循环执行直到 menu_running_ 被设为 false（shutdown_menu() 时置 false）
        // ── 第一步：在锁内快速复制所有需要渲染的状态（纯内存操作）───
        MenuPage     page_snap;  // 代码行
        // 当前页面的本地副本（复制后锁外使用，消除数据竞争）
        MenuMode     mode_snap;  // 代码行
        // 当前菜单模式的本地副本（NORMAL / VIEW / EDIT）
        MenuSnapshot data_snap;  // 代码行
        // AI 推理快照的本地副本（含结果标签、置信度、继电器状态等）
        bool         cursor_snap;  // 代码行
        // 光标可见状态的本地副本
        bool         shutdown_snap;  // 代码行
        // 关机确认状态的本地副本
        int          log_idx_snap;  // 代码行
        // 日志浏览索引的本地副本
        SettingsItem settings_snap;  // 代码行
        // 当前设置项的本地副本（CONF_THRESHOLD / ALARM_DELAY）
        float        conf_snap;  // 代码行
        // 置信度阈值的本地副本
        int          delay_snap;  // 代码行
        // 报警延迟的本地副本
        std::vector<AbnormalRecord> log_cache_snap;  // 日志本地副本（无竞态）
        // 日志缓存的完整副本（在锁内做值复制，复制后可在锁外自由访问）
        bool         log_needs_reload = false;  // 代码行
        // 标记是否需要重载日志文件（在锁外执行文件 IO）
        MenuPage     log_reload_page  = MenuPage::PAGE_COUNT;  // 代码行
        // 需要重载的日志页类型（LOG_BEARING 或 LOG_FIRE）

        {  // 块开始
        // 内层作用域：限制 lock_guard 的生命周期（出作用域自动解锁）
            std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
            // 加锁：保护 cur_page_ / menu_mode_ / snap_ / cursor_visible_ 等共享状态

            if (menu_mode_ == MenuMode::EDIT) {  // 条件判断
            // EDIT 模式下才需要光标闪烁逻辑
                uint32_t now = mono_ms();  // 代码行
                // 获取当前单调毫秒时间
                if (now - last_blink_ms_ >= BLINK_PERIOD_MS) {  // 条件判断
                // 若距上次闪烁切换已超过 BLINK_PERIOD_MS（通常 500 ms）
                    cursor_visible_ = !cursor_visible_;  // 代码行
                    // 翻转光标可见状态（实现光标闪烁效果）
                    last_blink_ms_  = now;  // 代码行
                    // 更新上次闪烁时刻
                }  // 块结束
            }  // 块结束

            page_snap      = cur_page_;  // 代码行
            // 复制当前页面
            mode_snap      = menu_mode_;  // 代码行
            // 复制当前模式
            data_snap      = snap_;  // 代码行
            // 复制 AI 推理快照
            cursor_snap    = cursor_visible_;  // 代码行
            // 复制光标状态
            shutdown_snap  = shutdown_confirm_;  // 代码行
            // 复制关机确认状态
            log_idx_snap   = log_idx_;  // 代码行
            // 复制日志浏览索引
            settings_snap  = settings_item_;  // 代码行
            // 复制当前设置项
            conf_snap      = conf_threshold_;  // 代码行
            // 复制置信度阈值
            delay_snap     = alarm_delay_;  // 代码行
            // 复制报警延迟
            log_cache_snap = log_cache_;  // 值拷贝，锁内完成，之后无竞态
            // 将日志缓存完整复制到本地（值语义，锁外访问 log_cache_snap 无竞态）

            if ((page_snap == MenuPage::LOG_BEARING || page_snap == MenuPage::LOG_FIRE)  // 条件判断
                && log_cache_page_ != page_snap) {  // 代码行
            // 当前页是日志页，且日志缓存不是当前页的缓存（需要重载）
                log_needs_reload = true;  // 代码行
                // 标记需要在锁外执行文件 IO 重载日志
                log_reload_page  = page_snap;  // 代码行
                // 记录需要重载的日志页类型
            }  // 块结束
        }  // 块结束
        // ── 锁已释放 ──────────────────────────────────────────────
        // 此后所有操作均在锁外执行（无 menu_mtx_ 保护，但使用的是本地副本）

        // ── 第二步：如需重载日志，在锁外执行文件IO ───────────────
        if (log_needs_reload && dl_) {  // 条件判断
        // 需要重载且 DisplayLoggingFramework 已注册时执行
            std::vector<AbnormalRecord> new_cache;  // 代码行
            // 声明新日志缓存（在锁外构建，完成后才加锁写回）
            ProjectType pt = (log_reload_page == MenuPage::LOG_BEARING)  // 代码行
                                 ? ProjectType::BEARING_VIBRATION  // 代码行
                                 : ProjectType::FIRE_DETECTION;  // 代码行
            // 根据日志页类型确定项目类型（轴承或火焰）
            dl_->load_week_abnormal(pt, new_cache);  // 文件IO，锁外
            // 在锁外执行耗时的文件 IO（读取一周内的异常记录），避免长时间持锁

            {  // 块开始
            // 内层作用域：加锁写回加载结果
                std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
                log_cache_      = std::move(new_cache);  // 代码行
                // 移动语义将新日志缓存写回成员变量（避免深拷贝）
                log_cache_page_ = log_reload_page;  // 代码行
                // 更新缓存对应的页面标识（防止下次重复加载）
                log_idx_        = log_cache_.empty() ? 0  // 代码行
                                  : static_cast<int>(log_cache_.size()) - 1;  // 代码行
                // 浏览索引跳到最新记录（最后一条）；空缓存时归零
                log_idx_snap    = log_idx_;  // 代码行
                // 同步更新本地副本，使当前帧渲染使用正确的索引
                log_cache_snap  = log_cache_;  // 更新本地副本
                // 同步更新日志缓存本地副本，用于当前帧渲染
            }  // 块结束
        }  // 块结束

        // ── 第三步：完全在锁外执行 I²C OLED 渲染 ────────────────
        render_page_unlocked(page_snap, mode_snap, data_snap,  // 代码行
                             cursor_snap, shutdown_snap,  // 代码行
                             log_idx_snap, settings_snap,  // 代码行
                             conf_snap, delay_snap,  // 代码行
                             log_cache_snap);   // ← 传入本地副本，无竞态
        // 使用本地副本驱动 OLED 渲染（I²C 通信耗时，在锁外执行防止阻塞按键响应）

        std::this_thread::sleep_for(std::chrono::milliseconds(MENU_PERIOD_MS));  // 代码行
        // 休眠 100 ms（MENU_PERIOD_MS），控制刷新频率并让出 CPU 给其他线程
    }  // 块结束
}  // 块结束

void ExtendedFeaturesFramework::render_page_unlocked(  // 代码行
// 无锁渲染函数：通过传入参数避免持锁访问成员变量
        MenuPage page, MenuMode mode,  // 代码行
        // 当前页面和菜单模式（均为本地副本）
        const MenuSnapshot& snap,  // 代码行
        // AI 推理数据快照（本地副本）
        bool cursor_visible, bool shutdown_confirm,  // 代码行
        // 光标可见状态和关机确认状态（本地副本）
        int log_idx,  // 代码行
        // 当前日志浏览索引（本地副本）
        SettingsItem settings_item,  // 代码行
        // 当前设置项（本地副本）
        float conf_threshold, int alarm_delay,  // 代码行
        // 置信度阈值和报警延迟（本地副本）
        const std::vector<AbnormalRecord>& log_cache)  // 代码行
        // 日志缓存的本地副本引用（无竞态，可安全访问）
{  // 块开始
    // EDIT 模式：渲染设置页（参数化，无需持锁）
    if (mode == MenuMode::EDIT) {  // 条件判断
    // EDIT 模式下渲染参数设置页面
        const char* cur_mark = cursor_visible ? ">" : " ";  // 代码行
        // 光标标记：可见时显示 ">"，不可见时显示空格（实现闪烁效果）
        std::string r2, r3;  // 代码行
        // 声明第 2、3 行内容
        switch (settings_item) {  // switch 分支选择
        // 根据当前设置项渲染对应的调整界面
            case SettingsItem::CONF_THRESHOLD: {  // case 分支
            // 当前设置项为置信度阈值
                r2 = std::string(cur_mark) + "Conf threshold";  // 代码行
                // 第 2 行：带光标标记的设置项名称
                char buf[20]; snprintf(buf, sizeof(buf), "< %.2f >", conf_threshold);  // 代码行
                // 格式化当前阈值为 "< 0.85 >" 形式（两端箭头提示可调整）
                r3 = buf; break;  // 代码行
                // 将格式化结果赋给第 3 行
            }  // 块结束
            case SettingsItem::ALARM_DELAY: {  // case 分支
            // 当前设置项为报警延迟
                r2 = std::string(cur_mark) + "Alarm delay";  // 代码行
                // 第 2 行：带光标标记的延迟设置项名称
                char buf[20]; snprintf(buf, sizeof(buf), "< %ds >", alarm_delay);  // 代码行
                // 格式化延迟值为 "< 5s >" 形式
                r3 = buf; break;  // 代码行
                // 将格式化结果赋给第 3 行
            }  // 块结束
            default: r2 = ">Settings"; r3 = ""; break;  // 代码行
            // 未知设置项时显示通用提示
        }  // 块结束
        drawOLED4("Settings [EDIT]",     false,  // 代码行
        // 第 1 行：设置页标题（英文，false 表示不使用中文字体）
                  r2.c_str(),            false,  // 代码行
                  // 第 2 行：当前设置项名称（含光标标记）
                  r3.c_str(),            false,  // 代码行
                  // 第 3 行：当前设置值（带 < > 括号）
                  "L/R:adj M:next 2s:exit", false);  // 代码行
                  // 第 4 行：操作提示（L/R 调整值，M 切换项目，超长按退出）
        return;  // 函数返回
        // EDIT 模式渲染完成，直接返回
    }  // 块结束

    // 普通页面渲染
    switch (page) {  // switch 分支选择
    // 根据当前页面类型选择对应的渲染逻辑
        // ── STANDBY ──────────────────────────────────────────────
        case MenuPage::STANDBY: {  // case 分支
        // 待机页面：显示项目名称和基本操作提示
#ifdef BEARING_VIBRATION_PROJECT  // 条件编译
            const char* NAME = "Bearing Diag.";  // 代码行
            // 轴承振动项目名称
#else  // 条件编译
            const char* NAME = "Multi-Detect";  // 代码行
            // 未定义项目宏时的通用名称
#endif  // 条件编译
            const char* hint = (mode == MenuMode::VIEW)  // 代码行
                               ? "MODE:Back  L/R:page"  // 代码行
                               // VIEW 模式提示（进入过某页后返回待机）
                               : "L:Prev  R:Next";  // 代码行
                               // NORMAL 模式提示
            drawOLED4(NAME,              false,  // 代码行
            // 第 1 行：项目名称
                      "Running...",      false,  // 代码行
                      // 第 2 行：系统运行状态
                      hint,              false,  // 代码行
                      // 第 3 行：操作提示（根据模式不同）
                      "MODE:Enter page", false);  // 代码行
                      // 第 4 行：进入页面提示
            break;  // 退出 switch/循环
        }  // 块结束

        // ── PROJECT_STATUS ────────────────────────────────────────
        case MenuPage::PROJECT_STATUS: {  // case 分支
        // 项目状态页面：显示最新推理结果、时间、电流和继电器状态
            std::string r2;  // 代码行
            // 第 2 行内容（运行状态）
            bool r2_cn = false;  // 代码行
            // 第 2 行是否使用中文字体（结果标签可能含中文）
            switch (snap.run_state) {  // switch 分支选择
            // 根据推理运行状态渲染不同内容
                case MenuSnapshot::RunState::RUNNING:  // case 分支
                    r2 = "Running..."; break;  // 代码行
                    // 推理进行中：显示 "Running..."
                case MenuSnapshot::RunState::SUCCESS:  // case 分支
                    r2 = snap.result_label + " " + fmt_conf(snap.result_confidence);  // 代码行
                    // 推理成功：显示类别标签（如"外圈故障"）加置信度（如"92.3%"）
                    r2_cn = true; break;  // 代码行
                    // 使用中文字体（类别标签可能为中文）
                case MenuSnapshot::RunState::FAILED:  // case 分支
                    r2 = "Fail:" + snap.fail_reason.substr(  // 代码行
                             0, std::min<size_t>(snap.fail_reason.size(), 8)); break;  // 代码行
                    // 推理失败：显示失败原因（截取前 8 个字符防止溢出屏幕）
            }  // 块结束
            std::string ts5 = snap.result_timestamp.size() >= 5  // 代码行
                                  ? snap.result_timestamp.substr(0, 5) : "--:--";  // 代码行
            // 从时间戳中提取 "HH:MM" 前 5 位（不足时显示 "--:--"）
            std::string r3 = ts5 + "  " + fmt_ma(snap.current_ma);  // 代码行
            // 第 3 行：时间 + 当前电流（如 "22:48  12.43mA"）
            std::string r4 = std::string("BRG:")  // 代码行
                           + (snap.relay_bearing ? "ON " : "OFF")  // 代码行
                           // BRG（轴承继电器）状态
                           + "  FIRE:"  // 代码行
                           + (snap.relay_fire ? "ON " : "OFF");  // 代码行
                           // FIRE（火焰继电器）状态
            drawOLED4("Project Status", false,  // 代码行
            // 第 1 行：页面标题
                      r2.c_str(), r2_cn,  // 代码行
                      // 第 2 行：推理结果（中文/英文自适应）
                      r3.c_str(), false,  // 代码行
                      // 第 3 行：时间 + 电流
                      r4.c_str(), false);  // 代码行
                      // 第 4 行：继电器状态
            break;  // 退出 switch/循环
        }  // 块结束

        // ── LOG_BEARING / LOG_FIRE ────────────────────────────────
        case MenuPage::LOG_BEARING:  // case 分支
        case MenuPage::LOG_FIRE: {  // case 分支
        // 日志页面：显示一周内的异常记录，支持翻页浏览
            const bool is_bearing = (page == MenuPage::LOG_BEARING);  // 代码行
            // 判断当前是轴承日志页还是火焰日志页
            const char* title = is_bearing ? "Bearing Log" : "Fire Log";  // 代码行
            // 根据页面类型设置标题

            if (log_cache.empty()) {  // 条件判断
            // 日志缓存为空（无记录）
                drawOLED4(title,          false,  // 代码行
                // 第 1 行：日志标题
                          "No records",   false,  // 代码行
                          // 第 2 行：无记录提示
                          "in past week", false,  // 代码行
                          // 第 3 行：时间范围说明
                          (mode == MenuMode::VIEW) ? "MODE:Back" : "MODE:Enter",  // 代码行
                          // 第 4 行：VIEW 模式显示返回提示，NORMAL 模式显示进入提示
                          false);  // 代码行
                break;  // 退出 switch/循环
                // 无记录时渲染完成，退出 switch
            }  // 块结束

            int total = static_cast<int>(log_cache.size());  // 代码行
            // 记录总数
            int idx   = std::max(0, std::min(log_idx, total - 1));  // 代码行
            // 当前显示索引，裁剪到 [0, total-1] 防止越界
            const AbnormalRecord& r = log_cache[static_cast<size_t>(idx)];  // 代码行
            // 获取当前索引对应的异常记录引用

            char nbuf[32];            snprintf(nbuf, sizeof(nbuf), " %d/%d", idx + 1, total);  // 代码行
            // 格式化序号字符串，如 " 3/7"（1-based）
            std::string r1 = std::string(title) + nbuf;  // 代码行
            // 第 1 行：标题 + 序号，如 "Bearing Log  3/7"
            std::string ts = r.timestamp.size() >= 16  // 代码行
                                 ? r.timestamp.substr(5, 11) : r.timestamp;  // 代码行
            // 从完整时间戳中提取 "MM-DD HH:MM" 部分（跳过年份节省显示空间）
            std::string r3 = r.class_name;  // 代码行
            // 第 3 行：异常类别名称（如 "外圈故障"）
            std::string r4;  // 代码行
            // 第 4 行内容（根据模式不同）
            if (mode == MenuMode::VIEW) {  // 条件判断
            // VIEW 模式：显示置信度、电流和翻页提示
                char cbuf[8], mbuf[10];  // 代码行
                snprintf(cbuf, sizeof(cbuf), "%.0f%%", r.confidence * 100.0f);  // 代码行
                // 格式化置信度为整数百分比（如 "92%"）
                snprintf(mbuf, sizeof(mbuf), "%.1fmA", r.current_ma);  // 代码行
                // 格式化电流值（保留一位小数，如 "14.0mA"）
                r4 = std::string(cbuf) + "  " + mbuf + " L/R:flip";  // 代码行
                // 拼接第 4 行：置信度 + 电流 + 翻页提示
            } else {  // else 分支
            // NORMAL 模式：显示置信度和进入提示
                char cbuf[8];  // 代码行
                snprintf(cbuf, sizeof(cbuf), "%.0f%%", r.confidence * 100.0f);  // 代码行
                // 格式化置信度
                r4 = std::string(cbuf) + "  MODE:Enter";  // 代码行
                // 拼接第 4 行：置信度 + 进入提示
            }  // 块结束
            drawOLED4(r1.c_str(), false, ts.c_str(), false,  // 代码行
            // 第 1 行：标题+序号；第 2 行：日期时间
                      r3.c_str(), is_bearing, r4.c_str(), false);  // 代码行
                      // 第 3 行：类别名称（轴承日志用中文字体）；第 4 行：统计+提示
            break;  // 退出 switch/循环
        }  // 块结束

        // ── EXPORT_LOG ────────────────────────────────────────────
        case MenuPage::EXPORT_LOG:  // case 分支
        // 日志导出页面
            if (mode == MenuMode::NORMAL) {  // 条件判断
            // NORMAL 模式：仅显示导航提示，不显示确认按钮
                drawOLED4("Export Log",        false,  // 代码行
                          "L:Prev  R:Next",    false,  // 代码行
                          "MODE:Enter",        false,  // 代码行
                          "to confirm export", false);  // 代码行
            } else {  // else 分支
            // VIEW 模式：显示确认导出操作提示
                drawOLED4("Export Log",       false,  // 代码行
                          "MODE: Do export",  false,  // 代码行
                          // MODE 键执行实际导出操作
                          "L/R: Back",        false,  // 代码行
                          // LEFT/RIGHT 退出 VIEW 模式
                          "",                 false);  // 代码行
            }  // 块结束
            break;  // 退出 switch/循环

        // ── SHUTDOWN ──────────────────────────────────────────────
        case MenuPage::SHUTDOWN:  // case 分支
        // 关机确认页面（三级确认防止误操作）
            if (mode == MenuMode::NORMAL) {  // 条件判断
            // NORMAL 模式：仅提示如何进入确认流程
                drawOLED4("Shutdown",           false,  // 代码行
                          "L:Prev  R:Next",     false,  // 代码行
                          "MODE:Enter",         false,  // 代码行
                          "to confirm",         false);  // 代码行
            } else if (!shutdown_confirm) {  // 代码行
            // VIEW 模式 + 第一次确认未完成
                drawOLED4("Shutdown",           false,  // 代码行
                          "MODE: Confirm",      false,  // 代码行
                          // 按 MODE 进行第一次确认
                          "L/R: Back",          false,  // 代码行
                          "",                   false);  // 代码行
            } else {  // else 分支
            // VIEW 模式 + 第一次已确认，等待第二次确认（二次确认防误操作）
                drawOLED4("Are you sure?",      false,  // 代码行
                          "Press MODE again",   false,  // 代码行
                          "to power off",       false,  // 代码行
                          "L/R: Cancel",        false);  // 代码行
                          // LEFT/RIGHT 取消关机操作
            }  // 块结束
            break;  // 退出 switch/循环

        default:  // 默认分支
        // 未知页面：显示默认待机界面（防御性兜底）
            drawOLED4("Bearing Diag.", false, "Running...", false,  // 代码行
                      "L:Prev  R:Next", false, "MODE:Enter page", false);  // 代码行
            break;  // 退出 switch/循环
    }  // 块结束
}  // 块结束

// render_page：按键回调持锁时调用，快速刷新当前页面
// 注意：必须已持有 menu_mtx_
void ExtendedFeaturesFramework::render_page() {  // 代码行
// 在已持有 menu_mtx_ 的情况下调用，直接传入成员变量引用渲染当前页面
    render_page_unlocked(cur_page_, menu_mode_, snap_,  // 代码行
    // 传入当前页面、模式和 AI 数据快照（成员变量，持锁时安全访问）
                         cursor_visible_, shutdown_confirm_,  // 代码行
                         // 传入光标状态和关机确认状态
                         log_idx_, settings_item_,  // 代码行
                         // 传入日志索引和当前设置项
                         conf_threshold_, alarm_delay_,  // 代码行
                         // 传入置信度阈值和报警延迟
                         log_cache_);  // 持锁时直接传成员变量，安全
                         // 持锁时传入成员变量的 const 引用（无竞态风险）
}  // 块结束

// ============================================================================
// 页面渲染
// ============================================================================

// ── STANDBY（待机）────────────────────────────────────────────────────
// NORMAL模式下：LEFT/RIGHT 切换页面，MODE 进入当前页的 VIEW 模式
void ExtendedFeaturesFramework::render_standby() {  // 代码行
// 渲染待机页面（由菜单刷新线程通过 render_page 间接调用）
#ifdef BEARING_VIBRATION_PROJECT   // 条件编译
    static const std::string NAME = "Bearing Diag.";  // 代码行
    // 轴承振动项目名称
#else  // 条件编译
    static const std::string NAME = "Multi-Detect";  // 代码行
    // 未定义项目宏的通用名称
#endif  // 条件编译
    const char* hint = (menu_mode_ == MenuMode::VIEW)  // 代码行
                       ? "MODE:Back  L/R:page"   // VIEW 模式下提示
                       : "L:Prev  R:Next";  // 代码行
    // 根据当前模式选择操作提示文本
    oled4(NAME,               false,  // 代码行
    // 第 1 行：项目名称
          "Running...",       false,  // 代码行
          // 第 2 行：运行状态
          hint,               false,  // 代码行
          // 第 3 行：操作提示
          "MODE:Enter page",  false);  // 代码行
          // 第 4 行：固定提示（MODE 键进入当前页）
}  // 块结束

// ── Project Status（运行状态）─────────────────────────────────────────
// 四行布局（6x13 英文，中文行用 wqy16）：
//   Row1(y=12): "Project Status"  标题（英文小字体）
//   Row2(y=26): 检测结果 + 置信度（中文 wqy16，若有中文标签）
//   Row3(y=40): "HH:MM  XX.XXmA"  时间+电流（英文）
//   Row4(y=55): "BRG:ON  FIRE:OFF" 继电器（英文）
void ExtendedFeaturesFramework::render_project_status() {  // 代码行
// 渲染项目状态页面，四行布局显示当前推理结果和系统状态
    // ── Row1：页面标题 ──────────────────────────────────────────
    const std::string r1 = "Project Status";  // 代码行
    // 标题行固定内容

    // ── Row2：检测状态 / 最新结果 + 置信度 ─────────────────────
    std::string r2;  // 代码行
    // 第 2 行内容
    bool r2_chinese = false;  // 代码行
    // 是否使用中文字体（类别标签可能为中文时设为 true）
    switch (snap_.run_state) {  // switch 分支选择
    // 根据 AI 推理运行状态渲染不同内容
        case MenuSnapshot::RunState::RUNNING:  // case 分支
            r2 = "Running...";  // 代码行
            // 推理进行中
            break;  // 退出 switch/循环
        case MenuSnapshot::RunState::SUCCESS:  // case 分支
            r2 = snap_.result_label + " " + fmt_conf(snap_.result_confidence);  // 代码行
            // 推理成功：类别标签 + 置信度（如 "外圈故障 92.3%"）
            r2_chinese = true;   // result_label 可能含中文
            // 标记使用中文字体渲染
            break;  // 退出 switch/循环
        case MenuSnapshot::RunState::FAILED:  // case 分支
            r2 = "Fail:" + snap_.fail_reason.substr(  // 代码行
                     0, std::min<size_t>(snap_.fail_reason.size(), 8));  // 代码行
            // 推理失败：显示失败原因（截取前 8 字符适应 OLED 行宽）
            break;  // 退出 switch/循环
    }  // 块结束

    // ── Row3：时间 + 电流 ───────────────────────────────────────
    // "22:48  12.43mA" ≈ 14字符 × 6px = 84px，绰绰有余
    std::string ts5 = snap_.result_timestamp.size() >= 5  // 代码行
                          ? snap_.result_timestamp.substr(0, 5)  // 代码行
                          : "--:--";  // 代码行
    // 提取时间戳的 "HH:MM" 部分（不足时显示 "--:--"）
    std::string r3 = ts5 + "  " + fmt_ma(snap_.current_ma);  // 代码行
    // 拼接第 3 行：时间 + 双空格 + 电流值

    // ── Row4：继电器状态 ────────────────────────────────────────
    // "BRG:ON  FIRE:OFF" = 16字符 × 6px = 96px ✓（原9x18B会超限）
    std::string r4 = std::string("BRG:")  // 代码行
                   + (snap_.relay_bearing ? "ON " : "OFF")  // 代码行
                   // K1（轴承继电器）状态
                   + "  FIRE:"  // 代码行
                   + (snap_.relay_fire ? "ON " : "OFF");  // 代码行
                   // K2（火焰继电器）状态

    oled4(r1, false, r2, r2_chinese, r3, false, r4, false);  // 代码行
    // 调用四行 OLED 渲染（第 2 行根据 r2_chinese 自动选择字体）
}  // 块结束

// ── Log Bearing / Log Fire（异常日志）────────────────────────────────
// Row1: 序号/总数  MM-DD HH:MM
// Row2: 类别名称（中文/英文）
// Row3: 置信度%  电流mA
void ExtendedFeaturesFramework::reload_log_cache(MenuPage page) {  // 代码行
// 重新加载指定页面的日志缓存（从 DisplayLoggingFramework 读取一周异常记录）
    if (!dl_) { log_cache_.clear(); log_cache_page_ = page; return; }  // 条件判断
    // 若 DisplayLoggingFramework 未注册（dl_=nullptr），清空缓存后返回
    log_cache_.clear();  // 代码行
    // 清空旧日志缓存
    ProjectType pt = (page == MenuPage::LOG_BEARING)  // 代码行
                         ? ProjectType::BEARING_VIBRATION  // 代码行
                         : ProjectType::FIRE_DETECTION;  // 代码行
    // 根据页面类型确定项目类型
    dl_->load_week_abnormal(pt, log_cache_);  // 代码行
    // 从文件加载一周内的异常记录到 log_cache_
    log_cache_page_ = page;  // 代码行
    // 更新缓存对应的页面标识
    log_idx_ = log_cache_.empty() ? 0 : static_cast<int>(log_cache_.size()) - 1;  // 代码行
    // 浏览索引初始化为最新记录（最后一条）；空缓存时为 0
}  // 块结束

void ExtendedFeaturesFramework::render_log_page(MenuPage page) {  // 代码行
// 渲染日志页面（持锁版本，在按键回调中调用）
    if (log_cache_page_ != page) reload_log_cache(page);  // 条件判断
    // 若缓存不是当前页的，先重载日志

    const bool is_bearing = (page == MenuPage::LOG_BEARING);  // 代码行
    // 判断是轴承日志还是火焰日志
    const char* title = is_bearing ? "Bearing Log" : "Fire Log";  // 代码行
    // 设置页面标题

    if (log_cache_.empty()) {  // 条件判断
    // 无记录时渲染空状态
        oled4(title,          false,  // 代码行
              "No records",   false,  // 代码行
              "in past week", false,  // 代码行
              (menu_mode_ == MenuMode::VIEW) ? "MODE:Back" : "MODE:Enter",  // 代码行
              false);  // 代码行
        return;  // 函数返回
        // 无记录渲染完成，直接返回
    }  // 块结束

    int total = static_cast<int>(log_cache_.size());  // 代码行
    // 总记录数
    int idx   = std::max(0, std::min(log_idx_, total - 1));  // 代码行
    // 当前索引，裁剪到合法范围
    const AbnormalRecord& r = log_cache_[static_cast<size_t>(idx)];  // 代码行
    // 获取当前记录的引用

    // Row1: "Bearing Log  3/7"
    char nbuf[32];  // 代码行
    snprintf(nbuf, sizeof(nbuf), " %d/%d", idx + 1, total);  // 代码行
    // 格式化序号（1-based）
    std::string r1 = std::string(title) + nbuf;  // 代码行
    // 拼接标题和序号

    // Row2: "MM-DD HH:MM"
    std::string ts = r.timestamp.size() >= 16  // 代码行
                         ? r.timestamp.substr(5, 11)  // 代码行
                         : r.timestamp;  // 代码行
    // 提取日期时间的 "MM-DD HH:MM" 部分（跳过年份节省空间）

    // Row3: 类别名称
    std::string r3 = r.class_name;  // 代码行
    // 直接使用记录中的类别名称字符串

    // Row4: VIEW模式显示翻页提示 + 置信度，NORMAL模式显示进入提示
    std::string r4;  // 代码行
    if (menu_mode_ == MenuMode::VIEW) {  // 条件判断
    // VIEW 模式：显示详细信息和操作提示
        char cbuf[8], mbuf[10];  // 代码行
        snprintf(cbuf, sizeof(cbuf), "%.0f%%", r.confidence * 100.0f);  // 代码行
        // 格式化置信度百分比（整数）
        snprintf(mbuf, sizeof(mbuf), "%.1fmA", r.current_ma);  // 代码行
        // 格式化电流值（保留一位小数）
        r4 = std::string(cbuf) + "  " + mbuf + " L/R:flip";  // 代码行
        // 拼接：置信度 + 电流 + 翻页提示
    } else {  // else 分支
    // NORMAL 模式：仅显示置信度和进入提示
        char cbuf[8];  // 代码行
        snprintf(cbuf, sizeof(cbuf), "%.0f%%", r.confidence * 100.0f);  // 代码行
        // 格式化置信度
        r4 = std::string(cbuf) + "  MODE:Enter";  // 代码行
        // 拼接：置信度 + 进入提示
    }  // 块结束

    oled4(r1, false, ts, false, r3, is_bearing, r4, false);  // 代码行
    // 四行渲染（第 3 行轴承类别使用中文字体 is_bearing=true）
}  // 块结束

// ── Export Log ────────────────────────────────────────────────────────
// NORMAL模式：显示切换提示，不显示确认按钮
// VIEW模式  ：显示 MODE:Export  L/R:Back
void ExtendedFeaturesFramework::render_export_page() {  // 代码行
// 渲染日志导出页面
    if (menu_mode_ == MenuMode::NORMAL) {  // 条件判断
    // NORMAL 模式：显示导航提示（尚未进入确认状态）
        oled4("Export Log",        false,  // 代码行
              "L:Prev  R:Next",    false,  // 代码行
              "MODE:Enter",        false,  // 代码行
              "to confirm export", false);  // 代码行
    } else {  // else 分支
    // VIEW 模式：显示导出确认界面
        oled4("Export Log",       false,  // 代码行
              "MODE: Do export",  false,  // 代码行
              // MODE 键触发实际导出
              "L/R: Back",        false,  // 代码行
              // LEFT/RIGHT 退出 VIEW 模式
              "",                 false);  // 代码行
    }  // 块结束
}  // 块结束

void ExtendedFeaturesFramework::do_export() {  // 代码行
    // 注意：此函数必须在 menu_mtx_ 释放后调用（锁外），避免死锁
    // 更新显示：导出中
    drawOLED4("Exporting...", false, "Please wait", false, "", false, "", false);  // 代码行
    // 在执行导出前先显示 "Exporting..." 提示，让用户知道操作正在进行

    ErrorCode r = dl_ ? dl_->export_week_logs("/tmp/log_export")  // 代码行
    // 若 DisplayLoggingFramework 已注册，调用导出接口将日志写入 /tmp/log_export
                      : ErrorCode::NOT_INITIALIZED;  // 代码行
                      // 未注册时返回未初始化错误
    if (r == ErrorCode::SUCCESS) {  // 条件判断
    // 导出成功
        drawOLED4("Export done!",    false, "/tmp/log_export", false,  // 代码行
        // 第 1 行：导出完成提示；第 2 行：导出文件路径
                  "", false, "MODE:Back", false);  // 代码行
                  // 第 4 行：返回操作提示
        std::cout << "[Menu] 日志已导出到 /tmp/log_export" << std::endl;  // 打印信息
        // 打印导出成功信息
    } else {  // else 分支
    // 导出失败
        drawOLED4("Export failed",  false, "Check storage",  false,  // 代码行
        // 第 1 行：导出失败提示；第 2 行：检查存储设备提示
                  "", false, "MODE:Back", false);  // 代码行
    }  // 块结束
    // sleep 在锁外，不会阻塞任何其他线程
    std::this_thread::sleep_for(std::chrono::seconds(2));  // 代码行
    // 等待 2 秒让用户看清导出结果，然后返回菜单主页
}  // 块结束

// ── Shutdown ──────────────────────────────────────────────────────────
// NORMAL模式：仅提示进入
// VIEW模式  ：显示确认/二次确认界面
void ExtendedFeaturesFramework::render_shutdown_page() {  // 代码行
// 渲染关机确认页面（三级防误操作）
    if (menu_mode_ == MenuMode::NORMAL) {  // 条件判断
    // NORMAL 模式：提示用户按 MODE 进入确认流程
        oled4("Shutdown",           false,  // 代码行
              "L:Prev  R:Next",     false,  // 代码行
              "MODE:Enter",         false,  // 代码行
              "to confirm",         false);  // 代码行
    } else if (!shutdown_confirm_) {  // 代码行
    // VIEW 模式 + 第一次确认未完成：等待 MODE 键第一次确认
        oled4("Shutdown",           false,  // 代码行
              "MODE: Confirm",      false,  // 代码行
              "L/R: Back",          false,  // 代码行
              "",                   false);  // 代码行
    } else {  // else 分支
    // VIEW 模式 + 第一次已确认：显示二次确认界面（最后防线）
        oled4("Are you sure?",      false,  // 代码行
              "Press MODE again",   false,  // 代码行
              "to power off",       false,  // 代码行
              "L/R: Cancel",        false);  // 代码行
    }  // 块结束
}  // 块结束

// ── Settings (EDIT模式) ───────────────────────────────────────────────
// 光标闪烁：cursor_visible_ 每500ms翻转，当前值行用 "[" / "]" 或 ">" 标记
void ExtendedFeaturesFramework::render_settings() {  // 代码行
// 渲染参数设置页面（EDIT 模式专用）
    // 光标标记：EDIT模式下当前值行加闪烁符号
    const char* cur_mark = cursor_visible_ ? ">" : " ";  // 代码行
    // cursor_visible_ 由菜单刷新线程按 BLINK_PERIOD_MS 翻转，实现光标闪烁

    std::string r2, r3;  // 代码行
    // 声明第 2、3 行内容
    switch (settings_item_) {  // switch 分支选择
    // 根据当前设置项渲染对应的调整界面
        case SettingsItem::CONF_THRESHOLD: {  // case 分支
        // 置信度阈值设置项
            r2 = std::string(cur_mark) + "Conf threshold";  // 代码行
            // 带光标标记的设置项名称
            char buf[20];  // 代码行
            snprintf(buf, sizeof(buf), "< %.2f >", conf_threshold_);  // 代码行
            // 格式化当前阈值，如 "< 0.85 >"
            r3 = buf;  // 代码行
            break;  // 退出 switch/循环
        }  // 块结束
        case SettingsItem::ALARM_DELAY: {  // case 分支
        // 报警延迟设置项
            r2 = std::string(cur_mark) + "Alarm delay";  // 代码行
            // 带光标标记的延迟设置项名称
            char buf[20];  // 代码行
            snprintf(buf, sizeof(buf), "< %ds >", alarm_delay_);  // 代码行
            // 格式化延迟值，如 "< 5s >"
            r3 = buf;  // 代码行
            break;  // 退出 switch/循环
        }  // 块结束
        default:  // 默认分支
            r2 = ">Settings";  // 代码行
            // 默认通用提示
            r3 = "";  // 代码行
            break;  // 退出 switch/循环
    }  // 块结束
    // Row4: 操作提示
    oled4("Settings [EDIT]",    false,  // 代码行
    // 第 1 行：EDIT 模式标题
          r2,                   false,  // 代码行
          // 第 2 行：当前设置项（含光标标记）
          r3,                   false,  // 代码行
          // 第 3 行：当前值（含调整箭头）
          "L/R:adj M:next 2s:exit", false);  // 代码行
          // 第 4 行：操作提示（L/R 调值，M 切换项目，超长按2s退出）
}  // 块结束

// ============================================================================
// 页面切换辅助（普通模式）
// ============================================================================

// 菜单顺序：STANDBY → STATUS → LOG_BEARING → LOG_FIRE → EXPORT → SHUTDOWN → STANDBY
static MenuPage next_page(MenuPage p) {  // 代码行
// 静态函数：返回指定页面的下一页（循环序列）
    switch (p) {  // switch 分支选择
        case MenuPage::STANDBY:        return MenuPage::PROJECT_STATUS;  // case 分支
        // STANDBY → PROJECT_STATUS
        case MenuPage::PROJECT_STATUS: return MenuPage::LOG_BEARING;  // case 分支
        // PROJECT_STATUS → LOG_BEARING
        case MenuPage::LOG_BEARING:    return MenuPage::LOG_FIRE;  // case 分支
        // LOG_BEARING → LOG_FIRE
        case MenuPage::LOG_FIRE:       return MenuPage::EXPORT_LOG;  // case 分支
        // LOG_FIRE → EXPORT_LOG
        case MenuPage::EXPORT_LOG:     return MenuPage::SHUTDOWN;  // case 分支
        // EXPORT_LOG → SHUTDOWN
        case MenuPage::SHUTDOWN:       return MenuPage::STANDBY;  // case 分支
        // SHUTDOWN → STANDBY（循环回到起点）
        default:                       return MenuPage::STANDBY;  // 代码行
        // 未知页面：回到 STANDBY（兜底保护）
    }  // 块结束
}  // 块结束
static MenuPage prev_page(MenuPage p) {  // 代码行
// 静态函数：返回指定页面的上一页（next_page 的逆向）
    switch (p) {  // switch 分支选择
        case MenuPage::STANDBY:        return MenuPage::SHUTDOWN;  // case 分支
        // STANDBY 的上一页是 SHUTDOWN（循环末尾）
        case MenuPage::PROJECT_STATUS: return MenuPage::STANDBY;  // case 分支
        case MenuPage::LOG_BEARING:    return MenuPage::PROJECT_STATUS;  // case 分支
        case MenuPage::LOG_FIRE:       return MenuPage::LOG_BEARING;  // case 分支
        case MenuPage::EXPORT_LOG:     return MenuPage::LOG_FIRE;  // case 分支
        case MenuPage::SHUTDOWN:       return MenuPage::EXPORT_LOG;  // case 分支
        default:                       return MenuPage::STANDBY;  // 代码行
        // 未知页面：回到 STANDBY
    }  // 块结束
}  // 块结束

void ExtendedFeaturesFramework::page_next() {  // 代码行
    // 切换到下一页时重置临时状态
    if (cur_page_ == MenuPage::SHUTDOWN) shutdown_confirm_ = false;  // 条件判断
    // 离开关机页时清除确认状态（防止残留）
    MenuPage np = next_page(cur_page_);  // 代码行
    // 计算下一页
    // 进入日志页时重载缓存
    if (np == MenuPage::LOG_BEARING || np == MenuPage::LOG_FIRE)  // 条件判断
        log_cache_page_ = MenuPage::PAGE_COUNT;  // 代码行
    // 将 log_cache_page_ 设为无效值，强制下次进入日志页时重新加载文件
    cur_page_ = np;  // 代码行
    // 更新当前页面
}  // 块结束
void ExtendedFeaturesFramework::page_prev() {  // 代码行
// 切换到上一页（与 page_next 逻辑对称）
    if (cur_page_ == MenuPage::SHUTDOWN) shutdown_confirm_ = false;  // 条件判断
    // 离开关机页时清除确认状态
    MenuPage pp = prev_page(cur_page_);  // 代码行
    // 计算上一页
    if (pp == MenuPage::LOG_BEARING || pp == MenuPage::LOG_FIRE)  // 条件判断
        log_cache_page_ = MenuPage::PAGE_COUNT;  // 代码行
    // 切换到日志页时强制重载缓存
    cur_page_ = pp;  // 代码行
    // 更新当前页面
}  // 块结束

// ============================================================================
// 按键响应逻辑（重构 v2）
//
// 三种交互模式（MenuMode）：
//   NORMAL : LEFT/RIGHT 切换页面，MODE 短按进入 VIEW 模式
//   VIEW   : 已进入某页查看（Log 翻页，Export/Shutdown 确认）
//            MODE 短按 = 退出 VIEW 回 NORMAL；
//            VIEW 页内操作：
//              Log    → LEFT:上一条  RIGHT:下一条（循环）
//              Export → MODE:执行导出  LEFT/RIGHT:退出 VIEW
//              Shutdown → MODE:确认/关机  LEFT/RIGHT:取消
//   EDIT   : 长按2s进入，调整设置值，光标闪烁
//            LEFT:减  RIGHT:加  MODE短按:切换下一项  MODE长按:退出EDIT
//
// 按键汇总：
//   LEFT  → NORMAL:上一页  VIEW/Log:上一条(循环)  VIEW/其他:退出  EDIT:减值
//   RIGHT → NORMAL:下一页  VIEW/Log:下一条(循环)  VIEW/其他:退出  EDIT:加值
//   MODE短按 → NORMAL:进VIEW  VIEW:退NORMAL/确认  EDIT:切换项
//   MODE长按(≥600ms) → 特殊确认（Shutdown二次确认保留，普通页不处理）
//   MODE超长按(≥2000ms) → NORMAL/VIEW:进EDIT  EDIT:退EDIT
// ============================================================================

// ── MODE 短按 ─────────────────────────────────────────────────────────
void ExtendedFeaturesFramework::on_key_mode_short() {  // 代码行
// MODE 键短按处理：EDIT→切换设置项；NORMAL→进VIEW；VIEW→页面专属操作
    bool do_export_flag    = false;  // 代码行
    // 标记是否需要在锁外执行导出操作（避免持锁时阻塞）
    bool do_shutdown_flag  = false;  // 代码行
    // 标记是否需要在锁外执行关机操作

    {  // 块开始
    // 内层作用域：限制 lock_guard 的生命周期
        std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
        // 加锁保护菜单状态的读写

        // ── EDIT 模式：短按切换下一设置项 ──────────────────────────
        if (menu_mode_ == MenuMode::EDIT) {  // 条件判断
        // 当前在 EDIT 模式
            int next_idx = (static_cast<int>(settings_item_) + 1)  // 代码行
                           % static_cast<int>(SettingsItem::ITEM_COUNT);  // 代码行
            // 计算下一个设置项的枚举索引（循环）
            settings_item_ = static_cast<SettingsItem>(next_idx);  // 代码行
            // 更新当前设置项
            cursor_visible_ = true;  // 代码行
            // 切换设置项时立即显示光标（重置闪烁状态）
            last_blink_ms_  = mono_ms();  // 代码行
            // 更新上次闪烁时刻（重置闪烁计时器）
            render_page();  // 代码行
            // 立即刷新页面显示新的设置项
            return;  // 函数返回
            // EDIT 模式处理完成，直接返回
        }  // 块结束

        // ── NORMAL 模式：进入 VIEW 模式 ─────────────────────────────
        if (menu_mode_ == MenuMode::NORMAL) {  // 条件判断
        // 当前在 NORMAL 模式
            menu_mode_ = MenuMode::VIEW;  // 代码行
            // 切换到 VIEW 模式
            if (cur_page_ == MenuPage::LOG_BEARING || cur_page_ == MenuPage::LOG_FIRE) {  // 条件判断
            // 若当前页是日志页，立即重载最新日志
                reload_log_cache(cur_page_);  // 代码行
                // 加载对应项目类型的一周异常记录
                if (!log_cache_.empty())  // 条件判断
                    log_idx_ = static_cast<int>(log_cache_.size()) - 1;  // 代码行
                // 日志非空时跳到最新记录（最后一条）
            }  // 块结束
            render_page();  // 代码行
            // 立即刷新页面（进入 VIEW 模式后更新按键提示文本）
            return;  // 函数返回
            // NORMAL 模式处理完成
        }  // 块结束

        // ── VIEW 模式：页面专属操作，仅设置标志，不在锁内执行 ───────
        switch (cur_page_) {  // switch 分支选择
        // 根据当前页面执行对应的 VIEW 模式操作
            case MenuPage::EXPORT_LOG:  // case 分支
                do_export_flag = true;   // ← 锁外执行
                // 导出操作耗时（文件IO + sleep），在锁外执行防止持锁阻塞
                break;  // 退出 switch/循环

            case MenuPage::SHUTDOWN:  // case 分支
                if (!shutdown_confirm_) {  // 条件判断
                // 第一次 MODE 按下：触发第一次确认
                    shutdown_confirm_ = true;  // 代码行
                    // 设置第一次确认标志
                    render_page();  // 代码行
                    // 立即刷新显示二次确认界面
                } else {  // else 分支
                    do_shutdown_flag = true;  // ← 锁外执行
                    // 第二次 MODE 按下：设置关机标志（锁外执行防止死锁）
                }  // 块结束
                break;  // 退出 switch/循环

            default:  // 默认分支
            // 其他 VIEW 页（如 Project Status）：MODE 短按退出 VIEW
                menu_mode_ = MenuMode::NORMAL;  // 代码行
                // 切回 NORMAL 模式
                if (cur_page_ == MenuPage::LOG_BEARING || cur_page_ == MenuPage::LOG_FIRE)  // 条件判断
                    log_cache_page_ = MenuPage::PAGE_COUNT;  // 代码行
                // 离开日志页时使缓存失效，下次进入时重新加载
                render_page();  // 代码行
                // 立即刷新页面
                break;  // 退出 switch/循环
        }  // 块结束
    }  // ← 锁在此处释放
    // menu_mtx_ 在此自动解锁

    // 锁外执行耗时操作，避免死锁
    if (do_export_flag) {  // 条件判断
    // 导出标志：执行实际导出操作
        do_export();                 // 内部含 sleep_for(2s)
        // 执行日志导出（2 秒耗时，在锁外执行）
        std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
        // 导出完成后重新加锁，更新菜单状态
        cur_page_  = MenuPage::STANDBY;  // 代码行
        // 导出完成后返回待机页面
        menu_mode_ = MenuMode::NORMAL;  // 代码行
        // 恢复 NORMAL 模式
        render_page();  // 代码行
        // 刷新页面
    }  // 块结束

    if (do_shutdown_flag) {  // 条件判断
        // 关机前显示提示，同样不能持锁
        drawOLED("Shutting down", false, "Goodbye~", false);  // 代码行
        // 显示关机提示（2 行，不持锁可直接调用）
        std::this_thread::sleep_for(std::chrono::milliseconds(800));  // 代码行
        // 等待 800 ms 让用户看清提示
        on_pwr_off();    // on_pwr_off 内部可能加锁，必须在 menu_mtx_ 释放后调用
        // 执行安全关机（in_pwr_off 内有原子比较交换和系统调用）
    }  // 块结束
}  // 块结束

// ── MODE 长按（600ms ≤ duration < 2000ms）─────────────────────────────
// 仅用于 Shutdown 页的二次保留（防止误触关机）
// 其他页面长按不做特殊响应（超长按2s才进EDIT）
void ExtendedFeaturesFramework::on_key_mode_long() {  // 代码行
// MODE 键长按（600ms~2000ms）处理：目前仅记录日志，不做实质操作
    std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
    // 加锁保护（虽然不修改状态，但保证与菜单刷新线程的同步）
    // 此函数现在不处理进入EDIT（已移到超长按），保留空实现
    // 只做日志记录
    std::cout << "[Menu] MODE long press (<2s), no action" << std::endl;  // 打印信息
    // 打印长按日志（600ms~2000ms 的长按不触发任何菜单操作）
}  // 块结束

// ── MODE 超长按（≥ 2000ms）：进入 / 退出 EDIT 模式 ───────────────────
void ExtendedFeaturesFramework::on_key_mode_very_long() {  // 代码行
// MODE 键超长按（≥2000ms）处理：在 NORMAL/VIEW 和 EDIT 之间切换
    std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
    // 加锁保护菜单模式状态

    if (menu_mode_ == MenuMode::EDIT) {  // 条件判断
    // 当前在 EDIT 模式：超长按退出 EDIT
        menu_mode_ = MenuMode::NORMAL;  // 代码行
        // 切回 NORMAL 模式
        std::cout << "[Menu] 退出EDIT  conf=" << conf_threshold_  // 打印信息
                  << "  delay=" << alarm_delay_ << "s" << std::endl;  // 代码行
        // 打印退出 EDIT 信息，显示保存的参数值（供验证）
    } else {  // else 分支
    // 当前在 NORMAL 或 VIEW 模式：超长按进入 EDIT
        menu_mode_      = MenuMode::EDIT;  // 代码行
        // 切换到 EDIT 模式
        settings_item_  = SettingsItem::CONF_THRESHOLD;  // 代码行
        // 进入 EDIT 时默认聚焦到置信度阈值设置项
        cursor_visible_ = true;  // 代码行
        // 立即显示光标（进入时光标可见）
        last_blink_ms_  = mono_ms();  // 代码行
        // 重置闪烁计时器
        std::cout << "[Menu] 进入EDIT设置模式" << std::endl;  // 打印信息
        // 打印进入 EDIT 模式的日志
    }  // 块结束
    render_page();  // 代码行
    // 立即刷新页面（显示或退出 EDIT 模式的界面）
}  // 块结束

// ── LEFT 键 ──────────────────────────────────────────────────────────
void ExtendedFeaturesFramework::on_key_left() {  // 代码行
// LEFT 键按下处理：EDIT→减小值；VIEW/Log→上一条；VIEW/其他→退出VIEW；NORMAL→上一页
    std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
    // 加锁保护菜单状态

    // EDIT 模式：减小当前设置值
    if (menu_mode_ == MenuMode::EDIT) {  // 条件判断
    // 当前在 EDIT 模式
        switch (settings_item_) {  // switch 分支选择
        // 根据当前设置项执行减小操作
            case SettingsItem::CONF_THRESHOLD:  // case 分支
                conf_threshold_ = std::max(CONF_MIN, conf_threshold_ - CONF_STEP);  // 代码行
                // 置信度阈值减小 CONF_STEP，不低于 CONF_MIN 下限
                conf_threshold_ = std::round(conf_threshold_ / CONF_STEP) * CONF_STEP;  // 代码行
                // 对齐到步长的整数倍（消除浮点误差）
                break;  // 退出 switch/循环
            case SettingsItem::ALARM_DELAY:  // case 分支
                alarm_delay_ = std::max(DELAY_MIN, alarm_delay_ - DELAY_STEP);  // 代码行
                // 报警延迟减小 DELAY_STEP 秒，不低于 DELAY_MIN 下限
                break;  // 退出 switch/循环
            default: break;  // 代码行
            // 未知设置项：不操作
        }  // 块结束
        render_page();  // 代码行
        // 立即刷新显示修改后的设置值
        return;  // 函数返回
        // EDIT 模式处理完成
    }  // 块结束

    // VIEW 模式
    if (menu_mode_ == MenuMode::VIEW) {  // 条件判断
    // 当前在 VIEW 模式
        switch (cur_page_) {  // switch 分支选择
            // Log 页：向前翻记录（更旧），到头后循环到最新
            case MenuPage::LOG_BEARING:  // case 分支
            case MenuPage::LOG_FIRE: {  // case 分支
            // 日志页：LEFT 向前（更旧的记录）翻页
                int total = static_cast<int>(log_cache_.size());  // 代码行
                if (total > 0) {  // 条件判断
                    log_idx_ = (log_idx_ - 1 + total) % total;  // 代码行
                    // 索引减 1，加 total 防止负数后取模（循环翻页）
                }  // 块结束
                break;  // 退出 switch/循环
            }  // 块结束
            // Shutdown：取消确认，退出 VIEW
            case MenuPage::SHUTDOWN:  // case 分支
                shutdown_confirm_ = false;  // 代码行
                // 取消关机确认（清除二次确认标志）
                menu_mode_ = MenuMode::NORMAL;  // 代码行
                // 退出 VIEW 模式，回到 NORMAL
                break;  // 退出 switch/循环
            // Export / 其他 VIEW 页：退出 VIEW
            default:  // 默认分支
                menu_mode_ = MenuMode::NORMAL;  // 代码行
                // 其他 VIEW 页按 LEFT 退出 VIEW 模式
                break;  // 退出 switch/循环
        }  // 块结束
        render_page();  // 代码行
        // 刷新页面
        return;  // 函数返回
        // VIEW 模式处理完成
    }  // 块结束

    // NORMAL 模式：切换到上一页
    page_prev();  // 代码行
    // 切换到上一页并重置临时状态
    render_page();  // 代码行
    // 刷新新页面
}  // 块结束

// ── RIGHT 键 ─────────────────────────────────────────────────────────
void ExtendedFeaturesFramework::on_key_right() {  // 代码行
// RIGHT 键按下处理：EDIT→增大值；VIEW/Log→下一条；VIEW/其他→退出VIEW；NORMAL→下一页
    std::lock_guard<std::mutex> lk(menu_mtx_);  // 代码行
    // 加锁保护菜单状态

    // EDIT 模式：增大当前设置值
    if (menu_mode_ == MenuMode::EDIT) {  // 条件判断
    // 当前在 EDIT 模式
        switch (settings_item_) {  // switch 分支选择
        // 根据当前设置项执行增大操作
            case SettingsItem::CONF_THRESHOLD:  // case 分支
                conf_threshold_ = std::min(CONF_MAX, conf_threshold_ + CONF_STEP);  // 代码行
                // 置信度阈值增大 CONF_STEP，不超过 CONF_MAX 上限
                conf_threshold_ = std::round(conf_threshold_ / CONF_STEP) * CONF_STEP;  // 代码行
                // 对齐到步长整数倍（消除浮点累积误差）
                break;  // 退出 switch/循环
            case SettingsItem::ALARM_DELAY:  // case 分支
                alarm_delay_ = std::min(DELAY_MAX, alarm_delay_ + DELAY_STEP);  // 代码行
                // 报警延迟增大 DELAY_STEP 秒，不超过 DELAY_MAX 上限
                break;  // 退出 switch/循环
            default: break;  // 代码行
            // 未知设置项：不操作
        }  // 块结束
        render_page();  // 代码行
        // 立即刷新显示修改后的设置值
        return;  // 函数返回
        // EDIT 模式处理完成
    }  // 块结束

    // VIEW 模式
    if (menu_mode_ == MenuMode::VIEW) {  // 条件判断
    // 当前在 VIEW 模式
        switch (cur_page_) {  // switch 分支选择
            // Log 页：向后翻记录（更新），到头后循环到最旧
            case MenuPage::LOG_BEARING:  // case 分支
            case MenuPage::LOG_FIRE: {  // case 分支
            // 日志页：RIGHT 向后（更新的记录）翻页
                int total = static_cast<int>(log_cache_.size());  // 代码行
                if (total > 0) {  // 条件判断
                    log_idx_ = (log_idx_ + 1) % total;  // 代码行
                    // 索引加 1 后取模（循环翻页）
                }  // 块结束
                break;  // 退出 switch/循环
            }  // 块结束
            // Shutdown：取消确认，退出 VIEW
            case MenuPage::SHUTDOWN:  // case 分支
                shutdown_confirm_ = false;  // 代码行
                // 取消关机确认
                menu_mode_ = MenuMode::NORMAL;  // 代码行
                // 退出 VIEW 模式
                break;  // 退出 switch/循环
            // Export / 其他 VIEW 页：退出 VIEW
            default:  // 默认分支
                menu_mode_ = MenuMode::NORMAL;  // 代码行
                // 其他 VIEW 页按 RIGHT 退出 VIEW 模式
                break;  // 退出 switch/循环
        }  // 块结束
        render_page();  // 代码行
        // 刷新页面
        return;  // 函数返回
        // VIEW 模式处理完成
    }  // 块结束

    // NORMAL 模式：切换到下一页
    page_next();  // 代码行
    // 切换到下一页并重置临时状态
    render_page();  // 代码行
    // 刷新新页面
}  // 块结束

// ============================================================================
// pigpio alert 回调 — KEY_MODE 三级判定（短按/长按/超长按）
// ============================================================================

void ExtendedFeaturesFramework::cb_key_mode(int, int level, uint32_t, void* ud) {  // 代码行
// pigpio alert 回调：BCM5（MODE 键）电平变化时由 pigpio 内部线程调用
// 三级判定：短按（<600ms）/ 长按（600~2000ms）/ 超长按（≥2000ms）
    auto* s = static_cast<ExtendedFeaturesFramework*>(ud);  // 代码行
    // 从用户数据指针还原 ExtendedFeaturesFramework 实例指针
    uint32_t now = mono_ms();  // 代码行
    // 获取当前单调毫秒时间

    if (level == 0) {  // 条件判断
        // 下降沿 = 按下：防抖后记录时刻
        if (now - s->key_mode_last_ms_.load() < DEBOUNCE_MS) return;  // 条件判断
        // 防抖处理：距上次有效按键事件不足 DEBOUNCE_MS 时忽略（消除机械抖动）
        s->key_mode_press_ms_.store(now);  // 代码行
        // 原子存储按下时刻（用于松开时计算持续时长）
    } else {  // else 分支
        // 上升沿 = 松开：计算持续时长，三级判定
        uint32_t press_ts = s->key_mode_press_ms_.load();  // 代码行
        // 读取之前存储的按下时刻
        if (press_ts == 0) return;  // 条件判断
        // press_ts 为 0 表示没有记录到对应的按下事件（直接松开或抖动），忽略
        uint32_t duration = now - press_ts;  // 代码行
        // 计算本次按键持续时长（毫秒）
        s->key_mode_press_ms_.store(0);  // 代码行
        // 清零按下时刻（防止重复触发）

        if (now - s->key_mode_last_ms_.load() < DEBOUNCE_MS) return;  // 条件判断
        // 松开时再次做防抖检查
        s->key_mode_last_ms_.store(now);  // 代码行
        // 更新上次有效事件时刻（用于下次防抖）

        if (duration >= LONG_PRESS_EDIT) {  // 条件判断
            // ≥ 2000ms：超长按 → 进入/退出 EDIT 模式
            s->on_key_mode_very_long();  // 代码行
            // 触发超长按处理（进入/退出 EDIT 参数设置模式）
        } else if (duration >= LONG_PRESS_MS) {  // 代码行
            // 600ms ≤ duration < 2000ms：长按（保留但不做特殊动作）
            s->on_key_mode_long();  // 代码行
            // 触发长按处理（当前版本仅记录日志）
        } else {  // else 分支
            // < 600ms：短按
            s->on_key_mode_short();  // 代码行
            // 触发短按处理（EDIT 切换项目/NORMAL 进VIEW/VIEW 页面操作）
        }  // 块结束
    }  // 块结束
}  // 块结束

void ExtendedFeaturesFramework::cb_key_left(int, int level, uint32_t, void* ud) {  // 代码行
// pigpio alert 回调：BCM6（LEFT 键）电平变化时调用
    if (level != 0) return;  // 条件判断
    // 仅处理下降沿（level=0 表示按下）；上升沿（level=1）忽略
    auto* s = static_cast<ExtendedFeaturesFramework*>(ud);  // 代码行
    // 还原实例指针
    uint32_t now = mono_ms();  // 代码行
    // 获取当前时间
    if (now - s->key_left_last_ms_.load() < DEBOUNCE_MS) return;  // 条件判断
    // 防抖：距上次有效 LEFT 按键事件不足 DEBOUNCE_MS 时忽略
    s->key_left_last_ms_.store(now);  // 代码行
    // 更新上次有效事件时刻
    s->on_key_left();  // 代码行
    // 调用 LEFT 键逻辑处理函数
}  // 块结束

void ExtendedFeaturesFramework::cb_key_right(int, int level, uint32_t, void* ud) {  // 代码行
// pigpio alert 回调：BCM13（RIGHT 键）电平变化时调用
    if (level != 0) return;  // 条件判断
    // 仅处理下降沿（按下事件）
    auto* s = static_cast<ExtendedFeaturesFramework*>(ud);  // 代码行
    // 还原实例指针
    uint32_t now = mono_ms();  // 代码行
    // 获取当前时间
    if (now - s->key_right_last_ms_.load() < DEBOUNCE_MS) return;  // 条件判断
    // 防抖处理
    s->key_right_last_ms_.store(now);  // 代码行
    // 更新上次有效事件时刻
    s->on_key_right();  // 代码行
    // 调用 RIGHT 键逻辑处理函数
}  // 块结束

// ============================================================================
// 原有存根接口
// ============================================================================

ErrorCode ExtendedFeaturesFramework::check_for_updates()     { return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：OTA 更新检查（功能未实现，返回 SUCCESS 使调用链不中断）
ErrorCode ExtendedFeaturesFramework::perform_update()        { return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：执行 OTA 更新（未实现）
ErrorCode ExtendedFeaturesFramework::switch_to_bootloader()  { return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：切换到 Bootloader 模式（未实现）
ErrorCode ExtendedFeaturesFramework::switch_to_application() { return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：从 Bootloader 切换回应用程序（未实现）
ErrorCode ExtendedFeaturesFramework::verify_checksum()       { return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：固件校验和验证（未实现）
ErrorCode ExtendedFeaturesFramework::send_sms_alert(const std::string& m, const std::vector<std::string>&)  // 代码行
    { std::cout << "[EXT] SMS: " << m << std::endl; return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：发送 SMS 告警（打印消息内容到 stdout，未实现实际发送）
ErrorCode ExtendedFeaturesFramework::send_email_alert(const std::string& s, const std::string&)  // 代码行
    { std::cout << "[EXT] Email: " << s << std::endl; return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：发送邮件告警（打印标题到 stdout，未实现实际发送）
ErrorCode ExtendedFeaturesFramework::connect_to_server(const std::string& u)  // 代码行
    { std::cout << "[EXT] Connect: " << u << std::endl; return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：连接到远程服务器（打印 URL 到 stdout，未实现实际连接）
ErrorCode ExtendedFeaturesFramework::send_diagnostic_data(const SystemStatus&)  // 代码行
    { return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：发送诊断数据（未实现）
ErrorCode ExtendedFeaturesFramework::auto_calibrate()        { return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：自动校准（未实现）
ErrorCode ExtendedFeaturesFramework::set_power_mode(int)     { return ErrorCode::SUCCESS; }  // 代码行
// 桩实现：设置电源模式（未实现）
float     ExtendedFeaturesFramework::get_battery_level()     { return 100.0f; }  // 代码行
// 桩实现：获取电池电量（始终返回 100%，未实现实际读取）

} // namespace MultiProject
// 关闭命名空间 MultiProject