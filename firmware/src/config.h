#pragma once
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

// ---- 电机过流保护阈值 (安培), 超过判定卡死/故障并停机 ----
#define CURTAIN_OVERCURRENT_A 8.0f
#define VENT_OVERCURRENT_A    6.0f

// ---- 运动监测 (判断"是否正常在升降": 失速/超时/欠流) ----
// 全行程脉冲数 (现场标定: 手动走一次全程读遥测脉冲计数). <=0 表示未装脉冲反馈, 关闭失速检测与开度计算
#define CURTAIN_PULSES_FULL 200
#define VENT_PULSES_FULL    150
// 失速判定: 运动中超过该时间无新脉冲 => 卡死/缠绕
#define STALL_TIMEOUT_MS    3000
// 行程超时: 从启动到限位的最大允许时间 (按实测全程时间 x1.5 设置)
#define CURTAIN_MAX_TRAVEL_MS 120000UL
#define VENT_MAX_TRAVEL_MS    90000UL
// 欠流判定: 启动 (留出启动浪涌时间) 后电流低于该值 => 绳断/链条脱落/电机空转. <=0 关闭
#define CURTAIN_UNDERCURRENT_A 0.3f
#define VENT_UNDERCURRENT_A    0.2f
#define UNDERCURRENT_GRACE_MS  2000

// ---- 本地兜底自动逻辑阈值 (断网时生效, 与后端规则保持一致) ----
#define LOCAL_VENT_TEMP_HIGH      30.0f
#define LOCAL_VENT_TEMP_HYST      3.0f
#define LOCAL_CURTAIN_TEMP_PROTECT 8.0f
#define LOCAL_CURTAIN_LUX_OPEN    8000.0f
#define LOCAL_CURTAIN_LUX_CLOSE   2000.0f
