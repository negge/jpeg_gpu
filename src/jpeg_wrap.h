#if !defined(_jpeg_wrap_H)
#define _jpeg_wrap_H (1)

#define NCOMPS_MAX (3)

#define NPLANES_MAX (3)

typedef struct jpeg_component jpeg_component;

struct jpeg_component {
  int hblocks;
  int vblocks;
  int hsamp;
  int vsamp;
};

typedef struct jpeg_header jpeg_header;

struct jpeg_header {
  int bits;
  int width;
  int height;
  int ncomps;
  jpeg_component comp[NCOMPS_MAX];
};

typedef struct jpeg_info jpeg_info;

struct jpeg_info {
  int size;
  unsigned char *buf;
};

int jpeg_info_init(jpeg_info *info, const char *name);
void jpeg_info_clear(jpeg_info *info);

typedef struct image_plane image_plane;

struct image_plane {
  int bitdepth;
  unsigned char xdec;
  unsigned char ydec;
  int xstride;
  int ystride;
  unsigned short width;
  unsigned short height;
  unsigned char *data;
};

typedef struct image image;

struct image {
  unsigned short width;
  unsigned short height;
  int nplanes;
  image_plane plane[NPLANES_MAX];
};

int image_init(image *img, jpeg_header *header);
void image_clear(image *img);

typedef struct jpeg_decode_ctx jpeg_decode_ctx;

typedef jpeg_decode_ctx *(*jpeg_decode_alloc_func)(jpeg_info *info);
typedef int (*jpeg_decode_header_func)(jpeg_decode_ctx *dec,
 jpeg_header *header);
typedef void (*jpeg_decode_free_func)(jpeg_decode_ctx *dec);

typedef struct jpeg_decode_ctx_vtbl jpeg_decode_ctx_vtbl;

struct jpeg_decode_ctx_vtbl {
  jpeg_decode_alloc_func decode_alloc;
  jpeg_decode_header_func decode_header;
  jpeg_decode_free_func decode_free;
};

extern const jpeg_decode_ctx_vtbl LIBJPEG_DECODE_CTX_VTBL;
extern const jpeg_decode_ctx_vtbl XJPEG_DECODE_CTX_VTBL;

#endif
