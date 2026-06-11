-- TimescaleDB 初始化脚本 (容器首次启动自动执行)
CREATE EXTENSION IF NOT EXISTS timescaledb;

CREATE TABLE IF NOT EXISTS devices (
    device_id   TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    status      TEXT NOT NULL DEFAULT 'offline',
    last_seen   TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS telemetry (
    device_id        TEXT NOT NULL,
    ts               TIMESTAMPTZ NOT NULL,
    temperature      DOUBLE PRECISION,
    humidity         DOUBLE PRECISION,
    lux              DOUBLE PRECISION,
    curtain_current  DOUBLE PRECISION,
    vent_current     DOUBLE PRECISION,
    curtain_state    TEXT,
    vent_state       TEXT,
    curtain_position DOUBLE PRECISION,
    vent_position    DOUBLE PRECISION,
    curtain_fault_reason TEXT,
    vent_fault_reason    TEXT,
    limits           JSONB
);
-- 已有库升级 (init.sql 仅首次建库执行, 老库手动跑一次):
ALTER TABLE telemetry ADD COLUMN IF NOT EXISTS curtain_position DOUBLE PRECISION;
ALTER TABLE telemetry ADD COLUMN IF NOT EXISTS vent_position DOUBLE PRECISION;
ALTER TABLE telemetry ADD COLUMN IF NOT EXISTS curtain_fault_reason TEXT;
ALTER TABLE telemetry ADD COLUMN IF NOT EXISTS vent_fault_reason TEXT;
SELECT create_hypertable('telemetry', 'ts', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_telemetry_device_ts ON telemetry (device_id, ts DESC);

CREATE TABLE IF NOT EXISTS commands (
    cmd_id     TEXT PRIMARY KEY,
    device_id  TEXT NOT NULL,
    ts         TIMESTAMPTZ NOT NULL,
    actuator   TEXT NOT NULL,
    action     TEXT NOT NULL,
    source     TEXT NOT NULL,
    acked      BOOLEAN NOT NULL DEFAULT FALSE
);
CREATE INDEX IF NOT EXISTS idx_commands_device_ts ON commands (device_id, ts DESC);

CREATE TABLE IF NOT EXISTS alarms (
    id         BIGSERIAL,
    device_id  TEXT NOT NULL,
    ts         TIMESTAMPTZ NOT NULL,
    level      TEXT NOT NULL,
    code       TEXT NOT NULL,
    message    TEXT NOT NULL,
    PRIMARY KEY (id, ts)
);
SELECT create_hypertable('alarms', 'ts', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_alarms_device_ts ON alarms (device_id, ts DESC);

CREATE TABLE IF NOT EXISTS rules (
    device_id  TEXT PRIMARY KEY,
    config     JSONB NOT NULL
);
