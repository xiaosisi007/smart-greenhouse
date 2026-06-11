#pragma once
// GPIO 引脚定义。按你的接线修改。

// ---- SIM7600 4G 模块 (UART2) ----
#define MODEM_TX 27
#define MODEM_RX 26
#define MODEM_PWRKEY 4   // 模块开机脚 (部分模块需要)

// ---- I2C 传感器 (SHT31 温湿度 + BH1750 光照) ----
#define I2C_SDA 21
#define I2C_SCL 22

// ---- 继电器输出 (控制接触器线圈, 低电平触发的模组请置 RELAY_ACTIVE_LOW=1) ----
#define RELAY_ACTIVE_LOW 1
#define RELAY_CURTAIN_UP   16  // 卷帘 卷起(正转)
#define RELAY_CURTAIN_DOWN 17  // 卷帘 放下(反转)
#define RELAY_VENT_OPEN    18  // 风口 开
#define RELAY_VENT_CLOSE   19  // 风口 关

// ---- 限位开关输入 (到位触发, 内部上拉, 触发为 LOW) ----
#define LIMIT_CURTAIN_TOP    32 // 卷帘到顶
#define LIMIT_CURTAIN_BOTTOM 33 // 卷帘到底
#define LIMIT_VENT_OPEN      34 // 风口全开 (34/35 仅输入)
#define LIMIT_VENT_CLOSED    35 // 风口全关

// ---- 运动脉冲反馈 (卷轴霍尔/接近开关, 每转 N 个脉冲, 内部上拉, 下降沿计数) ----
#define HALL_CURTAIN 5
#define HALL_VENT    15

// ---- 电流检测 (ACS712 模拟输入) ----
#define CURRENT_CURTAIN_ADC 36 // VP
#define CURRENT_VENT_ADC    39 // VN

// ---- 本地手动按钮 (内部上拉, 按下为 LOW) ----
#define BTN_CURTAIN_UP   13
#define BTN_CURTAIN_DOWN 14
#define BTN_VENT_OPEN    25
#define BTN_VENT_CLOSE   23
