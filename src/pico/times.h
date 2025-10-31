

#ifndef PICO_POSIX_COMPAT_H
#define PICO_POSIX_COMPAT_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

  // sysconf() name values (subset relevant for embedded systems)
#define _SC_CLK_TCK         2   // Clock ticks per second
#define _SC_PAGESIZE        30  // Page size
#define _SC_PAGE_SIZE       _SC_PAGESIZE
#define _SC_NPROCESSORS_CONF 83 // Number of processors configured
#define _SC_NPROCESSORS_ONLN 84 // Number of processors online
#define _SC_PHYS_PAGES      85  // Physical pages
#define _SC_AVPHYS_PAGES    86  // Available physical pages

  // times() structure
  struct tms {
    clock_t tms_utime;  // User CPU time
    clock_t tms_stime;  // System CPU time
    clock_t tms_cutime; // User CPU time of children
    clock_t tms_cstime; // System CPU time of children
  };

  /**
   * Get system configuration information
   *
   * @param name Configuration parameter to query
   * @return Value of the parameter, or -1 on error
   */
  long pico_sysconf(int name);

  /**
   * Get process times
   *
   * @param buf Pointer to tms structure to fill
   * @return Clock ticks since system start, or (clock_t)-1 on error
   */
  clock_t pico_times(struct tms *buf);

  // Optional: Define standard names if not conflicting
#ifndef sysconf
#define sysconf pico_sysconf
#endif

#ifndef times
#define times pico_times
#endif

#ifdef __cplusplus
}
#endif

#endif // PICO_POSIX_COMPAT_H