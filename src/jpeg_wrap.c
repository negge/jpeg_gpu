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
#include <jpeglib.h>
#include "jpeg_wrap.h"
#include "xjpeg.h"
#include "internal.h"

const char *JPEG_DECODE_OUT_NAMES[JPEG_DECODE_OUT_MAX] = {
  "pack",
  "quant",
  "dct",
  "yuv",
  "rgb",
};

static jpeg_subsamp decode_subsamp(jpeg_header *headers) {
  if (headers->ncomps == 1) {
    return JPEG_SUBSAMP_MONO;
  }
  /* If there is more than one component (color plane), assume both chroma
      planes have the same sampling (MCU size).
     This code also assumes that luma sampling is always greater than or
      equal to the chroma sampling. */
  else {
    int xdec;
    int ydec;
    xdec = GLJ_ILOG(headers->comp[0].hsamp) - GLJ_ILOG(headers->comp[1].hsamp);
    ydec = GLJ_ILOG(headers->comp[0].vsamp) - GLJ_ILOG(headers->comp[1].vsamp);
    if (xdec == 0 && ydec == 0) return JPEG_SUBSAMP_444;
    if (xdec == 1 && ydec == 0) return JPEG_SUBSAMP_422;
    if (xdec == 1 && ydec == 1) return JPEG_SUBSAMP_420;
    if (xdec == 0 && ydec == 1) return JPEG_SUBSAMP_440;
    if (xdec == 2 && ydec == 0) return JPEG_SUBSAMP_411;
  }
  return JPEG_SUBSAMP_UNKNOWN;
}

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
  int nhmb;
  int nvmb;
  int i;

  if (jpeg_read_header(&ctx->cinfo, TRUE) != JPEG_HEADER_OK) {
    fprintf(stderr, "Error reading jpeg heaers\n");
    return EXIT_FAILURE;
  }

  /* Copy jpeg headers out of libjpeg decoder struct */
  headers->width = ctx->cinfo.image_width;
  headers->height = ctx->cinfo.image_height;
  headers->bits = ctx->cinfo.data_precision;
  headers->ncomps = ctx->cinfo.num_components;
  headers->restart_interval = ctx->cinfo.restart_interval;

  if (headers->ncomps != 1 && headers->ncomps != 3) {
    fprintf(stderr, "Unsupported number of components %i\n", headers->ncomps);
    return EXIT_FAILURE;
  }

  nhmb = (headers->width + (ctx->cinfo.max_h_samp_factor << 3) - 1)/
   (ctx->cinfo.max_h_samp_factor << 3);
  nvmb = (headers->height + (ctx->cinfo.max_v_samp_factor << 3) - 1)/
   (ctx->cinfo.max_v_samp_factor << 3);

  for (i = 0; i < NUM_QUANT_TBLS; i++) {
    JQUANT_TBL *tbl;
    tbl = ctx->cinfo.quant_tbl_ptrs[i];
    headers->quant[i].valid = tbl != NULL;
    if (headers->quant[i].valid) {
      /* Assume 8 bit quantizers */
      headers->quant[i].bits = 8;
      memcpy(headers->quant[i].tbl, tbl->quantval, 64*sizeof(unsigned short));
    }
  }

  for (i = 0; i < headers->ncomps; i++) {
    jpeg_component_info *info;
    jpeg_component *comp;
    info = &ctx->cinfo.comp_info[i];
    comp = &headers->comp[i];
    comp->hblocks = nhmb*info->h_samp_factor;
    comp->vblocks = nvmb*info->v_samp_factor;
    comp->hsamp = info->h_samp_factor;
    comp->vsamp = info->v_samp_factor;
    if (ctx->cinfo.quant_tbl_ptrs[info->quant_tbl_no] == NULL) {
      fprintf(stderr, "Missing quantization table for components %i\n", i);
      return EXIT_FAILURE;
    }
    comp->quant = &headers->quant[info->quant_tbl_no];
  }

  headers->subsamp = decode_subsamp(headers);

  return EXIT_SUCCESS;
}

