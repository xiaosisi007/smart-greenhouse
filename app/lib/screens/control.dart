import 'package:flutter/material.dart';

import '../api.dart';
import '../models.dart';

/// 手动控制: 按电机列表动态渲染 (1/2/4 台), 卷帘 升/停/降, 风口 开/停/关 (带二次确认)。
class ControlScreen extends StatefulWidget {
  final String deviceId;
  const ControlScreen({super.key, required this.deviceId});
  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen> {
  final _api = Api();
  List<Motor> _motors = [
    Motor(id: 0, type: 'curtain'),
    Motor(id: 1, type: 'vent'),
  ];

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final t = await _api.latest(widget.deviceId);
    if (mounted && t != null && t.effectiveMotors.isNotEmpty) {
      setState(() => _motors = t.effectiveMotors);
    }
  }

  Future<void> _send(Motor m, String action, String label) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (c) => AlertDialog(
        title: const Text('确认操作'),
        content: Text('确定要执行「$label」吗?'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(c, false), child: const Text('取消')),
          FilledButton(onPressed: () => Navigator.pop(c, true), child: const Text('确定')),
        ],
      ),
    );
    if (ok != true) return;
    await _api.command(widget.deviceId, m.type, action, motorId: m.id);
    if (mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text('已下发: $label')));
    }
  }

  String _motorLabel(Motor m) {
    if (m.name != null && m.name!.isNotEmpty) return m.name!;
    final base = m.type == 'curtain' ? '卷帘棉被' : '顶部通风';
    final sameType = _motors.where((x) => x.type == m.type).length;
    return sameType > 1 ? '$base #${m.id}' : base;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('设备控制')),
      body: RefreshIndicator(
        onRefresh: _load,
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            for (final m in _motors)
              Padding(
                padding: const EdgeInsets.only(bottom: 16),
                child: m.type == 'curtain'
                    ? _group(_motorLabel(m), [
                        _btn(Icons.arrow_upward, '卷起',
                            () => _send(m, 'up', '${_motorLabel(m)}卷起'), Colors.green),
                        _btn(Icons.stop, '停止',
                            () => _send(m, 'stop', '${_motorLabel(m)}停止'), Colors.red),
                        _btn(Icons.arrow_downward, '放下',
                            () => _send(m, 'down', '${_motorLabel(m)}放下'), Colors.blue),
                      ])
                    : _group(_motorLabel(m), [
                        _btn(Icons.open_in_full, '开风口',
                            () => _send(m, 'open', '${_motorLabel(m)}打开'), Colors.green),
                        _btn(Icons.stop, '停止',
                            () => _send(m, 'stop', '${_motorLabel(m)}停止'), Colors.red),
                        _btn(Icons.close_fullscreen, '关风口',
                            () => _send(m, 'close', '${_motorLabel(m)}关闭'), Colors.blue),
                      ]),
              ),
          ],
        ),
      ),
    );
  }

  Widget _group(String title, List<Widget> btns) => Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(title, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
              const SizedBox(height: 16),
              Row(mainAxisAlignment: MainAxisAlignment.spaceEvenly, children: btns),
            ],
          ),
        ),
      );

  Widget _btn(IconData icon, String label, VoidCallback onTap, Color color) => Column(
        children: [
          SizedBox(
            width: 72,
            height: 72,
            child: FilledButton(
              style: FilledButton.styleFrom(backgroundColor: color, shape: const CircleBorder()),
              onPressed: onTap,
              child: Icon(icon, size: 30),
            ),
          ),
          const SizedBox(height: 8),
          Text(label),
        ],
      );
}
