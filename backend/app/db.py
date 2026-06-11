from __future__ import annotations

"""TimescaleDB 访问层 (asyncpg)。"""

import json
from datetime import datetime

import asyncpg

from .config import settings
from .models import (
    Alarm,
    AutomationRule,
    Command,
    Device,
    DeviceStatus,
    FaultReason,
    LimitSwitches,
    MotionState,
    Telemetry,
)

_pool: asyncpg.Pool | None = None


async def connect() -> None:
    global _pool
    if _pool is None:
        _pool = await asyncpg.create_pool(settings.database_url, min_size=1, max_size=10)


async def disconnect() -> None:
    global _pool
    if _pool is not None:
        await _pool.close()
        _pool = None


def _require_pool() -> asyncpg.Pool:
    if _pool is None:
        raise RuntimeError("数据库连接池未初始化, 请先调用 connect()")
    return _pool


# ----------------------------- 设备 -----------------------------

async def upsert_device(device_id: str, name: str | None = None) -> None:
    pool = _require_pool()
    await pool.execute(
        """
        INSERT INTO devices (device_id, name) VALUES ($1, $2)
        ON CONFLICT (device_id) DO UPDATE SET name = COALESCE($2, devices.name)
        """,
        device_id,
        name or device_id,
    )


async def set_device_status(device_id: str, status: DeviceStatus, ts: datetime) -> None:
    pool = _require_pool()
    await pool.execute(
        """
        INSERT INTO devices (device_id, name, status, last_seen) VALUES ($1, $1, $2, $3)
        ON CONFLICT (device_id) DO UPDATE SET status = $2, last_seen = $3
        """,
        device_id,
        status.value,
        ts,
    )


async def list_devices() -> list[Device]:
    pool = _require_pool()
    rows = await pool.fetch("SELECT device_id, name, status, last_seen FROM devices ORDER BY device_id")
    out: list[Device] = []
    for r in rows:
        out.append(
            Device(
                device_id=r["device_id"],
                name=r["name"],
                status=DeviceStatus(r["status"]),
                last_seen=r["last_seen"],
                last_telemetry=await latest_telemetry(r["device_id"]),
            )
        )
    return out


# ----------------------------- 遥测 -----------------------------

async def insert_telemetry(t: Telemetry) -> None:
    pool = _require_pool()
    await pool.execute(
        """
        INSERT INTO telemetry
            (device_id, ts, temperature, humidity, lux, curtain_current,
             vent_current, curtain_state, vent_state, curtain_position,
             vent_position, curtain_fault_reason, vent_fault_reason, limits)
        VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14)
        """,
        t.device_id, t.ts, t.temperature, t.humidity, t.lux,
        t.curtain_current, t.vent_current, t.curtain_state.value,
        t.vent_state.value, t.curtain_position, t.vent_position,
        t.curtain_fault_reason.value if t.curtain_fault_reason else None,
        t.vent_fault_reason.value if t.vent_fault_reason else None,
        json.dumps(t.limits.model_dump()),
    )


def _row_to_telemetry(r: asyncpg.Record) -> Telemetry:
    limits = r["limits"]
    if isinstance(limits, str):
        limits = json.loads(limits)
    return Telemetry(
        device_id=r["device_id"], ts=r["ts"], temperature=r["temperature"],
        humidity=r["humidity"], lux=r["lux"], curtain_current=r["curtain_current"],
        vent_current=r["vent_current"],
        curtain_state=MotionState(r["curtain_state"]) if r["curtain_state"] else MotionState.IDLE,
        vent_state=MotionState(r["vent_state"]) if r["vent_state"] else MotionState.IDLE,
        curtain_position=r["curtain_position"],
        vent_position=r["vent_position"],
        curtain_fault_reason=FaultReason(r["curtain_fault_reason"]) if r["curtain_fault_reason"] else None,
        vent_fault_reason=FaultReason(r["vent_fault_reason"]) if r["vent_fault_reason"] else None,
        limits=LimitSwitches(**(limits or {})),
    )


async def latest_telemetry(device_id: str) -> Telemetry | None:
    pool = _require_pool()
    r = await pool.fetchrow(
        "SELECT * FROM telemetry WHERE device_id=$1 ORDER BY ts DESC LIMIT 1", device_id
    )
    return _row_to_telemetry(r) if r else None


async def telemetry_history(device_id: str, hours: int = 24, limit: int = 2000) -> list[Telemetry]:
    pool = _require_pool()
    rows = await pool.fetch(
        """
        SELECT * FROM telemetry
        WHERE device_id=$1 AND ts > now() - ($2 || ' hours')::interval
        ORDER BY ts DESC LIMIT $3
        """,
        device_id, str(hours), limit,
    )
    return [_row_to_telemetry(r) for r in rows]


# ----------------------------- 指令 -----------------------------

async def insert_command(c: Command) -> None:
    pool = _require_pool()
    await pool.execute(
        """
        INSERT INTO commands (cmd_id, device_id, ts, actuator, action, source)
        VALUES ($1,$2,$3,$4,$5,$6)
        """,
        c.cmd_id, c.device_id, c.ts, c.actuator.value, c.action.value, c.source,
    )


async def ack_command(cmd_id: str) -> None:
    pool = _require_pool()
    await pool.execute("UPDATE commands SET acked=TRUE WHERE cmd_id=$1", cmd_id)


# ----------------------------- 告警 -----------------------------

async def insert_alarm(a: Alarm) -> None:
    pool = _require_pool()
    await pool.execute(
        "INSERT INTO alarms (device_id, ts, level, code, message) VALUES ($1,$2,$3,$4,$5)",
        a.device_id, a.ts, a.level, a.code, a.message,
    )


async def list_alarms(device_id: str, limit: int = 100) -> list[Alarm]:
    pool = _require_pool()
    rows = await pool.fetch(
        "SELECT device_id, ts, level, code, message FROM alarms WHERE device_id=$1 ORDER BY ts DESC LIMIT $2",
        device_id, limit,
    )
    return [Alarm(**dict(r)) for r in rows]


# ----------------------------- 规则 -----------------------------

async def get_rule(device_id: str) -> AutomationRule:
    pool = _require_pool()
    r = await pool.fetchrow("SELECT config FROM rules WHERE device_id=$1", device_id)
    if r is None:
        return AutomationRule(device_id=device_id)
    config = r["config"]
    if isinstance(config, str):
        config = json.loads(config)
    return AutomationRule(**config)


async def upsert_rule(rule: AutomationRule) -> None:
    pool = _require_pool()
    await pool.execute(
        """
        INSERT INTO rules (device_id, config) VALUES ($1, $2)
        ON CONFLICT (device_id) DO UPDATE SET config = $2
        """,
        rule.device_id, json.dumps(rule.model_dump()),
    )


async def all_rules() -> list[AutomationRule]:
    pool = _require_pool()
    rows = await pool.fetch("SELECT config FROM rules")
    out = []
    for r in rows:
        config = r["config"]
        if isinstance(config, str):
            config = json.loads(config)
        out.append(AutomationRule(**config))
    return out
