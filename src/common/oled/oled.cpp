/**
  ******************************************************************************
  * @file    oled.cpp
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
 * @brief   OLED 显示驱动 — oled.h 接口的 C++ 实现文件
 * ----------------------------------------------------------------------------
 * @details 本文件实现了 oled.h 中声明的全部 4 个公共接口函数
 *          （initOLED / drawOLED / drawOLED3 / drawOLED4），
 *          底层依赖 u8g2 图形库通过 Linux I2C 总线驱动
 *          SSD1309 控制芯片的 128×64 像素 OLED 屏幕。
 *
 *          文件内部结构如下：
 *
 *          ── 全局资源 ──
 *            u8g2（static u8g2_t）    — u8g2 图形库的唯一实例
 *            ROW_Y[3]                 — 三行模式各行基线 y 坐标
 *            ROW4_Y[4]                — 四行模式各行基线 y 坐标
 *
 *          ── 内部辅助函数 ──
 *            draw_line()             — 三行模式单行绘制（9x18B / wqy16 字体）
 *            draw_line_small()       — 四行模式单行绘制（6x13 / wqy16 字体）
 *
 *          ── 公共接口实现 ──
 *            initOLED()              — 初始化控制器并推送全黑帧
 *            drawOLED()              — 双行显示（向后兼容）
 *            drawOLED3()             — 三行显示
 *            drawOLED4()             — 四行显示（v3 新增）
 *
 *          显示流程统一为三步：
 *            u8g2_ClearBuffer() → 逐行 draw_line*() → u8g2_SendBuffer()
 *          即"清空内存缓冲区 → 将文本写入缓冲区 → 一次性刷新到 OLED 屏幕"；
 *          采用全帧缓冲（Full Buffer）模式（_f 后缀），
 *          保证每次刷新的原子性（屏幕不会出现半帧撕裂）
 *
 * @note    本文件使用 extern "C" 包裹 u8g2/u8x8/linux-i2c 三个 C 头文件，
 *          确保 C++ 编译器以 C 链接规范（无名称修饰）解析这些符号。
 *          u8g2 是开源 C 图形库，不支持 C++ 直接编译，
 *          须通过 extern "C" 桥接。
 *          u8g2 全局实例（u8g2）以 static 修饰，限制在本翻译单元内可见，
 *          外部模块通过 oled.h 的函数接口间接访问，避免全局命名空间污染。
 * ============================================================================
 */

#include "../../../include/common/oled/oled.h"
// 引入 OLED 驱动公共接口头文件，获取 initOLED()、drawOLED*() 的函数声明；
// 包含此头文件的同时也引入了 extern "C" 包裹，
// 确保本 .cpp 文件中的函数定义以 C 链接规范导出，
// 与其他 C++ 编译单元调用时的符号解析一致

#include <wiringPi.h>
// 引入 WiringPi 库头文件；
// u8g2 的 Linux I2C 底层驱动（linux-i2c.h）依赖 WiringPi 的
// I2C 设备句柄初始化（wiringPiI2CSetup()），
// 须在 main.cpp 中已调用 wiringPiSetupGpio() 之后使用

#include <cstring>
// 引入 C 字符串函数（strlen、memcpy、memset 等）；
// 本文件中 draw_line() 和 draw_line_small() 通过
// text[0] == '\0' 检查空字符串，strlen 在此处并未直接使用，
// 引入为通用防御性头文件包含

