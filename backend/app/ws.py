from __future__ import annotations

"""WebSocket 广播管理: 把遥测/告警实时推送给所有连接的 App 客户端。"""

import asyncio
import json
from typing import Any

from fastapi import WebSocket


class WSManager:
    def __init__(self) -> None:
        self._clients: set[WebSocket] = set()
        self._lock = asyncio.Lock()

    async def connect(self, ws: WebSocket) -> None:
        await ws.accept()
        async with self._lock:
            self._clients.add(ws)

    async def disconnect(self, ws: WebSocket) -> None:
        async with self._lock:
            self._clients.discard(ws)

    async def broadcast(self, event: str, data: dict[str, Any]) -> None:
        msg = json.dumps({"event": event, "data": data}, default=str)
        async with self._lock:
            clients = list(self._clients)
        dead: list[WebSocket] = []
        for ws in clients:
            try:
                await ws.send_text(msg)
            except Exception:
                dead.append(ws)
        if dead:
            async with self._lock:
                for ws in dead:
                    self._clients.discard(ws)


ws_manager = WSManager()
