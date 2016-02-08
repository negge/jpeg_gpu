#if !defined(_jpeg_wrap_H)
#define _jpeg_wrap_H (1)

#include "image.h"
#include "jpeg_info.h"

typedef struct jpeg_decode_ctx jpeg_decode_ctx;

typedef enum jpeg_decode_out {
  JPEG_DECODE_QUANT,
  JPEG_DECODE_DCT,
  JPEG_DECODE_YUV,
  JPEG_DECODE_RGB
} jpeg_decode_out;

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
