/* JPEG GPU project
Copyright (c) 2014-2016 JPEG GPU project contributors.  All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License"); you may not
 use this file except in compliance with the License.
You may obtain a copy of the License at:

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
 under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and limitations
 under the License. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jpeg_info.h"

int jpeg_info_init(jpeg_info *info, const char *name) {
  FILE *fp;
  int size;
  fp = fopen(name, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Error, could not open jpeg file %s\n", name);
    return EXIT_FAILURE;
  }
  jpeg_info_clear(info);
  fseek(fp, 0, SEEK_END);
  info->size = ftell(fp);
  info->buf = malloc(info->size);
  if (info->buf == NULL) {
    fprintf(stderr, "Error, could not allocate %i bytes\n", info->size);
    return EXIT_FAILURE;
  }
  fseek(fp, 0, SEEK_SET);
  size = fread(info->buf, 1, info->size, fp);
  if (size != info->size) {
    fprintf(stderr, "Error reading jpeg file, got %i of %i bytes\n", size,
     info->size);
    return EXIT_FAILURE;
  }
  fclose(fp);
  return EXIT_SUCCESS;
}

void jpeg_info_clear(jpeg_info *info) {
  free(info->buf);
  memset(info, 0, sizeof(jpeg_info));
}
