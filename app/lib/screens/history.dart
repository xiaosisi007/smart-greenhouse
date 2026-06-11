import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';

import '../api.dart';
import '../models.dart';

/// 历史曲线: 温度/湿度趋势图。
class HistoryScreen extends StatefulWidget {
  final String deviceId;
  const HistoryScreen({super.key, required this.deviceId});
  @override
  State<HistoryScreen> createState() => _HistoryScreenState();
}

class _HistoryScreenState extends State<HistoryScreen> {
  final _api = Api();
  List<Telemetry> _data = [];
  int _hours = 24;
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    setState(() => _loading = true);
    final d = await _api.history(widget.deviceId, hours: _hours);
    if (mounted) {
      setState(() {
        _data = d.reversed.toList(); // 时间正序
        _loading = false;
      });
    }
  }

  List<FlSpot> _spots(double? Function(Telemetry) sel) {
    final spots = <FlSpot>[];
    for (var i = 0; i < _data.length; i++) {
      final v = sel(_data[i]);
      if (v != null) spots.add(FlSpot(i.toDouble(), v));
    }
    return spots;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('历史曲线'),
        actions: [
          PopupMenuButton<int>(
            initialValue: _hours,
            onSelected: (h) {
              setState(() => _hours = h);
              _load();
            },
            itemBuilder: (c) => const [
              PopupMenuItem(value: 6, child: Text('近 6 小时')),
              PopupMenuItem(value: 24, child: Text('近 24 小时')),
              PopupMenuItem(value: 72, child: Text('近 3 天')),
              PopupMenuItem(value: 168, child: Text('近 7 天')),
            ],
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16),
              child: Row(children: [Text('${_hours}h'), const Icon(Icons.arrow_drop_down)]),
            ),
          ),
        ],
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : _data.isEmpty
              ? const Center(child: Text('暂无历史数据'))
              : ListView(
                  padding: const EdgeInsets.all(16),
                  children: [
                    _chartCard('温度 ℃', _spots((t) => t.temperature), Colors.orange),
                    const SizedBox(height: 16),
                    _chartCard('湿度 %', _spots((t) => t.humidity), Colors.blue),
                  ],
                ),
    );
  }

  Widget _chartCard(String title, List<FlSpot> spots, Color color) => Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(title, style: const TextStyle(fontWeight: FontWeight.bold)),
              const SizedBox(height: 16),
              SizedBox(
                height: 200,
                child: spots.isEmpty
                    ? const Center(child: Text('无数据'))
                    : LineChart(LineChartData(
                        titlesData: const FlTitlesData(
                          topTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                          rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                        ),
                        lineBarsData: [
                          LineChartBarData(
                            spots: spots,
                            color: color,
                            isCurved: true,
                            dotData: const FlDotData(show: false),
                            belowBarData: BarAreaData(
                                show: true, color: color.withValues(alpha: 0.15)),
                          ),
                        ],
                      )),
              ),
            ],
          ),
        ),
      );
}
