/**
 ******************************************************************************
 * @file    ssd1306.h
 * @brief   SSD1306 128x64 I2C OLED driver header
 *          Uses software I2C (soft_i2c.h) — no HAL I2C dependency
 *          Direct GDDRAM write mode (no framebuffer), 5x7 font
 ******************************************************************************
 */

#ifndef __SSD1306_H
#define __SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* SSD1306 I2C 7-bit address: 0x3C, shifted = 0x78 */
#define SSD1306_I2C_ADDR     0x3CU
#define SSD1306_I2C_ADDR_W   (SSD1306_I2C_ADDR << 1)   /* 0x78 */

#define SSD1306_WIDTH         128
#define SSD1306_HEIGHT         64
#define SSD1306_PAGES           8   /* 64 / 8 */

/* Control byte: next byte(s) are command or data */
#define SSD1306_CMD_SINGLE   0x80U   /* Single command byte */
#define SSD1306_CMD_STREAM   0x00U   /* Stream of command bytes */
#define SSD1306_DATA_STREAM  0x40U   /* Stream of data bytes */

/* Public API */
uint8_t ssd1306_init(void);
void    ssd1306_command(uint8_t cmd);
void    ssd1306_set_cursor(uint8_t page, uint8_t col);
void    ssd1306_clear(void);
void    ssd1306_write_char(char c);
void    ssd1306_write_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __SSD1306_H */
