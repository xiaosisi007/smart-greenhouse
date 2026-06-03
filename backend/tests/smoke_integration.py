"""端到端冒烟测试: 需先 docker compose up。

模拟设备经 MQTT 上报遥测 -> 验证后端入库/REST/自动规则下发指令; 再测手动下发。
"""
import asyncio
import json

import aiomqtt
import httpx

API = "http://localhost:8000"
MQTT_HOST = "localhost"
DEV = "gh1"


async def main() -> None:
    captured_cmds: list[dict] = []

    async with aiomqtt.Client(hostname=MQTT_HOST, port=1883, identifier="smoke-sub") as sub:
        await sub.subscribe(f"gh/{DEV}/cmd")

        async def listen():
            async for m in sub.messages:
                captured_cmds.append(json.loads(m.payload.decode()))

        task = asyncio.create_task(listen())

        # 1) 设备上线 + 上报"高温"遥测 (应触发自动开风口)
        async with aiomqtt.Client(hostname=MQTT_HOST, port=1883, identifier="smoke-dev") as dev:
            await dev.publish(f"gh/{DEV}/status", json.dumps({"status": "online"}), retain=True)
            await dev.publish(f"gh/{DEV}/telemetry", json.dumps({
                "temperature": 35.0, "humidity": 60.0, "lux": 5000,
                "curtain_current": 0.0, "vent_current": 0.0,
                "curtain_state": "idle", "vent_state": "idle",
                "limits": {"curtain_top": False, "curtain_bottom": False,
                           "vent_open": False, "vent_closed": False},
            }))
            await asyncio.sleep(2.0)

        assert any(c["actuator"] == "vent" and c["action"] == "open" for c in captured_cmds), \
            f"未收到自动开风口指令: {captured_cmds}"
        print("[OK] 高温遥测触发自动开风口:", captured_cmds)

        # 2) REST: 设备列表 / 最新遥测 / 告警
        async with httpx.AsyncClient(base_url=API) as http:
            devs = (await http.get("/api/devices")).json()
            assert any(d["device_id"] == DEV for d in devs), devs
            latest = (await http.get(f"/api/devices/{DEV}/telemetry/latest")).json()
            assert latest["temperature"] == 35.0, latest
            print("[OK] REST 最新遥测:", latest["temperature"], "℃", latest["humidity"], "%")

            # 3) 手动下发: 卷帘卷起 -> 应在 MQTT 收到
            captured_cmds.clear()
            r = await http.post(f"/api/devices/{DEV}/command",
                                 json={"actuator": "curtain", "action": "up", "source": "manual"})
            assert r.status_code == 200, r.text
            await asyncio.sleep(1.5)
            assert any(c["actuator"] == "curtain" and c["action"] == "up" for c in captured_cmds), \
                f"未收到手动卷帘指令: {captured_cmds}"
            print("[OK] 手动下发卷帘卷起经 MQTT 送达:", captured_cmds)

            # 4) 非法动作应被拒绝
            r = await http.post(f"/api/devices/{DEV}/command",
                                 json={"actuator": "curtain", "action": "open"})
            assert r.status_code == 400, r.text
            print("[OK] 非法动作 curtain/open 被拒绝 (400)")

            # 5) 历史
            hist = (await http.get(f"/api/devices/{DEV}/telemetry/history?hours=1")).json()
            assert len(hist) >= 1, hist
            print(f"[OK] 历史记录条数: {len(hist)}")

        task.cancel()
    print("\n全部冒烟测试通过 ✔")


if __name__ == "__main__":
    asyncio.run(main())