extern "C" {
// 开启 extern "C" 链接规范声明块；
// 块内所有头文件中的声明均以 C 链接（无 C++ 名称修饰）方式处理；
// u8g2、u8x8、linux-i2c 均为纯 C 库，其头文件中的函数和类型不支持
// C++ 名称修饰，须通过 extern "C" 告知编译器按 C 规则解析符号

#include "u8g2.h"
// 引入 u8g2 图形库核心头文件；
// 提供：
//   u8g2_t          — 图形库实例结构体类型
//   u8g2_Setup_*()  — 控制器初始化函数族（根据芯片型号和接口选择对应变体）
//   u8g2_ClearBuffer()   — 清空内存帧缓冲区（全部置零 = 全黑）
//   u8g2_SendBuffer()    — 将内存缓冲区内容一次性发送到 OLED 显示 RAM
//   u8g2_SetFont()       — 设置当前绘图字体
//   u8g2_DrawUTF8()      — 在指定坐标绘制 UTF-8 编码文本字符串
//   u8g2_InitDisplay()   — 发送控制器初始化指令序列
//   u8g2_SetPowerSave()  — 控制显示器开/关电（0=开启，1=关闭）
//   u8g2_font_9x18B_tf   — 9×18 像素等宽英文字体（粗体）
//   u8g2_font_6x13_tf    — 6×13 像素等宽英文字体（小字）
//   u8g2_font_wqy16_t_gb2312 — 文泉驿 16px 中文字体（含 GB2312 汉字集）

#include "u8x8.h"
// 引入 u8x8 底层硬件抽象层头文件；
// u8g2 构建于 u8x8 之上，u8x8 负责底层字节传输协议；
// 提供：
//   u8x8_SetI2CAddress()       — 设置 I2C 从机地址（7 位地址需左移变为 8 位写地址）
//   u8x8_byte_linux_i2c        — Linux I2C 字节传输回调函数指针（send_byte 实现）
//   u8x8_linux_i2c_delay       — Linux I2C 延时回调函数指针（硬件延时实现）
// u8g2_Setup_*() 函数的第三、四参数即为这两个回调函数

#include "linux-i2c.h"
// 引入 u8g2 的 Linux I2C 平台适配层头文件；
// 提供在树莓派 Linux 系统上通过 /dev/i2c-x 设备文件执行 I2C 通信的
// 底层实现（wiringPi I2C API 封装）；
// u8x8_byte_linux_i2c 和 u8x8_linux_i2c_delay 的实际函数体定义于对应 .c 文件
}
// 结束 extern "C" 块；块外的声明恢复 C++ 默认名称修饰规则

// ── 全局 u8g2 实例 ─────────────────────────────────────────────
static u8g2_t u8g2;
// u8g2 图形库的全局唯一实例；
// static 修饰将其可见性限制在本翻译单元（oled.cpp）内，
// 外部模块无法直接访问此变量，只能通过 oled.h 的函数接口间接使用；
// u8g2_t 结构体内部包含：帧缓冲区指针、u8x8 硬件层实例、当前字体指针等；
// 全帧缓冲（Full Buffer，_f 后缀的 Setup 函数）模式下，
// 帧缓冲区大小 = 128 × 64 / 8 = 1024 字节，存储在 u8g2_t 内部或堆上

// ── 三行基线坐标（字体 16px，屏幕 64px，间距 20px）───────────
static const int ROW_Y[3] = { 14, 34, 54 };
// 三行模式各行文字的基线（baseline）y 坐标数组；
// 基线是字符底部对齐线（英文小写字母 x 的底部位置）；
// 坐标系原点在屏幕左上角，y 轴向下增大：
//   ROW_Y[0] = 14：第 1 行基线，字符顶部约在 y = 14 - 16 = -2（略出屏幕上方）
//                   → 实际上 9x18B 字体 ascent（上升高度）约 13px，
//                     字符顶部在 y = 14 - 13 = 1，在屏幕内
//   ROW_Y[1] = 34：第 2 行基线，行间距 = 34 - 14 = 20px（略大于字体高度 18px）
//   ROW_Y[2] = 54：第 3 行基线，距屏幕底部 64 - 54 = 10px（容纳字母下延部分）
// 与 oled.h 中注释的布局规范一致

// ── 四行基线坐标（英文 6x13 字体，高13px，间距14px）──────────
// Row1: y=12  Row2: y=26  Row3: y=40  Row4: y=55
// 行顶边: 12-13=-1(~0), 26-13=13, 40-13=27, 55-13=42 → 均在64px内
static const int ROW4_Y[4] = { 12, 26, 40, 55 };
// 四行模式各行文字的基线 y 坐标数组；
// 使用 6x13 英文小字体（字符高 13px）以在 64px 高度内容纳四行：
//   ROW4_Y[0] = 12：第 1 行基线，行顶边 ≈ 12 - 13 = -1（约 0，紧贴屏幕顶部）
//   ROW4_Y[1] = 26：第 2 行基线，行间距 = 26 - 12 = 14px
//   ROW4_Y[2] = 40：第 3 行基线，行间距 = 40 - 26 = 14px
//   ROW4_Y[3] = 55：第 4 行基线，行间距 = 55 - 40 = 15px（末行稍大）
//                   距屏幕底部 64 - 55 = 9px（容纳字母下延）
// 原有注释中已验证各行顶边均在 [0, 64px) 范围内（不越界）；
// 注意：若某行使用 wqy16 中文字体（16px 高 > 14px 行间距），
//       会与相邻行产生约 2px 的像素重叠，排版时须避免相邻行同时含中文

