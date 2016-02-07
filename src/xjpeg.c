#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xjpeg.h"
#include "dct.h"
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

#define XJPEG_FILL_BITS(ctx, nbits) \
  do { \
    while ((ctx)->bits < nbits) { \
      unsigned char byte; \
      XJPEG_ERROR(ctx, ctx->marker, "Error found marker when filling bits."); \
      XJPEG_DECODE_BYTE(ctx, byte); \
      if (byte == 0xFF) { \
        do { \
          XJPEG_DECODE_BYTE(ctx, byte); \
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

#define XJPEG_SKIP_BITS(ctx, nbits) \
  do { \
    XJPEG_FILL_BITS(ctx, nbits); \
    (ctx)->bits -= nbits; \
  } \
  while (0)

#define XJPEG_PEEK_BITS(ctx, nbits, ret) \
  do { \
    XJPEG_FILL_BITS(ctx, nbits); \
    XJPEG_ERROR(ctx, (ctx)->bits < nbits, "Error not enough bits to peek."); \
    ret = (((ctx)->bitbuf >> ((ctx)->bits - (nbits))) & ((1 << (nbits)) - 1)); \
  } \
  while (0)

#define XJPEG_DECODE_BITS(ctx, nbits, ret) \
  do { \
    XJPEG_FILL_BITS(ctx, nbits); \
    XJPEG_ERROR(ctx, (ctx)->bits < nbits, "Error not enough bits to get."); \
    ret = (((ctx)->bitbuf >> ((ctx)->bits - (nbits))) & ((1 << (nbits)) - 1)); \
    (ctx)->bits -= nbits; \
  } \
  while (0)

#define XJPEG_DECODE_HUFF(ctx, huff, symbol) \
  do { \
    int bits; \
    int value; \
    XJPEG_FILL_BITS(ctx, 2*LOOKUP_BITS); \
    XJPEG_PEEK_BITS(ctx, LOOKUP_BITS, value); \
    symbol = (huff)->lookup[value]; \
    bits = symbol >> LOOKUP_BITS; \
    symbol &= (1 << LOOKUP_BITS) - 1; \
    XJPEG_SKIP_BITS(ctx, bits); \
    if (bits > LOOKUP_BITS) { \
      value = ((ctx)->bitbuf >> (ctx)->bits) & ((1 << bits) - 1); \
      while (value > (huff)->maxcode[bits - 1]) { \
        int bit; \
        XJPEG_DECODE_BITS(ctx, 1, bit); \
        value = (value << 1) | bit; \
        bits++; \
      } \
      symbol = (huff)->symbol[value + (huff)->index[bits - 1]]; \
    } \
    XJPEG_LOG(("bits = %i, code = %02X, symbol = %02x\n", bits, \
     value >> (LOOKUP_BITS - bits), symbol)); \
  } \
  while (0)

#define XJPEG_DECODE_VLC(ctx, huff, symbol, value) \
  do { \
    int len; \
    XJPEG_DECODE_HUFF(ctx, huff, symbol); \
    len = symbol & 0xf; \
    XJPEG_DECODE_BITS(ctx, len, value); \
    XJPEG_LOG(("len = %i\n", len)); \
    if (value < 1 << (len - 1)) { \
      value += 1 - (1 << len); \
    } \
    XJPEG_LOG(("vlc = %i\n", value)); \
    XJPEG_LOG(("\n")); \
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

static void xjpeg_decode_rsi(xjpeg_decode_ctx *ctx) {
  unsigned short len;
  XJPEG_DECODE_SHORT(ctx, len);
  len -= 2;
  XJPEG_DECODE_SHORT(ctx, ctx->restart_interval);
  XJPEG_LOG(("Restart Interval %i\n", ctx->restart_interval));
  len -= 2;
  XJPEG_ERROR(ctx, len != 0, "Error decoding DRI, unprocessed bytes.");
}

static void xjpeg_decode_scan(xjpeg_decode_ctx *ctx,
 image_plane *plane[NPLANES_MAX]) {
  int mcu_counter;
  int rst_counter;
  int dc_pred[NCOMPS_MAX];
  int mbx;
  int mby;
  mcu_counter = ctx->restart_interval;
  rst_counter = 0;
  memset(dc_pred, 0, sizeof(dc_pred));
  for (mby = 0; mby < ctx->frame.nvmb; mby++) {
    for (mbx = 0; mbx < ctx->frame.nhmb; mbx++) {
      int i;
      xjpeg_scan_comp *comp;
      for (i = 0, comp = ctx->scan.comp; i < ctx->scan.ncomps; i++, comp++) {
        xjpeg_comp_info *pi;
        image_plane *ip;
        int sby;
        int sbx;
        pi = &ctx->frame.comp[i];
        ip = plane[i];
        for (sby = 0; sby < pi->vsamp; sby++) {
          for (sbx = 0; sbx < pi->hsamp; sbx++) {
            int block[64];
            int symbol;
            int value;
            int j, k;
            memset(block, 0, sizeof(block));
            XJPEG_DECODE_VLC(ctx, &ctx->dc_huff[comp->td], symbol, value);
            XJPEG_LOG(("dc = %i\n", value));
            dc_pred[i] += value;
            XJPEG_LOG(("dc_pred = %i\n", pred[i]));
            j = 0;
            block[0] = dc_pred[i]*ctx->quant[pi->tq].tbl[0];
            do {
              XJPEG_DECODE_VLC(ctx, &ctx->ac_huff[comp->ta], symbol, value);
              if (!symbol) {
                XJPEG_LOG(("****************** EOB at j = %i\n\n", j));
                break;
              }
              j += (symbol >> 4) + 1;
              XJPEG_LOG(("j = %i, offset = %i, value = %i, dequant = %i\n", j,
               (symbol >> 4) + 1, value, value*ctx->quant[pi->tq].tbl[j]));
              XJPEG_ERROR(ctx, j > 63, "Error indexing outside block.");
              block[DE_ZIG_ZAG[j]] = value*ctx->quant[pi->tq].tbl[j];
            }
            while (j < 63);
#if LOGGING_ENABLED
            for (j = 1; j <= 64; j++) {
              XJPEG_LOG("%5i%s", block[j - 1], j & 0x7 ? ", " : "\n");
            }
#endif
            od_bin_idct8x8(block, 8, block, 8);
            {
              unsigned char *data;
              int *b;
              data = ip->data +
               ((mby*pi->vsamp + sby)*ip->ystride << 3) +
               ((mbx*pi->hsamp + sbx)*ip->xstride << 3);
              b = block;
              for (k = 0; k < 8; k++) {
                unsigned char *row;
                row = data;
                for (j = 0; j < 8; j++) {
                  *row = OD_CLAMP255(*b + 128);
                  row++;
                  b++;
                }
                data += ip->ystride;
              }
            }
          }
        }
      }
      mcu_counter--;
      XJPEG_LOG(("mbx = %i, mby = %i, rst_counter = %i, mcu_counter = %i\n",
       mbx, mby, rst_counter, mcu_counter));
      if (ctx->restart_interval && mcu_counter == 0) {
        XJPEG_ERROR(ctx, !ctx->marker, "Error, expected to find marker.");
        switch (ctx->marker) {
          case 0xD0 :
          case 0xD1 :
          case 0xD2 :
          case 0xD3 :
          case 0xD4 :
          case 0xD5 :
          case 0xD6 :
          case 0xD7 : {
            XJPEG_ERROR(ctx, (ctx->marker & 0x7) != (rst_counter & 0x7),
              "Error invalid RST counter in marker.");
            ctx->marker = 0;
            ctx->bits = 0;
            mcu_counter = ctx->restart_interval;
            rst_counter++;
            for (i = 0; i < ctx->scan.ncomps; i++) {
              dc_pred[i] = 0;
            }
            break;
          }
          /* End of Image */
          case 0xD9 : {
            return;
          }
          default : {
            XJPEG_ERROR(ctx, 1, "Error, unknown marker found in scan.");
          }
        }
      }
    }
  }
}

static void xjpeg_decode_sos(xjpeg_decode_ctx *ctx, image *img) {
  unsigned short len;
  xjpeg_scan_header *scan;
  int i, j;
  image_plane *plane[NPLANES_MAX];
  unsigned char byte;
  XJPEG_DECODE_SHORT(ctx, len);
  len -= 2;
  XJPEG_ERROR(ctx, len < 6, "Error SOS needs at least 6 bytes");
  scan = &ctx->scan;
  XJPEG_ERROR(ctx, scan->valid, "Error multiple SOS not supported.");
  scan->valid = 1;
  XJPEG_DECODE_BYTE(ctx, scan->ncomps);
  XJPEG_ERROR(ctx, scan->ncomps == 0 || scan->ncomps > 4,
   "Error SOS expected Ns value 1 to 4.");
  len--;
  XJPEG_ERROR(ctx, scan->ncomps != 1 && scan->ncomps != 3,
   "Error only scans with 1 or 3 components supported");
  for (i = 0; i < scan->ncomps; i++) {
    xjpeg_scan_comp *comp;
    comp = &scan->comp[i];
    XJPEG_DECODE_BYTE(ctx, comp->id);
    plane[i] = NULL;
    for (j = 0; j < ctx->frame.ncomps; j++) {
      if (ctx->frame.comp[j].id == comp->id) {
        plane[i] = &img->plane[j];
        break;
      }
    }
    XJPEG_ERROR(ctx, !plane[i], "Error SOS references invalid component.");
    XJPEG_DECODE_BYTE(ctx, byte);
    comp->td = byte >> 4;
    XJPEG_ERROR(ctx, !ctx->dc_huff[comp->td].valid,
     "Error SOS component references invalid DC entropy table.");
    comp->ta = byte & 0x7;
    XJPEG_ERROR(ctx, !ctx->ac_huff[comp->ta].valid,
     "Error SOS component references invalid AC entropy table.");
    len -= 2;
  }
  XJPEG_DECODE_BYTE(ctx, byte);
  XJPEG_ERROR(ctx, byte != 0, "Error SOS expected Ss value 0.");
  XJPEG_DECODE_BYTE(ctx, byte);
  XJPEG_ERROR(ctx, byte != 63, "Error SOS expected Se value 0.");
  XJPEG_DECODE_BYTE(ctx, byte);
  XJPEG_ERROR(ctx, byte >> 4 != 0, "Error SOS expected Ah value 0.");
  XJPEG_ERROR(ctx, byte & 0x7 != 0, "Error SOS expected Al value 0.");
  len -= 3;
  XJPEG_ERROR(ctx, len != 0, "Error decoding SOS, unprocessed bytes.");
  xjpeg_decode_scan(ctx, plane);
}

static void xjpeg_skip_marker(xjpeg_decode_ctx *ctx) {
  unsigned short len;
  XJPEG_DECODE_SHORT(ctx, len);
  XJPEG_LOG(("Skipping %i bytes\n", len - 2));
  XJPEG_SKIP_BYTES(ctx, len - 2);
}

void xjpeg_decode(xjpeg_decode_ctx *ctx, int headers_only, image *img) {
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
      /* Restart Interval */
      case 0xDD : {
        xjpeg_decode_rsi(ctx);
        break;
      }
      /* Start of Scans */
      case 0xDA : {
        xjpeg_decode_sos(ctx, img);
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
