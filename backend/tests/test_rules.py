from __future__ import annotations

from app.models import (
    Action,
    Actuator,
    AutomationRule,
    LimitSwitches,
    MotionState,
    Telemetry,
)
from app.rules import evaluate


def _rule(**kw) -> AutomationRule:
    return AutomationRule(device_id="gh1", **kw)


def _has(cmds, actuator: Actuator, action: Action) -> bool:
    return any(c.actuator is actuator and c.action is action for c in cmds)


def test_open_vent_when_too_hot():
    t = Telemetry(device_id="gh1", temperature=35.0, humidity=50.0, lux=5000)
    cmds, _ = evaluate(t, _rule(vent_temp_high=30.0))
    assert _has(cmds, Actuator.VENT, Action.OPEN)


def test_close_vent_when_cooled_below_hysteresis():
    t = Telemetry(device_id="gh1", temperature=25.0, humidity=50.0, lux=5000)
    cmds, _ = evaluate(t, _rule(vent_temp_high=30.0, vent_temp_hysteresis=3.0))
    assert _has(cmds, Actuator.VENT, Action.CLOSE)


def test_open_vent_when_too_humid_even_if_cool():
    t = Telemetry(device_id="gh1", temperature=20.0, humidity=95.0, lux=5000)
    cmds, _ = evaluate(t, _rule(vent_temp_high=30.0, vent_humidity_high=90.0))
    assert _has(cmds, Actuator.VENT, Action.OPEN)


def test_no_vent_command_in_deadband():
    # 介于 (上限-回差) 与 上限 之间, 不动作
    t = Telemetry(device_id="gh1", temperature=28.0, humidity=50.0, lux=5000)
    cmds, _ = evaluate(t, _rule(vent_temp_high=30.0, vent_temp_hysteresis=3.0))
    assert not any(c.actuator is Actuator.VENT for c in cmds)


def test_curtain_low_temp_protection_overrides_light():
    # 强光但低温 -> 应放下保温, 而非卷起采光
    t = Telemetry(device_id="gh1", temperature=5.0, lux=20000)
    cmds, _ = evaluate(t, _rule(curtain_temp_protect=8.0, curtain_lux_open=8000))
    assert _has(cmds, Actuator.CURTAIN, Action.DOWN)
    assert not _has(cmds, Actuator.CURTAIN, Action.UP)


def test_curtain_open_in_bright_light():
    t = Telemetry(device_id="gh1", temperature=20.0, lux=20000)
    cmds, _ = evaluate(t, _rule(curtain_lux_open=8000))
    assert _has(cmds, Actuator.CURTAIN, Action.UP)


def test_curtain_close_in_dark():
    t = Telemetry(device_id="gh1", temperature=20.0, lux=500)
    cmds, _ = evaluate(t, _rule(curtain_lux_close=2000))
    assert _has(cmds, Actuator.CURTAIN, Action.DOWN)


def test_skip_command_when_at_limit():
    t = Telemetry(
        device_id="gh1", temperature=35.0, lux=5000,
        limits=LimitSwitches(vent_open=True),
    )
    cmds, _ = evaluate(t, _rule(vent_temp_high=30.0))
    assert not _has(cmds, Actuator.VENT, Action.OPEN)


def test_skip_command_when_already_moving():
    t = Telemetry(
        device_id="gh1", temperature=35.0, lux=5000,
        vent_state=MotionState.MOVING_OPEN,
    )
    cmds, _ = evaluate(t, _rule(vent_temp_high=30.0))
    assert not _has(cmds, Actuator.VENT, Action.OPEN)


def test_fault_raises_critical_alarm():
    t = Telemetry(device_id="gh1", curtain_state=MotionState.FAULT)
    _, alarms = evaluate(t, _rule())
    assert any(a.code == "curtain_fault" and a.level == "critical" for a in alarms)


def test_disabled_rule_emits_no_commands_but_still_alarms():
    t = Telemetry(device_id="gh1", temperature=45.0, lux=20000)
    cmds, alarms = evaluate(t, _rule(enabled=False, vent_temp_high=30.0))
    assert cmds == []
    assert any(a.code == "over_temp" for a in alarms)
