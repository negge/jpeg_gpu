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

#if !defined(_xjpeg_H)
# define _xjpeg_h (1)

# include "image.h"

# define NQUANT_MAX (4)
# define NHUFF_MAX (4)
# define NCOMPS_MAX (3)

typedef struct xjpeg_quant xjpeg_quant;

struct xjpeg_quant {
  /* Has this quantization table has been loaded (for jpeg validation) */
  int valid;
  /* The bit depth of the quantization table (8 or 16 bits) */
  unsigned char bits;
  /* The loaded quantization table, in zig-zag order */
  unsigned short tbl[64];
};

typedef struct xjpeg_huff xjpeg_huff;

struct xjpeg_huff {
  /* Has this huffman table been loaded (for jpeg validation) */
  int valid;

  /* The number of codewords in this huffman table at each of the 16 possible
      bit lengths (0 indexed) */
  int nbits[16];
  /* The total number of symbols in this huffman table (the sum of nbits). */
  int nsymbs;
  /* The codeword / symbol pairs in graded lexicographical order by codeword */
  unsigned short codeword[256];
  /* All symbols are 8-bit */
  unsigned char symbol[256];

  /* Lookup tables used for fast decoding of the most common (shortest)
      codewords.
     For each codeword of up to 8-bits, an entry is placed in all indeces where
      the codeword is the proper prefix of the index, e.g., for codeword 010101
      there would be four entries at indeces 010101{00,01,10,11}.
     Each entry in lookup[] is the is a packed unsigned short where the high
      unsigned byte is the length of the codeword in bits and the low unsigned
      byte is the symbol.
     When the codeword is longer than 8 bits the low unsigned byte is unused.*/
  int lookup[256];
  int index[16];
  int maxcode[16];
};

typedef struct xjpeg_comp_info xjpeg_comp_info;

struct xjpeg_comp_info {
  unsigned char id;
  unsigned char hsamp;
  unsigned char vsamp;
  /* Index into the quantization table for this component */
  unsigned char tq;
};

typedef struct xjpeg_frame_header xjpeg_frame_header;

struct xjpeg_frame_header {
  int valid;
  unsigned char bits;
  unsigned char ncomps;
  xjpeg_comp_info comp[NCOMPS_MAX];
  unsigned short width;
  unsigned short height;
  unsigned short nhmb;
  unsigned short nvmb;
};

typedef struct xjpeg_scan_comp xjpeg_scan_comp;

struct xjpeg_scan_comp {
  unsigned char id;
  /* DC entropy coding table index */
  unsigned char td;
  /* AC entropy coding table index */
  unsigned char ta;
};

typedef struct xjpeg_scan_header xjpeg_scan_header;

struct xjpeg_scan_header {
  int valid;
  unsigned char ncomps;
  xjpeg_scan_comp comp[NCOMPS_MAX];
};

typedef size_t xjpeg_decode_word;

typedef struct xjpeg_decode_ctx xjpeg_decode_ctx;

struct xjpeg_decode_ctx {
  /* Pointer into in-memory jpeg file */
  const unsigned char *pos;
  /* Number of bytes left to read */
  int size;

  /* An MSB buffer of bits read from jpeg file */
  xjpeg_decode_word bitbuf;
  /* Number of bits available in bitbuf */
  int bits;

  xjpeg_quant quant[NQUANT_MAX];
  xjpeg_huff dc_huff[NHUFF_MAX];
  xjpeg_huff ac_huff[NHUFF_MAX];
  short restart_interval;
  xjpeg_frame_header frame;
  xjpeg_scan_header scan;

  const char *error;

  int start_of_image;
  int end_of_image;
  unsigned char marker;
};

typedef enum xjpeg_decode_out {
  XJPEG_DECODE_QUANT,
  XJPEG_DECODE_DCT,
  XJPEG_DECODE_YUV,
  XJPEG_DECODE_RGB
} xjpeg_decode_out;

void xjpeg_init(xjpeg_decode_ctx *ctx, const unsigned char *buf, int size);
void xjpeg_decode_header(xjpeg_decode_ctx *ctx);
void xjpeg_decode_image(xjpeg_decode_ctx *ctx, image *img,
 xjpeg_decode_out out);

#endif
