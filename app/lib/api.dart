import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:web_socket_channel/web_socket_channel.dart';

import 'config.dart';
import 'models.dart';

/// 后端 REST 客户端。
class Api {
  final String base;
  Api({String? base}) : base = base ?? Config.apiBase;

  Future<List<Device>> devices() async {
    final r = await http.get(Uri.parse('$base/api/devices'));
    final list = jsonDecode(r.body) as List;
    return list.map((e) => Device.fromJson(e)).toList();
  }

  Future<Telemetry?> latest(String deviceId) async {
    final r = await http.get(Uri.parse('$base/api/devices/$deviceId/telemetry/latest'));
    if (r.statusCode != 200) return null;
    return Telemetry.fromJson(jsonDecode(r.body));
  }

  Future<List<Telemetry>> history(String deviceId, {int hours = 24}) async {
    final r = await http
        .get(Uri.parse('$base/api/devices/$deviceId/telemetry/history?hours=$hours'));
    final list = jsonDecode(r.body) as List;
    return list.map((e) => Telemetry.fromJson(e)).toList();
  }

  Future<void> command(String deviceId, String actuator, String action) async {
    await http.post(
      Uri.parse('$base/api/devices/$deviceId/command'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({'actuator': actuator, 'action': action, 'source': 'manual'}),
    );
  }

  Future<AutomationRule> rule(String deviceId) async {
    final r = await http.get(Uri.parse('$base/api/devices/$deviceId/rule'));
    return AutomationRule.fromJson(jsonDecode(r.body));
  }

  Future<void> saveRule(AutomationRule rule) async {
    await http.put(
      Uri.parse('$base/api/devices/${rule.deviceId}/rule'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode(rule.toJson()),
    );
  }

  Future<List<Alarm>> alarms(String deviceId) async {
    final r = await http.get(Uri.parse('$base/api/devices/$deviceId/alarms'));
    final list = jsonDecode(r.body) as List;
    return list.map((e) => Alarm.fromJson(e)).toList();
  }
}

/// WebSocket 实时事件流 (telemetry / status / alarm / command)。
class RealtimeClient {
  WebSocketChannel? _ch;

  Stream<Map<String, dynamic>> connect() {
    _ch = WebSocketChannel.connect(Uri.parse(Config.wsUrl));
    return _ch!.stream.map((msg) => jsonDecode(msg) as Map<String, dynamic>);
  }

  void close() => _ch?.sink.close();
}
