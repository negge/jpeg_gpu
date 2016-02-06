#if !defined(_jpeg_wrap_H)
#define _jpeg_wrap_H (1)

#define NPLANES_MAX (3)

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

void image_clear(image *img);

#endif
