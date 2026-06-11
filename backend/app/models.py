from __future__ import annotations

from datetime import datetime, timezone
from enum import Enum
from typing import Literal

from pydantic import BaseModel, Field, model_validator


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


class FaultReason(str, Enum):
    """运动故障原因 (固件上报)。"""

    OVERCURRENT = "overcurrent"    # 过流: 卡死/过载
    STALL = "stall"                # 失速: 通电但无脉冲, 缠绕/卡死
    TIMEOUT = "timeout"            # 行程超时: 超时未到限位
    UNDERCURRENT = "undercurrent"  # 欠流: 绳断/脱落/电机空转


FAULT_REASON_TEXT: dict[FaultReason, str] = {
    FaultReason.OVERCURRENT: "过流(卡死/过载)",
    FaultReason.STALL: "失速(缠绕/卡死, 电机通电不转)",
    FaultReason.TIMEOUT: "行程超时(超时未到限位)",
    FaultReason.UNDERCURRENT: "欠流(绳断/脱落/空转)",
}


class CommandRequest(BaseModel):
    actuator: Actuator
    action: Action
    motor_id: int | None = None  # 多电机: 目标电机编号; None=该类型第一台 (兼容旧协议)
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


class MotorTelemetry(BaseModel):
    """单台电机的遥测 (多电机固件 motors 数组中的一项)。"""

    id: int
    type: Actuator
    name: str | None = None
    state: MotionState = MotionState.IDLE
    current: float | None = None       # 电机电流 A
    position: float | None = None      # 开度 % (0=全关/放下, 100=全开/卷起)
    fault_reason: FaultReason | None = None
    limit_open: bool = False           # 开/卷起方向限位到位
    limit_close: bool = False          # 关/放下方向限位到位


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
    curtain_position: float | None = None  # 卷帘开度 % (0=全放下, 100=全卷起)
    vent_position: float | None = None     # 风口开度 %
    curtain_fault_reason: FaultReason | None = None
    vent_fault_reason: FaultReason | None = None
    limits: LimitSwitches = Field(default_factory=LimitSwitches)
    motors: list[MotorTelemetry] = Field(default_factory=list)

    @model_validator(mode="after")
    def _sync_motors(self) -> "Telemetry":
        """motors 与旧版 curtain_*/vent_* 字段双向同步。

        - 旧固件 (无 motors): 由 curtain/vent 字段合成 motors 数组;
        - 新固件 (有 motors): 用首台卷帘/风口回填旧字段, 兼容历史查询与旧 App。
        """
        if not self.motors:
            self.motors = [
                MotorTelemetry(
                    id=0, type=Actuator.CURTAIN, state=self.curtain_state,
                    current=self.curtain_current, position=self.curtain_position,
                    fault_reason=self.curtain_fault_reason,
                    limit_open=self.limits.curtain_top, limit_close=self.limits.curtain_bottom,
                ),
                MotorTelemetry(
                    id=1, type=Actuator.VENT, state=self.vent_state,
                    current=self.vent_current, position=self.vent_position,
                    fault_reason=self.vent_fault_reason,
                    limit_open=self.limits.vent_open, limit_close=self.limits.vent_closed,
                ),
            ]
            return self
        first_curtain = next((m for m in self.motors if m.type is Actuator.CURTAIN), None)
        first_vent = next((m for m in self.motors if m.type is Actuator.VENT), None)
        if first_curtain is not None:
            self.curtain_state = first_curtain.state
            self.curtain_current = first_curtain.current
            self.curtain_position = first_curtain.position
            self.curtain_fault_reason = first_curtain.fault_reason
            self.limits.curtain_top = first_curtain.limit_open
            self.limits.curtain_bottom = first_curtain.limit_close
        if first_vent is not None:
            self.vent_state = first_vent.state
            self.vent_current = first_vent.current
            self.vent_position = first_vent.position
            self.vent_fault_reason = first_vent.fault_reason
            self.limits.vent_open = first_vent.limit_open
            self.limits.vent_closed = first_vent.limit_close
        return self


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
