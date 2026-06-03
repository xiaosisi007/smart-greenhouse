from __future__ import annotations

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    """运行配置，全部可通过环境变量覆盖。"""

    model_config = SettingsConfigDict(env_file=".env", env_prefix="GH_", extra="ignore")

    # 数据库 (TimescaleDB / PostgreSQL)
    database_url: str = "postgresql://greenhouse:greenhouse@localhost:5432/greenhouse"

    # MQTT (EMQX)
    mqtt_host: str = "localhost"
    mqtt_port: int = 1883
    mqtt_username: str | None = None
    mqtt_password: str | None = None
    mqtt_client_id: str = "greenhouse-backend"

    # MQTT 主题前缀: gh/{device_id}/...
    topic_prefix: str = "gh"

    # 是否启用 MQTT 桥接 (测试时可关闭)
    enable_mqtt: bool = True

    # 规则引擎评估间隔 (秒)
    rules_interval_s: float = 10.0

    # API
    api_host: str = "0.0.0.0"
    api_port: int = 8000


settings = Settings()
