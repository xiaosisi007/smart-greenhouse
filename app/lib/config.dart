/// 后端地址配置。打包前改成你的云服务器公网地址。
class Config {
  /// REST API 基地址
  static const String apiBase = String.fromEnvironment(
    'API_BASE',
    defaultValue: 'http://10.0.2.2:8000', // 安卓模拟器访问宿主机
  );

  /// WebSocket 实时推送地址
  static const String wsUrl = String.fromEnvironment(
    'WS_URL',
    defaultValue: 'ws://10.0.2.2:8000/ws',
  );

  /// 默认设备 ID
  static const String defaultDeviceId = String.fromEnvironment(
    'DEVICE_ID',
    defaultValue: 'gh1',
  );
}
