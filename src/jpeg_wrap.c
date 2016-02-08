#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include "jpeg_wrap.h"
#include "xjpeg.h"
#include "internal.h"

typedef struct libjpeg_decode_ctx libjpeg_decode_ctx;

struct libjpeg_decode_ctx {
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
};

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

static int libjpeg_decode_image(libjpeg_decode_ctx *ctx, image *img,
 jpeg_decode_out out) {
  JSAMPROW yrow_pointer[16];
  JSAMPROW cbrow_pointer[16];
  JSAMPROW crrow_pointer[16];
  JSAMPROW *plane_pointer[3];
  int i;

  plane_pointer[0] = yrow_pointer;
  plane_pointer[1] = cbrow_pointer;
  plane_pointer[2] = crrow_pointer;

  if (out != JPEG_DECODE_YUV) {
    fprintf(stderr, "Error, libjpeg wrapper only supports YUV output.\n");
    return EXIT_FAILURE;
  }

  ctx->cinfo.raw_data_out = TRUE;
  ctx->cinfo.do_fancy_upsampling = FALSE;
  ctx->cinfo.dct_method = JDCT_IFAST;

  jpeg_start_decompress(&ctx->cinfo);

  while (ctx->cinfo.output_scanline < ctx->cinfo.output_height) {
    int j;

    for (i = 0; i < img->nplanes; i++) {
      image_plane *plane;
      int y_off;
      plane = &img->plane[i];
      y_off = ctx->cinfo.output_scanline >> plane->ydec;
      for (j = 0; j < 16 >> plane->ydec; j++) {
        plane_pointer[i][j]=
         &plane->data[(y_off + j)*plane->width];
      }
    }

    jpeg_read_raw_data(&ctx->cinfo, plane_pointer, 16);
  }

  jpeg_finish_decompress(&ctx->cinfo);

  return EXIT_SUCCESS;
}

static void libjpeg_decode_reset(libjpeg_decode_ctx *ctx, jpeg_info *info) {
  jpeg_destroy_decompress(&ctx->cinfo);

  jpeg_create_decompress(&ctx->cinfo);
  jpeg_mem_src(&ctx->cinfo, info->buf, info->size);
}

static void libjpeg_decode_free(libjpeg_decode_ctx *ctx) {
  jpeg_destroy_decompress(&ctx->cinfo);
  free(ctx);
}

const jpeg_decode_ctx_vtbl LIBJPEG_DECODE_CTX_VTBL = {
  (jpeg_decode_alloc_func)libjpeg_decode_alloc,
  (jpeg_decode_header_func)libjpeg_decode_header,
  (jpeg_decode_image_func)libjpeg_decode_image,
  (jpeg_decode_reset_func)libjpeg_decode_reset,
  (jpeg_decode_free_func)libjpeg_decode_free
};

static xjpeg_decode_ctx *xjpeg_decode_alloc(jpeg_info *info) {
  xjpeg_decode_ctx *ctx;
  ctx = (xjpeg_decode_ctx *)malloc(sizeof(xjpeg_decode_ctx));
  if (ctx != NULL) {
    xjpeg_init(ctx, info->buf, info->size);
  }
  return ctx;
}

static int xjpeg_decode_header(xjpeg_decode_ctx *ctx, jpeg_header *headers) {
  xjpeg_frame_header *frame;
  int i;

  xjpeg_decode(ctx, 1, NULL);

  if (ctx->error) {
    fprintf(stderr, "%s\n", ctx->error);
    return EXIT_FAILURE;
  }

  frame = &ctx->frame;
  if (!frame->valid) {
    fprintf(stderr, "Error reading jpeg headers\n");
    return EXIT_FAILURE;
  }

  if (frame->ncomps != 1 && frame->ncomps != 3) {
    fprintf(stderr, "Unsupported number of components %i\n", frame->ncomps);
    return EXIT_FAILURE;
  }

  headers->width = frame->width;
  headers->height = frame->height;
  headers->ncomps = frame->ncomps;

  for (i = 0; i < headers->ncomps; i++) {
    xjpeg_comp_info *info;
    jpeg_component *comp;
    info = &frame->comp[i];
    comp = &headers->comp[i];
    comp->hblocks = frame->nhmb*info->hsamp;
    comp->vblocks = frame->nvmb*info->vsamp;
    comp->hsamp = info->hsamp;
    comp->vsamp = info->vsamp;
  }

  return EXIT_SUCCESS;
}

static int xjpeg_decode_image(xjpeg_decode_ctx *ctx, image *img,
 jpeg_decode_out out) {
  if (out != JPEG_DECODE_YUV) {
    fprintf(stderr, "Error, libjpeg wrapper only supports YUV output.\n");
    return EXIT_FAILURE;
  }

  xjpeg_decode(ctx, 0, img);

  if (ctx->error) {
    fprintf(stderr, "%s\n", ctx->error);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

static void xjpeg_decode_reset(xjpeg_decode_ctx *ctx, jpeg_info *info) {
  xjpeg_init(ctx, info->buf, info->size);
}

static void xjpeg_decode_free(xjpeg_decode_ctx *ctx) {
  free(ctx);
}

const jpeg_decode_ctx_vtbl XJPEG_DECODE_CTX_VTBL = {
  (jpeg_decode_alloc_func)xjpeg_decode_alloc,
  (jpeg_decode_header_func)xjpeg_decode_header,
  (jpeg_decode_image_func)xjpeg_decode_image,
  (jpeg_decode_reset_func)xjpeg_decode_reset,
  (jpeg_decode_free_func)xjpeg_decode_free
};
