#pragma once
#include <stdint.h>
// 设备与网络配置。请按现场实际修改后再烧录。

// ---- 设备标识 (与后端/App 对应) ----
#define DEVICE_ID "gh1"

// ---- 4G APN (按运营商填写, 移动/联通常用 "cmnet", 电信 "ctnet") ----
#define GSM_APN  "cmnet"
#define GSM_USER ""
#define GSM_PASS ""
#define GSM_PIN  "" // SIM 卡 PIN, 一般留空

// ---- MQTT (你的云服务器公网地址) ----
#define MQTT_HOST "your.server.com"
#define MQTT_PORT 1883
#define MQTT_USER ""        // EMQX 鉴权用户名, 没有则留空
#define MQTT_PASS ""

// ---- 主题前缀 (须与后端 GH_TOPIC_PREFIX 一致) ----
#define TOPIC_PREFIX "gh"

// ---- 遥测上报间隔 (毫秒) ----
#define TELEMETRY_INTERVAL_MS 10000

// ---- 电机数量 (1 / 2 / 4), 可用编译参数 -D MOTOR_COUNT=4 覆盖 ----
// 1~2 台: 引脚直连; 4 台: 需加 MCP23017(继电器+限位) + ADS1115(电流), 见 pins.h
#ifndef MOTOR_COUNT
#define MOTOR_COUNT 2
#endif

// ---- 每台电机的类型与运动监测参数 ----
// type: 0=curtain(卷帘/棉被, 动作 up/down), 1=vent(卷膜器/风口, 动作 open/close)
// overA: 过流阈值 A (超过判卡死并停机); underA: 欠流阈值 A (<=0 关闭, 低于判绳断/空转)
// pulsesFull: 全行程脉冲数 (现场标定, <=0 表示未装霍尔, 关闭失速检测与开度计算)
// maxTravelMs: 行程超时 (实测全程时间 x1.5)
struct MotorParams {
  uint8_t type;
  const char* name;  // 上报/告警显示名, 可改为 "东侧卷帘" 等
  float overA, underA;
  int32_t pulsesFull;
  uint32_t maxTravelMs;
};

#if MOTOR_COUNT == 1
static const MotorParams MOTOR_PARAMS[MOTOR_COUNT] = {
    {0, "卷帘", 8.0f, 0.3f, 200, 120000UL},
};
#elif MOTOR_COUNT == 2
static const MotorParams MOTOR_PARAMS[MOTOR_COUNT] = {
    {0, "卷帘", 8.0f, 0.3f, 200, 120000UL},
    {1, "风口", 6.0f, 0.2f, 150, 90000UL},
};
#elif MOTOR_COUNT == 4
static const MotorParams MOTOR_PARAMS[MOTOR_COUNT] = {
    {0, "卷帘1", 8.0f, 0.3f, 200, 120000UL},
    {0, "卷帘2", 8.0f, 0.3f, 200, 120000UL},
    {1, "风口1", 6.0f, 0.2f, 150, 90000UL},
    {1, "风口2", 6.0f, 0.2f, 150, 90000UL},
};
#endif

// ---- 运动监测公共参数 ----
// 失速判定: 运动中超过该时间无新脉冲 => 卡死/缠绕
#define STALL_TIMEOUT_MS    3000
// 欠流判定: 启动 (留出启动浪涌时间) 后电流低于阈值 => 绳断/链条脱落/电机空转
#define UNDERCURRENT_GRACE_MS  2000

// ---- 本地兜底自动逻辑阈值 (断网时生效, 与后端规则保持一致) ----
#define LOCAL_VENT_TEMP_HIGH      30.0f
#define LOCAL_VENT_TEMP_HYST      3.0f
#define LOCAL_CURTAIN_TEMP_PROTECT 8.0f
#define LOCAL_CURTAIN_LUX_OPEN    8000.0f
#define LOCAL_CURTAIN_LUX_CLOSE   2000.0f
