#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#define NPLANES_MAX (3)

#define NCOMPS_MAX (4)

#define NQUANT_MAX (4)

#define NHUFF_MAX (4)

#define LOOKUP_BITS (8)

#define JGPU_ERROR(ctx, cond, err) \
  do { \
    if (cond) { \
      (ctx)->error = (err); \
      return; \
    } \
  } \
  while (0)

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

typedef struct image_plane image_plane;

struct image_plane {
  int bitdepth;
  unsigned char xdec;
  unsigned char ydec;
  int xstride;
  int ystride;
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

  unsigned short width;
  unsigned short height;
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
  for (i = 0; i < frame->nplanes; i++) {
    image_plane *plane;
    int width;
    int height;
    plane = &ctx->img.plane[i];
    plane->bitdepth = frame->bits;
    plane->xdec = OD_ILOG(frame->hmax) - OD_ILOG(frame->plane[i].hsamp);
    plane->ydec = OD_ILOG(frame->vmax) - OD_ILOG(frame->plane[i].vsamp);
    /* Compute the component dimensions */
    width = frame->pic_width * frame->plane[i].hsamp / frame->hmax;
    height = frame->pic_height * frame->plane[i].vsamp / frame->vmax;
    /* Pad out to the next block of 8 pixels */
    width = PAD_POWER2(width, 3);
    height = PAD_POWER2(height, 3);
    plane->xstride = plane->bitdepth >> 3;
    plane->ystride = plane->xstride*width;
    plane->data = (unsigned char *)malloc(height*plane->ystride);
    JGPU_ERROR(ctx, !plane->data, "Error allocating image plane memory.");
    plane->coeffs = (int *)malloc(height*width*sizeof(*plane->coeffs));
    JGPU_ERROR(ctx, !plane->coeffs, "Error allocating coeffs plane memory.");
    printf(" P[%i]: %ix%i, xdec = %i, ydec = %i, xstride = %i, ystride = %i\n",
     i, width, height, plane->xdec, plane->ydec, plane->xstride, plane->ystride);
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
    int code; \
    JGPU_FILL_BITS(ctx, 2*LOOKUP_BITS); \
    JGPU_PEEK_BITS(ctx, LOOKUP_BITS, value); \
    printf("lookup = %2x\n", value); \
    symbol = (huff)->lookup[value]; \
    bits = symbol >> LOOKUP_BITS; \
    code = value >> (LOOKUP_BITS - bits); \
    symbol &= (1 << LOOKUP_BITS) - 1; \
    JGPU_SKIP_BITS(ctx, bits); \
    printf("bits = %i, code = %02X, symbol = %02x\n", bits, code, symbol); \
    if (bits > LOOKUP_BITS) { \
      value = ((ctx)->bitbuf >> (ctx)->bits) & ((1 << bits) - 1); \
      printf("starting value = %X\n", value); \
      printf("maxcode->bits[%i] = %i\n", bits, (huff)->maxcode[bits - 1]); \
      while (value > (huff)->maxcode[bits - 1]) { \
        int bit; \
        JGPU_DECODE_BITS(ctx, 1, bit); \
        value = (value << 1) | bit; \
        bits++; \
      } \
      symbol = (huff)->symbol[value + (huff)->index[bits - 1]]; \
      printf("bits = %i, code = %02X, symbol = %02x\n", bits, value, symbol); \
    } \
  } \
  while (0)

#define JGPU_DECODE_VLC(ctx, huff, symbol, value) \
  do { \
    int len; \
    JGPU_DECODE_HUFF(ctx, huff, symbol); \
    len = symbol & 0xf; \
    JGPU_DECODE_BITS(ctx, len, value); \
    printf("len = %i\n", len); \
    if (value < 1 << (len - 1)) { \
      value += 1 - (1 << len); \
    } \
    printf("vlc = %i\n", value); \
    printf("\n"); \
  } \
  while (0)

static void jgpu_skip_marker(jpeg_gpu_ctx *ctx) {
  unsigned short len;
  JGPU_DECODE_SHORT(ctx, len);
  printf(" skipping length %i\n", len - 2);
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
    printf("Reading Quantization Table %i (%i-bit)\n", tq, quant->bits);
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
    for (i = 1; i <= 64; i++) {
      printf("%i%s", quant->tbl[ZIG_ZAG[i - 1]], i & 0x7 ? ", " : "\n");
    }
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
  printf("bits = %i, height = %i, width = %i, nplanes = %i\n",
   frame->bits, frame->pic_height, frame->pic_width, frame->nplanes);
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
  printf("hmax = %i, vmax = %i, mbw = %i, mbh = %i, nhmb = %i, nvmb = %i\n",
   frame->hmax, frame->vmax, mbw, mbh, frame->nhmb, frame->nvmb);
}

static void printBits(int value, int bits) {
  int i;
  for (i = 1; i <= bits; i++) {
    printf("%s%s", value & (1 << (bits - i)) ? "1" : "0", i%4 ? "" : " ");
  }
  for (; i <= 16; i++) {
    printf(" %s", i%4 ? "" : " ");
  }
}

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
        printf("bits = %2i, codeword = ", i + 1);
        printBits(codeword, i + 1);
        printf(", symbol = %02X\n", huff->symbol[k]);
        k++;
        codeword++;
      }
      len -= huff->bits[i];
      JGPU_ERROR(ctx, codeword >= 1 << i + 1, "Error invalid DHT.");
      codeword <<= 1;
    }
    huff->symbs = k;
    k = 0;
    printf("building index table");
    for (i = 0; i < 16; i++) {
      huff->maxcode[i] = -1;
      if (huff->bits[i]) {
        printf("\nbits = %i, offset = %i\n", i + 1, k);
	printf("first codeword = ");
	printBits(huff->codeword[k], i + 1);
        huff->index[i] = k - huff->codeword[k];
	k += huff->bits[i];
	printf("\n last codeword = ");
	printBits(huff->codeword[k - 1], i + 1);
	huff->maxcode[i] = huff->codeword[k - 1];
      }
      printf("\n%i: index = %i, maxcode = ", i, huff->index[i]);
      printBits(huff->maxcode[i], i + 1);
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
    /* If this is a DC table, validate that the symbols are between 0 and 15 */
    if (!tc) {
      for (i = 0; i < huff->symbs; i++) {
        JGPU_ERROR(ctx, huff->symbol[i] > 15, "Error invalid DC symbol.");
      }
    }
    printf("Reading %s Huffman Table %i symbols %i\n", tc ? "AC" : "DC", th,
     huff->symbs);
  }
  JGPU_ERROR(ctx, len != 0, "Error decoding DHT, unprocessed bytes.");
}

