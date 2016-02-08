#include <stdlib.h>
#include <string.h>
#include "image.h"
#include "internal.h"

int image_init(image *img, jpeg_header *header) {
  int hmax;
  int vmax;
  int i;
  memset(img, 0, sizeof(image));
  img->width = header->width;
  img->height = header->height;
  img->nplanes = header->ncomps;
  hmax = 0;
  vmax = 0;
  for (i = 0; i < img->nplanes; i++) {
    jpeg_component *comp;
    comp = &header->comp[i];
    hmax = OD_MAXI(hmax, comp->hsamp);
    vmax = OD_MAXI(vmax, comp->vsamp);
  }
  for (i = 0; i < img->nplanes; i++) {
    jpeg_component *comp;
    image_plane *plane;
    comp = &header->comp[i];
    plane = &img->plane[i];
    plane->width = comp->hblocks << 3;
    plane->height = comp->vblocks << 3;
    /* TODO support 16-bit images */
    plane->xstride = 1;
    plane->ystride = plane->xstride*plane->width;
    plane->xdec = OD_ILOG(hmax) - OD_ILOG(comp->hsamp);
    plane->ydec = OD_ILOG(vmax) - OD_ILOG(comp->vsamp);
    plane->data = malloc(plane->ystride*plane->height);
    if (plane->data == NULL) {
      image_clear(img);
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}

void image_clear(image *img) {
  int i;
  for (i = 0; i < img->nplanes; i++) {
    free(img->plane[i].data);
  }
  memset(img, 0, sizeof(image));
}

