#version 140

#define OD_COEFF_SHIFT (4)

#define OD_UNBIASED_RSHIFT32(_a, _b) \
 (((_a) - ((_a) >> (32 - (_b)))) >> (_b))

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
  while (false)

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
  while (false)

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
  while (false)

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
  while (false)

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
  t0 = y[0] << OD_COEFF_SHIFT;
  t4 = y[1] << OD_COEFF_SHIFT;
  t2 = y[2] << OD_COEFF_SHIFT;
  t6 = y[3] << OD_COEFF_SHIFT;
  t1 = y[4] << OD_COEFF_SHIFT;
  t5 = y[5] << OD_COEFF_SHIFT;
  t3 = y[6] << OD_COEFF_SHIFT;
  t7 = y[7] << OD_COEFF_SHIFT;
  OD_IDCT_8(t0, t4, t2, t6, t1, t5, t3, t7);
  x[0] = OD_DCT_RSHIFT(t0, OD_COEFF_SHIFT);
  x[1] = OD_DCT_RSHIFT(t1, OD_COEFF_SHIFT);
  x[2] = OD_DCT_RSHIFT(t2, OD_COEFF_SHIFT);
  x[3] = OD_DCT_RSHIFT(t3, OD_COEFF_SHIFT);
  x[4] = OD_DCT_RSHIFT(t4, OD_COEFF_SHIFT);
  x[5] = OD_DCT_RSHIFT(t5, OD_COEFF_SHIFT);
  x[6] = OD_DCT_RSHIFT(t6, OD_COEFF_SHIFT);
  x[7] = OD_DCT_RSHIFT(t7, OD_COEFF_SHIFT);
}

in vec2 tex_coord;

out ivec4 v_low;
out ivec4 v_high;

uniform isampler2D h_low;
uniform isampler2D h_high;

void main() {
  int s=int(tex_coord.s);
  int t=int(tex_coord.t);
  int u=s>>3;
  int v=t<<3;
  int i;
  int x[8];
  int y[8];
  if ((s&0x4)==0) {
    for (i = 0; i < 8; i++) {
      y[7-i]=texelFetch(h_low,ivec2(u,v+i),0)[s&3];
    }
  }
  else {
    for (i = 0; i < 8; i++) {
      y[7-i]=texelFetch(h_high,ivec2(u,v+i),0)[s&3];
    }
  }
  od_bin_idct8(x, y);
  v_low=ivec4(x[0],x[1],x[2],x[3]);
  v_high=ivec4(x[4],x[5],x[6],x[7]);
}
