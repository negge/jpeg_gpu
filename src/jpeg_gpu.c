#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>

#define NPLANES_MAX (3)

#define NCOMPS_MAX (4)

#define NQUANT_MAX (4)

#define NHUFF_MAX (4)

#define LOOKUP_BITS (8)

#define LOGGING_ENABLED (0)

#if LOGGING_ENABLED
# define JGPU_LOG(a) printf a
#else
# define JGPU_LOG(a) (void)0
#endif

#define JGPU_ERROR(ctx, cond, err) \
  do { \
    if (cond) { \
      (ctx)->error = (err); \
      return; \
    } \
  } \
  while (0)

#define NAME "jpeg_gpu"

int od_ilog(uint32_t _v) {
  /*On a Pentium M, this branchless version tested as the fastest on
     1,000,000,000 random 32-bit integers, edging out a similar version with
     branches, and a 256-entry LUT version.*/
  int ret;
  int m;
  ret = !!_v;
  m = !!(_v&0xFFFF0000)<<4;
  _v >>= m;
  ret |= m;
  m = !!(_v&0xFF00)<<3;
  _v >>= m;
  ret |= m;
  m = !!(_v&0xF0)<<2;
  _v >>= m;
  ret |= m;
  m = !!(_v&0xC)<<1;
  _v >>= m;
  ret |= m;
  ret += !!(_v&0x2);
  return ret;
}

#define OD_ILOG(x) (od_ilog(x))

#define PAD_POWER2(x, b) ((x + ((1 << b) - 1)) & ~((1 << b) - 1))

/*Clamps a signed integer between 0 and 255, returning an unsigned char.
  This assumes a char is 8 bits.*/
#define OD_CLAMP255(x) \
 ((unsigned char)((((x) < 0) - 1) & ((x) | -((x) > 255))))

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
  int *coeffs;
};

typedef struct image image;

struct image {
  unsigned short width;
  unsigned short height;
  int nplanes;
  image_plane plane[NPLANES_MAX];
};

typedef struct jpeg_gpu_quant jpeg_gpu_quant;

struct jpeg_gpu_quant {
  int valid;
  unsigned char bits;
  unsigned short tbl[64];
};

typedef struct jpeg_gpu_plane_info jpeg_gpu_plane_info;

struct jpeg_gpu_plane_info {
  unsigned char id;
  unsigned char hsamp;
  unsigned char vsamp;
  /* */
  unsigned char tq;

  image_plane *plane;
};

typedef struct jpeg_gpu_frame_header jpeg_gpu_frame_header;

struct jpeg_gpu_frame_header {
  int valid;
  unsigned char bits;
  unsigned short pic_width;
  unsigned short pic_height;
  unsigned char nplanes;
  jpeg_gpu_plane_info plane[NPLANES_MAX];

  unsigned char hmax;
  unsigned char vmax;
  unsigned short nhmb;
  unsigned short nvmb;
};

typedef struct jpeg_gpu_scan_component jpeg_gpu_scan_component;

struct jpeg_gpu_scan_component {
  unsigned char id;
  /* DC entropy coding table index */
  unsigned char td;
  /* AC entropy coding table index */
  unsigned char ta;

  int dc_pred;
  jpeg_gpu_plane_info *plane;
};

typedef struct jpeg_gpu_scan_header jpeg_gpu_scan_header;

struct jpeg_gpu_scan_header {
  int valid;
  unsigned char ncomps;
  jpeg_gpu_scan_component comp[NCOMPS_MAX];
};

typedef struct jpeg_gpu_huff jpeg_gpu_huff;

struct jpeg_gpu_huff {
  int valid;
  int bits[16];
  unsigned short codeword[256];
  unsigned char symbol[256];
  int lookup[256];
  int index[16];
  int maxcode[16];
  int symbs;
};

typedef struct jpeg_gpu_ctx jpeg_gpu_ctx;

struct jpeg_gpu_ctx {
  const char *fname;
  int size;
  unsigned char *buf;
  const unsigned char *pos;

  unsigned char marker;

  int bits;
  unsigned int bitbuf;

  const char *error;
  int end_of_image;

  short restart_interval;

  jpeg_gpu_quant quant[NQUANT_MAX];
  jpeg_gpu_huff dc_huff[NHUFF_MAX];
  jpeg_gpu_huff ac_huff[NHUFF_MAX];
  jpeg_gpu_frame_header frame;
  jpeg_gpu_scan_header scan;

  image img;
};

static const int ZIG_ZAG[64] = {
   0,  1,  5,  6, 14, 15, 27, 28,
   2,  4,  7, 13, 16, 26, 29, 42,
   3,  8, 12, 17, 25, 30, 41, 43,
   9, 11, 18, 24, 31, 40, 44, 53,
  10, 19, 23, 32, 39, 45, 52, 54,
  20, 22, 33, 38, 46, 51, 55, 60,
  21, 34, 37, 47, 50, 56, 59, 61,
  35, 36, 48, 49, 57, 58, 62, 63
};

static const int DE_ZIG_ZAG[64] = {
   0,  1,  8, 16,  9,  2,  3, 10,
  17, 24, 32, 25, 18, 11,  4,  5,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13,  6,  7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63
};