// ── 内部：三行模式画一行文本（9x18B / wqy16）─────────────────
static void draw_line(int y, const char* text, bool chinese) {
    // 三行模式专用的单行文本绘制辅助函数；
    // static 修饰限制为文件内部可见，不对外暴露；
    // 参数 y：目标行的基线 y 坐标（来自 ROW_Y 数组）
    // 参数 text：UTF-8 编码的文本字符串指针
    // 参数 chinese：字体选择标志（true=中文字体，false=英文字体）

    if (!text || text[0] == '\0') return;
    // 防御性检查：text 为 NULL 指针或空字符串（首字符为 '\0'）时直接返回；
    // 避免将 NULL 或空字符串传入 u8g2_DrawUTF8() 导致崩溃或绘制无意义内容；
    // oled.h 接口说明中"传 NULL 或 "" 跳过该行"的行为即由此实现

    u8g2_SetFont(&u8g2, chinese ? u8g2_font_wqy16_t_gb2312
                                : u8g2_font_9x18B_tf);
    // 根据 chinese 标志选择字体：
    //   chinese=true  → u8g2_font_wqy16_t_gb2312（文泉驿 16px 中文字体）
    //                   包含 GB2312 汉字集（约 6763 个常用汉字）和 ASCII；
    //                   字符宽度 16px，高度 16px，每行约 8 个汉字
    //   chinese=false → u8g2_font_9x18B_tf（9×18 粗体等宽英文字体）
    //                   字符宽度 9px，高度 18px，每行约 14 个字符；
    // u8g2_SetFont() 仅更新 u8g2 实例内部的字体指针，不触发硬件操作

    u8g2_DrawUTF8(&u8g2, 0, y, text);
    // 在坐标 (x=0, y=y) 处绘制 UTF-8 文本 text 到内存帧缓冲区；
    // x=0 表示从屏幕最左侧开始绘制；
    // y 为基线坐标（字符底部对齐线）；
    // u8g2_DrawUTF8 支持多字节 UTF-8 编码（包含中文汉字的 3 字节序列），
    // 内部解码每个 Unicode 码位并从字体查找对应字形渲染到缓冲区；
    // 若字符超出屏幕宽度（128px），超出部分被自动裁剪（clipping）
}

// ── 内部：四行模式画一行文本（6x13 / wqy16）─────────────────
static void draw_line_small(int y, const char* text, bool chinese) {
    // 四行模式专用的单行文本绘制辅助函数；
    // 与 draw_line() 结构相同，仅字体选择不同：
    //   中文行仍使用 wqy16（无更小的中文字体备选）
    //   英文行改用 6x13 小字体（以容纳 4 行内容）

    if (!text || text[0] == '\0') return;
    // 防御性检查：空文本时跳过绘制，与 draw_line() 行为一致

    // 中文行：wqy16（16px高，汉字宽16px，约8字/行）
    // 英文行：6x13（13px高，每字符6px宽，约21字/行）
    u8g2_SetFont(&u8g2, chinese ? u8g2_font_wqy16_t_gb2312
                                : u8g2_font_6x13_tf);
    // 四行模式的字体选择：
    //   chinese=true  → u8g2_font_wqy16_t_gb2312（与三行模式相同的中文字体）
    //                   注意：16px 字高 > 四行模式 14px 行间距，
    //                         使用中文行时须确保相邻行无内容（留出空间）
    //   chinese=false → u8g2_font_6x13_tf（6×13 小字英文字体）
    //                   字符宽度 6px，高度 13px，每行约 21 个字符（128/6≈21）；
    //                   比三行模式的 9x18B 字体（约 14 字符/行）多约 50% 的字符容量

    u8g2_DrawUTF8(&u8g2, 0, y, text);
    // 在 (x=0, y=y) 坐标处绘制文本到内存帧缓冲区；
    // 与 draw_line() 中的调用完全相同，仅字体不同
}

