/**
  ******************************************************************************
  * @file    oled.h
  * @author  工科女的日常:The Daily Life of An Engineering Girl - RedBook - NewCoder - CSDN_ WeChat Official Account
  * @details link——RedBook: http://xhslink.com/a/ipmoHKmrFZWfb     NewCoder:https://www.nowcoder.com/share/unjump/7241315122413047270
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
 * @brief   OLED 显示驱动 — C 语言公共接口头文件
 * ----------------------------------------------------------------------------
 * @details 本文件声明了基于 SSD1306/SSD1309 控制芯片的 128×64 像素 OLED 屏幕
 *          在树莓派（Linux）平台上的全部驱动接口函数。
 *          底层通信采用 I2C 总线，从机地址 0x3C，通过 Linux 标准 I2C 设备文件
 *          （/dev/i2c-x）进行读写操作，无需 WiringPi 等第三方库。
 *
 *          本文件对外暴露以下 4 个公共函数：
 *
 *          1. initOLED()     — 初始化 OLED 控制器，推送全黑帧消除上电雪花噪点
 *
 *          2. drawOLED()     — 双行模式显示，保留向后兼容
 *             字体：9x18B（英文）/ wqy16（中文），每行约 14 个英文字符
 *
 *          3. drawOLED3()    — 三行模式显示
 *             字体：9x18B（英文）/ wqy16（中文），各行基线 y = 14 / 34 / 54
 *
 *          4. drawOLED4()    — 四行模式显示，解决三行模式内容被截断的问题
 *             字体：6x13（英文，约 21 字符/行）/ wqy16（中文，约 8 汉字/行）
 *             各行基线 y = 12 / 26 / 40 / 55
 *
 * @note    本文件使用 extern "C" 包裹全部声明，确保 C++ 编译单元（如
 *          BearingVibrationProject.cpp、FireDetectionProject.cpp）能够以
 *          C 链接规范正确调用上述函数，避免 C++ 名称修饰（Name Mangling）
 *          导致链接失败。
 *
 * @par     硬件规格
 *          控制芯片：SSD1306 或 SSD1309（接口兼容）
 *          显示面板：128 像素（宽）× 64 像素（高）单色 OLED
 *          通信接口：I2C，从机地址 0x3C
 *          供电电压：3.3 V（与树莓派 GPIO 电平兼容，无需电平转换）
 * ============================================================================
 */

#ifndef OLED_H
// 头文件保护宏：若本头文件已被包含（宏已定义），则跳过全部内容，防止重复包含
#define OLED_H
// 定义头文件保护宏，与上方 #ifndef 配对使用

