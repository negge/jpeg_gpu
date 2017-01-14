#version 140

int DE_ZIG_ZAG[64] = int[](
   0,  1,  8, 16,  9,  2,  3, 10,
  17, 24, 32, 25, 18, 11,  4,  5,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13,  6,  7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63
);

void glj_real_idct8(out float x[8], const float y[8]) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float u0;
  float u1;
  float u2;
  float u3;
  float u4;
  float u5;
  float u6;
  float u7;
  float u8;
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
  u2 = (t2 - t3)*1.4142135623730950488016887242097 - u3;
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
  u5 = (t7 - t5)*1.4142135623730950488016887242097;
  u8 = (t4 + t6)*1.8477590650225735122563663787936;
  u4 = u8 - t4*1.0823922002923939687994464107328;
  u6 = u8 - t6*2.6131259297527530557132863468544;
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
  x[0] = u0;
  x[1] = u1;
  x[2] = u2;
  x[3] = u3;
  x[4] = u4;
  x[5] = u5;
  x[6] = u6;
  x[7] = u7;
}

in vec2 tex_coord;

out vec4 h_low;
out vec4 h_high;

uniform int y_stride;
uniform samplerBuffer quant;
uniform isamplerBuffer index;
uniform usamplerBuffer pack;

void main() {
  int b[8*8]/* = int[](
     0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0
  )*/;
  int s = int(tex_coord.s);
  int t = int(tex_coord.t);
  int i = texelFetch(index, (t>>3)*(y_stride>>3) + s).r;
  uint p = texelFetch(pack, i).r;
  i++;
  int j = 0;
  for (j=0;j<64;j++) b[j]=0;
  j=0;
  b[0] = int(p | ((p & uint(0x0800)) == uint(0x0800) ? uint(~0xfff) : uint(0)));
  int len;
  while (j < 63) {
    p = texelFetch(pack, i).r;
    i++;
    if (p == uint(0)) {
      break;
    }
    len = int((p >> 12) & uint(0xf)) + 1;
    p &= uint(0xfff);
    int c = int(p | ((p & uint(0x0800)) == uint(0x0800) ? uint(~0xfff) : uint(0)));
    j += len;
    b[DE_ZIG_ZAG[j]] = c;
  }
  j = ((t & 0x7) << 3);
  float x[8];
  float y[8];
  for (i = 0; i < 8; i++) {
    float scale = texelFetch(quant, j + i).r;
    y[i]=scale*b[j + i];
  }
  glj_real_idct8(x, y);
  h_low=vec4(x[0],x[1],x[2],x[3]);
  h_high=vec4(x[4],x[5],x[6],x[7]);
  /*if (t>-1) {
    h_low=vec4(j,0,0,0);
    h_high=vec4(t,1,1,1);
    h_low=vec4(b[j+0],b[j+1],b[j+2],b[j+3]);
    h_high=vec4(b[j+4],b[j+5],b[j+6],b[j+7]);
  }*/
 }
