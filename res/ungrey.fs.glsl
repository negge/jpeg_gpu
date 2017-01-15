#version 140
in vec2 tex_coord;
out vec3 color;
uniform isampler2D low;
uniform isampler2D high;
void main() {
  int s=int(tex_coord.s);
  int t=int(tex_coord.t);
  int v=t>>3;
  int j=t%8;
  float y;
  if (j<4) {
    y=float(texelFetch(low, ivec2(s,v),0)[j]);
  }
  else {
    y=float(texelFetch(high, ivec2(s,v),0)[j-4]);
  }
  color=vec3(y,y,y)/255.0;
}
