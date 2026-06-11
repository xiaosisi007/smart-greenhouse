/*
 * 智能大棚控制器固件 (1/2/4 电机可配置)
 * ESP32 + SIM7600(4G) + SHT31(温湿度) + BH1750(光照)
 * 功能: 卷帘棉被升降 / 顶部通风 / 温湿度光照采集 / MQTT 上云与远程控制
 *       限位停机 + 过流保护 + 正反转互锁 + 本地手动按钮 + 断网本地兜底
 * 电机数量由 config.h 的 MOTOR_COUNT 决定 (4 台时经 MCP23017 + ADS1115 扩展)
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

#if MOTOR_COUNT > 2
#define USE_EXPANDER 1
#include <Adafruit_MCP23X17.h>
#include <Adafruit_ADS1X15.h>
#endif

// ----------------------- 全局对象 -----------------------
#define SerialMon Serial
#define SerialAT Serial2

TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
PubSubClient mqtt(gsmClient);
Adafruit_SHT31 sht31;
BH1750 lightMeter;
#ifdef USE_EXPANDER
Adafruit_MCP23X17 mcp;   // 继电器 + 限位扩展
Adafruit_ADS1115 ads;    // 电流采样扩展
#endif

bool hasSht = false;
bool hasLux = false;

// 执行器状态机
enum Motion { IDLE, MOVING_OPEN, MOVING_CLOSE, FAULT };
struct Actuator {
  MotorIO io;
  MotorParams prm;
  Motion state;
  // 运动监测运行时状态
  uint32_t seenPulses;             // 已消费的 ISR 脉冲计数
  int32_t position;                // 当前位置 (脉冲数, 0=全关/全放下)
  unsigned long moveStartMs, lastPulseMs;
  const char* faultReason;
};

Actuator motors[MOTOR_COUNT];

// 脉冲反馈 ISR (卷轴霍尔/接近开关), attachInterruptArg 共用一个 ISR
volatile uint32_t pulseCount[MOTOR_COUNT];
void IRAM_ATTR pulseISR(void* arg) { (*(volatile uint32_t*)arg)++; }

char topicTelemetry[64], topicStatus[64], topicCmd[64], topicAck[64];
unsigned long lastTelemetry = 0;

// ----------------------- IO 抽象 (直连 GPIO / MCP23017 / ADS1115) -----------------------
void ioWrite(const MotorIO &io, uint8_t pin, uint8_t lvl) {
#ifdef USE_EXPANDER
  if (io.onExpander) { mcp.digitalWrite(pin, lvl); return; }
#endif
  digitalWrite(pin, lvl);
}

int ioRead(const MotorIO &io, uint8_t pin) {
#ifdef USE_EXPANDER
  if (io.onExpander) return mcp.digitalRead(pin);
#endif
  return digitalRead(pin);
}

inline void relayWrite(const MotorIO &io, uint8_t pin, bool on) {
#if RELAY_ACTIVE_LOW
  ioWrite(io, pin, on ? LOW : HIGH);
#else
  ioWrite(io, pin, on ? HIGH : LOW);
#endif
}

// 限位: 上拉, 触发为低
bool limitOpenHit(const Actuator &a)  { return ioRead(a.io, a.io.limitOpen) == LOW; }
bool limitCloseHit(const Actuator &a) { return ioRead(a.io, a.io.limitClose) == LOW; }

float readCurrent(const Actuator &a) {
  // ACS712-20A: 100mV/A, 2.5V 偏置. 简化读数 (实际需多次采样取RMS)
#ifdef USE_EXPANDER
  if (a.io.onExpander) {
    float v = ads.computeVolts(ads.readADC_SingleEnded(a.io.currentCh));
    return fabs(v - 2.5f) / 0.100f;
  }
#endif
  int raw = analogRead(a.io.currentCh);
  float v = raw * 3.3f / 4095.0f;
  return fabs(v - 2.5f) / 0.100f;
}

// ----------------------- 执行器控制 -----------------------
// 立即停止某执行器 (双继电器全部断开)
void actuatorStop(Actuator &a) {
  relayWrite(a.io, a.io.relayOpen, false);
  relayWrite(a.io, a.io.relayClose, false);
  if (a.state != FAULT) a.state = IDLE;
}

// 朝某方向驱动, 内置互锁 + 限位检查
void actuatorMove(Actuator &a, bool openDir) {
  if (a.state == FAULT) return;
  if (openDir ? limitOpenHit(a) : limitCloseHit(a)) { actuatorStop(a); return; } // 已到位, 不动
  Motion target = openDir ? MOVING_OPEN : MOVING_CLOSE;
  if (a.state == target) return; // 已在朝该方向运动, 不重置监测计时
  // 互锁: 先断开反向, 再接通正向
  relayWrite(a.io, openDir ? a.io.relayClose : a.io.relayOpen, false);
  delay(50);
  relayWrite(a.io, openDir ? a.io.relayOpen : a.io.relayClose, true);
  a.state = target;
  a.moveStartMs = a.lastPulseMs = millis();
}

void actuatorFault(Actuator &a, const char* reason) {
  actuatorStop(a);
  a.state = FAULT; // 需远程或手动复位
  a.faultReason = reason;
}

// 运行中的安全监控: 到限位停; 过流/失速/超时/欠流判故障
void actuatorSafety(Actuator &a, volatile uint32_t &pulseCounter) {
  // 消费新脉冲, 更新位置与最后脉冲时间
  uint32_t total = pulseCounter;
  uint32_t delta = total - a.seenPulses;
  a.seenPulses = total;
  if (delta > 0) {
    a.lastPulseMs = millis();
    if (a.state == MOVING_OPEN) a.position += delta;
    else if (a.state == MOVING_CLOSE) a.position -= delta;
  }
  // 限位处校准位置
  if (limitOpenHit(a) && a.prm.pulsesFull > 0) a.position = a.prm.pulsesFull;
  if (limitCloseHit(a)) a.position = 0;

  if (a.state == MOVING_OPEN && limitOpenHit(a)) actuatorStop(a);
  if (a.state == MOVING_CLOSE && limitCloseHit(a)) actuatorStop(a);
  if (a.state == MOVING_OPEN || a.state == MOVING_CLOSE) {
    unsigned long now = millis();
    if (readCurrent(a) > a.prm.overA)
      actuatorFault(a, "overcurrent");          // 卡死/过载
    else if (a.prm.pulsesFull > 0 && now - a.lastPulseMs > STALL_TIMEOUT_MS)
      actuatorFault(a, "stall");                // 电机通电但不转: 缠绕/卡死
    else if (now - a.moveStartMs > a.prm.maxTravelMs)
      actuatorFault(a, "timeout");              // 超最大行程时间未到限位
    else if (a.prm.underA > 0 && now - a.moveStartMs > UNDERCURRENT_GRACE_MS
             && readCurrent(a) < a.prm.underA)
      actuatorFault(a, "undercurrent");         // 绳断/脱落/空转
  }
}

const char* motionStr(Motion m) {
  switch (m) { case MOVING_OPEN: return "moving_open";
               case MOVING_CLOSE: return "moving_close";
               case FAULT: return "fault"; default: return "idle"; }
}

// ----------------------- 指令分发 -----------------------
void clearFault(Actuator &a) {
  if (a.state == FAULT) { a.state = IDLE; a.faultReason = ""; }
}

// 该类型第一台电机编号 (兼容不带 motor_id 的旧协议)
int firstMotorOfType(uint8_t type) {
  for (int i = 0; i < MOTOR_COUNT; i++)
    if (motors[i].prm.type == type) return i;
  return -1;
}

void applyCommand(int motorId, const char* action) {
  if (motorId < 0 || motorId >= MOTOR_COUNT) return;
  Actuator &a = motors[motorId];
  if (strcmp(action, "up") == 0 || strcmp(action, "open") == 0) {
    clearFault(a); actuatorMove(a, true);
  } else if (strcmp(action, "down") == 0 || strcmp(action, "close") == 0) {
    clearFault(a); actuatorMove(a, false);
  } else if (strcmp(action, "stop") == 0) {
    actuatorStop(a);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) return;
  const char* actuator = doc["actuator"] | "";
  const char* action = doc["action"] | "";
  const char* cmdId = doc["cmd_id"] | "";
  int motorId = doc["motor_id"] | -1;
  if (motorId < 0)  // 旧协议: 按类型路由到该类型第一台
    motorId = firstMotorOfType(strcmp(actuator, "vent") == 0 ? 1 : 0);
  applyCommand(motorId, action);
  // 回执
  JsonDocument ack;
  ack["cmd_id"] = cmdId;
  char buf[128]; size_t n = serializeJson(ack, buf);
  mqtt.publish(topicAck, buf, n);
}

// ----------------------- 本地按钮 (仅 <=2 电机配置, 4 电机引脚被霍尔复用) -----------------------
void scanButtons() {
#if MOTOR_COUNT <= 2
  // 按下=LOW. 点动: 按住则动 (仅在该执行器非故障时); 自动运动靠限位/指令停
  int curtainId = firstMotorOfType(0);
  int ventId = firstMotorOfType(1);
  bool cu = digitalRead(BTN_CURTAIN_UP) == LOW;
  bool cd = digitalRead(BTN_CURTAIN_DOWN) == LOW;
  bool vo = digitalRead(BTN_VENT_OPEN) == LOW;
  bool vc = digitalRead(BTN_VENT_CLOSE) == LOW;
  if (curtainId >= 0 && (cu ^ cd)) applyCommand(curtainId, cu ? "up" : "down");
  if (ventId >= 0 && (vo ^ vc)) applyCommand(ventId, vo ? "open" : "close");
#endif
}

// ----------------------- 遥测上报 -----------------------
void publishTelemetry() {
  JsonDocument doc;
  if (hasSht) {
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();
    if (!isnan(t)) doc["temperature"] = t;
    if (!isnan(h)) doc["humidity"] = h;
  }
  if (hasLux) doc["lux"] = lightMeter.readLightLevel();

  // 每台电机一项 (后端由 motors 数组回填旧版 curtain_*/vent_* 字段)
  JsonArray arr = doc["motors"].to<JsonArray>();
  for (int i = 0; i < MOTOR_COUNT; i++) {
    Actuator &a = motors[i];
    JsonObject m = arr.add<JsonObject>();
    m["id"] = i;
    m["type"] = a.prm.type == 0 ? "curtain" : "vent";
    m["name"] = a.prm.name;
    m["state"] = motionStr(a.state);
    m["current"] = readCurrent(a);
    if (a.prm.pulsesFull > 0)
      m["position"] = constrain(a.position * 100.0f / a.prm.pulsesFull, 0.0f, 100.0f);
    if (a.faultReason[0]) m["fault_reason"] = a.faultReason;
    m["pulses"] = a.seenPulses; // 现场标定全行程脉冲数用
    m["limit_open"] = limitOpenHit(a);
    m["limit_close"] = limitCloseHit(a);
  }

  char buf[1024]; size_t n = serializeJson(doc, buf);
  mqtt.publish(topicTelemetry, buf, n);
}

