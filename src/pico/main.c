#include <core/agc_simulator.h>

#include "core/profile.h"
#include "core/dsky_dump.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "dsky_output_handler.h"

#include "max7221.c"
#include "ws2812.c"


const uint8_t *rom =  (const uint8_t *)0x10100000;
const uint8_t *core =  (const uint8_t *)0x1020000;
const uint8_t *profile =  (const uint8_t *)0x1030000;

void core1_entry() {
  while (true) {
    // Core 1 work here
    printf("Core 1 running\n");
    sleep_ms(1000);
  }
}

int main(int argc, char* argv[])
{
  stdio_init_all();  // Initialize USB or UART serial I/O

  vreg_set_voltage(VREG_VOLTAGE_1_25);
  set_sys_clock_khz(360000, true);

  printf("Hello world!\n");

  init_spi_default();

  gpio_init(OE_PIN);
  gpio_set_dir(OE_PIN, GPIO_OUT);
  gpio_put(OE_PIN, 1); // Set GPIO 22 HIGH

  init_indicator_display();
  init_numeric_display();
  init_keyboard();

  //multicore_reset_core1();
  //multicore_launch_core1(core1_entry);

  profile_load_file(profile, 25383);
  opt_t opt = {0};
  sim_t sim;
  agc_load_rom(&sim.state, rom, 73728);
  init_sim(&sim, &opt);
  agc_engine_init(&sim.state, core, 73728, 0);
  sim_exec(&sim);

  return (0);
}

void dsky_refresh(dsky_t *dsky)
{
  refresh_numeric_display(dsky);
  refresh_indicator_display(dsky);

  //dsky_print(dsky);
}
