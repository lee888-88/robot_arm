# STM32F103C8T6 机械臂控制 — HAL 库代码编写指导

> **芯片**: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM)
> **约束**: PB3~PB9 已被占用，其余引脚可用
> **功能**: 5路 PWM 舵机控制 + 4路 ADC 电位器读取 + 按键夹爪 + 串口调试

---

## 一、硬件连接总览

### 1.1 引脚分配表

| 功能 | 引脚 | 复用功能 | 接法 |
|------|:---:|---------|------|
| 舵机-底座 (base) | **PA8** | TIM1_CH1 | → 舵机信号线 |
| 舵机-肩部 (shoulder) | **PA9** | TIM1_CH2 | → 舵机信号线 |
| 舵机-肘部 (elbow) | **PA10** | TIM1_CH3 | → 舵机信号线 |
| 舵机-腕部 (wrist) | **PA11** | TIM1_CH4 | → 舵机信号线 |
| 舵机-夹爪 (hand) | **PA6** | TIM3_CH1 | → 舵机信号线 |
| 电位器-底座 | **PA0** | ADC12_IN0 | → 电位器中间脚 |
| 电位器-肩部 | **PA1** | ADC12_IN1 | → 电位器中间脚 |
| 电位器-肘部 | **PA2** | ADC12_IN2 | → 电位器中间脚 |
| 电位器-腕部 | **PA3** | ADC12_IN3 | → 电位器中间脚 |
| 夹爪按键 | **PA4** | GPIO 上拉输入 | → 按键(一端接地) |
| 串口 TX | **PB10** | USART3_TX | → USB转串口 RX |
| 串口 RX | **PB11** | USART3_RX | → USB转串口 TX |

### 1.2 电源接线

```
舵机电源: 外部 5V~6V 独立供电（不要从 STM32 3.3V 取电！）
         ├─ 正极 → 舵机 VCC (红)
         └─ 负极 → 舵机 GND (棕) + STM32 GND（必须共地！）

STM32:   3.3V 供电（USB 或外部）
电位器:  PA0~PA3 接中间脚，两端分别接 3.3V 和 GND
```

> **⚠️ 致命错误**: 舵机绝不能从 STM32 3.3V 引脚取电，会烧毁稳压器或导致芯片复位！

### 1.3 引脚占用全图

```
PA0  — ADC_IN0     [电位器-底座]
PA1  — ADC_IN1     [电位器-肩部]
PA2  — ADC_IN2     [电位器-肘部]
PA3  — ADC_IN3     [电位器-腕部]
PA4  — GPIO输入    [夹爪按键, 上拉]
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
PB0  — 空闲
PB1  — 空闲
PB2  — 空闲
PB3  |
PB4  |
PB5  |
PB6  |─ [已占用]
PB7  |
PB8  |
PB9  |
PB10 — USART3_TX   [串口调试]
PB11 — USART3_RX   [串口调试]
PB12~PB15 — 空闲
PC13~PC15 — 空闲
```

---

## 二、系统时钟配置 (72MHz)

```
HSE 8MHz → PLL ×9 → SYSCLK 72MHz
├─ AHB 预分频 /1  → HCLK  = 72MHz
├─ APB1 预分频 /2 → PCLK1 = 36MHz → TIM3时钟 = 72MHz (×2规则)
└─ APB2 预分频 /1 → PCLK2 = 72MHz → TIM1时钟 = 72MHz
                                    → ADC时钟 = 72/6 = 12MHz
```

```c
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // 1. 使能 HSE，配置 PLL
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;   // 8MHz × 9 = 72MHz
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    // 2. 配置系统时钟、AHB、APB1、APB2
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;    // HCLK  = 72MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;     // PCLK1 = 36MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     // PCLK2 = 72MHz
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

    // 3. ADC 时钟分频（必须在时钟配置之后）
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;  // 72/6 = 12MHz ✓
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}
```

> **关键**: APB1=/2 时，TIM3 时钟 = PCLK1×2 = **72MHz**，与 TIM1 相同。

---

## 三、PWM 舵机控制

### 3.1 舵机 PWM 参数计算

