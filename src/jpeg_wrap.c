#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jpeg_wrap.h"

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
}

void jpeg_info_clear(jpeg_info *info) {
  free(info->buf);
  memset(info, 0, sizeof(jpeg_info));
}

void image_clear(image *img) {
  int i;
  for (i = 0; i < img->nplanes; i++) {
    free(img->plane[i].data);
  }
  memset(img, 0, sizeof(image));
}
