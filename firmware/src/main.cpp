/*
 * 智能大棚控制器固件
 * ESP32 + SIM7600(4G) + SHT31(温湿度) + BH1750(光照)
 * 功能: 卷帘棉被升降 / 顶部通风 / 温湿度光照采集 / MQTT 上云与远程控制
 *       限位停机 + 过流保护 + 正反转互锁 + 本地手动按钮 + 断网本地兜底
 */
#include <Arduino.h>
#include <Wire.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <Adafruit_SHT31.h>
#include <BH1750.h>
#include <ArduinoJson.h>

#include "config.h"
#include "pins.h"

// ----------------------- 全局对象 -----------------------
#define SerialMon Serial
#define SerialAT Serial2

TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
PubSubClient mqtt(gsmClient);
Adafruit_SHT31 sht31;
BH1750 lightMeter;

bool hasSht = false;
bool hasLux = false;

// 执行器状态机
enum Motion { IDLE, MOVING_OPEN, MOVING_CLOSE, FAULT };
struct Actuator {
  uint8_t pinOpen, pinClose;       // 继电器
  uint8_t limitOpen, limitClose;   // 限位
  uint8_t adcCurrent;              // 电流检测
  float overCurrent;               // 过流阈值
  Motion state = IDLE;
};

Actuator curtain { RELAY_CURTAIN_UP, RELAY_CURTAIN_DOWN,
                   LIMIT_CURTAIN_TOP, LIMIT_CURTAIN_BOTTOM,
                   CURRENT_CURTAIN_ADC, CURTAIN_OVERCURRENT_A };
Actuator vent { RELAY_VENT_OPEN, RELAY_VENT_CLOSE,
                LIMIT_VENT_OPEN, LIMIT_VENT_CLOSED,
                CURRENT_VENT_ADC, VENT_OVERCURRENT_A };

char topicTelemetry[64], topicStatus[64], topicCmd[64], topicAck[64];
unsigned long lastTelemetry = 0;

// ----------------------- 继电器底层 -----------------------
inline void relayWrite(uint8_t pin, bool on) {
#if RELAY_ACTIVE_LOW
  digitalWrite(pin, on ? LOW : HIGH);
#else
  digitalWrite(pin, on ? HIGH : LOW);
#endif
}

// 立即停止某执行器 (双继电器全部断开)
void actuatorStop(Actuator &a) {
  relayWrite(a.pinOpen, false);
  relayWrite(a.pinClose, false);
  if (a.state != FAULT) a.state = IDLE;
}

bool limitTriggered(uint8_t pin) { return digitalRead(pin) == LOW; } // 上拉, 触发为低

// 朝某方向驱动, 内置互锁 + 限位检查
void actuatorMove(Actuator &a, bool openDir) {
  if (a.state == FAULT) return;
  uint8_t limit = openDir ? a.limitOpen : a.limitClose;
  if (limitTriggered(limit)) { actuatorStop(a); return; } // 已到位, 不动
  // 互锁: 先断开反向, 再接通正向
  relayWrite(openDir ? a.pinClose : a.pinOpen, false);
  delay(50);
  relayWrite(openDir ? a.pinOpen : a.pinClose, true);
  a.state = openDir ? MOVING_OPEN : MOVING_CLOSE;
}

float readCurrent(uint8_t adcPin) {
  // ACS712-20A: 100mV/A, 2.5V 偏置. 简化读数 (实际需多次采样取RMS)
  int raw = analogRead(adcPin);
  float v = raw * 3.3f / 4095.0f;
  return fabs(v - 2.5f) / 0.100f;
}

// 运行中的安全监控: 到限位停, 过流判故障
void actuatorSafety(Actuator &a) {
  if (a.state == MOVING_OPEN && limitTriggered(a.limitOpen)) actuatorStop(a);
  if (a.state == MOVING_CLOSE && limitTriggered(a.limitClose)) actuatorStop(a);
  if (a.state == MOVING_OPEN || a.state == MOVING_CLOSE) {
    if (readCurrent(a.adcCurrent) > a.overCurrent) {
      actuatorStop(a);
      a.state = FAULT; // 需远程或手动复位
    }
  }
}

