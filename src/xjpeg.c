#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xjpeg.h"
#include "internal.h"

#define LOOKUP_BITS (8)

#define LOGGING_ENABLED (0)

#if LOGGING_ENABLED
# define XJPEG_LOG(a) printf a
#else
# define XJPEG_LOG(a) (void)0
#endif

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

#define XJPEG_ERROR(ctx, cond, err) \
  do { \
    if (cond) { \
      (ctx)->error = (err); \
      return; \
    } \
  } \
  while (0)

#define XJPEG_SKIP_BYTES(ctx, nbytes) \
  do { \
    XJPEG_ERROR(ctx, (ctx)->size < nbytes, \
     "Error skipping past the end of file."); \
    (ctx)->pos += nbytes; \
    (ctx)->size -= nbytes; \
  } \
  while (0)

#define XJPEG_DECODE_BYTE(ctx, ret) \
  do { \
    XJPEG_ERROR(ctx, (ctx)->size < 1, "Error reading past the end of file."); \
    ret = (ctx)->pos[0]; \
    XJPEG_SKIP_BYTES(ctx, 1); \
  } \
  while (0)

#define XJPEG_DECODE_SHORT(ctx, ret) \
  do { \
    XJPEG_ERROR(ctx, (ctx)->size < 2, "Error reading past the end of file."); \
    ret = ((ctx)->pos[0] << 8) | (ctx)->pos[1]; \
    XJPEG_SKIP_BYTES(ctx, 2); \
  } \
  while (0)

static void xjpeg_decode_soi(xjpeg_decode_ctx *ctx) {
  XJPEG_ERROR(ctx, ctx->start_of_image, "Error, already found SOI.");
  XJPEG_LOG(("Start of Image\n"));
  ctx->start_of_image = 1;
}

static void xjpeg_decode_eoi(xjpeg_decode_ctx *ctx) {
  XJPEG_LOG(("End of Image\n"));
  ctx->end_of_image = 1;
  XJPEG_ERROR(ctx, ctx->size != 0, "Error decoding EOI, unprocessed bytes.");
}

static void xjpeg_decode_dqt(xjpeg_decode_ctx *ctx) {
  unsigned short len;
  XJPEG_DECODE_SHORT(ctx, len);
  len -= 2;
  while (len >= 65) {
    unsigned char byte;
    unsigned char pq;
    unsigned char tq;
    xjpeg_quant *quant;
    int i;
    XJPEG_DECODE_BYTE(ctx, byte);
    pq = byte >> 4;
    XJPEG_ERROR(ctx, pq > 1, "Error DQT expected Pq value 0 or 1.");
    tq = byte & 0x7;
    XJPEG_ERROR(ctx, tq > 3, "Error DQT expected Tq value 0 to 3.");
    quant = &ctx->quant[tq];
    quant->valid = 1;
    quant->bits = pq ? 16 : 8;
    XJPEG_LOG(("Reading Quantization Table %i (%i-bit)\n", tq, quant->bits));
    if (pq) {
      for (i = 0; i < 64; i++) {
        XJPEG_DECODE_SHORT(ctx, quant->tbl[i]);
      }
    }
    else {
      for (i = 0; i < 64; i++) {
        XJPEG_DECODE_BYTE(ctx, quant->tbl[i]);
      }
    }
    len -= 65 + 64*pq;
#if LOGGING_ENABLED
    for (i = 1; i <= 64; i++) {
      XJPEG_LOG(("%3i,%s", quant->tbl[ZIG_ZAG[i - 1]], i & 0x7 ? " " : "\n"));
    }
#endif
  }
  XJPEG_ERROR(ctx, len != 0, "Error decoding DQT, unprocessed bytes.");
}

