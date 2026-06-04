import 'package:flutter/material.dart';

import '../api.dart';
import '../models.dart';

/// 自动控制策略设置。
class SettingsScreen extends StatefulWidget {
  final String deviceId;
  const SettingsScreen({super.key, required this.deviceId});
  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final _api = Api();
  AutomationRule? _rule;
  bool _loading = true;

  // 可编辑字段
  late bool _enabled;
  final _ctl = <String, TextEditingController>{};

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final r = await _api.rule(widget.deviceId);
    setState(() {
      _rule = r;
      _enabled = r.enabled;
      _ctl['vent_temp_high'] = TextEditingController(text: r.ventTempHigh.toString());
      _ctl['vent_temp_hyst'] = TextEditingController(text: r.ventTempHysteresis.toString());
      _ctl['vent_humidity_high'] = TextEditingController(text: r.ventHumidityHigh.toString());
      _ctl['curtain_lux_open'] = TextEditingController(text: r.curtainLuxOpen.toString());
      _ctl['curtain_lux_close'] = TextEditingController(text: r.curtainLuxClose.toString());
      _ctl['curtain_temp_protect'] =
          TextEditingController(text: r.curtainTempProtect.toString());
      _loading = false;
    });
  }

  double _v(String k) => double.tryParse(_ctl[k]!.text) ?? 0;

  Future<void> _save() async {
    final r = AutomationRule(
      deviceId: widget.deviceId,
      enabled: _enabled,
      ventTempHigh: _v('vent_temp_high'),
      ventTempHysteresis: _v('vent_temp_hyst'),
      ventHumidityHigh: _v('vent_humidity_high'),
      curtainLuxOpen: _v('curtain_lux_open'),
      curtainLuxClose: _v('curtain_lux_close'),
      curtainTempProtect: _v('curtain_temp_protect'),
    );
    await _api.saveRule(r);
    if (mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('策略已保存')));
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_loading || _rule == null) {
      return Scaffold(
        appBar: AppBar(title: const Text('自动策略')),
        body: const Center(child: CircularProgressIndicator()),
      );
    }
    return Scaffold(
      appBar: AppBar(title: const Text('自动策略')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          SwitchListTile(
            title: const Text('启用自动控制'),
            subtitle: const Text('关闭后仅手动控制 (安全告警仍生效)'),
            value: _enabled,
            onChanged: (v) => setState(() => _enabled = v),
          ),
          const Divider(),
          _field('vent_temp_high', '通风温度上限 (℃)', '超过自动开风口'),
          _field('vent_temp_hyst', '温度回差 (℃)', '回落该值后关风口, 防频繁启停'),
          _field('vent_humidity_high', '排湿湿度上限 (%)', '湿度超过自动开风口排湿'),
          const Divider(),
          _field('curtain_lux_open', '卷起光照阈值 (lx)', '光照高于此值卷起采光'),
          _field('curtain_lux_close', '放下光照阈值 (lx)', '光照低于此值放下保温'),
          _field('curtain_temp_protect', '低温保护 (℃)', '棚温低于此值强制放棉被保温'),
          const SizedBox(height: 24),
          FilledButton.icon(
            onPressed: _save,
            icon: const Icon(Icons.save),
            label: const Text('保存策略'),
          ),
        ],
      ),
    );
  }

  Widget _field(String key, String label, String hint) => Padding(
        padding: const EdgeInsets.symmetric(vertical: 8),
        child: TextField(
          controller: _ctl[key],
          keyboardType: const TextInputType.numberWithOptions(decimal: true),
          decoration: InputDecoration(
            labelText: label,
            helperText: hint,
            border: const OutlineInputBorder(),
          ),
        ),
      );
}