#ifdef __cplusplus
// 编译器检测宏：若当前编译单元为 C++（__cplusplus 宏由 C++ 编译器自动定义），
// 则进入 extern "C" 块；若为纯 C 编译单元则跳过，直接使用 C 链接规范
extern "C" {
// 开启 C 链接规范声明块；
// 块内所有函数声明均以 C 链接方式导出（不进行 C++ 名称修饰），
// 确保 C++ 调用方通过与 C 实现文件（oled.cpp）相同的符号名称完成链接
#endif
// 结束 #ifdef __cplusplus 条件编译块

#include <stdbool.h>
// 引入 C99 标准布尔类型头文件；
// 提供 bool、true、false 三个符号的定义；
// 在纯 C 编译单元中（非 C++）必须显式包含本头文件才能使用 bool 类型，
// 用于 initOLED() 的返回值及各 drawOLED* 函数的 chineseN 参数类型

/* ─────────────────────────────────────────────────────────────────────────
 * OLED 驱动接口  (SSD1306/SSD1309 128×64, I2C 0x3C)
 *
 * 三行模式（drawOLED / drawOLED3）：字体 9x18B / wqy16（高16px）
 *   Row1 baseline y = 14    ≈ 14字符/行（英文9px宽）
 *   Row2 baseline y = 34
 *   Row3 baseline y = 54
 *
 * 四行模式（drawOLED4）：字体 6x13 / wqy16（英文高13px，中文高16px）
 *   Row1 baseline y = 12    ≈ 21字符/行（英文6px宽）
 *   Row2 baseline y = 26
 *   Row3 baseline y = 40
 *   Row4 baseline y = 55
 *   注：中文行高16px，四行中若有中文行需确保内容 ≤ 8个汉字
 * ─────────────────────────────────────────────────────────────────────── */
/*
 * 模块说明注释块（原有注释保留）：
 *
 * 适用芯片：SSD1306 / SSD1309，两者寄存器指令集兼容，驱动层无需区分
 * 显示规格：128 列 × 64 行，单色（白色像素/黑色背景）
 * I2C 地址：0x3C（SA0 引脚接 GND 时的默认地址；SA0 接 VCC 时地址为 0x3D）
 *
 * ---- 三行模式布局说明（drawOLED / drawOLED3） ----
 * 使用 9x18B 英文字体（字符宽 9px，字符高 18px）或 wqy16 中文字体（字符高 16px）
 *   Row1：文字基线（baseline）y 坐标 = 14px
 *         128px 宽 ÷ 9px/字符 ≈ 14 个英文字符/行
 *   Row2：基线 y = 34px（行间距 = 34 - 14 = 20px，略大于字体高度 18px 以留白）
 *   Row3：基线 y = 54px（距屏幕底部 64 - 54 = 10px，预留字母下沉部分空间）
 *
 * ---- 四行模式布局说明（drawOLED4） ----
 * 英文使用 6x13 小字体（字符宽 6px，字符高 13px），中文仍使用 wqy16（字符高 16px）
 *   Row1：基线 y = 12px
 *         128px 宽 ÷ 6px/字符 ≈ 21 个英文字符/行（较三行模式多约 50%）
 *   Row2：基线 y = 26px（行间距 14px，适应 13px 字体高度）
 *   Row3：基线 y = 40px
 *   Row4：基线 y = 55px
 *   重要约束：wqy16 中文字体字符高 16px，大于四行模式行间距（14px），
 *             若某行显示中文，该行内容须控制在 ≤ 8 个汉字（8×16=128px 满行），
 *             且与相邻行之间存在像素重叠风险，排版时需谨慎
 */

/** 初始化 OLED（推送全黑帧消除上电雪花），返回 true 表示成功 */
/**
 * @brief   初始化 OLED 显示控制器
 * @details 执行以下初始化流程：
 *          1. 打开 Linux I2C 设备文件（如 /dev/i2c-1）
 *          2. 通过 ioctl 设置 I2C 从机地址为 0x3C
 *          3. 向 SSD1306/SSD1309 发送初始化指令序列
 *             （包括显示时序、对比度、扫描方向、内存寻址模式等）
 *          4. 向显示 RAM 推送全黑帧（128×64 = 1024 字节，全部写 0x00），
 *             消除芯片上电后显示 RAM 未定义内容导致的雪花噪点
 *          本函数应在调用任何 drawOLED* 函数之前调用一次。
 *          若 I2C 设备不存在或总线通信失败，返回 false 并记录错误信息。
 * @param   void  无输入参数
 * @return  bool  初始化成功返回 true；
 *                I2C 设备文件打开失败或写入指令失败返回 false
 */
bool initOLED(void);

/**
 * 双行显示（保留向后兼容）
 * @param line1/line2   UTF-8 文本，传 NULL 或 "" 跳过该行
 * @param chinese1/2    true=wqy16 中文字体，false=9x18B 英文字体
 */
/**
 * @brief   双行模式 OLED 显示（向后兼容接口）
 * @details 在 128×64 OLED 屏幕上显示两行文本，
 *          内部调用 drawOLED3() 并将第三行设为空字符串。
 *          保留本函数以兼容项目中已有的双行显示调用代码，
 *          新增功能应优先使用 drawOLED3() 或 drawOLED4()。
 *
 *          布局参数（与三行模式相同，第三行留空）：
 *            Row1 基线 y = 14px，Row2 基线 y = 34px
 *
 *          字体选择：
 *            chinese=false → 9x18B 英文字体（9px 宽 × 18px 高，约 14 字符/行）
 *            chinese=true  → wqy16 文泉驿中文字体（16px 高，约 8 汉字/行）
 *            同一行内不支持中英文混合字体；若行内同时含有中英文，
 *            建议统一使用 chinese=true 以确保中文字符正确渲染
 *
 * @param   line1     第一行文本，UTF-8 编码字符串；
 *                    传入 NULL 或 "" 时跳过该行（保留上次内容或显示空白）
 * @param   chinese1  第一行字体选择：true=wqy16 中文字体，false=9x18B 英文字体
 * @param   line2     第二行文本，UTF-8 编码字符串；传入 NULL 或 "" 跳过该行
 * @param   chinese2  第二行字体选择：true=wqy16 中文字体，false=9x18B 英文字体
 * @return  void      无返回值；若底层 I2C 写入失败，错误信息记录至系统日志
 */
void drawOLED(const char* line1, bool chinese1,
              const char* line2, bool chinese2);

/**
 * 三行显示
 * @param line1/2/3     UTF-8 文本，传 NULL 或 "" 跳过该行
 * @param chinese1/2/3  true=wqy16 中文字体，false=9x18B 英文字体
 */
/**
 * @brief   三行模式 OLED 显示
 * @details 在 128×64 OLED 屏幕上同时显示三行文本，
 *          充分利用屏幕垂直空间（3 × 18px 字体高度 + 行间距 ≈ 64px）。
 *          适用于需要同时展示三类信息的场景，例如：
 *            Row1：设备状态（"Normal" / "ALARM"）
 *            Row2：关键数值（振动 RMS等）
 *            Row3：时间戳或剩余寿命
 *
 *          布局参数：
 *            Row1 基线 y = 14px
 *            Row2 基线 y = 34px（行间距 20px）
 *            Row3 基线 y = 54px（行间距 20px，距屏幕底部 10px）
 *
 *          字体选择（每行独立）：
 *            chinese=false → 9x18B 英文字体（约 14 英文字符/行）
 *            chinese=true  → wqy16 中文字体（约 8 汉字/行）
 *
 * @param   line1     第一行文本，UTF-8 编码字符串；传入 NULL 或 "" 跳过该行
 * @param   chinese1  第一行字体选择：true=wqy16 中文字体，false=9x18B 英文字体
 * @param   line2     第二行文本，UTF-8 编码字符串；传入 NULL 或 "" 跳过该行
 * @param   chinese2  第二行字体选择：true=wqy16 中文字体，false=9x18B 英文字体
 * @param   line3     第三行文本，UTF-8 编码字符串；传入 NULL 或 "" 跳过该行
 * @param   chinese3  第三行字体选择：true=wqy16 中文字体，false=9x18B 英文字体
 * @return  void      无返回值；若底层 I2C 写入失败，错误信息记录至系统日志
 */
void drawOLED3(const char* line1, bool chinese1,
               const char* line2, bool chinese2,
               const char* line3, bool chinese3);

/**
 * 四行显示（解决菜单内容被截断问题）
 * 英文行使用 6x13 小字体（约21字符/行），中文行使用 wqy16（约8汉字/行）
 * @param line1/2/3/4     UTF-8 文本，传 NULL 或 "" 跳过该行
 * @param chinese1/2/3/4  true=wqy16 中文字体，false=6x13 英文字体
 */
/**
 * @brief   四行模式 OLED 显示（小字体，解决菜单内容截断问题）
 * @details 在 128×64 OLED 屏幕上同时显示四行文本。
 *          为在有限的 64px 垂直空间内容纳四行内容，
 *          英文字体从 9x18B 降级为 6x13（字符高从 18px 降至 13px），
 *          每行英文字符容量从约 14 个提升至约 21 个，
 *          适用于菜单列表、多参数同时展示等需要更多行数的场景。
 *
 *          布局参数：
 *            Row1 基线 y = 12px
 *            Row2 基线 y = 26px（行间距 14px，适配 13px 英文字体高度）
 *            Row3 基线 y = 40px（行间距 14px）
 *            Row4 基线 y = 55px（行间距 15px，距屏幕底部 9px）
 *
 *          字体选择（每行独立）：
 *            chinese=false → 6x13 英文字体（约 21 英文字符/行）
 *            chinese=true  → wqy16 中文字体（约 8 汉字/行，字符高 16px）
 *
 *          重要约束：
 *            wqy16 中文字体字符高 16px > 四行模式行间距 14px，
 *            含中文的行与相邻行之间存在 2px 像素重叠；
 *            因此含中文的行须确保上下相邻行为空行，
 *            或相邻行内容在重叠区域无关键像素，避免显示内容互相覆盖。
 *            每行中文内容须控制在 ≤ 8 个汉字（8×16px = 128px = 满行宽度）。
 *
 * @param   line1     第一行文本，UTF-8 编码字符串；传入 NULL 或 "" 跳过该行
 * @param   chinese1  第一行字体：true=wqy16 中文字体，false=6x13 英文字体
 * @param   line2     第二行文本，UTF-8 编码字符串；传入 NULL 或 "" 跳过该行
 * @param   chinese2  第二行字体：true=wqy16 中文字体，false=6x13 英文字体
 * @param   line3     第三行文本，UTF-8 编码字符串；传入 NULL 或 "" 跳过该行
 * @param   chinese3  第三行字体：true=wqy16 中文字体，false=6x13 英文字体
 * @param   line4     第四行文本，UTF-8 编码字符串；传入 NULL 或 "" 跳过该行
 * @param   chinese4  第四行字体：true=wqy16 中文字体，false=6x13 英文字体
 * @return  void      无返回值；若底层 I2C 写入失败，错误信息记录至系统日志
 */
void drawOLED4(const char* line1, bool chinese1,
               const char* line2, bool chinese2,
               const char* line3, bool chinese3,
               const char* line4, bool chinese4);

#ifdef __cplusplus
// 若当前编译单元为 C++，则关闭 extern "C" 声明块
}
// 关闭 extern "C" 块；
// 此后的声明（若有）将恢复 C++ 默认的名称修饰链接规范
#endif
// 结束 #ifdef __cplusplus 条件编译块

#endif /* OLED_H */
// 结束头文件保护宏条件编译块，与文件开头的 #ifndef OLED_H 配对；
// 行尾注释 /* OLED_H */ 标明所关闭的宏名称，便于多层嵌套头文件时快速定位配对关系