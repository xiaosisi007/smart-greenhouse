# 部署文档

## 一、云服务器（后端）

要求：一台有公网 IP 的云服务器（2核4G 起），已装 Docker + Docker Compose。

```bash
git clone <repo-url> && cd smart-greenhouse
docker compose up -d --build
docker compose ps      # 三个服务都 healthy/up
```

服务端口：

| 服务 | 端口 | 说明 |
|---|---|---|
| 后端 API | 8000 | REST + WebSocket，文档 `/docs` |
| EMQX MQTT | 1883 | 设备接入 |
| EMQX Dashboard | 18083 | 默认 admin/public，**首次登录请改密码** |
| TimescaleDB | 5432 | 默认 greenhouse/greenhouse |

### 安全加固（上线必做）
1. 改 EMQX Dashboard 密码，并在 EMQX 启用客户端**用户名/密码认证**或证书；后端通过 `GH_MQTT_USERNAME/GH_MQTT_PASSWORD` 配置。
2. 防火墙只放行必要端口（1883、8000、443）。生产建议 1883 走 **8883(TLS)**，8000 前置 Nginx + HTTPS。
3. 改数据库默认密码（`docker-compose.yml` 的 `POSTGRES_PASSWORD` 与后端 `GH_DATABASE_URL`）。

### 配置项（环境变量，前缀 `GH_`）
见 `backend/app/config.py`，常用：`GH_DATABASE_URL`、`GH_MQTT_HOST`、`GH_MQTT_PORT`、`GH_MQTT_USERNAME`、`GH_MQTT_PASSWORD`、`GH_TOPIC_PREFIX`、`GH_RULES_INTERVAL_S`。

## 二、固件（ESP32 + 4G）

1. 安装 [PlatformIO](https://platformio.org/)（VS Code 插件或 CLI）。
2. 改 `firmware/src/config.h`：`DEVICE_ID`、`GSM_APN`（移动/联通 `cmnet`、电信 `ctnet`）、`MQTT_HOST`（你的服务器公网地址）、`MQTT_PORT`、鉴权。
3. 按 `firmware/src/pins.h` 接线（见 `docs/wiring.md`）。
4. 烧录与监视：
   ```bash
   cd firmware
   pio run -t upload
   pio device monitor
   ```

## 三、App（Flutter 安卓）

```bash
cd app
flutter pub get
# 调试 (安卓模拟器访问宿主机后端用 10.0.2.2)
flutter run
# 打包正式 APK，指定你的服务器地址
flutter build apk --release \
  --dart-define=API_BASE=http://你的域名:8000 \
  --dart-define=WS_URL=ws://你的域名:8000/ws \
  --dart-define=DEVICE_ID=gh1
```

> 生产建议后端走 HTTPS/WSS，App 的 `API_BASE`/`WS_URL` 相应改为 `https://`/`wss://`。

## 四、联调顺序

1. `docker compose up -d` 起后端，浏览器开 `http://服务器:8000/docs` 确认可用。
2. 固件烧录后看串口日志：4G 注册 → GPRS → MQTT 连接成功 → 周期上报。
3. EMQX Dashboard 看到设备在线、有 `gh/<id>/telemetry` 流量。
4. App 打开「总览」看到实时数据；「控制」下发指令，串口/EMQX 看到 `gh/<id>/cmd`，电机动作。
5. 在「策略」改阈值，制造高温/弱光验证自动控制。
