
#include "profile.h"
#include "file.h"

#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>



row_t rows[704];

row_t profile_get_data(int seconds)
{
  if(seconds > 703)
  {
    row_t def = {
      .second = 0.0,
      .third = 0.0,
      .fourth = 0.0,
      .stage = 0
    };
    return def;
  }
  return rows[seconds];
}

void profile_load_json(cJSON *json)
{
  cJSON *row = NULL;
  cJSON_ArrayForEach(row, json) {   // <— looping macro

    cJSON* elems = row->child;
    int idx = (elems++)->valueint;
    row_t* data_row = &rows[idx];
    data_row->second = (elems++)->valuedouble;
    data_row->third = (elems++)->valuedouble;
    data_row->fourth = (elems++)->valuedouble;
    data_row->stage = (elems++)->valueint;
  }
}

bool profile_load_file()
{
  const char *filename = "resources/profile.json";
  uint64_t len;
  char *file_contents = read_file(filename, &len);
  if (!file_contents) {
    return 1;
  }

  // Parse JSON string
  cJSON *json = cJSON_Parse(file_contents);
  if(json){
    profile_load_json(json);
  }
  free(file_contents);
  return json != NULL;
}