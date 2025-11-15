/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>

#include "hardware/spi.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "core/dsky.h"

/* Example code to talk to a Max7219 driving an 8 digit 7 segment display via SPI

   NOTE: The device is driven at 5v, but SPI communications are at 3v3

   Connections on Raspberry Pi Pico board and a generic Max7219 board, other
   boards may vary.

   * GPIO 17 (pin 22) Chip select -> CS on Max7219 board
   * GPIO 18 (pin 24) SCK/spi0_sclk -> CLK on Max7219 board
   * GPIO 19 (pin 25) MOSI/spi0_tx -> DIN on Max7219 board
   * 5v (pin 40) -> VCC on Max7219 board
   * GND (pin 38)  -> GND on Max7219 board

   Note: SPI devices can have a number of different naming schemes for pins. See
   the Wikipedia page at https://en.wikipedia.org/wiki/Serial_Peripheral_Interface
   for variations.

*/

// This defines how many Max7219 modules we have cascaded together, in this case, just the one.
#define NUM_MODULES 3


#define OE_PIN 22

const uint8_t CMD_NOOP        = 0;
const uint8_t CMD_DIGIT0      = 1; // Goes up to 8, for each line
const uint8_t CMD_DECODEMODE  = 9;
const uint8_t CMD_BRIGHTNESS  = 10;
const uint8_t CMD_SCANLIMIT   = 11;
const uint8_t CMD_SHUTDOWN    = 12;
const uint8_t CMD_DISPLAYTEST = 15;

#ifdef PICO_DEFAULT_SPI_CSN_PIN
static inline void cs_select()
{
  asm volatile("nop \n nop \n nop");
  gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);
  asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect()
{
  asm volatile("nop \n nop \n nop");
  gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
  asm volatile("nop \n nop \n nop");
}
#endif

#if defined(spi_default) && defined(PICO_DEFAULT_SPI_CSN_PIN)
static void write_register_all(uint8_t reg, uint8_t data)
{
  uint8_t buf[2] = {reg, data};
  cs_select();
  for(int idx = 0; idx < NUM_MODULES; idx++)
  {
    spi_write_blocking(spi_default, buf, 2);
  }
  cs_deselect();
}

static void write_register(uint8_t reg, uint8_t* data)
{
  uint8_t buf[2];
  buf[0] = reg;
  cs_select();
  for(int idx = 0; idx < NUM_MODULES; idx++)
  {
    buf[1] = data[idx];
    spi_write_blocking(spi_default, buf, 2);
  }
  cs_deselect();
}
#endif

void refresh_numeric_display(dsky_t *dsky)
{
  bool blink_on = !dsky->blink_off;
  write_register_all(CMD_DISPLAYTEST, 0);
  write_register_all(CMD_SCANLIMIT, 7);
  write_register_all(CMD_DECODEMODE, 255);
  write_register_all(CMD_BRIGHTNESS, 2);
  write_register_all(CMD_SHUTDOWN, 1);

  uint8_t plus_encoding = 15;
  uint8_t blank_encoding = 15;
  uint8_t first_sign = dsky->rows[0].plus ? plus_encoding : (dsky->rows[0].minus ? 10 : blank_encoding);
  uint8_t second_sign = dsky->rows[1].plus ? plus_encoding : (dsky->rows[1].minus ? 10 : blank_encoding);
  uint8_t third_sign = dsky->rows[2].plus ? plus_encoding : (dsky->rows[2].minus ? 10 : blank_encoding);

  uint8_t data0[3] = {third_sign, first_sign, dsky->prog.first};
  write_register(CMD_DIGIT0 , data0);

  uint8_t data1[3] = {dsky->rows[2].first, dsky->rows[0].first, dsky->prog.second};
  write_register(CMD_DIGIT0 + 1, data1);

  uint8_t data2[3] = {dsky->rows[2].second, second_sign, blink_on ? (uint8_t)dsky->noun.first : blank_encoding};
  write_register(CMD_DIGIT0 + 2, data2);

  uint8_t data3[3] = {dsky->rows[2].third, dsky->rows[1].first, blink_on ? (uint8_t)dsky->noun.second : blank_encoding};
  write_register(CMD_DIGIT0 + 3, data3);

  uint8_t data4[3] = {dsky->rows[2].fourth, dsky->rows[1].second, dsky->rows[0].fourth};
  write_register(CMD_DIGIT0 + 4, data4);

  uint8_t data5[3] = {dsky->rows[2].fifth, dsky->rows[1].third, dsky->rows[0].fifth};
  write_register(CMD_DIGIT0 + 5, data5);

  uint8_t data6[3] = {dsky->rows[1].fourth, blink_on ? (uint8_t)dsky->verb.first : blank_encoding, dsky->rows[0].second};
  write_register(CMD_DIGIT0 + 6, data6);

  uint8_t data7[3] = {dsky->rows[1].fifth, blink_on ? (uint8_t)dsky->verb.second : blank_encoding, dsky->rows[0].third};
  write_register(CMD_DIGIT0 + 7, data7);
}

void clear()
{
  for(int i = 0; i < 8; i++)
  {
    write_register_all(CMD_DIGIT0 + i, 15);
  }
}

void init_numeric_display()
{

  gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
  gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
  gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);

  sleep_ms(1);

#if !defined(spi_default) || !defined(PICO_DEFAULT_SPI_SCK_PIN) \
|| !defined(PICO_DEFAULT_SPI_TX_PIN) || !defined(PICO_DEFAULT_SPI_CSN_PIN)
#warning spi/max7219_8x7seg_spi example requires a board with SPI pins
  puts("Default SPI pins were not defined");
#else


  // This example will use SPI0 at 1MHz.
  spi_init(spi_default, 1 * 1000 * 1000);
  gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

  // Make the SPI pins available to picotool
  bi_decl(bi_2pins_with_func(
    PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI));


  // Make the CS pin available to picotool
  bi_decl(bi_1pin_with_name(PICO_DEFAULT_SPI_CSN_PIN, "SPI CS"));

  // Send init sequence to device

  write_register_all(CMD_SHUTDOWN, 0);
  write_register_all(CMD_DISPLAYTEST, 0);
  write_register_all(CMD_SCANLIMIT, 7);
  write_register_all(CMD_DECODEMODE, 255);
  write_register_all(CMD_BRIGHTNESS, 2);
  write_register_all(CMD_SHUTDOWN, 1);

  clear();
#endif

}
