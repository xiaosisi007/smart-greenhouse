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

// ---- 本地兜底自动逻辑阈值 (断网时生效, 与后端规则保持一致) ----
#define LOCAL_VENT_TEMP_HIGH      30.0f
#define LOCAL_VENT_TEMP_HYST      3.0f
#define LOCAL_CURTAIN_TEMP_PROTECT 8.0f
#define LOCAL_CURTAIN_LUX_OPEN    8000.0f
#define LOCAL_CURTAIN_LUX_CLOSE   2000.0f