// ── 公共接口实现 ───────────────────────────────────────────────

bool initOLED(void) {
    // oled.h 声明的初始化接口实现；
    // 执行 SSD1309 控制器的完整初始化序列，最终推送全黑帧消除雪花

    u8g2_Setup_ssd1309_i2c_128x64_noname2_f(
        &u8g2, U8G2_R0,
        u8x8_byte_linux_i2c,
        u8x8_linux_i2c_delay);
    // 调用 u8g2 针对 SSD1309 + I2C + 128×64 分辨率的配置函数；
    // 函数名分解：
    //   u8g2_Setup_      → u8g2 初始化函数族前缀
    //   ssd1309_         → 控制器型号（SSD1309，与 SSD1306 寄存器兼容）
    //   i2c_             → 通信接口为 I2C（非 SPI）
    //   128x64_          → 显示分辨率 128×64 像素
    //   noname2_         → 具体屏幕型号/品牌变体（布局参数不同）
    //   f                → Full Buffer 模式（完整 1024 字节帧缓冲，保证原子刷新）
    // 参数说明：
    //   &u8g2            → 指向全局 u8g2 实例的指针
    //   U8G2_R0          → 旋转方向：R0=0° 不旋转（屏幕正向安装）
    //   u8x8_byte_linux_i2c  → I2C 字节传输回调（通过 WiringPi /dev/i2c-1 发送）
    //   u8x8_linux_i2c_delay → I2C 延时回调（为 I2C 信号电平稳定提供等待）

    // 7-bit 0x3C → 8-bit 0x78
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
    // 设置 I2C 从机地址；
    // u8g2/u8x8 内部使用 8 位地址格式（含读写位），
    // 而硬件 I2C 地址 0x3C 是 7 位格式；
    // 转换关系：8 位写地址 = 7 位地址 << 1 = 0x3C << 1 = 0x78；
    // 若 OLED 模块 SA0 引脚接 VCC（高电平），7 位地址为 0x3D，
    // 对应 8 位地址 0x7A，须相应修改此行

    u8g2_InitDisplay(&u8g2);
    // 向 SSD1309 发送完整的初始化指令序列（约 20~30 个寄存器写入操作）；
    // 包括：显示方向设置、对比度初始化、多路复用比（Multiplex Ratio）设置、
    //       显示时钟频率、电荷泵（Charge Pump）使能等；
    // 此时显示器仍处于关闭（Power Save）状态，不显示内容

    u8g2_SetPowerSave(&u8g2, 0);   // 开启显示
    // 向 SSD1309 发送 Display ON 命令，使显示器退出省电（Power Save）模式；
    // 参数 0：关闭省电模式（即开启显示）；
    // 参数 1：进入省电模式（关闭显示）；
    // 调用后 OLED 屏幕点亮，开始显示 GRAM 中的内容

    // 关键：推送全黑帧，消除硬件 GRAM 随机值（雪花/白屏）
    u8g2_ClearBuffer(&u8g2);
    // 将 u8g2 内存帧缓冲区的全部 1024 字节清零（全部置 0x00 = 全黑）；
    // 必须在 SendBuffer 之前调用，否则发送的是未初始化的随机缓冲区内容

    u8g2_SendBuffer(&u8g2);
    // 将内存帧缓冲区的 1024 字节通过 I2C 一次性写入 SSD1309 显示 RAM（GRAM）；
    // SSD1309 收到数据后立即刷新显示内容：全黑帧覆盖芯片上电后 GRAM 中的
    // 随机值，消除雪花噪点或白屏现象；
    // 全帧传输（128×64/8=1024 字节）在 100kHz I2C 下约需 80ms，
    // 400kHz 快速模式下约需 20ms

    return true;
    // 当前实现总是返回 true（无错误检测）；
    // 若需要检测 I2C 写入失败（如设备未连接），应检查
    // u8x8_byte_linux_i2c 的返回值或捕获 WiringPi 的错误标志
}

