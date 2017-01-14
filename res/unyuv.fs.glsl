#version 140
in vec2 tex_coord;
out vec3 color;
uniform int y_width;
uniform int y_height;
uniform int u_xdec;
uniform int u_ydec;
uniform int v_xdec;
uniform int v_ydec;
uniform isampler2D low;
uniform isampler2D high;
mat3 yuvColor = mat3(
  1.0,    1.0,     1.0,
  0.0,   -0.34414, 1.772,
  1.402, -0.71414, 0.0
);
void main() {
  int s=int(tex_coord.s);
  int t=int(tex_coord.t);
  float y;
  float u;
  float v;
  if ((t&0x4)==0) {
    y=float(texelFetch(low, ivec2(s,t>>3),0)[t&3])+128;
  }
  else {
    y=float(texelFetch(high, ivec2(s,t>>3),0)[t&3])+128;
  }
  int by_u=t>>(u_ydec+3);
  int s_u=(y_width>>u_xdec)*(by_u&((1<<u_xdec)-1))+(s>>u_xdec);
  int t_u=y_height+((by_u>>u_xdec)<<3)+((t>>u_ydec)&0x7);
  if ((t_u&0x4)==0) {
    u=float(texelFetch(low, ivec2(s_u,t_u>>3),0)[t_u&3]);
  }
  else {
    u=float(texelFetch(high, ivec2(s_u,t_u>>3),0)[t_u&3]);
  }
  by_u=((y_height>>(u_ydec+3))+((1<<u_xdec)-1))>>u_xdec;
  int by_v=t>>(v_ydec+3);
  int s_v=(y_width>>v_xdec)*(by_v&((1<<v_xdec)-1))+(s>>v_xdec);
  int t_v=y_height+((by_u+(by_v>>v_xdec))<<3)+((t>>v_ydec)&0x7);
  if ((t_v&0x4)==0) {
    v=float(texelFetch(low, ivec2(s_v,t_v>>3),0)[t_v&3]);
  }
  else {
    v=float(texelFetch(high, ivec2(s_v,t_v>>3),0)[t_v&3]);
  }
  vec3 rgb=yuvColor*vec3(y,u,v);
  color=rgb/255.0;
}