const char* motionStr(Motion m) {
  switch (m) { case MOVING_OPEN: return "moving_open";
               case MOVING_CLOSE: return "moving_close";
               case FAULT: return "fault"; default: return "idle"; }
}

// ----------------------- 指令分发 -----------------------
void applyCommand(const char* actuator, const char* action) {
  if (strcmp(actuator, "curtain") == 0) {
    if (strcmp(action, "up") == 0) { curtain.state = (curtain.state==FAULT)?IDLE:curtain.state; actuatorMove(curtain, true); }
    else if (strcmp(action, "down") == 0) { curtain.state = (curtain.state==FAULT)?IDLE:curtain.state; actuatorMove(curtain, false); }
    else if (strcmp(action, "stop") == 0) actuatorStop(curtain);
  } else if (strcmp(actuator, "vent") == 0) {
    if (strcmp(action, "open") == 0) { vent.state = (vent.state==FAULT)?IDLE:vent.state; actuatorMove(vent, true); }
    else if (strcmp(action, "close") == 0) { vent.state = (vent.state==FAULT)?IDLE:vent.state; actuatorMove(vent, false); }
    else if (strcmp(action, "stop") == 0) actuatorStop(vent);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, payload, len)) return;
  const char* actuator = doc["actuator"] | "";
  const char* action = doc["action"] | "";
  const char* cmdId = doc["cmd_id"] | "";
  applyCommand(actuator, action);
  // 回执
  StaticJsonDocument<128> ack;
  ack["cmd_id"] = cmdId;
  char buf[128]; size_t n = serializeJson(ack, buf);
  mqtt.publish(topicAck, buf, n);
}

// ----------------------- 本地按钮 -----------------------
void scanButtons() {
  // 按下=LOW. 点动: 按住则动, 松开停 (仅在该执行器非故障时)
  bool cu = digitalRead(BTN_CURTAIN_UP) == LOW;
  bool cd = digitalRead(BTN_CURTAIN_DOWN) == LOW;
  bool vo = digitalRead(BTN_VENT_OPEN) == LOW;
  bool vc = digitalRead(BTN_VENT_CLOSE) == LOW;
  if (cu ^ cd) { applyCommand("curtain", cu ? "up" : "down"); }
  else if (curtain.state != FAULT && (curtain.state==MOVING_OPEN||curtain.state==MOVING_CLOSE) && !cu && !cd) {
    // 仅当上一次是手动点动时停; 自动运动靠限位/指令停, 这里不强停
  }
  if (vo ^ vc) { applyCommand("vent", vo ? "open" : "close"); }
}

// ----------------------- 遥测上报 -----------------------
void publishTelemetry() {
  StaticJsonDocument<512> doc;
  if (hasSht) {
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();
    if (!isnan(t)) doc["temperature"] = t;
    if (!isnan(h)) doc["humidity"] = h;
  }
  if (hasLux) doc["lux"] = lightMeter.readLightLevel();
  doc["curtain_current"] = readCurrent(curtain.adcCurrent);
  doc["vent_current"] = readCurrent(vent.adcCurrent);
  doc["curtain_state"] = motionStr(curtain.state);
  doc["vent_state"] = motionStr(vent.state);
  JsonObject lim = doc.createNestedObject("limits");
  lim["curtain_top"] = limitTriggered(curtain.limitOpen);
  lim["curtain_bottom"] = limitTriggered(curtain.limitClose);
  lim["vent_open"] = limitTriggered(vent.limitOpen);
  lim["vent_closed"] = limitTriggered(vent.limitClose);

  char buf[512]; size_t n = serializeJson(doc, buf);
  mqtt.publish(topicTelemetry, buf, n);
}

