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

#if !defined(_jpeg_wrap_H)
#define _jpeg_wrap_H (1)

#include "image.h"
#include "jpeg_info.h"

typedef struct jpeg_decode_ctx jpeg_decode_ctx;

typedef enum jpeg_decode_out {
  JPEG_DECODE_QUANT,
  JPEG_DECODE_DCT,
  JPEG_DECODE_YUV,
  JPEG_DECODE_RGB,
  JPEG_DECODE_OUT_MAX
} jpeg_decode_out;

extern const char *JPEG_DECODE_OUT_NAMES[JPEG_DECODE_OUT_MAX];

typedef jpeg_decode_ctx *(*jpeg_decode_alloc_func)(jpeg_info *info);
typedef int (*jpeg_decode_header_func)(jpeg_decode_ctx *dec,
 jpeg_header *header);
typedef int (*jpeg_decode_image_func)(jpeg_decode_ctx *dec, image *img,
 jpeg_decode_out out);
typedef void (*jpeg_decode_reset_func)(jpeg_decode_ctx *dec, jpeg_info *info);
typedef void (*jpeg_decode_free_func)(jpeg_decode_ctx *dec);

typedef struct jpeg_decode_ctx_vtbl jpeg_decode_ctx_vtbl;

struct jpeg_decode_ctx_vtbl {
  jpeg_decode_alloc_func decode_alloc;
  jpeg_decode_header_func decode_header;
  jpeg_decode_image_func decode_image;
  jpeg_decode_reset_func decode_reset;
  jpeg_decode_free_func decode_free;
};

extern const jpeg_decode_ctx_vtbl LIBJPEG_DECODE_CTX_VTBL;
extern const jpeg_decode_ctx_vtbl XJPEG_DECODE_CTX_VTBL;

#endif
