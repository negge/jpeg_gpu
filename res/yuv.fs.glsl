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
void main() {
  int s=int(tex_coord.s);
  int t=int(tex_coord.t);
  float y=float(texelFetch(y_tex,ivec2(s,t),0).r);
  float u=float(texelFetch(u_tex,ivec2(s>>u_xdec,t>>u_ydec),0).r);
  float v=float(texelFetch(v_tex,ivec2(s>>v_xdec,t>>v_ydec),0).r);
  float r=y+1.402*(v-128);
  float g=y-0.34414*(u-128)-0.71414*(v-128);
  float b=y+1.772*(u-128);
  color=vec3(r,g,b)/255.0;
}
