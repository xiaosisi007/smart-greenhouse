from __future__ import annotations

from fastapi import APIRouter

from .. import db
from ..models import AutomationRule

router = APIRouter(prefix="/api/devices", tags=["rules"])


@router.get("/{device_id}/rule", response_model=AutomationRule)
async def get_rule(device_id: str) -> AutomationRule:
    return await db.get_rule(device_id)


@router.put("/{device_id}/rule", response_model=AutomationRule)
async def put_rule(device_id: str, rule: AutomationRule) -> AutomationRule:
    rule.device_id = device_id
    await db.upsert_rule(rule)
    return rule
