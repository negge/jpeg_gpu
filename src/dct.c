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

#include <math.h>
#include "dct.h"

#define GLJ_COEFF_SHIFT (4)

/*This is the strength reduced version of ((_a)/(1 << (_b))).
  This will not work for _b == 0, however currently this is only used for
   b == 1 anyway.*/
#define GLJ_UNBIASED_RSHIFT32(_a, _b) \
 (((int)(((unsigned int)(_a) >> (32 - (_b))) + (_a))) >> (_b))

#define GLJ_DCT_RSHIFT(_a, _b) GLJ_UNBIASED_RSHIFT32(_a, _b)

#define GLJ_IDCT_2(t0, t1) \
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

#define GLJ_IDST_2(t0, t1) \
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

#define GLJ_IDCT_4_ASYM(t0, t2, t1, t1h, t3, t3h) \
  /* Embedded 4-point asymmetric Type-II iDCT. */ \
  do { \
    GLJ_IDST_2(t3, t2); \
    GLJ_IDCT_2(t0, t1); \
    t1 = t2 - t1; \
    t1h = GLJ_DCT_RSHIFT(t1, 1); \
    t2 = t1h - t2; \
    t3 = t0 - t3; \
    t3h = GLJ_DCT_RSHIFT(t3, 1); \
    t0 -= t3h; \
  } \
  while (0)

