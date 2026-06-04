from __future__ import annotations

from datetime import datetime, timezone
from enum import Enum
from typing import Literal

from pydantic import BaseModel, Field


# ----------------------------- 执行器 / 指令 -----------------------------

class Actuator(str, Enum):
    """可控执行器。"""

    CURTAIN = "curtain"  # 卷帘棉被
    VENT = "vent"        # 顶部通风口


class Action(str, Enum):
    """执行动作。curtain: up=卷起/采光, down=放下/保温; vent: open=开风口, close=关风口。"""

    UP = "up"
    DOWN = "down"
    OPEN = "open"
    CLOSE = "close"
    STOP = "stop"


# 每个执行器允许的动作 (用于服务端校验，避免给卷帘下发 open 这种无意义指令)
ALLOWED_ACTIONS: dict[Actuator, set[Action]] = {
    Actuator.CURTAIN: {Action.UP, Action.DOWN, Action.STOP},
    Actuator.VENT: {Action.OPEN, Action.CLOSE, Action.STOP},
}


class MotionState(str, Enum):
    """执行器运动状态。"""

    IDLE = "idle"
    MOVING_OPEN = "moving_open"   # 卷起 / 开
    MOVING_CLOSE = "moving_close"  # 放下 / 关
    FAULT = "fault"               # 过流/卡死等故障


class CommandRequest(BaseModel):
    actuator: Actuator
    action: Action
    source: Literal["manual", "auto", "schedule"] = "manual"

    def validate_action(self) -> bool:
        return self.action in ALLOWED_ACTIONS[self.actuator]


class Command(CommandRequest):
    device_id: str
    cmd_id: str
    ts: datetime


# ----------------------------- 遥测 -----------------------------

class LimitSwitches(BaseModel):
    """限位开关到位状态 (True=已触发到位)。"""

    curtain_top: bool = False     # 卷帘到顶 (完全卷起)
    curtain_bottom: bool = False  # 卷帘到底 (完全放下)
    vent_open: bool = False       # 风口全开
    vent_closed: bool = False     # 风口全关


class Telemetry(BaseModel):
    """设备上报的一帧遥测数据。"""

    device_id: str
    ts: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))
    temperature: float | None = None  # 空气温度 ℃
    humidity: float | None = None     # 相对湿度 %
    lux: float | None = None          # 光照度 lx
    curtain_current: float | None = None  # 卷帘电机电流 A
    vent_current: float | None = None     # 风口电机电流 A
    curtain_state: MotionState = MotionState.IDLE
    vent_state: MotionState = MotionState.IDLE
    limits: LimitSwitches = Field(default_factory=LimitSwitches)


# ----------------------------- 设备 -----------------------------

class DeviceStatus(str, Enum):
    ONLINE = "online"
    OFFLINE = "offline"


class Device(BaseModel):
    device_id: str
    name: str
    status: DeviceStatus = DeviceStatus.OFFLINE
    last_seen: datetime | None = None
    last_telemetry: Telemetry | None = None


# ----------------------------- 自动规则 -----------------------------

class AutomationRule(BaseModel):
    """单棚自动控制策略。所有阈值均可在 App 修改。"""

    device_id: str
    enabled: bool = True

    # 通风: 温度上限 → 开风口; 回落到 (上限 - 回差) → 关
    vent_temp_high: float = 30.0
    vent_temp_hysteresis: float = 3.0
    # 排湿: 湿度高于该值也开风口
    vent_humidity_high: float = 90.0

    # 卷帘: 光照高于阈值卷起采光, 低于阈值放下保温
    curtain_lux_open: float = 8000.0
    curtain_lux_close: float = 2000.0
    # 低温保护: 棚温低于该值强制放下棉被保温 (优先于光照)
    curtain_temp_protect: float = 8.0


class Alarm(BaseModel):
    device_id: str
    ts: datetime
    level: Literal["info", "warning", "critical"]
    code: str
    message: str