static void jgpu_image_init(jpeg_gpu_ctx *ctx) {
  jpeg_gpu_frame_header *frame;
  int i;
  frame = &ctx->frame;
  JGPU_ERROR(ctx, !frame->valid,
   "Error, must see SOF before initializing image data.");
  ctx->img.width = frame->pic_width;
  ctx->img.height = frame->pic_height;
  ctx->img.nplanes = frame->nplanes;
  for (i = 0; i < frame->nplanes; i++) {
    image_plane *plane;
    plane = &ctx->img.plane[i];
    plane->bitdepth = frame->bits;
    plane->xdec = OD_ILOG(frame->hmax) - OD_ILOG(frame->plane[i].hsamp);
    plane->ydec = OD_ILOG(frame->vmax) - OD_ILOG(frame->plane[i].vsamp);
    /* Compute the component dimensions */
    plane->width = frame->pic_width*frame->plane[i].hsamp/frame->hmax;
    plane->height = frame->pic_height*frame->plane[i].vsamp/frame->vmax;
    /* Pad out to the next block of 8 pixels */
    plane->width = PAD_POWER2(plane->width, 3);
    plane->height = PAD_POWER2(plane->height, 3);
    plane->xstride = plane->bitdepth >> 3;
    plane->ystride = plane->xstride*plane->width;
    plane->data = (unsigned char *)malloc(plane->height*plane->ystride);
    JGPU_ERROR(ctx, !plane->data, "Error allocating image plane memory.");
    plane->coeffs =
     (int *)malloc(plane->height*plane->width*sizeof(*plane->coeffs));
    JGPU_ERROR(ctx, !plane->coeffs, "Error allocating coeffs plane memory.");
    JGPU_LOG((
     " P[%i]: %ix%i, xdec = %i, ydec = %i, xstride = %i, ystride = %i\n",
     i, plane->width, plane->height, plane->xdec, plane->ydec, plane->xstride,
     plane->ystride));
    frame->plane[i].plane = plane; 
  }
}

static void jgpu_image_clear(jpeg_gpu_ctx *ctx) {
  int i;
  for (i = 0; i < NPLANES_MAX; i++) {
    free(ctx->img.plane[i].data);
    free(ctx->img.plane[i].coeffs);
  }
}

static int jgpu_init(jpeg_gpu_ctx *ctx, const char *fname) {
  FILE *fp;
  int size;
  int i;
  memset(ctx, 0, sizeof(*ctx));
  ctx->fname = fname;
  fp = fopen(fname, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Error, could not open jpeg file %s\n", ctx->fname);
    return 1;
  }
  fseek(fp, 0, SEEK_END);
  ctx->size = ftell(fp);
  ctx->buf = malloc(ctx->size);
  if (ctx->buf == NULL) {
    fprintf(stderr, "Error, could not allocate %i bytes\n", ctx->size);
    return 1;
  }
  fseek(fp, 0, SEEK_SET);
  size = fread(ctx->buf, 1, ctx->size, fp);
  if (size != ctx->size) {
    fprintf(stderr, "Error reading jpeg file, got %i of %i bytes\n", size,
     ctx->size);
    return 1;
  }
  fclose(fp);
  ctx->pos = ctx->buf;
  ctx->marker = 0;
  ctx->bits = 0;
  ctx->bitbuf = 0;
  ctx->error = 0;
  ctx->end_of_image = 0;
  for (i = 0; i < NQUANT_MAX; i++) {
    ctx->quant[i].valid = 0;
  }
  for (i = 0; i < NHUFF_MAX; i++) {
    ctx->dc_huff[i].valid = 0;
    ctx->ac_huff[i].valid = 0;
  }
  ctx->frame.valid = 0;
  ctx->scan.valid = 0;
  for (i = 0; i < NCOMPS_MAX; i++) {
    ctx->scan.comp[i].dc_pred = 0;
  }
  for (i = 0; i < NPLANES_MAX; i++) {
    ctx->img.plane[i].data = NULL;
  }
  return 0;
}

static void jgpu_clear(jpeg_gpu_ctx *ctx) {
  free(ctx->buf);
  jgpu_image_clear(ctx);
}

#define JGPU_SKIP_BYTES(ctx, nbytes) \
  do { \
    JGPU_ERROR(ctx, (ctx)->size < nbytes, \
     "Error skipping past the end of file."); \
    (ctx)->pos += nbytes; \
    (ctx)->size -= nbytes; \
  } \
  while (0)

#define JGPU_DECODE_BYTE(ctx, ret) \
  do { \
    JGPU_ERROR(ctx, (ctx)->size < 1, "Error reading past the end of file."); \
    ret = (ctx)->pos[0]; \
    JGPU_SKIP_BYTES(ctx, 1); \
  } \
  while (0)

#define JGPU_DECODE_SHORT(ctx, ret) \
  do { \
    JGPU_ERROR(ctx, (ctx)->size < 2, "Error reading past the end of file."); \
    ret = ((ctx)->pos[0] << 8) | (ctx)->pos[1]; \
    JGPU_SKIP_BYTES(ctx, 2); \
  } \
  while (0)

#define JGPU_FILL_BITS(ctx, nbits) \
  do { \
    while ((ctx)->bits < nbits) { \
      unsigned char byte; \
      JGPU_ERROR(ctx, ctx->marker, "Error found marker when filling bits."); \
      JGPU_DECODE_BYTE(ctx, byte); \
      if (byte == 0xFF) { \
        do { \
          JGPU_DECODE_BYTE(ctx, byte); \
        } \
        while (byte == 0xFF); \
        if (byte == 0) { \
          byte = 0xFF; \
        } \
        else { \
          (ctx)->marker = byte; \
          (ctx)->bits += nbits; \
          (ctx)->bitbuf <<= nbits; \
           break; \
        } \
      } \
      (ctx)->bits += 8; \
      (ctx)->bitbuf = ((ctx)->bitbuf << 8) | byte; \
    } \
  } \
  while (0)

#define JGPU_SKIP_BITS(ctx, nbits) \
  do { \
    JGPU_FILL_BITS(ctx, nbits); \
    (ctx)->bits -= nbits; \
  } \
  while (0)

