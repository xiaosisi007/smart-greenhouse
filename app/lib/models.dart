/// 数据模型, 与后端 pydantic 模型对应。

class LimitSwitches {
  final bool curtainTop, curtainBottom, ventOpen, ventClosed;
  LimitSwitches({
    this.curtainTop = false,
    this.curtainBottom = false,
    this.ventOpen = false,
    this.ventClosed = false,
  });
  factory LimitSwitches.fromJson(Map<String, dynamic> j) => LimitSwitches(
        curtainTop: j['curtain_top'] ?? false,
        curtainBottom: j['curtain_bottom'] ?? false,
        ventOpen: j['vent_open'] ?? false,
        ventClosed: j['vent_closed'] ?? false,
      );
}

class Telemetry {
  final String deviceId;
  final DateTime ts;
  final double? temperature, humidity, lux, curtainCurrent, ventCurrent;
  final String curtainState, ventState;
  final LimitSwitches limits;

  Telemetry({
    required this.deviceId,
    required this.ts,
    this.temperature,
    this.humidity,
    this.lux,
    this.curtainCurrent,
    this.ventCurrent,
    this.curtainState = 'idle',
    this.ventState = 'idle',
    required this.limits,
  });

  factory Telemetry.fromJson(Map<String, dynamic> j) => Telemetry(
        deviceId: j['device_id'],
        ts: DateTime.parse(j['ts']).toLocal(),
        temperature: (j['temperature'] as num?)?.toDouble(),
        humidity: (j['humidity'] as num?)?.toDouble(),
        lux: (j['lux'] as num?)?.toDouble(),
        curtainCurrent: (j['curtain_current'] as num?)?.toDouble(),
        ventCurrent: (j['vent_current'] as num?)?.toDouble(),
        curtainState: j['curtain_state'] ?? 'idle',
        ventState: j['vent_state'] ?? 'idle',
        limits: LimitSwitches.fromJson(j['limits'] ?? {}),
      );
}

class Device {
  final String deviceId, name, status;
  final DateTime? lastSeen;
  final Telemetry? lastTelemetry;
  Device({
    required this.deviceId,
    required this.name,
    required this.status,
    this.lastSeen,
    this.lastTelemetry,
  });
  factory Device.fromJson(Map<String, dynamic> j) => Device(
        deviceId: j['device_id'],
        name: j['name'],
        status: j['status'],
        lastSeen: j['last_seen'] != null ? DateTime.parse(j['last_seen']).toLocal() : null,
        lastTelemetry:
            j['last_telemetry'] != null ? Telemetry.fromJson(j['last_telemetry']) : null,
      );
}

class AutomationRule {
  final String deviceId;
  final bool enabled;
  final double ventTempHigh, ventTempHysteresis, ventHumidityHigh;
  final double curtainLuxOpen, curtainLuxClose, curtainTempProtect;
  AutomationRule({
    required this.deviceId,
    this.enabled = true,
    this.ventTempHigh = 30,
    this.ventTempHysteresis = 3,
    this.ventHumidityHigh = 90,
    this.curtainLuxOpen = 8000,
    this.curtainLuxClose = 2000,
    this.curtainTempProtect = 8,
  });
  factory AutomationRule.fromJson(Map<String, dynamic> j) => AutomationRule(
        deviceId: j['device_id'],
        enabled: j['enabled'] ?? true,
        ventTempHigh: (j['vent_temp_high'] as num).toDouble(),
        ventTempHysteresis: (j['vent_temp_hysteresis'] as num).toDouble(),
        ventHumidityHigh: (j['vent_humidity_high'] as num).toDouble(),
        curtainLuxOpen: (j['curtain_lux_open'] as num).toDouble(),
        curtainLuxClose: (j['curtain_lux_close'] as num).toDouble(),
        curtainTempProtect: (j['curtain_temp_protect'] as num).toDouble(),
      );
  Map<String, dynamic> toJson() => {
        'device_id': deviceId,
        'enabled': enabled,
        'vent_temp_high': ventTempHigh,
        'vent_temp_hysteresis': ventTempHysteresis,
        'vent_humidity_high': ventHumidityHigh,
        'curtain_lux_open': curtainLuxOpen,
        'curtain_lux_close': curtainLuxClose,
        'curtain_temp_protect': curtainTempProtect,
      };
}

class Alarm {
  final String deviceId, level, code, message;
  final DateTime ts;
  Alarm({
    required this.deviceId,
    required this.ts,
    required this.level,
    required this.code,
    required this.message,
  });
  factory Alarm.fromJson(Map<String, dynamic> j) => Alarm(
        deviceId: j['device_id'],
        ts: DateTime.parse(j['ts']).toLocal(),
        level: j['level'],
        code: j['code'],
        message: j['message'],
      );
}
