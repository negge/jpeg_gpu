#version 140
in vec2 tex_coord;
out vec3 color;
uniform int u_xdec;
uniform int u_ydec;
uniform int v_xdec;
uniform int v_ydec;
uniform usampler2D y_tex;
uniform usampler2D u_tex;
uniform usampler2D v_tex;
mat3 yuvColor = mat3(
  1.0,    1.0,     1.0,
  0.0,   -0.34414, 1.772,
  1.402, -0.71414, 0.0
);
void main() {
  int s=int(tex_coord.s);
  int t=int(tex_coord.t);
  float y=float(texelFetch(y_tex,ivec2(s,t),0).r);
  float u=float(texelFetch(u_tex,ivec2(s>>u_xdec,t>>u_ydec),0).r);
  float v=float(texelFetch(v_tex,ivec2(s>>v_xdec,t>>v_ydec),0).r);
  vec3 rgb=yuvColor*vec3(y,u-128,v-128);
  color=rgb/255.0;
}
