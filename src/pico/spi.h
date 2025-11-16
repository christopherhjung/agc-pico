#pragma once

#include <stdio.h>
#include <string.h>

#include "hardware/spi.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "core/dsky.h"

#define OE_PIN 22

static inline void cs_select(uint cs_pin)
{
  asm volatile("nop \n nop \n nop");
  gpio_put(cs_pin, false);
  asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect(uint cs_pin)
{
  asm volatile("nop \n nop \n nop");
  gpio_put(cs_pin, true);
  asm volatile("nop \n nop \n nop");
}

void init_spi_default();
