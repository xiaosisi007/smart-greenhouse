# 智能农业大棚一体化系统

卷帘棉被升降 + 顶部通风 + 温湿度/光照监测，一体化「硬件 → 后端 → App」方案。

- **现场网络**：4G（SIM7600）
- **App**：Flutter（安卓）
- **后端**：自研 EMQX(MQTT) + FastAPI + TimescaleDB

```
传感器/执行器 → ESP32控制器 → 4G → EMQX(MQTT) → FastAPI后端 → TimescaleDB
                                                   ↓ REST / WebSocket
                                              Flutter 安卓 App
```

## 目录结构

| 目录 | 说明 |
|---|---|
| `backend/` | FastAPI 后端：MQTT 桥接、REST/WebSocket、自动规则引擎、告警 |
| `firmware/` | ESP32 + SIM7600 固件（PlatformIO）：采集、控制、限位/过流保护、断网兜底 |
| `app/` | Flutter 安卓 App：总览 / 控制 / 曲线 / 告警 / 策略 |
| `docs/` | 接线图说明、部署文档 |
| `docker-compose.yml` | 一键起 EMQX + TimescaleDB + 后端 |

## 快速开始（后端）

```bash
docker compose up -d --build
# 后端 API:        http://localhost:8000  (文档 /docs)
# EMQX 控制台:     http://localhost:18083 (默认 admin/public)
# TimescaleDB:     localhost:5432 (greenhouse/greenhouse)
```

冒烟测试（模拟设备经 MQTT 上报并验证自动控制/REST/手动下发）：

```bash
cd backend && python -m venv .venv && . .venv/bin/activate
pip install -e ".[dev]"
pytest                       # 规则引擎单元测试
python tests/smoke_integration.py   # 端到端 (需 docker compose 已启动)
```

## MQTT 主题约定

| 方向 | 主题 | 载荷 |
|---|---|---|
| 设备→云 | `gh/{device_id}/telemetry` | 温湿度/光照/电流/状态/限位 JSON |
| 设备→云 | `gh/{device_id}/status` | `{"status":"online\|offline"}`（LWT 离线） |
| 设备→云 | `gh/{device_id}/cmd/ack` | `{"cmd_id":"..."}` 指令回执 |
| 云→设备 | `gh/{device_id}/cmd` | `{"actuator":"curtain\|vent","action":"up\|down\|open\|close\|stop"}` |

## REST / WebSocket 接口

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/devices` | 设备列表（含在线状态/最新遥测） |
| GET | `/api/devices/{id}/telemetry/latest` | 最新一帧遥测 |
| GET | `/api/devices/{id}/telemetry/history?hours=24` | 历史曲线数据 |
| POST | `/api/devices/{id}/command` | 手动下发控制指令 |
| GET/PUT | `/api/devices/{id}/rule` | 读取/保存自动控制策略 |
| GET | `/api/devices/{id}/alarms` | 告警记录 |
| WS | `/ws` | 实时推送 telemetry/status/alarm/command |

## 自动控制策略（可在 App 调整）

- **通风**：棚温 > 上限 → 开风口；回落到 (上限−回差) → 关；湿度 > 上限 → 开风口排湿
- **卷帘**：棚温 < 低温保护值 → 放下保温（优先）；否则光照 > 阈值卷起采光，< 阈值放下保温
- **安全**：到限位停机、电机过流判故障停机并告警（后端 + 固件双层保护）

## 安全须知（现场强电）

固件与后端均做了软件层保护，但**硬件安全必须到位**：正反转接触器机械互锁、限位开关串入接触器回路、断路器+漏电保护+急停按钮、本地手动按钮（断网仍可操作）。详见 `docs/wiring.md`。

> 详细接线见 [docs/wiring.md](docs/wiring.md)，部署见 [docs/deploy.md](docs/deploy.md)。
