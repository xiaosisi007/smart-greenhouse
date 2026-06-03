from __future__ import annotations

from fastapi import APIRouter, HTTPException

from ..models import Command, CommandRequest
from ..mqtt import bridge

router = APIRouter(prefix="/api/devices", tags=["control"])


@router.post("/{device_id}/command", response_model=Command)
async def send_command(device_id: str, req: CommandRequest) -> Command:
    """手动下发控制指令 (卷帘 up/down/stop, 风口 open/close/stop)。"""
    if not req.validate_action():
        raise HTTPException(
            status_code=400,
            detail=f"执行器 {req.actuator.value} 不支持动作 {req.action.value}",
        )
    return await bridge.send_command(device_id, req)
