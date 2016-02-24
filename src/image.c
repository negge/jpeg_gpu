#include <stdlib.h>
#include <string.h>
#include "image.h"
#include "internal.h"
#include "logging.h"

#define IMAGE_ALIGN (16)

int image_init(image *img, jpeg_header *header) {
  int hmax;
  int vmax;
  int i;
  int blocks;
  short *coef;
  memset(img, 0, sizeof(image));
  img->width = header->width;
  img->height = header->height;
  img->nplanes = header->ncomps;
  hmax = 0;
  vmax = 0;
  for (i = 0; i < header->ncomps; i++) {
    jpeg_component *comp;
    comp = &header->comp[i];
    hmax = OD_MAXI(hmax, comp->hsamp);
    vmax = OD_MAXI(vmax, comp->vsamp);
  }
  blocks = 0;
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
    GLJ_LOG((GLJ_LOG_GENERIC, GLJ_LOG_DEBUG,
     "Plane %i: %ix%i (xstride %i, ystride %i, xdec %i, ydec %i)", i,
     plane->width, plane->height, plane->xstride, plane->ystride, plane->xdec,
     plane->ydec));
    plane->data = od_aligned_malloc(plane->ystride*plane->height, IMAGE_ALIGN);
    if (plane->data == NULL) {
      image_clear(img);
      return EXIT_FAILURE;
    }
    /* Compute the distance to the next plane in rows of blocks assuming they
        are packed at the same width as luma (plane 0). */
    plane->cstride = (comp->vblocks + ((1 << plane->xdec) - 1)) >> plane->xdec;
    blocks += (comp->hblocks << plane->xdec)*plane->cstride;
  }
  img->pixels = od_aligned_malloc(img->width*img->height*3, IMAGE_ALIGN);
  if (img->pixels == NULL) {
    image_clear(img);
    return EXIT_FAILURE;
  }
  coef = img->coef = od_aligned_malloc(blocks*64*sizeof(short), IMAGE_ALIGN);
  if (img->coef == NULL) {
    image_clear(img);
    return EXIT_FAILURE;
  }
  for (i = 0; i < img->nplanes; i++) {
    image_plane *plane;
    plane = &img->plane[i];
    plane->coef = coef;
    coef += (plane->width << (plane->xdec + 3))*plane->cstride;
  }
  return EXIT_SUCCESS;
}

void image_clear(image *img) {
  int i;
  for (i = 0; i < img->nplanes; i++) {
    od_aligned_free(img->plane[i].data);
  }
  od_aligned_free(img->pixels);
  od_aligned_free(img->coef);
  memset(img, 0, sizeof(image));
}