```
舵机控制信号: 50Hz (周期 20ms)，脉冲宽度 0.5ms ~ 2.5ms

定时器配置 (72MHz时钟源):
  PSC = 72 - 1 = 71          → 计数器时钟 = 72MHz / 72 = 1MHz (1μs/格)
  ARR = 20000 - 1 = 19999    → PWM频率 = 1MHz / 20000 = 50Hz ✓

脉冲转换:
  0.5ms  =  500 个计数值 → 舵机 0°
  1.0ms  = 1000 个计数值 → 舵机 45°
  1.5ms  = 1500 个计数值 → 舵机 90°
  2.0ms  = 2000 个计数值 → 舵机 135°
  2.5ms  = 2500 个计数值 → 舵机 180°

操作范围 (SG90 舵机):
  最小值 650  (约 0.65ms) → 0°
  最大值 2350 (约 2.35ms) → 180°
  
  ADC → 舵机角度映射 (实测电位器范围 600~3500, 反方向):
  ADC=600  → 2350μs → 180° (电位器一端极限)
  ADC=2050 → 1500μs → 90°  (中点)
  ADC=3500 → 650μs  → 0°   (电位器另一端极限)
  
  映射公式: pulse = map_range(adc, 600, 3500, 2350, 650)
             = 2350 - (adc - 600) × 1700 / 2900
```

### 3.2 TIM1 四通道 PWM — 底座/肩部/肘部/腕部

TIM1 是高级定时器，挂载 APB2，时钟 72MHz。

```c
#include "stm32f1xx_hal.h"

TIM_HandleTypeDef htim1;

// ==================== TIM1 GPIO 初始化 ====================
static void MX_GPIO_TIM1_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    // PA8=TIM1_CH1, PA9=TIM1_CH2, PA10=TIM1_CH3, PA11=TIM1_CH4
    // 全部配置为复用推挽输出
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// ==================== TIM1 PWM 初始化 ====================
static void MX_TIM1_PWM_Init(void) {
    TIM_OC_InitTypeDef sConfigOC = {0};

    __HAL_RCC_TIM1_CLK_ENABLE();

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 72 - 1;                    // 72MHz / 72 = 1MHz
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 20000 - 1;                    // 1MHz / 20000 = 50Hz
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim1);

    // 通用 PWM 通道配置 (CH1~CH4 一样)
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 1500;                           // 初始脉宽 1.5ms (90° 中位)
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    // CH1 — PA8 — 底座
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);
    // CH2 — PA9 — 肩部
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2);
    // CH3 — PA10 — 肘部
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3);
    // CH4 — PA11 — 腕部
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4);
}

// ==================== 启动 TIM1 四路 PWM ====================
static void TIM1_PWM_All_Start(void) {
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);  // 底座
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);  // 肩部
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);  // 肘部
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);  // 腕部
}
```

> **⚠️ TIM1 陷阱**: TIM1 是高级定时器，它**不需要**像互补输出那样调用 `HAL_TIMEx_PWMN_Start()`。普通 CH1~CH4 用 `HAL_TIM_PWM_Start()` 即可。但如果需要刹车功能，注意 PA6 默认是 TIM1_BKIN，这里 PA6 用作 TIM3_CH1，两者不冲突。

### 3.3 TIM3 单通道 PWM — 夹爪

TIM3 是通用定时器，挂载 APB1。因 APB1=/2 (PCLK1=36MHz)，定时器时钟 = 36×2 = **72MHz**。

```c
TIM_HandleTypeDef htim3;

// ==================== TIM3 GPIO 初始化 ====================
static void MX_GPIO_TIM3_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    // PA6 = TIM3_CH1，复用推挽输出
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// ==================== TIM3 PWM 初始化 ====================
static void MX_TIM3_PWM_Init(void) {
    TIM_OC_InitTypeDef sConfigOC = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 72 - 1;                    // 72MHz / 72 = 1MHz
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 20000 - 1;                    // 1MHz / 20000 = 50Hz
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim3);

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 1500;                           // 初始脉宽 1.5ms
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1);
}
```

### 3.4 舵机角度宏定义（方便调参）

```c
// ===== 舵机脉宽范围（μs 对应计数值，1计数值=1μs）=====
#define SERVO_MIN_PULSE     650     // 0° 位置
#define SERVO_MAX_PULSE     2350    // 180° 位置
#define SERVO_MID_PULSE     1500    // 90° 中位

// ===== 夹爪位置 =====
#define HAND_OPEN_PULSE     439     // 夹爪张开 (对应原代码 pwm=90)
#define HAND_CLOSE_PULSE    879    // 夹爪闭合 (对应原代码 pwm=180)

// ===== 快捷宏：设置舵机角度（直接写 CCR 寄存器）=====
#define SERVO_BASE(pluse)       __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (pluse))
#define SERVO_SHOULDER(pluse)   __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (pluse))
#define SERVO_ELBOW(pluse)      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (pluse))
#define SERVO_WRIST(pluse)      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, (pluse))
#define SERVO_HAND(pluse)       __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, (pluse))
```