#define JGPU_PEEK_BITS(ctx, nbits, ret) \
  do { \
    JGPU_FILL_BITS(ctx, nbits); \
    JGPU_ERROR(ctx, (ctx)->bits < nbits, "Error not enough bits to peek."); \
    ret = (((ctx)->bitbuf >> ((ctx)->bits - (nbits))) & ((1 << (nbits)) - 1)); \
  } \
  while (0)

#define JGPU_DECODE_BITS(ctx, nbits, ret) \
  do { \
    JGPU_FILL_BITS(ctx, nbits); \
    JGPU_ERROR(ctx, (ctx)->bits < nbits, "Error not enough bits to get."); \
    ret = (((ctx)->bitbuf >> ((ctx)->bits - (nbits))) & ((1 << (nbits)) - 1)); \
    (ctx)->bits -= nbits; \
  } \
  while (0)

#define JGPU_DECODE_HUFF(ctx, huff, symbol) \
  do { \
    int bits; \
    int value; \
    JGPU_FILL_BITS(ctx, 2*LOOKUP_BITS); \
    JGPU_PEEK_BITS(ctx, LOOKUP_BITS, value); \
    JGPU_LOG(("lookup = %2x\n", value)); \
    symbol = (huff)->lookup[value]; \
    bits = symbol >> LOOKUP_BITS; \
    symbol &= (1 << LOOKUP_BITS) - 1; \
    JGPU_SKIP_BITS(ctx, bits); \
    JGPU_LOG(("bits = %i, code = %02X, symbol = %02x\n", bits, \
     value >> (LOOKUP_BITS - bits), symbol)); \
    if (bits > LOOKUP_BITS) { \
      value = ((ctx)->bitbuf >> (ctx)->bits) & ((1 << bits) - 1); \
      JGPU_LOG(("starting value = %X\n", value)); \
      JGPU_LOG(("maxcode->bits[%i] = %i\n", bits, (huff)->maxcode[bits - 1])); \
      while (value > (huff)->maxcode[bits - 1]) { \
        int bit; \
        JGPU_DECODE_BITS(ctx, 1, bit); \
        value = (value << 1) | bit; \
        bits++; \
      } \
      symbol = (huff)->symbol[value + (huff)->index[bits - 1]]; \
      JGPU_LOG(("bits = %i, code = %02X, symbol = %02x\n", bits, \
       value >> (LOOKUP_BITS - bits), symbol)); \
    } \
  } \
  while (0)

#define JGPU_DECODE_VLC(ctx, huff, symbol, value) \
  do { \
    int len; \
    JGPU_DECODE_HUFF(ctx, huff, symbol); \
    len = symbol & 0xf; \
    JGPU_DECODE_BITS(ctx, len, value); \
    JGPU_LOG(("len = %i\n", len)); \
    if (value < 1 << (len - 1)) { \
      value += 1 - (1 << len); \
    } \
    JGPU_LOG(("vlc = %i\n", value)); \
    JGPU_LOG(("\n")); \
  } \
  while (0)

static void jgpu_skip_marker(jpeg_gpu_ctx *ctx) {
  unsigned short len;
  JGPU_DECODE_SHORT(ctx, len);
  JGPU_LOG((" skipping length %i\n", len - 2));
  JGPU_SKIP_BYTES(ctx, len - 2);
}

static void jgpu_decode_dqt(jpeg_gpu_ctx *ctx) {
  unsigned short len;
  JGPU_DECODE_SHORT(ctx, len);
  len -= 2;
  while (len >= 65) {
    unsigned char byte;
    unsigned char pq;
    unsigned char tq;
    jpeg_gpu_quant *quant;
    int i;
    JGPU_DECODE_BYTE(ctx, byte);
    pq = byte >> 4;
    JGPU_ERROR(ctx, pq > 1, "Error DQT expected Pq value 0 or 1.");
    tq = byte & 0x7;
    JGPU_ERROR(ctx, tq > 3, "Error DQT expected Tq value 0 to 3.");
    quant = &ctx->quant[tq];
    quant->valid = 1;
    quant->bits = pq ? 16 : 8;
    JGPU_LOG(("Reading Quantization Table %i (%i-bit)\n", tq, quant->bits));
    for (i = 0; i < 64; i++) {
      if (pq) {
        JGPU_DECODE_SHORT(ctx, quant->tbl[i]);
      }
      else {
        JGPU_DECODE_BYTE(ctx, quant->tbl[i]); 
      }
      /*quant->tbl[i] = pq ? jgpu_decode_short(ctx) : jgpu_decode_byte(ctx);*/
    }
    len -= 65 + 64*pq;
#if LOGGING_ENABLED
    for (i = 1; i <= 64; i++) {
      printf("%i%s", quant->tbl[ZIG_ZAG[i - 1]], i & 0x7 ? ", " : "\n");
    }
#endif
  }
  JGPU_ERROR(ctx, len != 0, "Error decoding DQT, unprocessed bytes.");
}

