import 'package:flutter/material.dart';

import 'config.dart';
import 'screens/dashboard.dart';
import 'screens/control.dart';
import 'screens/history.dart';
import 'screens/alarms.dart';
import 'screens/settings.dart';

void main() => runApp(const GreenhouseApp());

class GreenhouseApp extends StatelessWidget {
  const GreenhouseApp({super.key});
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: '智能大棚',
      theme: ThemeData(
        colorSchemeSeed: Colors.green,
        useMaterial3: true,
      ),
      home: const HomeShell(),
    );
  }
}

class HomeShell extends StatefulWidget {
  const HomeShell({super.key});
  @override
  State<HomeShell> createState() => _HomeShellState();
}

class _HomeShellState extends State<HomeShell> {
  int _index = 0;
  final String deviceId = Config.defaultDeviceId;

  @override
  Widget build(BuildContext context) {
    final pages = [
      DashboardScreen(deviceId: deviceId),
      ControlScreen(deviceId: deviceId),
      HistoryScreen(deviceId: deviceId),
      AlarmsScreen(deviceId: deviceId),
      SettingsScreen(deviceId: deviceId),
    ];
    return Scaffold(
      body: pages[_index],
      bottomNavigationBar: NavigationBar(
        selectedIndex: _index,
        onDestinationSelected: (i) => setState(() => _index = i),
        destinations: const [
          NavigationDestination(icon: Icon(Icons.dashboard), label: '总览'),
          NavigationDestination(icon: Icon(Icons.gamepad), label: '控制'),
          NavigationDestination(icon: Icon(Icons.show_chart), label: '曲线'),
          NavigationDestination(icon: Icon(Icons.notifications), label: '告警'),
          NavigationDestination(icon: Icon(Icons.settings), label: '策略'),
        ],
      ),
    );
  }
}
