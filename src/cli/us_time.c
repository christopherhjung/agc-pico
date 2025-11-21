
#include <time.h>

long long time_us_64(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000000LL + ts.tv_nsec / 1000);
}