#version 140

#define GLJ_COEFF_SHIFT (4)

#define GLJ_UNBIASED_RSHIFT(_a, _b) \
 (((_a) < 0 ? ((_a) + ((1 << (_b)) - 1)) : (_a)) >> (_b))

#define GLJ_DCT_RSHIFT(_a, _b) GLJ_UNBIASED_RSHIFT(_a, _b)

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
  while (false)

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
  while (false)

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
  while (false)

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
  while (false)

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
  while (false)

void od_bin_idct8(out int x[8], const int y[8]) {
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
  x[0] = GLJ_DCT_RSHIFT(t0, GLJ_COEFF_SHIFT);
  x[1] = GLJ_DCT_RSHIFT(t1, GLJ_COEFF_SHIFT);
  x[2] = GLJ_DCT_RSHIFT(t2, GLJ_COEFF_SHIFT);
  x[3] = GLJ_DCT_RSHIFT(t3, GLJ_COEFF_SHIFT);
  x[4] = GLJ_DCT_RSHIFT(t4, GLJ_COEFF_SHIFT);
  x[5] = GLJ_DCT_RSHIFT(t5, GLJ_COEFF_SHIFT);
  x[6] = GLJ_DCT_RSHIFT(t6, GLJ_COEFF_SHIFT);
  x[7] = GLJ_DCT_RSHIFT(t7, GLJ_COEFF_SHIFT);
}

in vec2 tex_coord;

out ivec4 h_low;
out ivec4 h_high;

uniform isampler2D tex;

void main() {
  int s=int(tex_coord.s);
  int t=int(tex_coord.t);
  int u=(s<<6)+((t&0x7)<<3);
  int v=t>>3;
  int i;
  int x[8];
  int y[8];
  for (i=0;i<8;i++) {
    y[i]=texelFetch(tex,ivec2(u+i,v),0).r;
  }
  od_bin_idct8(x, y);
  h_low=ivec4(x[0],x[1],x[2],x[3]);
  h_high=ivec4(x[4],x[5],x[6],x[7]);
}
