#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include "jpeg_wrap.h"
#include "internal.h"

typedef struct libjpeg_decode_ctx libjpeg_decode_ctx;

struct libjpeg_decode_ctx {
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
};

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

static libjpeg_decode_ctx *libjpeg_decode_alloc(jpeg_info *info) {
  libjpeg_decode_ctx *ctx;
  ctx = (libjpeg_decode_ctx *)malloc(sizeof(libjpeg_decode_ctx));
  if (ctx != NULL) {
    ctx->cinfo.err=jpeg_std_error(&ctx->jerr);
    /* TODO add error checking */
    jpeg_create_decompress(&ctx->cinfo);

    jpeg_mem_src(&ctx->cinfo, info->buf, info->size);
  }
  return ctx;
}

static int libjpeg_decode_header(libjpeg_decode_ctx *ctx,
 jpeg_header *headers) {
  int i;

  if (jpeg_read_header(&ctx->cinfo, TRUE) != JPEG_HEADER_OK) {
    fprintf(stderr, "Error reading jpeg heaers\n");
    return EXIT_FAILURE;
  }

  /* Copy jpeg headers out of libjpeg decoder struct */
  headers->width = ctx->cinfo.image_width;
  headers->height = ctx->cinfo.image_height;
  headers->ncomps = ctx->cinfo.num_components;

  if (headers->ncomps != 1 && headers->ncomps != 3) {
    fprintf(stderr, "Unsupported number of components %i\n", headers->ncomps);
    return EXIT_FAILURE;
  }

  for (i = 0; i < headers->ncomps; i++) {
    jpeg_component_info *info;
    jpeg_component *comp;
    info = &ctx->cinfo.comp_info[i];
    comp = &headers->comp[i];
    comp->hblocks = info->width_in_blocks;
    comp->vblocks = info->height_in_blocks;
    comp->hsamp = info->h_samp_factor;
    comp->vsamp = info->v_samp_factor;
  }

  return EXIT_SUCCESS;
}

static void libjpeg_decode_free(libjpeg_decode_ctx *ctx) {
  jpeg_destroy_decompress(&ctx->cinfo);
  free(ctx);
}

const jpeg_decode_ctx_vtbl LIBJPEG_DECODE_CTX_VTBL = {
  (jpeg_decode_alloc_func)libjpeg_decode_alloc,
  (jpeg_decode_header_func)libjpeg_decode_header,
  (jpeg_decode_free_func)libjpeg_decode_free
};
