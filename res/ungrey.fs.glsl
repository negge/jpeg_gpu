#version 140
in vec2 tex_coord;
out vec3 color;
uniform isampler2D low;
uniform isampler2D high;
void main() {
  int s=int(tex_coord.s);
  int t=int(tex_coord.t);
  float y;
  if ((t&0x4)==0) {
    y=float(texelFetch(low, ivec2(s,t>>3),0)[t&3]);
  }
  else {
    y=float(texelFetch(high, ivec2(s,t>>3),0)[t&3]);
  }
  color=vec3(y,y,y)/255.0;
}
