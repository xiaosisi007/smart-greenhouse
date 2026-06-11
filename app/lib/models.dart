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

/// 单台电机遥测 (多电机固件 motors 数组中的一项)。
class Motor {
  final int id;
  final String type; // curtain / vent
  final String? name;
  final String state;
  final double? current, position;
  final String? faultReason;
  final bool limitOpen, limitClose;
  Motor({
    required this.id,
    required this.type,
    this.name,
    this.state = 'idle',
    this.current,
    this.position,
    this.faultReason,
    this.limitOpen = false,
    this.limitClose = false,
  });
  factory Motor.fromJson(Map<String, dynamic> j) => Motor(
        id: j['id'] ?? 0,
        type: j['type'] ?? 'curtain',
        name: j['name'],
        state: j['state'] ?? 'idle',
        current: (j['current'] as num?)?.toDouble(),
        position: (j['position'] as num?)?.toDouble(),
        faultReason: j['fault_reason'],
        limitOpen: j['limit_open'] ?? false,
        limitClose: j['limit_close'] ?? false,
      );
}

class Telemetry {
  final String deviceId;
  final DateTime ts;
  final double? temperature, humidity, lux, curtainCurrent, ventCurrent;
  final double? curtainPosition, ventPosition; // 开度 % (0=全关/放下, 100=全开/卷起)
  final String curtainState, ventState;
  final String? curtainFaultReason, ventFaultReason;
  final LimitSwitches limits;
  final List<Motor> motors;

  Telemetry({
    required this.deviceId,
    required this.ts,
    this.temperature,
    this.humidity,
    this.lux,
    this.curtainCurrent,
    this.ventCurrent,
    this.curtainPosition,
    this.ventPosition,
    this.curtainState = 'idle',
    this.ventState = 'idle',
    this.curtainFaultReason,
    this.ventFaultReason,
    required this.limits,
    this.motors = const [],
  });

  factory Telemetry.fromJson(Map<String, dynamic> j) => Telemetry(
        deviceId: j['device_id'],
        ts: DateTime.parse(j['ts']).toLocal(),
        temperature: (j['temperature'] as num?)?.toDouble(),
        humidity: (j['humidity'] as num?)?.toDouble(),
        lux: (j['lux'] as num?)?.toDouble(),
        curtainCurrent: (j['curtain_current'] as num?)?.toDouble(),
        ventCurrent: (j['vent_current'] as num?)?.toDouble(),
        curtainPosition: (j['curtain_position'] as num?)?.toDouble(),
        ventPosition: (j['vent_position'] as num?)?.toDouble(),
        curtainState: j['curtain_state'] ?? 'idle',
        ventState: j['vent_state'] ?? 'idle',
        curtainFaultReason: j['curtain_fault_reason'],
        ventFaultReason: j['vent_fault_reason'],
        limits: LimitSwitches.fromJson(j['limits'] ?? {}),
        motors: (j['motors'] as List?)
                ?.map((e) => Motor.fromJson(e as Map<String, dynamic>))
                .toList() ??
            const [],
      );

  /// motors 数组为空时 (旧固件), 由旧版字段合成两台电机, 供 UI 统一按列表渲染。
  List<Motor> get effectiveMotors {
    if (motors.isNotEmpty) return motors;
    return [
      Motor(
        id: 0,
        type: 'curtain',
        state: curtainState,
        current: curtainCurrent,
        position: curtainPosition,
        faultReason: curtainFaultReason,
        limitOpen: limits.curtainTop,
        limitClose: limits.curtainBottom,
      ),
      Motor(
        id: 1,
        type: 'vent',
        state: ventState,
        current: ventCurrent,
        position: ventPosition,
        faultReason: ventFaultReason,
        limitOpen: limits.ventOpen,
        limitClose: limits.ventClosed,
      ),
    ];
  }
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
