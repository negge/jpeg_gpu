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

#if !defined(_jpeg_info_H)
# define _jpeg_info_H (1)

# define NCOMPS_MAX (3)
# define NQUANT_MAX (4)

typedef enum jpeg_subsamp {
  JPEG_SUBSAMP_UNKNOWN,
  JPEG_SUBSAMP_444,
  JPEG_SUBSAMP_422,
  JPEG_SUBSAMP_420,
  JPEG_SUBSAMP_440,
  JPEG_SUBSAMP_411,
  JPEG_SUBSAMP_MONO,
  JPEG_SUBSAMP_MAX
} jpeg_subsamp;

extern const char *JPEG_SUBSAMP_NAMES[JPEG_SUBSAMP_MAX];

typedef struct jpeg_quant jpeg_quant;

struct jpeg_quant {
  int valid;
  unsigned char bits;
  unsigned short tbl[64];
};

typedef struct jpeg_component jpeg_component;

struct jpeg_component {
  int hblocks;
  int vblocks;
  int hsamp;
  int vsamp;
  jpeg_quant *quant;
};

typedef struct jpeg_header jpeg_header;

struct jpeg_header {
  int bits;
  int width;
  int height;
  int ncomps;
  jpeg_subsamp subsamp;
  int restart_interval;
  jpeg_component comp[NCOMPS_MAX];
  jpeg_quant quant[NQUANT_MAX];
};

typedef struct jpeg_info jpeg_info;

struct jpeg_info {
  int size;
  unsigned char *buf;
};

int jpeg_info_init(jpeg_info *info, const char *name);
void jpeg_info_clear(jpeg_info *info);

#endif
