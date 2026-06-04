from __future__ import annotations

from fastapi import APIRouter, HTTPException

from .. import db
from ..models import Device

router = APIRouter(prefix="/api/devices", tags=["devices"])


@router.get("", response_model=list[Device])
async def list_devices() -> list[Device]:
    return await db.list_devices()


@router.get("/{device_id}", response_model=Device)
async def get_device(device_id: str) -> Device:
    devices = await db.list_devices()
    for d in devices:
        if d.device_id == device_id:
            return d
    raise HTTPException(status_code=404, detail="设备不存在")
