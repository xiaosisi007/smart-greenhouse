from __future__ import annotations

"""MQTT 桥接 (EMQX <-> 后端)。

订阅:
  gh/{device_id}/telemetry  设备遥测上报
  gh/{device_id}/status     设备上下线 (LWT)
  gh/{device_id}/cmd/ack    指令回执

下发:
  gh/{device_id}/cmd        控制指令
"""

import asyncio
import json
import logging
import uuid
from datetime import datetime, timezone

import aiomqtt

from . import db
from .config import settings
from .models import (
    Command,
    CommandRequest,
    DeviceStatus,
    Telemetry,
)
from .rules import evaluate
from .ws import ws_manager

log = logging.getLogger("mqtt")


class MqttBridge:
    def __init__(self) -> None:
        self._client: aiomqtt.Client | None = None
        self._task: asyncio.Task | None = None
        self._stop = asyncio.Event()

    def _topic(self, device_id: str, suffix: str) -> str:
        return f"{settings.topic_prefix}/{device_id}/{suffix}"

    async def start(self) -> None:
        self._stop.clear()
        self._task = asyncio.create_task(self._run())

    async def stop(self) -> None:
        self._stop.set()
        if self._task:
            self._task.cancel()
            try:
                await self._task
            except asyncio.CancelledError:
                pass

    async def _run(self) -> None:
        """带自动重连的主循环。"""
        while not self._stop.is_set():
            try:
                async with aiomqtt.Client(
                    hostname=settings.mqtt_host,
                    port=settings.mqtt_port,
                    username=settings.mqtt_username,
                    password=settings.mqtt_password,
                    identifier=settings.mqtt_client_id,
                ) as client:
                    self._client = client
                    await client.subscribe(f"{settings.topic_prefix}/+/telemetry")
                    await client.subscribe(f"{settings.topic_prefix}/+/status")
                    await client.subscribe(f"{settings.topic_prefix}/+/cmd/ack")
                    log.info("MQTT connected to %s:%s", settings.mqtt_host, settings.mqtt_port)
                    async for message in client.messages:
                        await self._handle(message)
            except asyncio.CancelledError:
                raise
            except Exception as exc:  # noqa: BLE001
                log.warning("MQTT 连接断开, 5s 后重连: %s", exc)
                self._client = None
                await asyncio.sleep(5)

    async def _handle(self, message: aiomqtt.Message) -> None:
        topic = str(message.topic)
        parts = topic.split("/")
        if len(parts) < 3:
            return
        device_id = parts[1]
        kind = "/".join(parts[2:])
        try:
            payload = json.loads(message.payload.decode())
        except (ValueError, UnicodeDecodeError):
            payload = {}

        if kind == "telemetry":
            await self._on_telemetry(device_id, payload)
        elif kind == "status":
            await self._on_status(device_id, payload)
        elif kind == "cmd/ack":
            cmd_id = payload.get("cmd_id")
            if cmd_id:
                await db.ack_command(cmd_id)

    async def _on_status(self, device_id: str, payload: dict) -> None:
        status = DeviceStatus(payload.get("status", "online"))
        await db.set_device_status(device_id, status, datetime.now(timezone.utc))
        await ws_manager.broadcast("status", {"device_id": device_id, "status": status.value})

    async def _on_telemetry(self, device_id: str, payload: dict) -> None:
        payload["device_id"] = device_id
        payload.setdefault("ts", datetime.now(timezone.utc).isoformat())
        try:
            t = Telemetry(**payload)
        except Exception as exc:  # noqa: BLE001
            log.warning("遥测解析失败 %s: %s", device_id, exc)
            return

        await db.upsert_device(device_id)
        await db.set_device_status(device_id, DeviceStatus.ONLINE, t.ts)
        await db.insert_telemetry(t)
        await ws_manager.broadcast("telemetry", t.model_dump())

        # 自动规则评估
        rule = await db.get_rule(device_id)
        cmds, alarms = evaluate(t, rule)
        for a in alarms:
            await db.insert_alarm(a)
            await ws_manager.broadcast("alarm", a.model_dump())
        for c in cmds:
            await self.send_command(device_id, c)

    async def send_command(self, device_id: str, req: CommandRequest) -> Command:
        """下发一条控制指令 (供规则引擎和 REST API 共用)。"""
        cmd = Command(
            device_id=device_id,
            cmd_id=str(uuid.uuid4()),
            ts=datetime.now(timezone.utc),
            **req.model_dump(),
        )
        await db.insert_command(cmd)
        payload = json.dumps({
            "cmd_id": cmd.cmd_id,
            "actuator": cmd.actuator.value,
            "action": cmd.action.value,
            "source": cmd.source,
        })
        if self._client is not None:
            await self._client.publish(self._topic(device_id, "cmd"), payload, qos=1)
        else:
            log.warning("MQTT 未连接, 指令仅入库未下发: %s", cmd.cmd_id)
        await ws_manager.broadcast("command", cmd.model_dump())
        return cmd


bridge = MqttBridge()