static void xjpeg_decode_dht(xjpeg_decode_ctx *ctx) {
  unsigned short len;
  XJPEG_DECODE_SHORT(ctx, len);
  len -= 2;
  while (len >= 17) {
    unsigned char byte;
    unsigned int tc;
    unsigned int th;
    xjpeg_huff *huff;
    int i, j, k, l;
    unsigned short codeword;
    XJPEG_DECODE_BYTE(ctx, byte);
    tc = byte >> 4;
    XJPEG_ERROR(ctx, tc > 1, "Error DHT expected Tc value 0 or 1.");
    th = byte & 0x7;
    XJPEG_ERROR(ctx, th > 3, "Error DHT expected Th value 0 to 3.");
    if (tc) {
      huff = &ctx->ac_huff[th];
    }
    else {
      huff = &ctx->dc_huff[th];
    }
    huff->valid = 1;
    huff->nsymbs = 0;
    for (i = 0; i < 16; i++) {
      XJPEG_DECODE_BYTE(ctx, huff->nbits[i]);
      huff->nsymbs += huff->nbits[i];
    }
    len -= 17;
    XJPEG_ERROR(ctx, huff->nsymbs > 256,
     "Error DHT has more than 256 symbols.");
    XJPEG_ERROR(ctx, huff->nsymbs > len,
     "Error DHT needs more bytes than available.");
    XJPEG_LOG(("Reading %s Huffman Table %i symbols %i\n", tc ? "AC" : "DC", th,
     huff->nsymbs));
    k = 0;
    codeword = 0;
    for (i = 0; i < 16; i++) {
      for (j = 0; j < huff->nbits[i]; j++) {
        huff->codeword[k] = codeword;
        XJPEG_DECODE_BYTE(ctx, huff->symbol[k]);
#if LOGGING_ENABLED
        XJPEG_LOG(("bits = %2i, codeword = ", i + 1));
        printBits(codeword, i + 1);
        XJPEG_LOG((", symbol = %02X\n", huff->symbol[k]));
#endif
        k++;
        codeword++;
      }
      len -= huff->nbits[i];
      XJPEG_ERROR(ctx, codeword >= 1 << i + 1, "Error invalid DHT.");
      codeword <<= 1;
    }
    /* Generate a lookup table to speed up decoding */
    for (i = 0; i < 1 << LOOKUP_BITS; i++) {
      huff->lookup[i] = (LOOKUP_BITS + 1) << LOOKUP_BITS;
    }
    k = 0;
    for (i = 1; i <= LOOKUP_BITS; i++) {
      for (j = 0; j < huff->nbits[i - 1]; j++) {
        codeword = huff->codeword[k] << (LOOKUP_BITS - i);
        for (l = 0; l < 1 << LOOKUP_BITS - i; l++) {
          huff->lookup[codeword] = (i << LOOKUP_BITS) | huff->symbol[k];
          codeword++;
        }
        k++;
      }
    }
    /* Build an index into the codeword table and store the largest codeword
        by bit in maxcode. */
    k = 0;
    for (i = 0; i < 16; i++) {
      huff->maxcode[i] = -1;
      if (huff->nbits[i]) {
        huff->index[i] = k - huff->codeword[k];
        k += huff->nbits[i];
        huff->maxcode[i] = huff->codeword[k - 1];
      }
    }
    /* If this is a DC table, validate that the symbols are between 0 and 15 */
    if (!tc) {
      for (i = 0; i < huff->nsymbs; i++) {
        XJPEG_ERROR(ctx, huff->symbol[i] > 15, "Error invalid DC symbol.");
      }
    }
  }
  XJPEG_ERROR(ctx, len != 0, "Error decoding DHT, unprocessed bytes.");
}