static void jgpu_decode_sof(jpeg_gpu_ctx *ctx) {
  unsigned short len;
  jpeg_gpu_frame_header *frame;
  int i;
  int mbw;
  int mbh;
  JGPU_DECODE_SHORT(ctx, len);
  len -= 2;
  JGPU_ERROR(ctx, len < 9, "Error SOF needs at least 9 bytes");
  frame = &ctx->frame;
  JGPU_ERROR(ctx, frame->valid, "Error multiple SOF not supported.");
  frame->valid = 1;
  JGPU_DECODE_BYTE(ctx, frame->bits);
  JGPU_DECODE_SHORT(ctx, frame->pic_height);
  JGPU_ERROR(ctx, frame->pic_height == 0, "Error SOF has invalid height.");
  JGPU_DECODE_SHORT(ctx, frame->pic_width);
  JGPU_ERROR(ctx, frame->pic_width == 0, "Error SOF has invalid width.");
  JGPU_DECODE_BYTE(ctx, frame->nplanes);
  JGPU_LOG(("bits = %i, height = %i, width = %i, nplanes = %i\n",
   frame->bits, frame->pic_height, frame->pic_width, frame->nplanes));
  JGPU_ERROR(ctx, frame->nplanes != 1 && frame->nplanes != 3,
   "Error only frames with 1 or 3 components supported");
  len -= 6;
  JGPU_ERROR(ctx, len < 3*frame->nplanes,
   "Error SOF needs more bytes than available.");
  frame->hmax = 0;
  frame->vmax = 0;
  for (i = 0; i < frame->nplanes; i++) {
    jpeg_gpu_plane_info *plane;
    unsigned char byte;
    plane = &frame->plane[i];
    JGPU_DECODE_BYTE(ctx, plane->id);
    JGPU_DECODE_BYTE(ctx, byte);
    plane->hsamp = byte >> 4;
    JGPU_ERROR(ctx, plane->hsamp == 0 || plane->hsamp > 4,
     "Error SOF expected Hi value 1 to 4.");
    JGPU_ERROR(ctx, plane->hsamp == 3, "Unsupported horizontal sampling.");
    if (plane->hsamp > frame->hmax) {
      frame->hmax = plane->hsamp;
    }
    plane->vsamp = byte & 0x7;
    JGPU_ERROR(ctx, plane->vsamp == 0 || plane->vsamp > 4,
     "Error SOF expected Vi value 1 to 4.");
    JGPU_ERROR(ctx, plane->vsamp == 3, "Unsupported vertical sampling.");
    if (plane->vsamp > frame->vmax) {
      frame->vmax = plane->vsamp;
    }
    JGPU_DECODE_BYTE(ctx, plane->tq);
    JGPU_ERROR(ctx, plane->tq > 3, "Error SOF expected Tq value 0 to 3.");
    JGPU_ERROR(ctx, !ctx->quant[plane->tq].valid,
     "Error SOF referenced invalid quantization table.");
    JGPU_ERROR(ctx, ctx->quant[plane->tq].bits > frame->bits,
     "Error SOF mismatch in frame bits and quantization table bits.");
    len -= 3;
  }
  JGPU_ERROR(ctx, len != 0, "Error decoding SOF, unprocessed bytes.");
  JGPU_ERROR(ctx, frame->hmax < 1 || frame->hmax > 4 || frame->hmax == 3,
   "Error unsupported hmax.");
  JGPU_ERROR(ctx, frame->vmax < 1 || frame->vmax > 4 || frame->vmax == 3,
   "Error unsupported vmax.");
  /* Compute the size in pixels of the MCU */
  mbw = frame->hmax << 3;
  mbh = frame->vmax << 3;
  /* Compute the number of horizontal and vertical MCU blocks for the frame. */
  frame->nhmb = (frame->pic_width + mbw - 1)/mbw;
  frame->nvmb = (frame->pic_height + mbh - 1)/mbh;
  JGPU_LOG(("hmax = %i, vmax = %i, mbw = %i, mbh = %i, nhmb = %i, nvmb = %i\n",
   frame->hmax, frame->vmax, mbw, mbh, frame->nhmb, frame->nvmb));
}

#if LOGGING_ENABLED
static void printBits(int value, int bits) {
  int i;
  for (i = 1; i <= bits; i++) {
    printf("%s%s", value & (1 << (bits - i)) ? "1" : "0", i%4 ? "" : " ");
  }
  for (; i <= 16; i++) {
    printf(" %s", i%4 ? "" : " ");
  }
}
#endif