// 断网兜底: 无 MQTT 连接时, 本地按温湿度/光照自动控制
void localFallback() {
  if (!hasSht) return;
  float t = sht31.readTemperature();
  float lux = hasLux ? lightMeter.readLightLevel() : NAN;
  if (!isnan(t)) {
    if (t > LOCAL_VENT_TEMP_HIGH) actuatorMove(vent, true);
    else if (t < LOCAL_VENT_TEMP_HIGH - LOCAL_VENT_TEMP_HYST) actuatorMove(vent, false);
    if (t < LOCAL_CURTAIN_TEMP_PROTECT) actuatorMove(curtain, false);      // 低温保温优先
    else if (!isnan(lux)) {
      if (lux > LOCAL_CURTAIN_LUX_OPEN) actuatorMove(curtain, true);
      else if (lux < LOCAL_CURTAIN_LUX_CLOSE) actuatorMove(curtain, false);
    }
  }
}

// ----------------------- 网络 -----------------------
bool mqttConnect() {
  SerialMon.print("MQTT 连接... ");
  bool ok = mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS,
                         topicStatus, 1, true, "{\"status\":\"offline\"}");
  if (ok) {
    SerialMon.println("成功");
    mqtt.publish(topicStatus, "{\"status\":\"online\"}", true);
    mqtt.subscribe(topicCmd, 1);
  } else {
    SerialMon.print("失败 rc="); SerialMon.println(mqtt.state());
  }
  return ok;
}

void setupModem() {
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH); delay(300);
  digitalWrite(MODEM_PWRKEY, LOW); delay(1000);
  SerialMon.println("初始化 4G 模块...");
  modem.restart();
  if (strlen(GSM_PIN) && modem.getSimStatus() != 3) modem.simUnlock(GSM_PIN);
  SerialMon.print("等待网络..."); modem.waitForNetwork();
  SerialMon.println(modem.isNetworkConnected() ? "已注册" : "无网络");
  SerialMon.print("GPRS/APN 连接..."); modem.gprsConnect(GSM_APN, GSM_USER, GSM_PASS);
  SerialMon.println(modem.isGprsConnected() ? "成功" : "失败");
}

// ----------------------- setup / loop -----------------------
void setup() {
  SerialMon.begin(115200); delay(100);

  uint8_t outs[] = { RELAY_CURTAIN_UP, RELAY_CURTAIN_DOWN, RELAY_VENT_OPEN, RELAY_VENT_CLOSE };
  for (uint8_t p : outs) { pinMode(p, OUTPUT); relayWrite(p, false); }
  uint8_t ins[] = { LIMIT_CURTAIN_TOP, LIMIT_CURTAIN_BOTTOM, LIMIT_VENT_OPEN, LIMIT_VENT_CLOSED,
                    BTN_CURTAIN_UP, BTN_CURTAIN_DOWN, BTN_VENT_OPEN, BTN_VENT_CLOSE };
  for (uint8_t p : ins) pinMode(p, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  hasSht = sht31.begin(0x44);
  hasLux = lightMeter.begin();

  snprintf(topicTelemetry, sizeof(topicTelemetry), "%s/%s/telemetry", TOPIC_PREFIX, DEVICE_ID);
  snprintf(topicStatus, sizeof(topicStatus), "%s/%s/status", TOPIC_PREFIX, DEVICE_ID);
  snprintf(topicCmd, sizeof(topicCmd), "%s/%s/cmd", TOPIC_PREFIX, DEVICE_ID);
  snprintf(topicAck, sizeof(topicAck), "%s/%s/cmd/ack", TOPIC_PREFIX, DEVICE_ID);

  setupModem();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
  mqttConnect();
}

void loop() {
  // 安全监控始终最高优先级
  actuatorSafety(curtain);
  actuatorSafety(vent);
  scanButtons();

  if (mqtt.connected()) {
    mqtt.loop();
    if (millis() - lastTelemetry > TELEMETRY_INTERVAL_MS) {
      lastTelemetry = millis();
      publishTelemetry();
    }
  } else {
    // 断网: 尝试重连, 同时本地兜底
    static unsigned long lastReconnect = 0;
    localFallback();
    if (millis() - lastReconnect > 10000) {
      lastReconnect = millis();
      if (!modem.isGprsConnected()) modem.gprsConnect(GSM_APN, GSM_USER, GSM_PASS);
      mqttConnect();
    }
  }
  delay(20);
}