> `__HAL_TIM_SET_COMPARE()` 是宏，直接写 CCR 寄存器，比调用 HAL 函数快得多，适合在主循环中高频更新。

---

## 四、ADC 电位器读取（4通道 DMA 扫描）

### 4.1 方案选择

4 个电位器需要 4 路 ADC，选择 **ADC1 DMA 扫描模式**：
- 单次触发 → DMA 搬运 → 循环更新
- 无需 CPU 干预，主循环直接读 `adc_values[]` 数组

```c
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

// DMA 目标数组（4通道 × 4字节对齐）
volatile uint32_t adc_values[4];  // [0]=PA0底座, [1]=PA1肩部, [2]=PA2肘部, [3]=PA3腕部

// ==================== ADC GPIO 初始化 ====================
static void MX_GPIO_ADC_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    // PA0~PA3 模拟输入模式（不是普通 INPUT！）
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// ==================== ADC1 DMA 初始化 ====================
static void MX_ADC1_DMA_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};

    // --- DMA 配置 ---
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_adc1.Instance = DMA1_Channel1;               // ADC1 固定用 DMA1 通道1
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;      // 外设地址不变
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;           // 内存地址递增
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;                // 循环模式，持续更新
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&hdma_adc1);

    // 将 DMA 句柄链接到 ADC 句柄
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    // --- ADC1 配置 ---
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;         // 扫描模式（多通道）
    hadc1.Init.ContinuousConvMode = ENABLE;             // 连续转换（DMA循环）
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;   // 软件触发
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 4;                     // 4 个通道
    HAL_ADC_Init(&hadc1);

    // --- 配置 4 个通道（Rank 1~4）---
    // 注意: 每次调用 HAL_ADC_ConfigChannel 前必须清零结构体
    uint32_t channels[] = {ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3};
    for (int i = 0; i < 4; i++) {
        sConfig.Channel = channels[i];
        sConfig.Rank = i + 1;                           // Rank1,2,3,4
        sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5; // 55.5周期 ≈ 4.6μs (@12MHz)
        HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    }

    // --- ADC 校准（必须在 Init 之后、Start 之前）---
    HAL_ADCEx_Calibration_Start(&hadc1);
}
```

> **ADC 通道顺序**: Rank1→adc_values[0]→PA0 底座, Rank2→adc_values[1]→PA1 肩部, Rank3→adc_values[2]→PA2 肘部, Rank4→adc_values[3]→PA3 腕部

---

## 五、按键与串口

### 5.1 夹爪按键 (PA4)

```c
static void MX_GPIO_Button_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    // PA4 上拉输入：默认高电平，按下接地 → 低电平
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
```

### 5.2 USART3 调试串口 (PB10/PB11)

```c
UART_HandleTypeDef huart3;

static void MX_USART3_UART_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // PB10 = USART3_TX，复用推挽
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // PB11 = USART3_RX，浮空输入
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

// ===== printf 重定向到 USART3 =====
// 使用前需在项目选项中勾选 "Use MicroLIB"
// 或在 Keil → Target → 勾选 Use MicroLIB
int fputc(int ch, FILE *f) {
    HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
```

> **注意**: 如果不想用 MicroLIB，可手写 `HAL_UART_Transmit` 替代 `printf`。

---

## 六、主程序 (main.c)

### 6.1 完整初始化序列

