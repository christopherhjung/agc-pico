
#include "profile.h"

#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>



row_t rows[704];

char *read_file(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    perror("Failed to open file");
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  rewind(file);

  char *data = (char *)malloc(length + 1);
  if (!data) {
    perror("Memory allocation failed");
    fclose(file);
    return NULL;
  }

  fread(data, 1, length, file);
  data[length] = '\0';
  fclose(file);
  return data;
}

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
  char *file_contents = read_file(filename);
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