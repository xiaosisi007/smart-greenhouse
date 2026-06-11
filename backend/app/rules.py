from __future__ import annotations

"""自动控制规则引擎 (纯函数, 便于单元测试)。

给定一帧遥测 + 规则配置, 输出"期望执行的指令"和"告警"。
调用方负责对指令去重 (避免重复下发) 与实际 MQTT 下发。
"""

from .models import (
    FAULT_REASON_TEXT,
    Action,
    Actuator,
    Alarm,
    AutomationRule,
    CommandRequest,
    FaultReason,
    MotionState,
    MotorTelemetry,
    Telemetry,
)


def _fault_detail(reason: FaultReason | None) -> str:
    return FAULT_REASON_TEXT.get(reason, "过流/卡死") if reason else "过流/卡死"


def _motor_label(m: MotorTelemetry, t: Telemetry) -> str:
    """电机显示名: 自定义名 > 类型名(+编号, 仅同类型多台时)。"""
    if m.name:
        return m.name
    base = "卷帘电机" if m.type is Actuator.CURTAIN else "风口电机"
    same_type = [x for x in t.motors if x.type is m.type]
    return f"{base}#{m.id}" if len(same_type) > 1 else base


def _at_limit(m: MotorTelemetry, action: Action) -> bool:
    """目标方向是否已到限位 (到位则无需再动)。"""
    if action in (Action.UP, Action.OPEN):
        return m.limit_open
    if action in (Action.DOWN, Action.CLOSE):
        return m.limit_close
    return False


def _already_moving(state: MotionState, action: Action) -> bool:
    """执行器是否已在朝目标方向运动 (避免重复下发)。"""
    opening = {Action.UP, Action.OPEN}
    closing = {Action.DOWN, Action.CLOSE}
    if action in opening:
        return state is MotionState.MOVING_OPEN
    if action in closing:
        return state is MotionState.MOVING_CLOSE
    return False


def evaluate(
    t: Telemetry, rule: AutomationRule
) -> tuple[list[CommandRequest], list[Alarm]]:
    """评估一帧遥测, 返回 (期望指令列表, 告警列表)。"""
    cmds: list[CommandRequest] = []
    alarms: list[Alarm] = []

    # ---- 安全告警 (始终评估, 不受 enabled 影响): 逐台电机检查故障 ----
    for m in t.motors:
        if m.state is MotionState.FAULT:
            alarms.append(Alarm(
                device_id=t.device_id, ts=t.ts, level="critical",
                code=f"{m.type.value}_fault",
                message=f"{_motor_label(m, t)}故障: {_fault_detail(m.fault_reason)}, 已停机"))
    if t.temperature is not None and t.temperature >= rule.vent_temp_high + 8:
        alarms.append(Alarm(device_id=t.device_id, ts=t.ts, level="warning",
                            code="over_temp", message=f"棚温过高 {t.temperature:.1f}℃"))
    if t.temperature is not None and t.temperature <= rule.curtain_temp_protect - 3:
        alarms.append(Alarm(device_id=t.device_id, ts=t.ts, level="warning",
                            code="low_temp", message=f"棚温过低 {t.temperature:.1f}℃"))

    if not rule.enabled:
        return cmds, alarms

    # ---- 通风口控制 ----
    want_vent: Action | None = None
    if t.temperature is not None or t.humidity is not None:
        too_hot = t.temperature is not None and t.temperature > rule.vent_temp_high
        too_humid = t.humidity is not None and t.humidity > rule.vent_humidity_high
        cooled = (
            t.temperature is not None
            and t.temperature < rule.vent_temp_high - rule.vent_temp_hysteresis
        )
        if too_hot or too_humid:
            want_vent = Action.OPEN
        elif cooled and not too_humid:
            want_vent = Action.CLOSE

    if want_vent is not None:
        for m in t.motors:
            if m.type is Actuator.VENT and m.state is not MotionState.FAULT \
                    and not _at_limit(m, want_vent) and not _already_moving(m.state, want_vent):
                cmds.append(CommandRequest(actuator=Actuator.VENT, action=want_vent,
                                           motor_id=m.id, source="auto"))

    # ---- 卷帘棉被控制 (低温保护优先) ----
    want_curtain: Action | None = None
    if t.temperature is not None and t.temperature < rule.curtain_temp_protect:
        want_curtain = Action.DOWN  # 保温优先
    elif t.lux is not None:
        if t.lux > rule.curtain_lux_open:
            want_curtain = Action.UP    # 采光
        elif t.lux < rule.curtain_lux_close:
            want_curtain = Action.DOWN  # 保温

    if want_curtain is not None:
        for m in t.motors:
            if m.type is Actuator.CURTAIN and m.state is not MotionState.FAULT \
                    and not _at_limit(m, want_curtain) and not _already_moving(m.state, want_curtain):
                cmds.append(CommandRequest(actuator=Actuator.CURTAIN, action=want_curtain,
                                           motor_id=m.id, source="auto"))

    return cmds, alarms
