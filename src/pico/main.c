#include <core/agc_simulator.h>

#include "core/profile.h"
#include "core/dsky_dump.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"
#include "dsky_output_handler.h"

#include "max7221.c"
#include "ws2812.c"


const uint8_t *rom =  (const uint8_t *)0x10100000;
const uint8_t *core =  (const uint8_t *)0x1020000;
const uint8_t *profile =  (const uint8_t *)0x1030000;

int main(int argc, char* argv[])
{
  stdio_init_all();  // Initialize USB or UART serial I/O

  vreg_set_voltage(VREG_VOLTAGE_1_25);
  set_sys_clock_khz(360000, true);

  init_spi_default();

  gpio_init(OE_PIN);
  gpio_set_dir(OE_PIN, GPIO_OUT);
  gpio_put(OE_PIN, 1); // Set GPIO 22 HIGH

  init_indicator_display();
  init_numeric_display();
  init_keyboard();

  profile_load_file(profile, 25383);
  opt_t opt = {0};
  agc_load_rom(&Simulator.state, rom, 73728);
  sim_init(&opt);
  agc_engine_init(&Simulator.state, core, 73728, 0);
  sim_exec();

  return (0);
}

void dsky_refresh(dsky_t *dsky)
{
  refresh_numeric_display(dsky);
  refresh_indicator_display(dsky);

  dsky_print(dsky);
}
