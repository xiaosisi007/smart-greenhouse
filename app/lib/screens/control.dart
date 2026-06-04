import 'package:flutter/material.dart';

import '../api.dart';

/// 手动控制: 卷帘 升/停/降, 风口 开/停/关 (带二次确认)。
class ControlScreen extends StatefulWidget {
  final String deviceId;
  const ControlScreen({super.key, required this.deviceId});
  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen> {
  final _api = Api();

  Future<void> _send(String actuator, String action, String label) async {
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
    await _api.command(widget.deviceId, actuator, action);
    if (mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text('已下发: $label')));
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('设备控制')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _group('卷帘棉被', [
            _btn(Icons.arrow_upward, '卷起', () => _send('curtain', 'up', '卷帘卷起'), Colors.green),
            _btn(Icons.stop, '停止', () => _send('curtain', 'stop', '卷帘停止'), Colors.red),
            _btn(Icons.arrow_downward, '放下', () => _send('curtain', 'down', '卷帘放下'), Colors.blue),
          ]),
          const SizedBox(height: 16),
          _group('顶部通风', [
            _btn(Icons.open_in_full, '开风口', () => _send('vent', 'open', '风口打开'), Colors.green),
            _btn(Icons.stop, '停止', () => _send('vent', 'stop', '风口停止'), Colors.red),
            _btn(Icons.close_fullscreen, '关风口', () => _send('vent', 'close', '风口关闭'), Colors.blue),
          ]),
        ],
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
