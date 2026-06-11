import 'dart:async';
import 'package:flutter/material.dart';

import '../api.dart';
import '../models.dart';

/// 总览: 实时温湿度/光照 + 设备状态, 通过 WebSocket 实时刷新。
class DashboardScreen extends StatefulWidget {
  final String deviceId;
  const DashboardScreen({super.key, required this.deviceId});
  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  final _api = Api();
  final _rt = RealtimeClient();
  Telemetry? _t;
  String _status = 'unknown';
  StreamSubscription? _sub;

  @override
  void initState() {
    super.initState();
    _load();
    _sub = _rt.connect().listen((msg) {
      if (msg['event'] == 'telemetry') {
        final t = Telemetry.fromJson(msg['data']);
        if (t.deviceId == widget.deviceId) setState(() => _t = t);
      } else if (msg['event'] == 'status') {
        if (msg['data']['device_id'] == widget.deviceId) {
          setState(() => _status = msg['data']['status']);
        }
      }
    });
  }

  Future<void> _load() async {
    final t = await _api.latest(widget.deviceId);
    if (mounted) setState(() => _t = t);
  }

  @override
  void dispose() {
    _sub?.cancel();
    _rt.close();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('大棚总览 · ${widget.deviceId}'),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 12),
            child: Chip(
              avatar: Icon(
                _status == 'online' ? Icons.cloud_done : Icons.cloud_off,
                size: 18,
                color: _status == 'online' ? Colors.green : Colors.grey,
              ),
              label: Text(_status == 'online' ? '在线' : '离线'),
            ),
          ),
        ],
      ),
      body: RefreshIndicator(
        onRefresh: _load,
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            Row(
              children: [
                _metric('温度', _t?.temperature, '℃', Icons.thermostat, Colors.orange),
                _metric('湿度', _t?.humidity, '%', Icons.water_drop, Colors.blue),
              ],
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                _metric('光照', _t?.lux, 'lx', Icons.wb_sunny, Colors.amber),
                _metric('卷帘电流', _t?.curtainCurrent, 'A', Icons.bolt, Colors.purple),
              ],
            ),
            const SizedBox(height: 16),
            // 按电机列表动态渲染 (1/2/4 台; 旧固件合成卷帘+风口两张卡片)
            for (final m in _t?.effectiveMotors ?? _defaultMotors())
              Padding(
                padding: const EdgeInsets.only(bottom: 8),
                child: _motorCard(m, _t?.effectiveMotors ?? const []),
              ),
          ],
        ),
      ),
    );
  }

  Widget _metric(String label, double? v, String unit, IconData icon, Color color) {
    return Expanded(
      child: Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Icon(icon, color: color),
              const SizedBox(height: 8),
              Text(label, style: const TextStyle(color: Colors.grey)),
              const SizedBox(height: 4),
              Text(
                v == null ? '--' : '${v.toStringAsFixed(1)} $unit',
                style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
              ),
            ],
          ),
        ),
      ),
    );
  }

  List<Motor> _defaultMotors() => [
        Motor(id: 0, type: 'curtain'),
        Motor(id: 1, type: 'vent'),
      ];

  String _motorLabel(Motor m, List<Motor> all) {
    if (m.name != null && m.name!.isNotEmpty) return m.name!;
    final base = m.type == 'curtain' ? '卷帘棉被' : '顶部通风';
    final sameType = all.where((x) => x.type == m.type).length;
    return sameType > 1 ? '$base #${m.id}' : base;
  }

  Widget _motorCard(Motor m, List<Motor> all) {
    final name = _motorLabel(m, all);
    final state = m.state;
    final position = m.position;
    final faultReason = m.faultReason;
    final isCurtain = m.type == 'curtain';
    final limA = m.limitOpen;
    final limB = m.limitClose;
    final labelA = isCurtain ? '到顶' : '全开';
    final labelB = isCurtain ? '到底' : '全关';
    final moving = state.startsWith('moving');
    final fault = state == 'fault';
    return Card(
      child: Column(children: [
        ListTile(
          leading: Icon(
            fault ? Icons.error : (moving ? Icons.sync : Icons.check_circle),
            color: fault ? Colors.red : (moving ? Colors.blue : Colors.green),
          ),
          title: Text(name),
          subtitle: Text(fault ? '故障: ${_faultText(faultReason)}, 已停机' : _stateText(state)),
          trailing: Wrap(spacing: 6, children: [
            if (position != null)
              Chip(label: Text('开度 ${position.toStringAsFixed(0)}%'),
                  visualDensity: VisualDensity.compact),
            if (limA) Chip(label: Text(labelA), visualDensity: VisualDensity.compact),
            if (limB) Chip(label: Text(labelB), visualDensity: VisualDensity.compact),
          ]),
        ),
        if (position != null)
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 0, 16, 12),
            child: LinearProgressIndicator(
              value: (position / 100).clamp(0.0, 1.0),
              color: fault ? Colors.red : Colors.green,
            ),
          ),
      ]),
    );
  }

  String _faultText(String? reason) {
    switch (reason) {
      case 'overcurrent':
        return '过流(卡死/过载)';
      case 'stall':
        return '失速(缠绕/卡死, 电机通电不转)';
      case 'timeout':
        return '行程超时(未到限位)';
      case 'undercurrent':
        return '欠流(绳断/脱落/空转)';
      default:
        return '过流/卡死';
    }
  }

  String _stateText(String s) {
    switch (s) {
      case 'moving_open':
        return '运行中 · 卷起/开';
      case 'moving_close':
        return '运行中 · 放下/关';
      case 'fault':
        return '故障/过流, 已停机';
      default:
        return '空闲';
    }
  }
}
