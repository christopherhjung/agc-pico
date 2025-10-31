
#include "times.h"

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/clocks.h"
#include <errno.h>

// RP2040 has 264KB of SRAM
#define PICO_SRAM_SIZE (264 * 1024)
#define PICO_PAGE_SIZE 4096  // Typical page size for embedded systems

// Track timing information
static uint64_t start_time_us = 0;
static bool timing_initialized = false;

static void ensure_timing_init(void) {
    if (!timing_initialized) {
        start_time_us = time_us_64();
        timing_initialized = true;
    }
}

long pico_sysconf(int name) {
    switch (name) {
        case _SC_CLK_TCK:
            // Return clock ticks per second
            // On Pico, we use microsecond resolution
            return 150000000; // 1 MHz (1 tick per microsecond)

        case _SC_PAGESIZE:
            // Return typical page size
            return PICO_PAGE_SIZE;

        case _SC_NPROCESSORS_CONF:
        case _SC_NPROCESSORS_ONLN:
            // RP2040 has 2 ARM Cortex-M0+ cores
            return 2;

        case _SC_PHYS_PAGES:
            // Total physical memory pages
            return PICO_SRAM_SIZE / PICO_PAGE_SIZE;

        case _SC_AVPHYS_PAGES:
            // Available physical memory pages
            // This is a simplified estimate - in reality would need
            // heap tracking to determine actual free memory
            {
                extern char __StackLimit, __bss_end__;
                extern char __HeapLimit;

                // Rough estimate: space between heap and stack
                size_t used = (size_t)&__bss_end__;
                size_t total = (size_t)&__HeapLimit;
                size_t available = total - used;

                return available / PICO_PAGE_SIZE;
            }

        default:
            errno = EINVAL;
            return -1;
    }
}

clock_t pico_times(struct tms *buf) {
    if (buf == NULL) {
        errno = EFAULT;
        return (clock_t)-1;
    }

    ensure_timing_init();

    // Get current time in microseconds
    uint64_t current_time_us = time_us_64();
    uint64_t elapsed_us = current_time_us - start_time_us;

    // On a bare-metal system, all time is "user" time
    // We don't have system vs user time distinction
    buf->tms_utime = (clock_t)elapsed_us;
    buf->tms_stime = 0;  // No system time in bare-metal

    // No child processes in bare-metal environment
    buf->tms_cutime = 0;
    buf->tms_cstime = 0;

    // Return total clock ticks since start
    return (clock_t)elapsed_us;
}

// Optional: Reset timing base (useful for testing or restarting measurements)
void pico_times_reset(void) {
    start_time_us = time_us_64();
    timing_initialized = true;
}

// Optional: Get high-resolution time in microseconds
uint64_t pico_get_time_us(void) {
    ensure_timing_init();
    return time_us_64() - start_time_us;
}

// Optional: Get CPU frequency
uint32_t pico_get_cpu_freq_hz(void) {
    return clock_get_hz(clk_sys);
}