static void xjpeg_decode_sof(xjpeg_decode_ctx *ctx) {
  unsigned short len;
  xjpeg_frame_header *frame;
  int i;
  int hmax;
  int vmax;
  int mcu_width;
  int mcu_height;
  XJPEG_DECODE_SHORT(ctx, len);
  len -= 2;
  XJPEG_ERROR(ctx, len < 9, "Error SOF needs at least 9 bytes");
  frame = &ctx->frame;
  XJPEG_ERROR(ctx, frame->valid, "Error multiple SOF not supported.");
  frame->valid = 1;
  XJPEG_DECODE_BYTE(ctx, frame->bits);
  XJPEG_DECODE_SHORT(ctx, frame->height);
  XJPEG_ERROR(ctx, frame->height == 0, "Error SOF has invalid height.");
  XJPEG_DECODE_SHORT(ctx, frame->width);
  XJPEG_ERROR(ctx, frame->width == 0, "Error SOF has invalid width.");
  XJPEG_DECODE_BYTE(ctx, frame->ncomps);
  XJPEG_LOG(("bits = %i, height = %i, width = %i, ncomps = %i\n",
   frame->bits, frame->height, frame->width, frame->ncomps));
  len -= 6;
  XJPEG_ERROR(ctx, len < 3*frame->ncomps,
   "Error SOF needs more bytes than available.");
  hmax = 0;
  vmax = 0;
  for (i = 0; i < frame->ncomps; i++) {
    xjpeg_comp_info *plane;
    unsigned char byte;
    plane = &frame->comp[i];
    XJPEG_DECODE_BYTE(ctx, plane->id);
    XJPEG_DECODE_BYTE(ctx, byte);
    plane->hsamp = byte >> 4;
    XJPEG_ERROR(ctx, plane->hsamp == 0 || plane->hsamp > 4,
     "Error SOF expected Hi value 1 to 4.");
    XJPEG_ERROR(ctx, plane->hsamp == 3, "Unsupported horizontal sampling.");
    hmax = OD_MAXI(hmax, plane->hsamp);
    plane->vsamp = byte & 0x7;
    XJPEG_ERROR(ctx, plane->vsamp == 0 || plane->vsamp > 4,
     "Error SOF expected Vi value 1 to 4.");
    XJPEG_ERROR(ctx, plane->vsamp == 3, "Unsupported vertical sampling.");
    vmax = OD_MAXI(vmax, plane->vsamp);
    XJPEG_DECODE_BYTE(ctx, plane->tq);
    XJPEG_ERROR(ctx, plane->tq > 3, "Error SOF expected Tq value 0 to 3.");
    XJPEG_ERROR(ctx, !ctx->quant[plane->tq].valid,
     "Error SOF referenced invalid quantization table.");
    XJPEG_ERROR(ctx, ctx->quant[plane->tq].bits > frame->bits,
     "Error SOF mismatch in frame bits and quantization table bits.");
    len -= 3;
  }
  XJPEG_ERROR(ctx, len != 0, "Error decoding SOF, unprocessed bytes.");
  /* Compute the size in pixels of the MCU */
  mcu_width = hmax << 3;
  mcu_height = vmax << 3;
  /* Compute the number of horizontal and vertical MCU blocks for the frame. */
  frame->nhmb = (frame->width + mcu_width - 1)/mcu_width;
  frame->nvmb = (frame->height + mcu_height - 1)/mcu_height;
  XJPEG_LOG(("hmax = %i, vmax = %i, mbw = %i, mbh = %i, nhmb = %i, nvmb = %i\n",
   hmax, vmax, mcu_width, mcu_height, frame->nhmb, frame->nvmb));
}

static void xjpeg_skip_marker(xjpeg_decode_ctx *ctx) {
  unsigned short len;
  XJPEG_DECODE_SHORT(ctx, len);
  XJPEG_LOG(("Skipping %i bytes\n", len - 2));
  XJPEG_SKIP_BYTES(ctx, len - 2);
}

void xjpeg_decode(xjpeg_decode_ctx *ctx, int headers_only) {
  while (!ctx->error && !ctx->end_of_image) {
    unsigned char marker;
    marker = ctx->marker;
    ctx->marker = 0;
    if (marker == 0) {
      XJPEG_ERROR(ctx, ctx->size < 2, "Error underflow reading marker.");
      XJPEG_ERROR(ctx, ctx->pos[0] != 0xFF, "Error, invalid JPEG syntax.");
      XJPEG_SKIP_BYTES(ctx, 1);
      XJPEG_DECODE_BYTE(ctx, marker);
    }
    if (headers_only && marker == 0xDA) {
      ctx->marker = marker;
      break;
    }
    XJPEG_LOG(("Marker %02X\n", marker));
    switch (marker) {
      /* Start of Image */
      case 0xD8 : {
        xjpeg_decode_soi(ctx);
        break;
      }
      /* End of Image */
      case 0xD9 : {
        xjpeg_decode_eoi(ctx);
        break;
      }
      /* Quantization Tables */
      case 0xDB : {
        xjpeg_decode_dqt(ctx);
        break;
      }
      /* Huffman Tables */
      case 0xC4 : {
        xjpeg_decode_dht(ctx);
        break;
      }
      /* Start of Frame (SOF0 Baseline DCT) */
      case 0xC0 : {
        xjpeg_decode_sof(ctx);
        break;
      }
      default : {
        xjpeg_skip_marker(ctx);
        break;
      }
    }
  }
}

void xjpeg_init(xjpeg_decode_ctx *ctx, const unsigned char *buf, int size) {
  memset(ctx, 0, sizeof(xjpeg_decode_ctx));
  ctx->pos = buf;
  ctx->size = size;
  /* check that this is a valid JPEG file by looking for SOI marker. */
  XJPEG_ERROR(ctx,
   ctx->pos[0] != 0xFF || ctx->pos[1] != 0xD8 || ctx->pos[2] != 0xFF,
   "Error, not a JPEG (invalid SOI marker).");
}
