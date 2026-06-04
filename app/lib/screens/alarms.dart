import 'package:flutter/material.dart';
import 'package:intl/intl.dart';

import '../api.dart';
import '../models.dart';

/// 告警列表。
class AlarmsScreen extends StatefulWidget {
  final String deviceId;
  const AlarmsScreen({super.key, required this.deviceId});
  @override
  State<AlarmsScreen> createState() => _AlarmsScreenState();
}

class _AlarmsScreenState extends State<AlarmsScreen> {
  final _api = Api();
  List<Alarm> _alarms = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    setState(() => _loading = true);
    final a = await _api.alarms(widget.deviceId);
    if (mounted) setState(() { _alarms = a; _loading = false; });
  }

  Color _color(String level) => switch (level) {
        'critical' => Colors.red,
        'warning' => Colors.orange,
        _ => Colors.blue,
      };

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('告警记录')),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : _alarms.isEmpty
              ? const Center(child: Text('暂无告警'))
              : RefreshIndicator(
                  onRefresh: _load,
                  child: ListView.separated(
                    itemCount: _alarms.length,
                    separatorBuilder: (_, __) => const Divider(height: 1),
                    itemBuilder: (c, i) {
                      final a = _alarms[i];
                      return ListTile(
                        leading: Icon(Icons.warning, color: _color(a.level)),
                        title: Text(a.message),
                        subtitle: Text(DateFormat('MM-dd HH:mm:ss').format(a.ts)),
                        trailing: Text(a.level.toUpperCase(),
                            style: TextStyle(color: _color(a.level))),
                      );
                    },
                  ),
                ),
    );
  }
}
