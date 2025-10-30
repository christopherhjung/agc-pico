#pragma once
#include <stdbool.h>

typedef struct
{
  double second;
  double third;
  double fourth;
  int stage;
} row_t;

row_t profile_get_data(int seconds);

bool profile_load_file();