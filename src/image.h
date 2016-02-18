#if !defined(_image_H)
# define _image_H (1)

#include "jpeg_info.h"

#define NPLANES_MAX (3)

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
  short *coef;
  int cstride;
};

typedef struct image image;

struct image {
  unsigned short width;
  unsigned short height;
  int nplanes;
  image_plane plane[NPLANES_MAX];
  short *coef;
  unsigned char *pixels;
};

int image_init(image *img, jpeg_header *header);
void image_clear(image *img);

#endif