static void jgpu_decode_dht(jpeg_gpu_ctx *ctx) {
  unsigned short len;
  JGPU_DECODE_SHORT(ctx, len);
  len -= 2;
  while (len >= 17) {
    unsigned char byte;
    unsigned int tc;
    unsigned int th;
    jpeg_gpu_huff *huff;
    int i, j, k, l;
    int total;
    unsigned short codeword;
    JGPU_DECODE_BYTE(ctx, byte);
    tc = byte >> 4;
    JGPU_ERROR(ctx, tc > 1, "Error DHT expected Tc value 0 or 1.");
    th = byte & 0x7;
    JGPU_ERROR(ctx, th > 3, "Error DHT expected Th value 0 to 3.");
    if (tc) {
      huff = &ctx->ac_huff[th];
    }
    else {
      huff = &ctx->dc_huff[th];
    }
    huff->valid = 1;
    total = 0;
    for (i = 0; i < 16; i++) { 
      JGPU_DECODE_BYTE(ctx, huff->bits[i]);
      total += huff->bits[i];
    }
    len -= 17;
    JGPU_ERROR(ctx, total > 256, "Error DHT has more than 256 symbols.");
    JGPU_ERROR(ctx, total > len, "Error DHT needs more bytes than available.");
    k = 0;
    codeword = 0;
    for (i = 0; i < 16; i++) {
      for (j = 0; j < huff->bits[i]; j++) {
        huff->codeword[k] = codeword;
        JGPU_DECODE_BYTE(ctx, huff->symbol[k]);
#if LOGGING_ENABLED
        printf("bits = %2i, codeword = ", i + 1);
        printBits(codeword, i + 1);
        printf(", symbol = %02X\n", huff->symbol[k]);
#endif
        k++;
        codeword++;
      }
      len -= huff->bits[i];
      JGPU_ERROR(ctx, codeword >= 1 << i + 1, "Error invalid DHT.");
      codeword <<= 1;
    }
    huff->symbs = k;
    k = 0;
    JGPU_LOG(("building index table"));
    for (i = 0; i < 16; i++) {
      huff->maxcode[i] = -1;
      if (huff->bits[i]) {
#if LOGGING_ENABLED
        printf("\nbits = %i, offset = %i\n", i + 1, k);
        printf("first codeword = ");
	printBits(huff->codeword[k], i + 1);
#endif
        huff->index[i] = k - huff->codeword[k];
	k += huff->bits[i];
#if LOGGING_ENABLED
	printf("\n last codeword = ");
	printBits(huff->codeword[k - 1], i + 1);
#endif
	huff->maxcode[i] = huff->codeword[k - 1];
      }
#if LOGGING_ENABLED
      printf("\n%i: index = %i, maxcode = ", i, huff->index[i]);
      printBits(huff->maxcode[i], i + 1);
#endif
    }
    /* Generate a lookup table to speed up decoding */
    for (i = 0; i < 1 << LOOKUP_BITS; i++) {
      huff->lookup[i] = (LOOKUP_BITS + 1) << LOOKUP_BITS;
    }
    k = 0;
    for (i = 1; i <= LOOKUP_BITS; i++) {
      for (j = 0; j < huff->bits[i - 1]; j++) {
        codeword = huff->codeword[k] << (LOOKUP_BITS - i);
        for (l = 0; l < 1 << LOOKUP_BITS - i; l++) {
	  huff->lookup[codeword] = (i << LOOKUP_BITS) | huff->symbol[k];
	  codeword++;
	}
	k++;
      }
    }
#if LOGGING_ENABLED
    for (i = 0; i < 1 << LOOKUP_BITS; i++) {
      int value;
      int bits;
      int code;
      value = huff->lookup[i];
      bits = value >> LOOKUP_BITS;
      code = value & ((1 << LOOKUP_BITS) - 1);
      printf("%02X, bits = %2i, codeword = ", i, bits);
      printBits(i >> (LOOKUP_BITS - bits), bits);
      printf(", symbol = %i\n", code);
    }
#endif
    /* If this is a DC table, validate that the symbols are between 0 and 15 */
    if (!tc) {
      for (i = 0; i < huff->symbs; i++) {
        JGPU_ERROR(ctx, huff->symbol[i] > 15, "Error invalid DC symbol.");
      }
    }
    JGPU_LOG(("Reading %s Huffman Table %i symbols %i\n", tc ? "AC" : "DC", th,
     huff->symbs));
  }
  JGPU_ERROR(ctx, len != 0, "Error decoding DHT, unprocessed bytes.");
}

static void jgpu_decode_rsi(jpeg_gpu_ctx *ctx) {
  unsigned short len;
  JGPU_DECODE_SHORT(ctx, len);
  len -= 2;
  JGPU_DECODE_SHORT(ctx, ctx->restart_interval);
  JGPU_LOG(("Restart Interval %i\n", ctx->restart_interval));
  len -= 2;
  JGPU_ERROR(ctx, len != 0, "Error decoding DRI, unprocessed bytes.");
}

static void jgpu_decode_sos(jpeg_gpu_ctx *ctx) {
  unsigned short len;
  jpeg_gpu_scan_header *scan;
  int i, j;
  unsigned char byte;
  JGPU_DECODE_SHORT(ctx, len);
  len -= 2;
  JGPU_ERROR(ctx, len < 6, "Error SOS needs at least 6 bytes");
  scan = &ctx->scan;
  JGPU_ERROR(ctx, scan->valid, "Error multiple SOS not supported.");
  scan->valid = 1;
  JGPU_DECODE_BYTE(ctx, scan->ncomps);
  JGPU_ERROR(ctx, scan->ncomps == 0 || scan->ncomps > 4,
   "Error SOS expected Ns value 1 to 4.");
  len--;
  JGPU_ERROR(ctx, scan->ncomps != 1 && scan->ncomps != 3,
   "Error only scans with 1 or 3 components supported");
  for (i = 0; i < scan->ncomps; i++) {
    jpeg_gpu_scan_component *comp;
    comp = &scan->comp[i];
    JGPU_DECODE_BYTE(ctx, comp->id);
    comp->plane = NULL;
    for (j = 0; j < ctx->frame.nplanes; j++) {
      if (ctx->frame.plane[j].id == comp->id) {
        comp->plane = &ctx->frame.plane[j];
        break;
      }
    }
    JGPU_ERROR(ctx, !comp->plane, "Error SOS references invalid component.");
    JGPU_DECODE_BYTE(ctx, byte);
    comp->td = byte >> 4;
    JGPU_ERROR(ctx, !ctx->dc_huff[comp->td].valid,
     "Error SOS component references invalid DC entropy table.");
    comp->ta = byte & 0x7;
    JGPU_ERROR(ctx, !ctx->ac_huff[comp->ta].valid,
     "Error SOS component references invalid AC entropy table.");
    len -= 2;
  }
  JGPU_DECODE_BYTE(ctx, byte);
  JGPU_ERROR(ctx, byte != 0, "Error SOS expected Ss value 0.");
  JGPU_DECODE_BYTE(ctx, byte);
  JGPU_ERROR(ctx, byte != 63, "Error SOS expected Se value 0.");
  JGPU_DECODE_BYTE(ctx, byte);
  JGPU_ERROR(ctx, byte >> 4 != 0, "Error SOS expected Ah value 0.");
  JGPU_ERROR(ctx, byte & 0x7 != 0, "Error SOS expected Al value 0.");
  len -= 3;
  JGPU_ERROR(ctx, len != 0, "Error decoding SOS, unprocessed bytes.");
}

/*This is the strength reduced version of ((_a)/(1 << (_b))).
  This will not work for _b == 0, however currently this is only used for
   b == 1 anyway.*/
