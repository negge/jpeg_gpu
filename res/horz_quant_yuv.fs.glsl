#version 140

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

uniform int u_cstride;
uniform int v_cstride;
uniform samplerBuffer quant;
uniform isampler2D tex;

void main() {
  int s=int(tex_coord.s);
  int t=int(tex_coord.t);
  int j=((t&0x7)<<3);
  int u=(s<<6)+j;
  int v=t>>3;
  if (v>u_cstride) j+=64;
  if (v>v_cstride) j+=64;
  int i;
  float x[8];
  float y[8];
  for (i=0;i<8;i++) {
    float scale=texelFetch(quant,j+i).r;
    y[i]=scale*texelFetch(tex,ivec2(u+i,v),0).r;
  }
  glj_real_idct8(x, y);
  h_low=vec4(x[0],x[1],x[2],x[3]);
  h_high=vec4(x[4],x[5],x[6],x[7]);
}
