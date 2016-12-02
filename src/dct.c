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
