/**
 ******************************************************************************
 * @file    soft_i2c.c
 * @brief   Software I2C implementation via bit-banging PB8(SCL) and PB9(SDA)
 *          Uses DWT cycle counter for ~100kHz timing accuracy
 *          SDA direction: OUT_PP for write, INPUT+pull-up for read (ACK)
 ******************************************************************************
 */

#include "soft_i2c.h"

/* ------------------------------------------------------------------ */
/* DWT delay using Cortex-M3 cycle counter (72 MHz = 72 cycles/μs)   */
/* ------------------------------------------------------------------ */

static void dwt_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000UL);
    while ((DWT->CYCCNT - start) < ticks) {
        /* spin */
    }
}

#define I2C_DELAY  dwt_delay_us(5)   /* ~5μs half-period → ~100kHz */

/* ------------------------------------------------------------------ */
/* SDA direction helpers — manipulate GPIOB CRH for PB9 (bits 7:4)   */
/* CRH reset: CNF=10 MODE=00 = 0x8 (Input with pull-up/pull-down)    */
/* CRH orig:  CNF=00 MODE=11 = 0x3 (Output PP, 50MHz) from CubeMX   */
/* ------------------------------------------------------------------ */

static void sda_set_input(void)
{
    /* Switch PB9 to input with pull-up:
     * Clear CNF[1:0] and MODE[1:0] for pin 9 → write 0x8 = 10_00
     * Then set ODR bit 9 to enable internal pull-up */
    uint32_t crh = GPIOB->CRH;
    crh &= ~(0xFU << 4);          /* Clear bits [7:4] for PB9 */
    crh |=  (0x8U << 4);           /* Input mode with pull-up/pull-down */
    GPIOB->CRH = crh;
    GPIOB->ODR |= (1U << 9);       /* Pull-up enabled by ODR=1 in input mode */
}

static void sda_set_output(void)
{
    /* Restore PB9 to output push-pull, 50MHz */
    uint32_t crh = GPIOB->CRH;
    crh &= ~(0xFU << 4);          /* Clear bits [7:4] for PB9 */
    crh |=  (0x3U << 4);           /* Output PP, 50MHz (matching CubeMX MX_GPIO_Init) */
    GPIOB->CRH = crh;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief  Initialize software I2C — enable DWT cycle counter, release bus
 */
void soft_i2c_init(void)
{
    /* Enable DWT cycle counter (Cortex-M3 debug unit) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* Release I2C bus: SCL=H, SDA=H (already set by MX_GPIO_Init) */
    SCL_HIGH();
    SDA_HIGH();
    dwt_delay_us(10);             /* Bus stabilization */
}

/**
 * @brief  I2C start condition: SDA ↓ while SCL=H, then SCL ↓
 */
void soft_i2c_start(void)
{
    sda_set_output();             /* Ensure SDA is output */
    SDA_HIGH();
    I2C_DELAY;
    SCL_HIGH();
    I2C_DELAY;
    SDA_LOW();                    /* START: SDA falls while SCL high */
    I2C_DELAY;
    SCL_LOW();                    /* Pull SCL low to prepare data */
    I2C_DELAY;
}

/**
 * @brief  I2C stop condition: SCL ↑ first, then SDA ↑ while SCL=H
 */
void soft_i2c_stop(void)
{
    sda_set_output();             /* Ensure SDA is output */
    SDA_LOW();                    /* SDA low first */
    I2C_DELAY;
    SCL_HIGH();                   /* SCL goes high */
    I2C_DELAY;
    SDA_HIGH();                   /* STOP: SDA rises while SCL high */
    I2C_DELAY;
}

/**
 * @brief  Write one byte to I2C bus, MSB first. Reads ACK from slave.
 * @param  byte: data to send
 * @retval 0 = ACK received, 1 = NAK received
 *
 * Sequence: 8 data bits (MSB→LSB) then release SDA for ACK bit
 * During ACK: switch SDA to input, slave pulls low = ACK
 */
uint8_t soft_i2c_write_byte(uint8_t byte)
{
    uint8_t ack;

    /* Send 8 data bits, MSB first */
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80U) {
            SDA_HIGH();
        } else {
            SDA_LOW();
        }
        I2C_DELAY;
        SCL_HIGH();               /* Clock out the data bit */
        I2C_DELAY;
        SCL_LOW();
        I2C_DELAY;
        byte <<= 1;
    }

    /* --- ACK phase: release SDA, let slave pull it low --- */
    SDA_HIGH();                   /* Release as output (write 1) */
    sda_set_input();              /* Switch PB9 to input mode so slave can pull down */
    I2C_DELAY;
    SCL_HIGH();                   /* 9th clock: slave should pull SDA low for ACK */
    I2C_DELAY;
    ack = SDA_READ();             /* 0 = ACK (slave pulled low), 1 = NAK */
    SCL_LOW();
    I2C_DELAY;
    sda_set_output();             /* Restore PB9 to output mode */

    return ack;
}
