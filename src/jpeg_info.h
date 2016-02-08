#if !defined(_jpeg_info_H)
# define _jpeg_info_H (1)

# define NCOMPS_MAX (3)

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

#endif
