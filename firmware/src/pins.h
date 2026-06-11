#pragma once
#include <stdint.h>

#include "config.h"  // MOTOR_COUNT

// ============ ESP32 引脚分配 ============
// SIM7600 4G 模块 (UART1)
#define MODEM_TX 27
#define MODEM_RX 26
#define MODEM_PWRKEY 4

// I2C 传感器总线 (SHT31 + BH1750; 4 电机时还挂 MCP23017 + ADS1115)
#define I2C_SDA 21
#define I2C_SCL 22

// 继电器低电平触发 (常见 8 路继电器板)
#define RELAY_ACTIVE_LOW 1

// 本地手动按钮 (仅 MOTOR_COUNT<=2 时启用, 对应第 1 台卷帘 / 第 1 台风口)
#define BTN_CURTAIN_UP 13
#define BTN_CURTAIN_DOWN 14
#define BTN_VENT_OPEN 25
#define BTN_VENT_CLOSE 23

// ============ 电机 IO 映射表 ============
// onExpander=false: relay*/limit* 为 ESP32 GPIO, currentCh 为 ADC GPIO
// onExpander=true : relay*/limit* 为 MCP23017 引脚号(0-15), currentCh 为 ADS1115 通道(0-3)
// hall 始终为 ESP32 GPIO (需要中断), <0 表示未装霍尔
struct MotorIO {
  bool onExpander;
  uint8_t relayOpen, relayClose;  // 开(卷起)/关(放下) 方向继电器
  uint8_t limitOpen, limitClose;  // 开/关方向限位 (低电平=触发)
  int8_t hall;                    // 霍尔脉冲输入
  uint8_t currentCh;              // ACS712 电流采样
};

#if MOTOR_COUNT == 1
// 1 台卷帘机, 全部直连
static const MotorIO MOTOR_IO[MOTOR_COUNT] = {
    {false, 16, 17, 32, 33, 5, 36},
};
#elif MOTOR_COUNT == 2
// 1 卷帘 + 1 风口, 全部直连 (与旧版引脚一致)
static const MotorIO MOTOR_IO[MOTOR_COUNT] = {
    {false, 16, 17, 32, 33, 5, 36},   // 卷帘
    {false, 18, 19, 34, 35, 15, 39},  // 风口
};
#elif MOTOR_COUNT == 4
// 4 台电机: 继电器走 MCP23017 A口(0-7), 限位走 B口(8-15),
// 电流走 ADS1115 通道 0-3, 霍尔仍直连 ESP32 GPIO (手动按钮引脚被复用, 故禁用按钮)
static const MotorIO MOTOR_IO[MOTOR_COUNT] = {
    {true, 0, 1, 8, 9, 5, 0},     // 卷帘 1
    {true, 2, 3, 10, 11, 15, 1},  // 卷帘 2
    {true, 4, 5, 12, 13, 13, 2},  // 风口 1
    {true, 6, 7, 14, 15, 14, 3},  // 风口 2
};
#else
#error "MOTOR_COUNT 仅支持 1 / 2 / 4"
#endif
