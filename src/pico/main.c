#include <core/agc_simulator.h>

#include "core/profile.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"

#include "max7221.c"


const uint8_t *rom =  (const uint8_t *)0x10100000;
const uint8_t *core =  (const uint8_t *)0x1020000;
const uint8_t *profile =  (const uint8_t *)0x1030000;

int main(int argc, char* argv[])
{
  stdio_init_all();  // Initialize USB or UART serial I/O
  init_numeric_display();

  vreg_set_voltage(VREG_VOLTAGE_1_25);
  set_sys_clock_khz(360000, true);

  profile_load_file(profile, 25383);
  opt_t opt = {0};

  agc_load_rom(&Simulator.state, rom, 73728);
  sim_init(&opt);
  agc_engine_init(&Simulator.state, core, 73728, 0);
  sim_exec();

  return (0);
}
