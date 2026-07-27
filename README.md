# 五轴机械臂控制器 (STM32F103C8T6)

[![MCU](https://img.shields.io/badge/MCU-STM32F103C8T6-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8.html)
[![HAL](https://img.shields.io/badge/HAL-STM32Cube_F1_v1.8.7-green)](https://github.com/STMicroelectronics/STM32CubeF1)
[![Build](https://img.shields.io/badge/build-CMake%20%2B%20Ninja-orange)](#编译与烧录)
[![Compiler](https://img.shields.io/badge/compiler-arm--none--eabi--gcc-lightgrey)](#编译与烧录)

基于 STM32F103C8T6 的**五轴机械臂**裸机固件。4 个电位器分别控制底座、肩部、肘部、腕部舵机，按键控制夹爪开合，实时状态通过 SSD1306 OLED 显示。

---

## 目录

- [特性](#特性)
- [硬件需求](#硬件需求)
- [引脚分配与接线](#引脚分配与接线)
- [系统架构](#系统架构)
- [编译与烧录](#编译与烧录)
- [使用方法](#使用方法)
- [后续规划](#后续规划)
- [项目结构](#项目结构)
- [常见问题](#常见问题)

---

## 特性

- **5 路舵机** 由硬件 PWM 驱动 (TIM1 四通道 + TIM3 单通道)，50 Hz，1 μs 分辨率
- **4 通道 ADC** 通过 DMA 循环扫描模式采集电位器，零 CPU 轮询开销
- **自适应 EMA 滤波**：大变化时快速响应，稳定时重度平滑，消除电位器抖动
- **夹爪按键** 接 PA4（内部上拉），按下闭合、松开张开
- **SSD1306 128×64 OLED** 通过软件 I2C (PB8/PB9) 实时显示 ADC 原始值、电压、舵机角度
- **直接写 CCR 寄存器** (`__HAL_TIM_SET_COMPARE()`)，绕过 HAL 开销，实现低延迟舵机更新
- **兼容 CubeMX**：自定义代码全部写在 `USER CODE BEGIN` / `USER CODE END` 保护区内

---

## 硬件需求

| 元件 | 数量 | 备注 |
|---|---|---|
| STM32F103C8T6 (Blue Pill / 最小系统板) | 1 | 64 KB Flash，20 KB SRAM |
| SG90 / MG996R 舵机 (5 V) | 5 | 或任何 500–2500 μs 脉宽的舵机 |
| 10 kΩ 电位器 | 4 | 线性型，接成分压电路 |
| SSD1306 128×64 OLED (I2C) | 1 | 地址 0x3C |
| 轻触按键 | 1 | 使用 PA4 内部上拉，可不外接电阻 |
| 外部 5–6 V 电源（舵机供电） | 1 | **绝不能从 STM32 3.3 V 引脚给舵机供电！** |
| USB 转 TTL 串口模块（可选） | 1 | 通过 USART3 输出调试信息 |

---

## 引脚分配与接线

### 舵机连接

| 舵机 | 定时器 | 通道 | 引脚 | 控制宏 |
|---|---|---|---|---|
| 底座 | TIM1 | CH1 | PA8 | `SERVO_BASE(pulse)` |
| 肩部 | TIM1 | CH2 | PA9 | `SERVO_SHOULDER(pulse)` |
| 肘部 | TIM1 | CH3 | PA10 | `SERVO_ELBOW(pulse)` |
| 腕部 | TIM1 | CH4 | PA11 | `SERVO_WRIST(pulse)` |
| 夹爪 | TIM3 | CH1 | PA6 | `SERVO_HAND(pulse)` |

### 传感器与外设连接

| 功能 | 引脚 | 模式 | 说明 |
|---|---|---|---|
| 电位器 — 底座 | PA0 | ADC1_IN0 | 3.3 V — 电位器 — GND，中间脚接 PA0 |
| 电位器 — 肩部 | PA1 | ADC1_IN1 | 3.3 V — 电位器 — GND，中间脚接 PA1 |
| 电位器 — 肘部 | PA2 | ADC1_IN2 | 3.3 V — 电位器 — GND，中间脚接 PA2 |
| 电位器 — 腕部 | PA3 | ADC1_IN3 | 3.3 V — 电位器 — GND，中间脚接 PA3 |
| 夹爪按键 | PA4 | GPIO 输入，上拉 | 按键另一端接地 |
| OLED SCL | PB8 | GPIO 输出（软 I2C） | 接 SSD1306 SCL |
| OLED SDA | PB9 | GPIO 输出/输入 | 接 SSD1306 SDA (地址 0x3C) |
| USART3 TX | PB10 | 复用推挽输出 | 接 USB-TTL RX (115200 波特率, 8N1) |
| USART3 RX | PB11 | GPIO 输入 | 接 USB-TTL TX |
| SWDIO / SWCLK | PA13 / PA14 | 调试口（保留） | 不要用作其他功能 |

### 电源接线

```
舵机独立电源 (5 V – 6 V)
 ├─ 正极 → 所有舵机红线 (VCC)
 └─ 负极 → 所有舵机棕线 (GND)
          → STM32 GND  ⬅ 必须共地！

STM32
 └─ 通过 USB 或外部 3.3 V 供电

电位器接线
 ├─ 脚 1 → 3.3 V
 ├─ 脚 2 (中间脚) → PA0 / PA1 / PA2 / PA3
 └─ 脚 3 → GND
```

> **⚠️ 舵机绝不能从 STM32 的 3.3 V 引脚取电。** 舵机堵转电流可能超过 1 A，会烧毁板载稳压器或导致芯片反复复位。务必使用独立 5 V 电源，并将两地线连接在一起。

### 引脚占用全图

```
PA0  — ADC_IN0     [电位器-底座]
PA1  — ADC_IN1     [电位器-肩部]
PA2  — ADC_IN2     [电位器-肘部]
PA3  — ADC_IN3     [电位器-腕部]
PA4  — GPIO 输入   [夹爪按键, 上拉]
PA5  — 空闲
PA6  — TIM3_CH1    [舵机-夹爪]
PA7  — 空闲
PA8  — TIM1_CH1    [舵机-底座]
PA9  — TIM1_CH2    [舵机-肩部]
PA10 — TIM1_CH3    [舵机-肘部]
PA11 — TIM1_CH4    [舵机-腕部]
PA12 — 空闲
PA13 — SWDIO       [调试口·保留]
PA14 — SWCLK       [调试口·保留]
PA15 — 空闲
PB0–PB7 — 空闲
PB8–PB9 — 软 I2C  [OLED]
PB10 — USART3_TX   [串口调试]
PB11 — USART3_RX   [串口调试]
PB12–PB15 — 空闲
PC13–PC15 — 空闲
```

---

## 系统架构

```
[4× 电位器] ──模拟──> PA0–PA3 ──> ADC1 ──DMA1_CH1──> adc_values[4]
                                                                  │
                                                      自适应 EMA 滤波
                                                                  │
                                                      map_range() + pulse_to_angle()
                                                                  │
         ┌─────────────────┬─────────────────┬──────────────────────┤
         │                 │                 │                      │
SERVO_BASE ──TIM1_CH1──>PA8  SERVO_SHOULDER   SERVO_ELBOW     SERVO_WRIST
         │                 │                 │                      │
    TIM1_CH2──>PA9    TIM1_CH3──>PA10    TIM1_CH4──>PA11    TIM3_CH1──>PA6
                                                                  │
                                                          [夹爪/手爪]

[夹爪按键] ──PA4──> 数字读取 ──> HAND 开/关
[SSD1306 OLED] ──PB8/PB9──> 软 I2C ──> 实时状态显示
```

### 时钟树

```
HSE 8 MHz → PLL ×9 → SYSCLK 72 MHz
 ├─ AHB  /1  → HCLK  = 72 MHz
 ├─ APB1 /2  → PCLK1 = 36 MHz  →  TIM3 时钟 = 72 MHz (APB1 ×2 规则)
 ├─ APB2 /1  → PCLK2 = 72 MHz  →  TIM1 时钟 = 72 MHz
 └─ ADC 预分频 /6 → ADCCLK = 12 MHz
```

> **关键提醒**：APB1 预分频为 /2 时，挂载在 APB1 上的定时器（如 TIM3）时钟会翻倍，即 PCLK1 × 2 = 72 MHz，与 TIM1 相同。

### 舵机脉宽映射

| 脉宽 (μs) | CCR 值 | 舵机角度 |
|---|---|---|
| 650 | 650 | 0° |
| 1500 | 1500 | 90° (中位) |
| 2350 | 2350 | 180° |

**映射方向为反向**：ADC 读值高（电位器一端极限）→ 脉宽短 → 角度 0°；ADC 读值低 → 脉宽长 → 角度 180°。这符合常见的电位器控制面板布局。

实测电位器范围约 600–3500（12 位 ADC），映射公式：

```
pulse = 2350 - (adc - 600) × 1700 / 2900
```

夹爪位置：`HAND_OPEN = 439 μs`，`HAND_CLOSE = 879 μs`。

---

## 编译与烧录

### 环境准备

- **工具链**：`arm-none-eabi-gcc` (ARM GNU Toolchain)
- **构建系统**：CMake ≥ 3.22 + Ninja
- **烧录工具**：OpenOCD 或 STM32CubeProgrammer
- **调试器**：ST-Link V2 (通过 PA13/PA14 SWD 接口)

### 编译

```bash
# 1. 配置项目（必须使用 preset）
cmake --preset Debug

# 2. 编译
cmake --build build/Debug
```

编译产物在 `build/Debug/` 目录下：`dianweitest.elf`、`dianweitest.hex`、`dianweitest.map`。

### 烧录

```bash
# 使用 OpenOCD + ST-Link
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/Debug/dianweitest.elf verify reset exit"
```

### IDE 支持

项目已包含 `.clangd` 配置文件，指向 `build/Debug/compile_commands.json`，可直接用于 VSCode + clangd 的代码补全和跳转。`.ioc` 文件可用 STM32CubeMX ≥ 6.17 打开以重新生成外设初始化代码。

---

## 使用方法

1. **上电**：先给 STM32 上电，再给舵机独立电源上电。
2. OLED 初始化后显示实时电位器读数。
3. **旋转 4 个电位器** 分别实时控制底座、肩部、肘部、腕部舵机。
4. **按下按键** (PA4) 闭合夹爪；松开按键则张开夹爪。
5. （可选）用 USB-TTL 串口模块连接 PB10/PB11，可在串口助手查看调试输出（115200 波特率）。

### OLED 显示界面

```
Pot1: 2048  1.65V  90   ← 原始 ADC 值, 电压, 计算角度
Pot2: 1024  0.82V  45
Pot3: 3072  2.47V 135
Pot4: 1500  1.21V  90
SW: OPEN     Hand:OPN   ← 按键状态 + 夹爪状态
```

> 电压计算采用定点整数运算 (`raw × 3300 / 4096`)，避免 newlib-nano 的 `%f` 格式化引发堆分配问题。

---

## 后续规划

- [ ] 逆运动学解算，支持坐标定位
- [ ] 编码器反馈 + PID 闭环角度控制
- [ ] UART 指令协议，对接上位机 / ROS
- [ ] 动作录制与回放
- [ ] RTOS 多任务改造

---

## 项目结构

```
dianweitest/
├── CMakeLists.txt              # 顶层 CMake 配置
├── CMakePresets.json           # 构建预设 (Debug)
├── dianweitest.ioc             # STM32CubeMX 项目文件
├── startup_stm32f103xb.s       # 启动汇编文件
├── STM32F103XX_FLASH.ld        # 链接脚本
├── cmake/
│   └── stm32cubemx/            # CubeMX 生成的 CMake 辅助脚本
├── Core/
│   ├── Inc/
│   │   ├── main.h              # 舵机宏定义、全局 extern 声明
│   │   ├── soft_i2c.h          # 软件 I2C 头文件
│   │   └── ssd1306.h           # OLED 驱动头文件
│   └── Src/
│       ├── main.c              # 主程序入口 + 主循环
│       ├── soft_i2c.c          # 软件 I2C（基于 DWT 微秒延时）
│       └── ssd1306.c           # SSD1306 128×64 驱动
└── Drivers/                    # STM32CubeF1 HAL 库
```

> **CubeMX 代码生成规则**：自定义代码**必须**写在 `USER CODE BEGIN` / `USER CODE END` 标记之间。标记之外的代码在重新生成 `.ioc` 时会被覆盖。

---

## 常见问题

| 现象 | 可能原因 | 解决方法 |
|---|---|---|
| 舵机不转 | 舵机没有外部供电 | 给舵机接入独立 5 V 电源 |
| 舵机抖动 / 芯片复位 | 从 STM32 3.3 V 给舵机供电 | 改用外部 5 V 供电，两地共地 |
| 舵机动作异常 | 未共地 | 将舵机电源 GND ↔ STM32 GND 连通 |
| OLED 无显示 | I2C 接线错误或地址不对 | 检查 PB8→SCL, PB9→SDA；地址为 0x3C |
| ADC 值始终为 0 | 引脚未设为模拟模式 | 确认 GPIO 初始化中 PA0–PA3 为 `GPIO_MODE_ANALOG` |
| 烧录后 ADC 值卡在 4095 | 缺少 ADC 校准 | 确保 `HAL_ADCEx_Calibration_Start()` 在 `HAL_ADC_Start_DMA()` 之前执行 |
| printf 无输出 | 未启用 MicroLIB | OLED 使用定点运算替代；或链接器中启用 MicroLIB |
| CubeMX 重新生成后代码丢失 | 代码写在 USER CODE 之外 | 将所有自定义代码移入 `BEGIN`/`END` 保护区内 |

---

## 参考文档

- [STM32F103C8T6 数据手册](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [STM32CubeF1 HAL 文档](https://www.st.com/en/embedded-software/stm32cubef1.html)
- [ARM GCC 工具链](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
- [OpenOCD](https://openocd.org/)

---

*本项目基于 Arduino 参考设计，从 Adafruit PWM 驱动 + `analogRead`/`map()` 移植至 STM32 HAL 裸机平台，采用 DMA 驱动 ADC 采集与硬件 PWM 输出。*
