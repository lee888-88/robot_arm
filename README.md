# 5-Axis Robotic Arm Controller (STM32F103C8T6)

[![MCU](https://img.shields.io/badge/MCU-STM32F103C8T6-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8.html)
[![HAL](https://img.shields.io/badge/HAL-STM32Cube_F1_v1.8.7-green)](https://github.com/STMicroelectronics/STM32CubeF1)
[![Build](https://img.shields.io/badge/build-CMake%20%2B%20Ninja-orange)](#build--flash)
[![Compiler](https://img.shields.io/badge/compiler-arm--none--eabi--gcc-lightgrey)](#build--flash)

A bare-metal STM32F103C8T6 firmware that drives a **5-axis robotic arm** (4 joints + gripper) using 4 potentiometers for manual control and a push button for gripper toggle. Real-time status is displayed on a 128×64 SSD1306 OLED via software I2C.

---

## Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Pinout & Wiring](#pinout--wiring)
- [System Architecture](#system-architecture)
- [Build & Flash](#build--flash)
- [Usage](#usage)
- [Planned Enhancements](#planned-enhancements)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)

---

## Features

- **5 servos** driven by hardware PWM (TIM1 4-channel + TIM3 1-channel) at 50 Hz, 1 μs resolution
- **4 potentiometers** sampled by ADC1 via DMA circular scan mode — zero CPU polling overhead
- **Adaptive EMA filter** on ADC readings: fast response on large changes, heavy smoothing on steady signals
- **Gripper button** on PA4 (internal pull-up), press to close, release to open
- **SSD1306 128×64 OLED** status display via software I2C (PB8/PB9), showing raw ADC, voltage, and computed servo angle
- **Direct CCR register writes** via `__HAL_TIM_SET_COMPARE()` — bypasses HAL overhead for low-latency servo updates
- **CubeMX-compatible**: all custom code lives inside `USER CODE BEGIN` / `USER CODE END` guards

---

## Hardware Requirements

| Component | Qty | Notes |
|---|---|---|
| STM32F103C8T6 (Blue Pill / custom) | 1 | 64 KB Flash, 20 KB SRAM |
| SG90 / MG996R servos (5 V) | 5 | Or any servo using 500–2500 μs pulse |
| 10 kΩ potentiometers | 4 | Linear taper, wired as voltage dividers |
| SSD1306 128×64 OLED (I2C) | 1 | Address 0x3C |
| Push button + 10 kΩ pull-up resistor (optional) | 1 | Internal pull-up on PA4 can be used instead |
| External 5–6 V power supply for servos | 1 | **Do not power servos from STM32's 3.3 V regulator!** |
| USB-TTL serial adapter (optional) | 1 | For debugging via USART3 |

---

## Pinout & Wiring

### Servo Connections

| Servo | Timer | Channel | Pin | Control Macro |
|---|---|---|---|---|
| Base | TIM1 | CH1 | PA8 | `SERVO_BASE(pulse)` |
| Shoulder | TIM1 | CH2 | PA9 | `SERVO_SHOULDER(pulse)` |
| Elbow | TIM1 | CH3 | PA10 | `SERVO_ELBOW(pulse)` |
| Wrist | TIM1 | CH4 | PA11 | `SERVO_WRIST(pulse)` |
| Hand / Gripper | TIM3 | CH1 | PA6 | `SERVO_HAND(pulse)` |

### Sensor & Peripheral Connections

| Function | Pin | Mode | Details |
|---|---|---|---|
| Potentiometer – Base | PA0 | ADC1_IN0 | 3.3 V — Pot — GND, wiper to PA0 |
| Potentiometer – Shoulder | PA1 | ADC1_IN1 | 3.3 V — Pot — GND, wiper to PA1 |
| Potentiometer – Elbow | PA2 | ADC1_IN2 | 3.3 V — Pot — GND, wiper to PA2 |
| Potentiometer – Wrist | PA3 | ADC1_IN3 | 3.3 V — Pot — GND, wiper to PA3 |
| Gripper Button | PA4 | GPIO Input, Pull-up | Button to GND |
| OLED SCL | PB8 | GPIO Output (software I2C) | SSD1306 SCL |
| OLED SDA | PB9 | GPIO Output/Input | SSD1306 SDA (0x3C) |
| USART3 TX | PB10 | AF Push-Pull | USB-TTL RX (115200 baud, 8N1) |
| USART3 RX | PB11 | GPIO Input | USB-TTL TX |
| SWDIO / SWCLK | PA13 / PA14 | Debug (reserved) | Do not repurpose |

### Power Wiring (Critical)

```
Servo Power Supply (5 V – 6 V)
 ├─ VCC (+)  →  All servo red wires
 └─ GND (-)  →  All servo brown wires
               →  STM32 GND ⬅ must share common ground!

STM32
 └─ Powered via USB or external 3.3 V

Potentiometers
 ├─ Pin 1 → 3.3 V
 ├─ Pin 2 (wiper) → PA0 / PA1 / PA2 / PA3
 └─ Pin 3 → GND
```

> **⚠️ Never power servos from the STM32's 3.3 V pin.** Servos can draw >1 A at stall, which will burn the onboard regulator or cause brown-out resets. Always use a separate 5 V supply and tie the grounds together.

---

## System Architecture

```
[4× Potentiometers] ──analog──> PA0–PA3 ──> ADC1 ──DMA1_CH1──> adc_values[4]
                                                                      │
                                                          Adaptive EMA Filter
                                                                      │
                                                             map_range() + pulse_to_angle()
                                                                      │
              ┌────────────────────┬────────────────┬──────────────────┤
              │                    │                │                  │
   SERVO_BASE ──TIM1_CH1──> PA8   SERVO_SHOULDER   SERVO_ELBOW    SERVO_WRIST
              │                    │                │                  │
         TIM1_CH2──> PA9      TIM1_CH3──> PA10  TIM1_CH4──> PA11  TIM3_CH1──> PA6
                                                                       │
                                                              [Hand/Gripper]

[Gripper Button] ──PA4──> digitalRead ──> HAND open / close
[SSD1306 OLED]   ──PB8/PB9──> software I2C ──> real-time status display
```

### Clock Tree

```
HSE 8 MHz → PLL ×9 → SYSCLK 72 MHz
 ├─ AHB  /1  → HCLK  = 72 MHz
 ├─ APB1 /2  → PCLK1 = 36 MHz  →  TIM3 clock = 72 MHz (APB1 ×2 rule)
 ├─ APB2 /1  → PCLK2 = 72 MHz  →  TIM1 clock = 72 MHz
 └─ ADC prescaler /6 → ADCCLK = 12 MHz
```

### Servo Pulse Mapping

| Pulse Width (μs) | CCR Value | Servo Angle |
|---|---|---|
| 650 | 650 | 0° |
| 1500 | 1500 | 90° (mid) |
| 2350 | 2350 | 180° |

**Mapping direction is reversed**: High ADC reading (potentiometer at one extreme) → short pulse → 0°, low ADC reading → long pulse → 180°. This matches a physical potentiometer panel layout.

Gripper positions: `HAND_OPEN = 439 μs`, `HAND_CLOSE = 879 μs`.

---

## Build & Flash

### Prerequisites

- **Toolchain**: `arm-none-eabi-gcc` (ARM GNU Toolchain)
- **Build system**: CMake ≥ 3.22 + Ninja
- **Flash tool**: OpenOCD or STM32CubeProgrammer
- **Debug probe**: ST-Link V2 (SWD on PA13/PA14)

### Build

```bash
# 1. Configure
cmake --preset Debug

# 2. Build
cmake --build build/Debug
```

Build artifacts are in `build/Debug/`: `dianweitest.elf`, `dianweitest.hex`, `dianweitest.map`.

### Flash

```bash
# Using OpenOCD + ST-Link
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/Debug/dianweitest.elf verify reset exit"
```

### IDE Support

A `.clangd` file is committed and points to `build/Debug/compile_commands.json` for IntelliSense. The `.ioc` file can be opened with STM32CubeMX ≥ 6.17 to regenerate peripheral init code.

---

## Usage

1. **Power up** the STM32 and the external servo power supply.
2. The OLED will initialize and show real-time potentiometer readings.
3. **Turn the 4 potentiometers** to control Base, Shoulder, Elbow, and Wrist servos in real time.
4. **Press the button** (PA4) to close the gripper; release to open it.
5. (Optional) Connect a USB-TTL serial adapter to PB10/PB11 to view debug output at 115200 baud.

### OLED Display Layout

```
Pot1: 2048  1.65V  90   ← raw ADC, voltage, computed angle
Pot2: 1024  0.82V  45
Pot3: 3072  2.47V 135
Pot4: 1500  1.21V  90
SW: OPEN     Hand:OPN   ← button state + gripper state
```

> **Note**: Voltage is computed with fixed-point integer arithmetic (`raw × 3300 / 4096`) to avoid `%f` in newlib-nano, which would pull in heap allocation.

---

## Planned Enhancements

- [ ] Inverse kinematics solver for coordinate-based positioning
- [ ] PID controller with encoder feedback for precise angle control
- [ ] UART command protocol for PC / ROS integration
- [ ] Store and replay motion sequences
- [ ] RTOS-based multitasking

---

## Project Structure

```
dianweitest/
├── CMakeLists.txt              # Top-level CMake config
├── CMakePresets.json           # Build presets (Debug)
├── dianweitest.ioc             # STM32CubeMX project file
├── startup_stm32f103xb.s       # Startup assembly
├── STM32F103XX_FLASH.ld        # Linker script
├── cmake/
│   └── stm32cubemx/            # CubeMX-generated CMake helpers
├── Core/
│   ├── Inc/
│   │   ├── main.h              # Servo macros, extern declarations
│   │   ├── soft_i2c.h          # Software I2C header
│   │   └── ssd1306.h           # OLED driver header
│   └── Src/
│       ├── main.c              # Entry point + main loop
│       ├── soft_i2c.c          # Software I2C (DWT-based μs delays)
│       └── ssd1306.c           # SSD1306 128×64 driver
└── Drivers/                    # STM32CubeF1 HAL library
```

> **CubeMX code generation rule**: Custom code **must** stay inside `USER CODE BEGIN` / `USER CODE END` markers. Everything outside those markers is overwritten when regenerating from the `.ioc` file.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| Servos not moving | No external power | Connect a separate 5 V supply to servos |
| Servos jitter / brown-out | Powering servos from STM32 3.3 V | Use external 5 V; tie grounds together |
| Servos move erratically | Missing common ground | Connect servo PSU GND ↔ STM32 GND |
| OLED blank | I2C wiring or address | Check PB8→SCL, PB9→SDA; address 0x3C |
| ADC values stuck at 0 | Pins not in analog mode | Verify GPIO init sets `GPIO_MODE_ANALOG` for PA0–PA3 |
| ADC values stuck at 4095 after flashing | ADC calibration missing | Ensure `HAL_ADCEx_Calibration_Start()` runs before `HAL_ADC_Start_DMA()` |
| Build fails with "TIM3 clock = 36 MHz" | APB1 ×2 rule not applied | TIM3 gets 72 MHz because PCLK1=36 MHz → ×2 |
| `printf` produces nothing | No MicroLIB | Use fixed-point math on OLED instead; or enable MicroLIB in linker |
| CubeMX overwrites custom code | Code outside `USER CODE` guards | Move all custom code inside `BEGIN`/`END` markers |

---

## References

- [STM32F103C8T6 Datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [STM32CubeF1 HAL Documentation](https://www.st.com/en/embedded-software/stm32cubef1.html)
- [ARM GCC Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
- [OpenOCD](https://openocd.org/)

---

*Based on an Arduino reference design — ported from Adafruit PWM driver + `analogRead`/`map()` to bare-metal STM32 HAL with DMA-driven ADC and hardware PWM.*
