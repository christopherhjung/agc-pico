/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "spi.h"

void init_spi_default()
{
  gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
  gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
  gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);

  sleep_ms(1);

  // This example will use SPI0 at 1MHz.
  spi_init(spi_default, 1 * 1000 * 1000);
  gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);

  // Make the SPI pins available to picotool
  bi_decl(bi_3pins_with_func(
    PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI));

  // Make the CS pin available to picotool
  bi_decl(bi_1pin_with_name(PICO_DEFAULT_SPI_CSN_PIN, "SPI CS"));
}