static int libjpeg_decode_image(libjpeg_decode_ctx *ctx, image *img,
 jpeg_decode_out out) {
  switch (out) {
    case JPEG_DECODE_QUANT : {
      int i, j;
      jvirt_barray_ptr *coeffs;
      coeffs = jpeg_read_coefficients(&ctx->cinfo);
      for (i = 0; i < ctx->cinfo.num_components; i++) {
        jpeg_component_info *info;
        JBLOCKARRAY buf;
        short *coef;
        JDIMENSION r, bx;
        info = &ctx->cinfo.comp_info[i];
        coef = img->plane[i].coef;
        for (r = 0; r < info->height_in_blocks; r += info->v_samp_factor) {
          buf = (ctx->cinfo.mem->access_virt_barray)
           ((j_common_ptr)&ctx->cinfo, coeffs[i], r, info->v_samp_factor, 0);
          for (j = 0; j < info->v_samp_factor; j++) {
            for (bx = 0; bx < info->width_in_blocks; bx++) {
              memcpy(coef, buf[j][bx], sizeof(JBLOCK));
              coef += 64;
            }
          }
        }
      }
      break;
    }
    case JPEG_DECODE_YUV : {
      JSAMPROW yrow_pointer[16];
      JSAMPROW cbrow_pointer[16];
      JSAMPROW crrow_pointer[16];
      JSAMPROW *plane_pointer[3];

      plane_pointer[0] = yrow_pointer;
      plane_pointer[1] = cbrow_pointer;
      plane_pointer[2] = crrow_pointer;

      ctx->cinfo.raw_data_out = TRUE;
      ctx->cinfo.do_fancy_upsampling = FALSE;
      /* JPEG optimization tools like mozjpeg (based on libjpeg) assume a
          specific DCT implementation when doing rate-distortion trellis
          optimization for coefficient coding.
         The JDCT_ISLOW is the default DCT method in mozjpeg and is also the
          most accurate. */
      ctx->cinfo.dct_method = JDCT_ISLOW;

      jpeg_start_decompress(&ctx->cinfo);

      while (ctx->cinfo.output_scanline < ctx->cinfo.output_height) {
        int i, j;

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
      break;
    }
    case JPEG_DECODE_RGB : {
      JSAMPROW row_pointer[1];

      ctx->cinfo.do_fancy_upsampling = FALSE;
      /* JPEG optimization tools like mozjpeg (based on libjpeg) assume a
          specific DCT implementation when doing rate-distortion trellis
          optimization for coefficient coding.
         The JDCT_ISLOW is the default DCT method in mozjpeg and is also the
          most accurate. */
      ctx->cinfo.dct_method = JDCT_ISLOW;

      jpeg_start_decompress(&ctx->cinfo);

      row_pointer[0] = img->pixels;
      while (ctx->cinfo.output_scanline < ctx->cinfo.output_height) {
        jpeg_read_scanlines(&ctx->cinfo, row_pointer, 1);
        /* TODO add support for 16-bit output later */
        row_pointer[0] += img->width*img->nplanes;
      }
      jpeg_finish_decompress(&ctx->cinfo);
      break;
    }
    default : {
      fprintf(stderr, "Unsupported output '%s' for libjpeg wrapper.\n",
       JPEG_DECODE_OUT_NAMES[out]);
      return EXIT_FAILURE;
    }
  }

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

static int xjpeg_decode_header_(xjpeg_decode_ctx *ctx, jpeg_header *headers) {
  xjpeg_frame_header *frame;
  int i;

  xjpeg_decode_header(ctx);

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
  headers->bits = frame->bits;
  headers->ncomps = frame->ncomps;
  headers->restart_interval = ctx->restart_interval;

  for (i = 0; i < NQUANT_MAX; i++) {
    headers->quant[i].valid = ctx->quant[i].valid;
    if (headers->quant[i].valid) {
      headers->quant[i].bits = ctx->quant[i].bits;
      memcpy(headers->quant[i].tbl, ctx->quant[i].tbl,
       64*sizeof(unsigned short));
    }
  }

  for (i = 0; i < headers->ncomps; i++) {
    xjpeg_comp_info *info;
    jpeg_component *comp;
    info = &frame->comp[i];
    comp = &headers->comp[i];
    comp->hblocks = frame->nhmb*info->hsamp;
    comp->vblocks = frame->nvmb*info->vsamp;
    comp->hsamp = info->hsamp;
    comp->vsamp = info->vsamp;
    if (!ctx->quant[info->tq].valid) {
      fprintf(stderr, "Invalid quantization table for components %i\n", i);
      return EXIT_FAILURE;
    }
    comp->quant = &headers->quant[info->tq];
  }

  headers->subsamp = decode_subsamp(headers);

  return EXIT_SUCCESS;
}

static int xjpeg_decode_image_(xjpeg_decode_ctx *ctx, image *img,
 jpeg_decode_out out) {
  switch (out) {
    case JPEG_DECODE_PACK :
    case JPEG_DECODE_QUANT :
    case JPEG_DECODE_DCT :
    case JPEG_DECODE_YUV : {
      xjpeg_decode_image(ctx, img, (xjpeg_decode_out)out);
      if (ctx->error) {
        fprintf(stderr, "%s\n", ctx->error);
        return EXIT_FAILURE;
      }
      break;
    }
    default : {
      fprintf(stderr, "Unsupported output '%s' for xjpeg wrapper.\n",
       JPEG_DECODE_OUT_NAMES[out]);
      return EXIT_FAILURE;
    }
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
  (jpeg_decode_header_func)xjpeg_decode_header_,
  (jpeg_decode_image_func)xjpeg_decode_image_,
  (jpeg_decode_reset_func)xjpeg_decode_reset,
  (jpeg_decode_free_func)xjpeg_decode_free
};