// 断网兜底: 无 MQTT 连接时, 本地按温湿度/光照自动控制 (按类型作用到每台电机)
void localFallback() {
  if (!hasSht) return;
  float t = sht31.readTemperature();
  float lux = hasLux ? lightMeter.readLightLevel() : NAN;
  if (isnan(t)) return;
  for (int i = 0; i < MOTOR_COUNT; i++) {
    Actuator &a = motors[i];
    if (a.prm.type == 1) {  // 风口
      if (t > LOCAL_VENT_TEMP_HIGH) actuatorMove(a, true);
      else if (t < LOCAL_VENT_TEMP_HIGH - LOCAL_VENT_TEMP_HYST) actuatorMove(a, false);
    } else {                // 卷帘
      if (t < LOCAL_CURTAIN_TEMP_PROTECT) actuatorMove(a, false);  // 低温保温优先
      else if (!isnan(lux)) {
        if (lux > LOCAL_CURTAIN_LUX_OPEN) actuatorMove(a, true);
        else if (lux < LOCAL_CURTAIN_LUX_CLOSE) actuatorMove(a, false);
      }
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

  Wire.begin(I2C_SDA, I2C_SCL);
#ifdef USE_EXPANDER
  if (!mcp.begin_I2C()) SerialMon.println("MCP23017 未检测到!");
  if (!ads.begin()) SerialMon.println("ADS1115 未检测到!");
#endif

  // 初始化电机数组与 IO
  for (int i = 0; i < MOTOR_COUNT; i++) {
    Actuator &a = motors[i];
    a.io = MOTOR_IO[i];
    a.prm = MOTOR_PARAMS[i];
    a.state = IDLE;
    a.seenPulses = 0; a.position = 0;
    a.moveStartMs = a.lastPulseMs = 0;
    a.faultReason = "";
    pulseCount[i] = 0;

#ifdef USE_EXPANDER
    if (a.io.onExpander) {
      mcp.pinMode(a.io.relayOpen, OUTPUT);
      mcp.pinMode(a.io.relayClose, OUTPUT);
      mcp.pinMode(a.io.limitOpen, INPUT_PULLUP);
      mcp.pinMode(a.io.limitClose, INPUT_PULLUP);
    } else
#endif
    {
      pinMode(a.io.relayOpen, OUTPUT);
      pinMode(a.io.relayClose, OUTPUT);
      pinMode(a.io.limitOpen, INPUT_PULLUP);
      pinMode(a.io.limitClose, INPUT_PULLUP);
    }
    relayWrite(a.io, a.io.relayOpen, false);
    relayWrite(a.io, a.io.relayClose, false);

    if (a.io.hall >= 0) {
      pinMode(a.io.hall, INPUT_PULLUP);
      attachInterruptArg(digitalPinToInterrupt(a.io.hall), pulseISR,
                         (void*)&pulseCount[i], FALLING);
    }
  }

#if MOTOR_COUNT <= 2
  uint8_t btns[] = { BTN_CURTAIN_UP, BTN_CURTAIN_DOWN, BTN_VENT_OPEN, BTN_VENT_CLOSE };
  for (uint8_t p : btns) pinMode(p, INPUT_PULLUP);
#endif

  hasSht = sht31.begin(0x44);
  hasLux = lightMeter.begin();

  snprintf(topicTelemetry, sizeof(topicTelemetry), "%s/%s/telemetry", TOPIC_PREFIX, DEVICE_ID);
  snprintf(topicStatus, sizeof(topicStatus), "%s/%s/status", TOPIC_PREFIX, DEVICE_ID);
  snprintf(topicCmd, sizeof(topicCmd), "%s/%s/cmd", TOPIC_PREFIX, DEVICE_ID);
  snprintf(topicAck, sizeof(topicAck), "%s/%s/cmd/ack", TOPIC_PREFIX, DEVICE_ID);

  setupModem();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(1024);
  mqttConnect();
}

void loop() {
  // 安全监控始终最高优先级
  for (int i = 0; i < MOTOR_COUNT; i++) actuatorSafety(motors[i], pulseCount[i]);
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