// ── 两行显示（向后兼容） ───────────────────────────────────────
void drawOLED(const char* line1, bool chinese1,
              const char* line2, bool chinese2) {
    // oled.h 声明的双行显示接口实现（向后兼容旧版调用代码）；
    // 等效于 drawOLED3() 但第三行为空（不传入第三行参数）

    u8g2_ClearBuffer(&u8g2);
    // 清空内存帧缓冲区：将 1024 字节全部清零；
    // 必须在每次绘制新内容之前调用，否则上一帧的内容会残留在缓冲区中
    // 与新内容叠加（u8g2 绘制操作是"或"操作，不会自动清除已有像素）

    draw_line(ROW_Y[0], line1, chinese1);
    // 在第 1 行基线（y=14）绘制 line1；
    // 根据 chinese1 选择 9x18B 英文字体或 wqy16 中文字体

    draw_line(ROW_Y[1], line2, chinese2);
    // 在第 2 行基线（y=34）绘制 line2

    u8g2_SendBuffer(&u8g2);
    // 将完整帧缓冲区（含两行文字）一次性推送到 SSD1309 GRAM；
    // 推送完成后 OLED 屏幕立即更新显示内容；
    // 全帧刷新（而非局部更新）确保屏幕内容与缓冲区完全一致
}

// ── 三行显示 ──────────────────────────────────────────────────
void drawOLED3(const char* line1, bool chinese1,
               const char* line2, bool chinese2,
               const char* line3, bool chinese3) {
    // oled.h 声明的三行显示接口实现；
    // 三行充分利用 128×64 屏幕的垂直空间
    // （3 × 约 20px 行高 ≈ 60px，接近 64px 上限）

    u8g2_ClearBuffer(&u8g2);
    // 清空内存帧缓冲区，准备绘制新帧

    draw_line(ROW_Y[0], line1, chinese1);
    // 在第 1 行基线（y=14）绘制 line1

    draw_line(ROW_Y[1], line2, chinese2);
    // 在第 2 行基线（y=34）绘制 line2

    draw_line(ROW_Y[2], line3, chinese3);
    // 在第 3 行基线（y=54）绘制 line3；
    // y=54 距屏幕底部（64）留有 10px 空间，容纳字母下延部分（如 g、p、y 等）

    u8g2_SendBuffer(&u8g2);
    // 将三行文字的完整帧缓冲区一次性推送到 SSD1309 GRAM
}

// ── 四行显示（v3 新增）────────────────────────────────────────
// 英文行：u8g2_font_6x13_tf（约21字符/行，高13px）
// 中文行：u8g2_font_wqy16_t_gb2312（约8汉字/行，高16px）
void drawOLED4(const char* line1, bool chinese1,
               const char* line2, bool chinese2,
               const char* line3, bool chinese3,
               const char* line4, bool chinese4) {
    // oled.h 声明的四行显示接口实现（v3 新增）；
    // 注释标注"v3 新增"表示此接口在 oled 驱动第三版本中引入，
    // 用于解决菜单内容在三行模式下被截断的问题（见 oled.h 注释）；
    // 英文字体从 9x18B（约 14 字符/行）降级为 6x13（约 21 字符/行），
    // 每行字符容量提升约 50%，以显示更多内容

    u8g2_ClearBuffer(&u8g2);
    // 清空内存帧缓冲区，准备绘制新帧

    draw_line_small(ROW4_Y[0], line1, chinese1);
    // 在第 1 行基线（y=12）绘制 line1；
    // draw_line_small 使用 6x13 英文字体或 wqy16 中文字体

    draw_line_small(ROW4_Y[1], line2, chinese2);
    // 在第 2 行基线（y=26）绘制 line2

    draw_line_small(ROW4_Y[2], line3, chinese3);
    // 在第 3 行基线（y=40）绘制 line3

    draw_line_small(ROW4_Y[3], line4, chinese4);
    // 在第 4 行基线（y=55）绘制 line4；
    // y=55 距屏幕底部（64）留有 9px 空间

    u8g2_SendBuffer(&u8g2);
    // 将四行文字的完整帧缓冲区一次性推送到 SSD1309 GRAM；
    // 四行模式下若有中文行（wqy16，16px 高 > 14px 行间距），
    // 相邻行之间可能产生 2px 像素重叠，渲染正确性取决于相邻行内容是否
    // 在重叠区域有关键像素；ExtendedFeaturesFramework 的菜单渲染函数
    // 在设计布局时已规避此问题（中文行相邻行留空或内容不在边界像素处）
}