static void jgpu_decode_rsi(jpeg_gpu_ctx *ctx) {
  unsigned short len;
  JGPU_DECODE_SHORT(ctx, len);
  len -= 2;
  JGPU_DECODE_SHORT(ctx, ctx->restart_interval);
  printf("Restart Interval %i\n", ctx->restart_interval);
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
            memset(block, 0, sizeof(block));
            JGPU_DECODE_VLC(ctx, &ctx->dc_huff[comp->td], symbol, value);
            printf("dc = %i\n", value);
            comp->dc_pred += value;
	    printf("dc_pred = %i\n", comp->dc_pred);
            j = 0;
            block[0] = comp->dc_pred*ctx->quant[pi->tq].tbl[0];
            do {
              JGPU_DECODE_VLC(ctx, &ctx->ac_huff[comp->ta], symbol, value);
              if (!symbol) {
	        printf("****************** EOB at j = %i\n\n", j);
                break;
              }
              j += (symbol >> 4) + 1;
	      printf("j = %i, offset = %i, value = %i, dequant = %i\n", j,
	       (symbol >> 4) + 1, value, value*ctx->quant[pi->tq].tbl[j]);
	      JGPU_ERROR(ctx, j > 63, "Error indexing outside block.");
              block[DE_ZIG_ZAG[j]] = value*ctx->quant[pi->tq].tbl[j];
            }
            while (j < 63);
	    for (j = 1; j <= 64; j++) {
	      printf("%5i%s", block[j - 1], j & 0x7 ? ", " : "\n");
	    }
            coeffs = ip->coeffs +
             ((mby*pi->vsamp + sby)*pi->width << 3) +
             ((mbx*pi->hsamp + sbx) << 3);
            for (k = 0; k < 8; k++) {
              for (j = 0; j < 8; j++) {
                coeffs[k*pi->width + j] = block[k*8 + j];
              }
            }
          }
        }
      }
      mcu_counter--;
      printf("mbx = %i, mby = %i, rst_counter = %i, mcu_counter = %i\n",
       mbx, mby, rst_counter, mcu_counter);
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
  fprintf(stderr, "Start of Image\n");
  JGPU_SKIP_BYTES(ctx, 2);
}

static void jgpu_decode_eoi(jpeg_gpu_ctx *ctx) {
  fprintf(stderr, "End of Image reached\n");
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
      printf("Marker = %2X %2X\n", ctx->pos[0], ctx->pos[1]);
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

int main(int argc, const char *argv[]) {
  jpeg_gpu_ctx ctx;
  if (argc < 2) {
    return EXIT_FAILURE;
  }
  if (jgpu_init(&ctx, argv[1])) {
    return EXIT_FAILURE;
  }
  /*if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("Unable to initialize SDL: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }*/
  jgpu_decode(&ctx);
  if (ctx.error) {
    fprintf(stderr, "%s\n", ctx.error);
  }
  jgpu_clear(&ctx);
  /*SDL_Quit();*/
  return EXIT_SUCCESS;
}
