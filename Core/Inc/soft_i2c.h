/**
 ******************************************************************************
 * @file    soft_i2c.h
 * @brief   Software I2C header for SSD1306 OLED
 *          PB8 = SCL, PB9 = SDA, ~100kHz via DWT cycle counter
 ******************************************************************************
 */

#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Pin definitions — PB8=SCL, PB9=SDA */
#define SOFT_I2C_SCL_PIN    GPIO_PIN_8
#define SOFT_I2C_SDA_PIN    GPIO_PIN_9
#define SOFT_I2C_PORT       GPIOB

/*
 * BSRR: 低16位置位(输出高), 高16位复位(输出低)
 * BRR:  低16位复位(输出低)
 * IDR:  读取引脚电平
 */
#define SCL_HIGH()   (SOFT_I2C_PORT->BSRR = (uint32_t)SOFT_I2C_SCL_PIN)
#define SCL_LOW()    (SOFT_I2C_PORT->BRR  = (uint32_t)SOFT_I2C_SCL_PIN)
#define SDA_HIGH()   (SOFT_I2C_PORT->BSRR = (uint32_t)SOFT_I2C_SDA_PIN)
#define SDA_LOW()    (SOFT_I2C_PORT->BRR  = (uint32_t)SOFT_I2C_SDA_PIN)
#define SDA_READ()   ((SOFT_I2C_PORT->IDR & SOFT_I2C_SDA_PIN) ? 1U : 0U)

/* Public API */
void    soft_i2c_init(void);
void    soft_i2c_start(void);
void    soft_i2c_stop(void);
uint8_t soft_i2c_write_byte(uint8_t byte);  /* Returns 0=ACK, 1=NAK */

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_I2C_H */