#define GLJ_IDST_4_ASYM(t0, t0h, t2, t1, t3) \
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
    t0h = GLJ_DCT_RSHIFT(t0, 1); \
    t2 = t0h - t2; \
    t1 += t3; \
    t3 -= GLJ_DCT_RSHIFT(t1, 1); \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    t1 -= (t2*19195 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    t2 -= (t1*11585 + 8192) >> 14; \
    /* 7489/8192 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    t1 += (t2*7489 + 4096) >> 13; \
  } \
  while (0)

#define GLJ_IDCT_8(r0, r4, r2, r6, r1, r5, r3, r7) \
  /* Embedded 8-point orthonormal Type-II iDCT. */ \
  do { \
    int r1h; \
    int r3h; \
    int r5h; \
    int r7h; \
    GLJ_IDST_4_ASYM(r7, r7h, r5, r6, r4); \
    GLJ_IDCT_4_ASYM(r0, r2, r1, r1h, r3, r3h); \
    r0 += r7h; \
    r7 = r0 - r7; \
    r6 = r1h - r6; \
    r1 -= r6; \
    r5h = GLJ_DCT_RSHIFT(r5, 1); \
    r2 += r5h; \
    r5 = r2 - r5; \
    r4 = r3h - r4; \
    r3 -= r4; \
  } \
  while (0)

static void od_bin_idct8(short *x, int xstride, const short y[8]) {
  int t0;
  int t1;
  int t2;
  int t3;
  int t4;
  int t5;
  int t6;
  int t7;
  t0 = y[0] << GLJ_COEFF_SHIFT;
  t4 = y[1] << GLJ_COEFF_SHIFT;
  t2 = y[2] << GLJ_COEFF_SHIFT;
  t6 = y[3] << GLJ_COEFF_SHIFT;
  t1 = y[4] << GLJ_COEFF_SHIFT;
  t5 = y[5] << GLJ_COEFF_SHIFT;
  t3 = y[6] << GLJ_COEFF_SHIFT;
  t7 = y[7] << GLJ_COEFF_SHIFT;
  GLJ_IDCT_8(t0, t4, t2, t6, t1, t5, t3, t7);
  x[0*xstride] = (short)GLJ_DCT_RSHIFT(t0, GLJ_COEFF_SHIFT);
  x[1*xstride] = (short)GLJ_DCT_RSHIFT(t1, GLJ_COEFF_SHIFT);
  x[2*xstride] = (short)GLJ_DCT_RSHIFT(t2, GLJ_COEFF_SHIFT);
  x[3*xstride] = (short)GLJ_DCT_RSHIFT(t3, GLJ_COEFF_SHIFT);
  x[4*xstride] = (short)GLJ_DCT_RSHIFT(t4, GLJ_COEFF_SHIFT);
  x[5*xstride] = (short)GLJ_DCT_RSHIFT(t5, GLJ_COEFF_SHIFT);
  x[6*xstride] = (short)GLJ_DCT_RSHIFT(t6, GLJ_COEFF_SHIFT);
  x[7*xstride] = (short)GLJ_DCT_RSHIFT(t7, GLJ_COEFF_SHIFT);
}

void od_bin_idct8x8(short *x, int xstride, const short *y, int ystride) {
  short z[8*8];
  int i;
  for (i = 0; i < 8; i++) od_bin_idct8(z + i, 8, y + ystride*i);
  for (i = 0; i < 8; i++) od_bin_idct8(x + i, xstride, z + 8*i);
}

typedef float glj_real;

static void glj_real_idct8(glj_real *x, int xstride, const glj_real y[8]) {
  glj_real t0;
  glj_real t1;
  glj_real t2;
  glj_real t3;
  glj_real t4;
  glj_real t5;
  glj_real t6;
  glj_real t7;
  glj_real u0;
  glj_real u1;
  glj_real u2;
  glj_real u3;
  glj_real u4;
  glj_real u5;
  glj_real u6;
  glj_real u7;
  glj_real u8;
  t0 = y[0];
  u4 = y[1];
  t2 = y[2];
  u6 = y[3];
  t1 = y[4];
  u5 = y[5];
  t3 = y[6];
  u7 = y[7];
  /* Embedded scaled inverse 4-point Type-II DCT */
  u0 = t0 + t1;
  u1 = t0 - t1;
  u3 = t2 + t3;
  u2 = (t2 - t3)*((glj_real)1.4142135623730950488016887242097) - u3;
  t0 = u0 + u3;
  t3 = u0 - u3;
  t1 = u1 + u2;
  t2 = u1 - u2;
  /* Embedded scaled inverse 4-point Type-IV DST */
  t5 = u5 + u6;
  t6 = u5 - u6;
  t7 = u4 + u7;
  t4 = u4 - u7;
  u7 = t7 + t5;
  u5 = (t7 - t5)*((glj_real)1.4142135623730950488016887242097);
  u8 = (t4 + t6)*((glj_real)1.8477590650225735122563663787936);
  u4 = u8 - t4*((glj_real)1.0823922002923939687994464107328);
  u6 = u8 - t6*((glj_real)2.6131259297527530557132863468544);
  t7 = u7;
  t6 = t7 - u6;
  t5 = t6 + u5;
  t4 = t5 - u4;
  /* Butterflies */
  u0 = t0 + t7;
  u7 = t0 - t7;
  u6 = t1 + t6;
  u1 = t1 - t6;
  u2 = t2 + t5;
  u5 = t2 - t5;
  u4 = t3 + t4;
  u3 = t3 - t4;
  x[0*xstride] = u0;
  x[1*xstride] = u1;
  x[2*xstride] = u2;
  x[3*xstride] = u3;
  x[4*xstride] = u4;
  x[5*xstride] = u5;
  x[6*xstride] = u6;
  x[7*xstride] = u7;
}

static const glj_real GLJ_REAL_IDCT8_SCALES[8] = {
  0.35355339059327376220042218105242,
  0.49039264020161522456309111806712,
  0.46193976625564337806409159469839,
  0.41573480615127261853939418880895,
  0.35355339059327376220042218105242,
  0.27778511650980111237141540697427,
  0.19134171618254488586422999201520,
  0.097545161008064133924142434238511,
};

void glj_real_idct8x8(short *x, int xstride, const short *y, int ystride) {
  int j;
  int i;
  glj_real t[8*8];
  glj_real z[8*8];
  for (j = 0; j < 8; j++) {
    for (i = 0; i < 8; i++) {
      t[j*8 + i] =
       y[j*ystride + i]*GLJ_REAL_IDCT8_SCALES[j]*GLJ_REAL_IDCT8_SCALES[i];
    }
  }
  for (i = 0; i < 8; i++) glj_real_idct8(z + i, 8, t + 8*i);
  for (i = 0; i < 8; i++) {
    z[8*i] += 0.5;
    glj_real_idct8(t + i, 8, z + 8*i);
  }
  for (j = 0; j < 8; j++) {
    for (i = 0; i < 8; i++) {
      x[j*xstride + i] = (short)floor(t[j*8 + i]);
    }
  }
}
