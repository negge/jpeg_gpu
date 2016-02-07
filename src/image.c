#include <stdlib.h>
#include <string.h>
#include "image.h"

void image_clear(image *img) {
  int i;
  for (i = 0; i < img->nplanes; i++) {
    free(img->plane[i].data);
  }
  memset(img, 0, sizeof(image));
}