```c
int main(void) {
    // ============ 1. HAL 库初始化 ============
    HAL_Init();

    // ============ 2. 系统时钟 72MHz ============
    SystemClock_Config();

    // ============ 3. 外设初始化 ============
    MX_GPIO_TIM1_Init();        // PA8,PA9,PA10,PA11 → 复用推挽
    MX_TIM1_PWM_Init();         // TIM1 四路 PWM (底座/肩部/肘部/腕部)

    MX_GPIO_TIM3_Init();        // PA6 → 复用推挽
    MX_TIM3_PWM_Init();         // TIM3 单路 PWM (夹爪)

    MX_GPIO_ADC_Init();         // PA0,PA1,PA2,PA3 → 模拟输入
    MX_ADC1_DMA_Init();         // ADC1 DMA 扫描 4通道

    MX_GPIO_Button_Init();      // PA4 → 上拉输入 (夹爪按键)

    MX_USART3_UART_Init();      // PB10,PB11 → 串口调试

    // ============ 4. 启动所有 PWM 输出 ============
    TIM1_PWM_All_Start();       // 启动 底座/肩部/肘部/腕部
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);  // 启动 夹爪

    // ============ 5. 启动 ADC DMA 扫描 ============
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_values, 4);

    // ============ 6. 初始化舵机到中位 ============
    SERVO_BASE(SERVO_MID_PULSE);
    SERVO_SHOULDER(SERVO_MID_PULSE);
    SERVO_ELBOW(SERVO_MID_PULSE);
    SERVO_WRIST(SERVO_MID_PULSE);
    SERVO_HAND(HAND_CLOSE_PULSE);    // 夹爪初始闭合

    printf("Robot Arm STM32F103C8T6 Ready\r\n");
    printf("System Clock: %ld Hz\r\n", HAL_RCC_GetSysClockFreq());

    // ============ 7. 主循环 ============
    while (1) {
        // --- 读取 4 个电位器，映射到舵机脉宽 ---
        // ADC 实测范围: 600~3500 (12位), 反向映射到舵机 180°~0°
        // map_range(in, in_min, in_max, out_max, out_min) — out端反写实现反向
        uint32_t pulse_base     = map_range(adc_values[0], 600, 3500, SERVO_MAX_PULSE, SERVO_MIN_PULSE);
        uint32_t pulse_shoulder = map_range(adc_values[1], 600, 3500, SERVO_MAX_PULSE, SERVO_MIN_PULSE);
        uint32_t pulse_elbow    = map_range(adc_values[2], 600, 3500, SERVO_MAX_PULSE, SERVO_MIN_PULSE);
        uint32_t pulse_wrist    = map_range(adc_values[3], 600, 3500, SERVO_MAX_PULSE, SERVO_MIN_PULSE);

        // --- 更新舵机位置 ---
        SERVO_BASE(pulse_base);
        SERVO_SHOULDER(pulse_shoulder);
        SERVO_ELBOW(pulse_elbow);
        SERVO_WRIST(pulse_wrist);

        // --- 夹爪按键处理 ---
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) {
            // 按键按下（低电平）→ 闭合夹爪
            SERVO_HAND(HAND_CLOSE_PULSE);
        } else {
            // 按键松开（高电平）→ 张开夹爪
            SERVO_HAND(HAND_OPEN_PULSE);
        }

        // --- 串口输出调试信息（每 100ms） ---
        static uint32_t last_tick = 0;
        if (HAL_GetTick() - last_tick > 100) {
            last_tick = HAL_GetTick();
            printf("ADC: B=%4lu S=%4lu E=%4lu W=%4lu | PWM: B=%4lu S=%4lu E=%4lu W=%4lu\r\n",
                   adc_values[0], adc_values[1], adc_values[2], adc_values[3],
                   pulse_base, pulse_shoulder, pulse_elbow, pulse_wrist);
        }
    }
}
```

### 6.2 工具函数：数值映射

```c
/**
 * @brief  将 value 从 [in_min, in_max] 线性映射到 [out_min, out_max]
 * @note   等价于 Arduino 的 map() 函数
 */
static uint32_t map_range(uint32_t value, uint32_t in_min, uint32_t in_max,
                          uint32_t out_min, uint32_t out_max) {
    // 防止除零
    if (in_max == in_min) return out_min;

    // 限幅
    if (value < in_min) value = in_min;
    if (value > in_max) value = in_max;

    return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
```

> **注意**: STM32 的 ADC 是 12 位 (0~4095)，而 Arduino 是 10 位 (0~1023)。此处映射范围已调整为 0~4095。

---

## 七、完整文件结构建议

```
Project/
├── Core/
│   ├── Inc/
│   │   └── main.h              ← 全局宏定义、句柄 extern 声明
│   └── Src/
│       ├── main.c              ← 主程序（上述 main 函数）
│       ├── gpio.c              ← MX_GPIO_TIM1_Init / TIM3 / ADC / Button
│       ├── tim.c               ← MX_TIM1_PWM_Init / MX_TIM3_PWM_Init
│       ├── adc.c               ← MX_ADC1_DMA_Init
│       ├── usart.c             ← MX_USART3_UART_Init + printf 重定向
│       └── system_stm32f1xx.c  ← SystemClock_Config
```

### main.h 参考内容

