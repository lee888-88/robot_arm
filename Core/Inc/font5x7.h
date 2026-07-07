/**
 ******************************************************************************
 * @file    font5x7.h
 * @brief   5x7 pixel ASCII font table for SSD1306 OLED
 *          Covers 0x20 (space) through 0x7E (~), 95 characters
 *          Each glyph: 5 bytes (columns), displayed at 6px width with spacer
 ******************************************************************************
 */

#ifndef __FONT5X7_H
#define __FONT5X7_H

#include <stdint.h>

#define FONT5X7_START     0x20U
#define FONT5X7_END       0x7EU
#define FONT5X7_COUNT      95U
#define FONT5X7_WIDTH       6U    /* 5 data + 1 spacer column */
#define FONT5X7_HEIGHT      8U

extern const uint8_t font5x7[FONT5X7_COUNT][5];

#endif /* __FONT5X7_H */
