from __future__ import annotations

from fastapi import APIRouter, Query

from .. import db
from ..models import Alarm

router = APIRouter(prefix="/api/devices", tags=["alarms"])


@router.get("/{device_id}/alarms", response_model=list[Alarm])
async def list_alarms(device_id: str, limit: int = Query(100, ge=1, le=1000)) -> list[Alarm]:
    return await db.list_alarms(device_id, limit=limit)