```c
#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f1xx_hal.h"
#include <stdio.h>

// ===== 舵机脉宽范围 =====
#define SERVO_MIN_PULSE     650
#define SERVO_MAX_PULSE     2350
#define SERVO_MID_PULSE     1500
#define HAND_OPEN_PULSE     439
#define HAND_CLOSE_PULSE    879

// ===== 舵机控制宏 =====
#define SERVO_BASE(p)       __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (p))
#define SERVO_SHOULDER(p)   __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (p))
#define SERVO_ELBOW(p)      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (p))
#define SERVO_WRIST(p)      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, (p))
#define SERVO_HAND(p)       __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, (p))

// ===== 外设句柄 extern 声明 =====
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;
extern UART_HandleTypeDef huart3;
extern volatile uint32_t adc_values[4];

// ===== 函数声明 =====
void SystemClock_Config(void);
void MX_GPIO_TIM1_Init(void);
void MX_TIM1_PWM_Init(void);
void MX_GPIO_TIM3_Init(void);
void MX_TIM3_PWM_Init(void);
void MX_GPIO_ADC_Init(void);
void MX_ADC1_DMA_Init(void);
void MX_GPIO_Button_Init(void);
void MX_USART3_UART_Init(void);
void TIM1_PWM_All_Start(void);
uint32_t map_range(uint32_t value, uint32_t in_min, uint32_t in_max,
                   uint32_t out_min, uint32_t out_max);

#endif
```

---

## 八、调试检查清单

| 检查项 | 验证方法 |
|--------|---------|
| 时钟是否 72MHz | `printf("HCLK=%ld\n", HAL_RCC_GetHCLKFreq());` 应输出 72000000 |
| TIM1 PWM 是否输出 | 示波器测 PA8~PA11，应有 50Hz 方波 |
| TIM3 PWM 是否输出 | 示波器测 PA6，应有 50Hz 方波 |
| ADC 值是否变化 | 串口输出 `adc_values[]`，旋电位器看数值 0~4095 |
| 按键是否响应 | 串口打印按键状态，按下=0 松开=1 |
| 舵机是否转动 | 确认共地 + 外部供电 + 脉宽在 500~2500 范围 |

---

## 九、常见陷阱汇总

| 陷阱 | 正确做法 |
|------|---------|
| **ADC 引脚忘了设 ANALOG** | 必须 `GPIO_MODE_ANALOG`，不是 `GPIO_MODE_INPUT` |
| **PWM 引脚忘了设 AF_PP** | 必须 `GPIO_MODE_AF_PP`，不是 `GPIO_MODE_OUTPUT_PP` |
| **ADC 忘了校准** | `HAL_ADCEx_Calibration_Start()` 必须在 `HAL_ADC_Init` 之后 |
| **舵机从 STM32 3.3V 取电** | 舵机必须外部 5V 独立供电，只共地 |
| **忘了共地** | STM32 GND ↔ 舵机电源 GND ↔ 舵机 GND 三者必须连通 |
| **printf 不输出** | Keil 勾选 Use MicroLIB；或手写 `HAL_UART_Transmit` |
| **APB1 定时器时钟算错** | APB1=/2 时 TIM3 时钟 = 36×2 = 72MHz |
| **ADC 时钟超过 14MHz** | 72MHz 下 ADC 预分频至少 /6 (12MHz) |
| **DMA 用了 CIRCULAR 但 ADC 没设连续转换** | `ContinuousConvMode = ENABLE` 配合 `DMA_CIRCULAR` |
| **PA9/PA10 用作 TIM1_CH2/CH3 时忘了 AF_PP** | PA9/PA10 默认是 USART1，要主动配置为 `GPIO_MODE_AF_PP` |

---

## 十、CubeMX 等效配置（参考）

如果你用 CubeMX 生成初始化代码，按以下配置：

| 外设 | CubeMX 配置 |
|------|-----------|
| **RCC** | HSE = Crystal, PLLMUL = ×9, APB1 = /2, APB2 = /1 |
| **TIM1** | CH1~CH4 = PWM Generation, PSC=71, ARR=19999 |
| **TIM3** | CH1 = PWM Generation, PSC=71, ARR=19999 |
| **ADC1** | IN0~IN3, Scan+Continuous, DMA1 Channel1 Circular |
| **USART3** | Async, 115200, 8N1 |
| **PA4** | GPIO Input, Pull-up |
| **SYS** | Serial Wire (保留 PA13/PA14) |

---

*文档版本: v1.0 | 基于 STM32F103C8T6 HAL 库 | 2026-07-04*
