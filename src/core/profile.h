#pragma once
#include <stdbool.h>

#include <stdint.h>

typedef struct
{
  double rot_y;
  double accel_x;
  double rot_x;
  int stage;
} row_t;

row_t profile_get_data(int seconds);

bool profile_load_file(const uint8_t* data, uint64_t len);