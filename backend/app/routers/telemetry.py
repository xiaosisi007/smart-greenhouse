from __future__ import annotations

from fastapi import APIRouter, HTTPException, Query

from .. import db
from ..models import Telemetry

router = APIRouter(prefix="/api/devices", tags=["telemetry"])


@router.get("/{device_id}/telemetry/latest", response_model=Telemetry)
async def latest(device_id: str) -> Telemetry:
    t = await db.latest_telemetry(device_id)
    if t is None:
        raise HTTPException(status_code=404, detail="暂无遥测数据")
    return t


@router.get("/{device_id}/telemetry/history", response_model=list[Telemetry])
async def history(
    device_id: str,
    hours: int = Query(24, ge=1, le=720),
    limit: int = Query(2000, ge=1, le=10000),
) -> list[Telemetry]:
    return await db.telemetry_history(device_id, hours=hours, limit=limit)
