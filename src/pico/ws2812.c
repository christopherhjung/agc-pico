/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iso646.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/dsky.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"

/**
 * NOTE:
 *  Take into consideration if your WS2812 is a RGB or RGBW variant.
 *
 *  If it is RGBW, you need to set IS_RGBW to true and provide 4 bytes per
 *  pixel (Red, Green, Blue, White) and use urgbw_u32().
 *
 *  If it is RGB, set IS_RGBW to false and provide 3 bytes per pixel (Red,
 *  Green, Blue) and use urgb_u32().
 *
 *  When RGBW is used with urgb_u32(), the White channel will be ignored (off).
 *
 */
#define IS_RGBW false
#define NUM_PIXELS 37

#ifdef PICO_DEFAULT_WS2812_PIN
#define WS2812_PIN PICO_DEFAULT_WS2812_PIN
#else
// default to pin 2 if the board doesn't have a default WS2812 pin defined
#define WS2812_PIN 2
#endif

// Check the pin is compatible with the platform
#if WS2812_PIN >= NUM_BANK0_GPIOS
#error Attempting to use a pin>=32 on a platform that does not support it
#endif

static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb)
{
  pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
  return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

static inline uint32_t urgbw_u32(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
  return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16)
    | ((uint32_t)(w) << 24) | (uint32_t)(b);
}

static PIO  pio;
static uint sm;
static uint offset;

static int dma_chan;

static uint32_t pixel_buffer[NUM_PIXELS] = {0};

void set_pixel_color(uint index, uint32_t grb)
{
  pixel_buffer[index] = grb;
}

void refresh_ws2812(PIO pio, uint sm, uint len) {
  for (uint i = 0; i < len; ++i) {
    put_pixel(pio, sm, pixel_buffer[i]);
  }
}

void init_indicator_display()
{

  memset(pixel_buffer, 0, sizeof(pixel_buffer));

  bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
    &ws2812_program, &pio, &sm, &offset, WS2812_PIN, 1, true);
  hard_assert(success);

  ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
  refresh_ws2812(pio, sm, NUM_PIXELS);
}

#define VERB_BACK_IDX 0
#define NOUN_BACK_IDX 1
#define PROG_BACK_IDX 2
#define COMP_ACTY_IDX 3
#define TEMP_IDX 4
#define UPLINK_ACTY_IDX 5
#define NO_ATT_IDX 6
#define GIMBAL_LOCK_IDX 7
#define PROG_IDX 8
#define STBY_IDX 9
#define KEY_REL_IDX 10
#define RESTART_IDX 11
#define TRACKER_IDX 12
#define OPER_ERR_IDX 13
#define ALT_IDX 15
#define VEL_IDX 16
#define KY_BACKLIGHT 18


void refresh_indicator_display(dsky_t* dsky)
{
  bool blink_on = !dsky->blink_off;
  uint32_t black = urgb_u32(0, 0, 0);
  uint32_t white = urgb_u32(60, 60, 60);
  uint32_t green = urgb_u32(0, 80, 0);
  uint32_t orange = urgb_u32(80, 40, 0);

  set_pixel_color(VERB_BACK_IDX, green);
  set_pixel_color(NOUN_BACK_IDX, green);
  set_pixel_color(PROG_BACK_IDX, green);
  set_pixel_color(COMP_ACTY_IDX, dsky->indicator.comp_acty ? orange : black);
  set_pixel_color(TEMP_IDX, dsky->indicator.temp ? orange : black);
  set_pixel_color(UPLINK_ACTY_IDX, dsky->indicator.uplink_acty ? white : black);
  set_pixel_color(NO_ATT_IDX, dsky->indicator.no_att ? white : black);
  set_pixel_color(GIMBAL_LOCK_IDX, dsky->indicator.gimbal_lock ? orange : black);
  set_pixel_color(PROG_IDX, dsky->indicator.prog ? orange : black);
  set_pixel_color(STBY_IDX, dsky->indicator.stby ? white : black);
  set_pixel_color(KEY_REL_IDX, dsky->indicator.key_rel && blink_on ? white : black);
  set_pixel_color(RESTART_IDX, dsky->indicator.restart ? orange : black);
  set_pixel_color(TRACKER_IDX, dsky->indicator.tracker ? orange : black);
  set_pixel_color(OPER_ERR_IDX, dsky->indicator.opr_err && blink_on ? white : black);
  set_pixel_color(ALT_IDX, dsky->indicator.alt ? orange : black);
  set_pixel_color(VEL_IDX, dsky->indicator.vel ? orange : black);

  for(size_t idx = 0; idx < 0; idx++)
    set_pixel_color(KY_BACKLIGHT + idx, white);

  refresh_ws2812(pio, sm, NUM_PIXELS);
}

void deinit_indicator_display()
{
  dma_channel_abort(dma_chan);

  pio_remove_program_and_unclaim_sm(&ws2812_program, pio, sm, offset);
}