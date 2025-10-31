
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *read_file(const char *filename, uint64_t* len) {
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
  *len = length;
  fclose(file);
  return data;
}
