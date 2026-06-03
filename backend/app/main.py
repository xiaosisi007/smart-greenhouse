from __future__ import annotations

import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from . import db
from .config import settings
from .mqtt import bridge
from .routers import alarms, control, devices, rules, telemetry
from .ws import ws_manager

logging.basicConfig(level=logging.INFO)


@asynccontextmanager
async def lifespan(app: FastAPI):
    await db.connect()
    if settings.enable_mqtt:
        await bridge.start()
    yield
    if settings.enable_mqtt:
        await bridge.stop()
    await db.disconnect()


app = FastAPI(title="智能大棚后端", version="0.1.0", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(devices.router)
app.include_router(telemetry.router)
app.include_router(control.router)
app.include_router(rules.router)
app.include_router(alarms.router)


@app.get("/health", tags=["meta"])
async def health() -> dict[str, str]:
    return {"status": "ok"}


@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket) -> None:
    """App 实时订阅: 推送 telemetry / status / alarm / command 事件。"""
    await ws_manager.connect(ws)
    try:
        while True:
            await ws.receive_text()  # 仅保活, 忽略客户端内容
    except WebSocketDisconnect:
        await ws_manager.disconnect(ws)