#define OD_UNBIASED_RSHIFT32(_a, _b) \
 (((int32_t)(((uint32_t)(_a) >> (32 - (_b))) + (_a))) >> (_b))

#define OD_DCT_RSHIFT(_a, _b) OD_UNBIASED_RSHIFT32(_a, _b)

#define OD_IDCT_2(t0, t1) \
  /* Embedded 2-point orthonormal Type-II iDCT. */ \
  do { \
    /* 3393/8192 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    t0 += (t1*3393 + 4096) >> 13; \
    /* 5793/8192 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    t1 -= (t0*5793 + 4096) >> 13; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    t0 += (t1*13573 + 16384) >> 15; \
  } \
  while (0)

#define OD_IDST_2(t0, t1) \
  /* Embedded 2-point orthonormal Type-IV iDST. */ \
  do { \
    /* 10947/16384 ~= Tan[3*Pi/16]) ~= 0.668178637919299 */ \
    t0 += (t1*10947 + 8192) >> 14; \
    /* 473/512 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    t1 -= (t0*473 + 256) >> 9; \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    t0 += (t1*10947 + 8192) >> 14; \
  } \
  while (0)

#define OD_IDCT_4_ASYM(t0, t2, t1, t1h, t3, t3h) \
  /* Embedded 4-point asymmetric Type-II iDCT. */ \
  do { \
    OD_IDST_2(t3, t2); \
    OD_IDCT_2(t0, t1); \
    t1 = t2 - t1; \
    t1h = OD_DCT_RSHIFT(t1, 1); \
    t2 = t1h - t2; \
    t3 = t0 - t3; \
    t3h = OD_DCT_RSHIFT(t3, 1); \
    t0 -= t3h; \
  } \
  while (0)

#define OD_IDST_4_ASYM(t0, t0h, t2, t1, t3) \
  /* Embedded 4-point asymmetric Type-IV iDST. */ \
  do { \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    t1 -= (t2*8757 + 8192) >> 14; \
    /* 6811/8192 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    t2 += (t1*6811 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    t1 -= (t2*8757 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    t3 -= (t0*6723 + 4096) >> 13; \
    /* 8035/8192 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    t0 += (t3*8035 + 4096) >> 13; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    t3 -= (t0*6723 + 4096) >> 13; \
    t0 += t2; \
    t0h = OD_DCT_RSHIFT(t0, 1); \
    t2 = t0h - t2; \
    t1 += t3; \
    t3 -= OD_DCT_RSHIFT(t1, 1); \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    t1 -= (t2*19195 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    t2 -= (t1*11585 + 8192) >> 14; \
    /* 7489/8192 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    t1 += (t2*7489 + 4096) >> 13; \
  } \
  while (0)

#define OD_IDCT_8(r0, r4, r2, r6, r1, r5, r3, r7) \
  /* Embedded 8-point orthonormal Type-II iDCT. */ \
  do { \
    int r1h; \
    int r3h; \
    int r5h; \
    int r7h; \
    OD_IDST_4_ASYM(r7, r7h, r5, r6, r4); \
    OD_IDCT_4_ASYM(r0, r2, r1, r1h, r3, r3h); \
    r0 += r7h; \
    r7 = r0 - r7; \
    r6 = r1h - r6; \
    r1 -= r6; \
    r5h = OD_DCT_RSHIFT(r5, 1); \
    r2 += r5h; \
    r5 = r2 - r5; \
    r4 = r3h - r4; \
    r3 -= r4; \
  } \
  while (0)

static void od_bin_idct8(int *x, int xstride, const int y[8]) {
  int t0;
  int t1;
  int t2;
  int t3;
  int t4;
  int t5;
  int t6;
  int t7;
  t0 = y[0];
  t4 = y[1];
  t2 = y[2];
  t6 = y[3];
  t1 = y[4];
  t5 = y[5];
  t3 = y[6];
  t7 = y[7];
  OD_IDCT_8(t0, t4, t2, t6, t1, t5, t3, t7);
  x[0*xstride] = t0;
  x[1*xstride] = t1;
  x[2*xstride] = t2;
  x[3*xstride] = t3;
  x[4*xstride] = t4;
  x[5*xstride] = t5;
  x[6*xstride] = t6;
  x[7*xstride] = t7;
}

static void od_bin_idct8x8(int *x, int xstride, const int *y, int ystride) {
  int z[8*8];
  int i;
  for (i = 0; i < 8; i++) od_bin_idct8(z + i, 8, y + ystride*i);
  for (i = 0; i < 8; i++) od_bin_idct8(x + i, xstride, z + 8*i);
}

static void jgpu_decode_scan(jpeg_gpu_ctx *ctx) {
  int mcu_counter;
  int rst_counter;
  int mbx;
  int mby;
  mcu_counter = ctx->restart_interval;
  rst_counter = 0;
  for (mby = 0; mby < ctx->frame.nvmb; mby++) {
    for (mbx = 0; mbx < ctx->frame.nhmb; mbx++) {
      int i;
      jpeg_gpu_scan_component *comp;
      for (i = 0, comp = ctx->scan.comp; i < ctx->scan.ncomps; i++, comp++) {
        jpeg_gpu_plane_info *pi;
        image_plane *ip;
        int sby;
        int sbx;
        pi = comp->plane;
        ip = pi->plane;
        for (sby = 0; sby < pi->vsamp; sby++) {
          for (sbx = 0; sbx < pi->hsamp; sbx++) {
            int block[64];
            int symbol;
            int value;
            int j, k;
            int *coeffs;
            unsigned char *data;
            memset(block, 0, sizeof(block));
            JGPU_DECODE_VLC(ctx, &ctx->dc_huff[comp->td], symbol, value);
            JGPU_LOG(("dc = %i\n", value));
            comp->dc_pred += value;
            JGPU_LOG(("dc_pred = %i\n", comp->dc_pred));
            j = 0;
            block[0] = comp->dc_pred*ctx->quant[pi->tq].tbl[0];
            do {
              JGPU_DECODE_VLC(ctx, &ctx->ac_huff[comp->ta], symbol, value);
              if (!symbol) {
                JGPU_LOG(("****************** EOB at j = %i\n\n", j));
                break;
              }
              j += (symbol >> 4) + 1;
              JGPU_LOG(("j = %i, offset = %i, value = %i, dequant = %i\n", j,
               (symbol >> 4) + 1, value, value*ctx->quant[pi->tq].tbl[j]));
	      JGPU_ERROR(ctx, j > 63, "Error indexing outside block.");
              block[DE_ZIG_ZAG[j]] = value*ctx->quant[pi->tq].tbl[j];
            }
            while (j < 63);
#if LOGGING_ENABLED
	    for (j = 1; j <= 64; j++) {
	      printf("%5i%s", block[j - 1], j & 0x7 ? ", " : "\n");
	    }
#endif
            coeffs = ip->coeffs +
             ((mby*pi->vsamp + sby)*ip->width << 3) +
             ((mbx*pi->hsamp + sbx) << 3);
            for (k = 0; k < 8; k++) {
              for (j = 0; j < 8; j++) {
                coeffs[k*ip->width + j] = block[k*8 + j];
              }
            }
            od_bin_idct8x8(block, 8, block, 8);
            data = ip->data +
             ((mby*pi->vsamp + sby)*ip->ystride << 3) +
             ((mbx*pi->hsamp + sbx)*ip->xstride << 3);
            for (k = 0; k < 8; k++) {
              for (j = 0; j < 8; j++) {
                data[k*ip->ystride + j] = OD_CLAMP255(block[k*8 + j] + 128);
              }
            }
          }
        }
      }
      mcu_counter--;
      JGPU_LOG(("mbx = %i, mby = %i, rst_counter = %i, mcu_counter = %i\n",
       mbx, mby, rst_counter, mcu_counter));
      if (ctx->restart_interval && mcu_counter == 0) {
        JGPU_ERROR(ctx, !ctx->marker, "Error, expected to find marker.");
        switch (ctx->marker) {
          case 0xD0 :
          case 0xD1 :
          case 0xD2 :
          case 0xD3 :
          case 0xD4 :
          case 0xD5 :
          case 0xD6 :
          case 0xD7 : {
            JGPU_ERROR(ctx, (ctx->marker & 0x7) != (rst_counter & 0x7),
              "Error invalid RST counter in marker.");
            ctx->marker = 0;
            ctx->bits = 0;
            mcu_counter = ctx->restart_interval;
            rst_counter++;
            for (i = 0; i < ctx->scan.ncomps; i++) {
              ctx->scan.comp[i].dc_pred = 0;
            }
            break;
          }
          /* End of Image marker */
          case 0xD9 : {
            return;
          }
          default : {
            JGPU_ERROR(ctx, 1, "Error, unknown marker found in scan.");
          }
        }
      }
    }
  }
}

static void jgpu_decode_soi(jpeg_gpu_ctx *ctx) {
  JGPU_ERROR(ctx, ctx->size < 2, "Error, not a JPEG (too small).");
  JGPU_ERROR(ctx, ctx->pos[0] != 0xFF || ctx->pos[1] != 0xD8,
   "Error, not a JPEG (invalid SOI marker)");
  JGPU_LOG(("Start of Image\n"));
  JGPU_SKIP_BYTES(ctx, 2);
}

static void jgpu_decode_eoi(jpeg_gpu_ctx *ctx) {
  JGPU_LOG((stderr, "End of Image reached\n"));
  ctx->end_of_image = 1;
  JGPU_ERROR(ctx, ctx->size != 0, "Error decoding EOI, unprocessed bytes.");
}

static void jgpu_decode(jpeg_gpu_ctx *ctx) {
  /* Check for Start of Image marker */
  jgpu_decode_soi(ctx);
  while (!ctx->error && !ctx->end_of_image) {
    unsigned char marker;
    marker = ctx->marker;
    ctx->marker = 0;
    if (marker == 0) {
      JGPU_ERROR(ctx, ctx->size < 2, "Error, underflow reading marker.");
      JGPU_ERROR(ctx, ctx->pos[0] != 0xFF, "Error, invalid JPEG syntax");
      JGPU_LOG(("Marker = %2X %2X\n", ctx->pos[0], ctx->pos[1]));
      JGPU_SKIP_BYTES(ctx, 1);
      JGPU_DECODE_BYTE(ctx, marker);
    }
    switch (marker) {
      /* Quantization Tables */
      case 0xDB : {
        jgpu_decode_dqt(ctx);
        break;
      }
      /* Baseline DCT */
      case 0xC0 : {
        jgpu_decode_sof(ctx);
        jgpu_image_init(ctx);
        break;
      }
      /* Huffman Tables */
      case 0xC4 : {
        jgpu_decode_dht(ctx);
        break;
      }
      /* Restart Interval */
      case 0xDD : {
        jgpu_decode_rsi(ctx);
        break;
      }
      /* Start of Scans */
      case 0xDA : {
        jgpu_decode_sos(ctx);
        jgpu_decode_scan(ctx);
        break;
      }
      /* End of Image */
      case 0xD9 : {
        jgpu_decode_eoi(ctx);
        break;
      }
      default : {
        jgpu_skip_marker(ctx);
      }
    }
  }
}

static const char MEM_FRAG[]="\
#version 130\n\
uniform sampler2D myTexture;\n\
void main() {\n\
  int x=int(gl_FragCoord.x);\n\
  int y=int(gl_FragCoord.y);\n\
  gl_FragColor=texelFetch(myTexture,ivec2(x,y),0);\n\
}";

static void error_callback(int error, const char* description) {
  fprintf(stderr, "glfw error %i: %s\n", error, description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action,
 int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GL_TRUE);
  }
}

/* Compile the shader fragment. */
static GLint load_shader(GLuint *_shad,const char *_src) {
  int    len;
  GLuint shad;
  GLint  status;
  len=strlen(_src);
  shad=glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(shad,1,&_src,&len);
  glCompileShader(shad);
  glGetShaderiv(shad,GL_COMPILE_STATUS,&status);
  if (status!=GL_TRUE) {
    char info[8192];
    glGetShaderInfoLog(shad,8192,NULL,info);
    printf("Failed to compile fragment shader.\n%s\n",info);
    return GL_FALSE;
  }
  *_shad=shad;
  return GL_TRUE;
}

static GLint setup_shader(GLuint *_prog,const char *_src) {
  GLuint shad;
  GLuint prog;
  GLint  status;
  if (!load_shader(&shad,_src)) {
    return GL_FALSE;
  }
  prog=glCreateProgram();
  glAttachShader(prog,shad);
  glLinkProgram(prog);
  glGetProgramiv(prog,GL_LINK_STATUS,&status);
  if (status!=GL_TRUE) {
    char info[8192];
    glGetProgramInfoLog(prog,8192,NULL,info);
    printf("Failed to link program.\n%s\n",info);
    return GL_FALSE;
  }
  glUseProgram(prog);
  *_prog=prog;
  return GL_TRUE;
}

static GLint bind_texture(GLuint prog,const char *name, int tex) {
  GLint loc;
  loc = glGetUniformLocation(prog, name);
  if (loc < 0) {
    printf("Error finding texture '%s' in program %i\n", name, prog);
    return GL_FALSE;
  }
  glUniform1i(loc, tex);
  return GL_TRUE;
}


int main(int argc, const char *argv[]) {
  jpeg_gpu_ctx ctx;
  if (argc < 2) {
    return EXIT_FAILURE;
  }
  if (jgpu_init(&ctx, argv[1])) {
    return EXIT_FAILURE;
  }
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    return EXIT_FAILURE;
  }
  jgpu_decode(&ctx);
  if (ctx.error) {
    fprintf(stderr, "%s\n", ctx.error);
  }
  else {
    GLFWwindow *window;
    image *img;
    unsigned char *texture;
    GLuint tex;
    GLuint prog;

    img = &ctx.img;
    JGPU_LOG((stderr, "width = %i, height = %i\n", img->width, img->height));

    /* Allocate enough memory for ARGB buffer */
    /* TODO handle out of memory error better */
    texture = (unsigned char *)malloc(img->width*img->height*4);
    if (texture) {
      int i;
      int j;
      /* paint the texture */
      for (j = 0; j < img->height; j++) {
        unsigned char *pixels;
        pixels = texture;
        pixels += j*img->width*4;
        for (i = 0; i < img->width; i++) {
          if (img->nplanes == 1) {
            pixels[0] = pixels[1] = pixels[2] =
             img->plane[0].data[j*img->plane[0].ystride + i];
            pixels[3] = 0xff;
          }
          else {
            unsigned char r;
            unsigned char g;
            unsigned char b;
            int y;
            int cb;
            int cr;
            /* This assumes 8-bit and no decimation on the Y plane */
            y = img->plane[0].data[j*img->plane[0].ystride + i];
            cb = img->plane[1].data[(i >> img->plane[1].xdec) +
             (j >> img->plane[1].ydec)*img->plane[1].ystride] - 128;
            cr = img->plane[2].data[(i >> img->plane[2].xdec) +
             (j >> img->plane[2].ydec)*img->plane[2].ystride] - 128;
            /* TODO rewrite this color conversion code in fixed point */
            r = OD_CLAMP255(((int)(y + 1.402*cr)));
            g = OD_CLAMP255(((int)(y - 0.34414*cb - 0.71414*cr)));
            b = OD_CLAMP255(((int)(y + 1.1772*cb)));
            pixels[0] = r;
            pixels[1] = g;
            pixels[2] = b;
            pixels[3] = 0xff;
          }
          pixels += 4;
        }
      }
    }

    /* TODO handle this error better */
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    window = glfwCreateWindow(img->width, img->height, NAME, NULL, NULL);
    if (!window) {
      glfwTerminate();
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSwapInterval(0);

    glViewport(0, 0, img->width, img->height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, img->width, img->height, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0, GL_RGBA,
     GL_UNSIGNED_BYTE, texture);

    if (!setup_shader(&prog, MEM_FRAG)) {
      return EXIT_FAILURE;
    }
    if (!bind_texture(prog, "myTexture", 0)) {
      return EXIT_FAILURE;
    }

    glUseProgram(prog);
    while (!glfwWindowShouldClose(window)) {
      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      glVertex2i(0, 0);
      glTexCoord2i(img->width, 0);
      glVertex2i(img->width, 0);
      glTexCoord2i(img->width, img->height);
      glVertex2i(img->width, img->height);
      glTexCoord2i(0, img->height);
      glVertex2i(0, img->height);
      glEnd();

      glfwSwapBuffers(window);

      glfwPollEvents();
    }
    if (texture) {
      glDeleteTextures(1, &tex);
      free(texture);
    }
    glfwDestroyWindow(window);
  }
  jgpu_clear(&ctx);
  glfwTerminate();
  return EXIT_SUCCESS;
}
