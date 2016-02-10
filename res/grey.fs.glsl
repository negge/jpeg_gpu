#version 140
in vec2 tex_coord;
out vec3 color;
uniform usampler2D grey_tex;
void main() {
  int s=int(tex_coord.s);
  int t=int(tex_coord.t);
  float y=float(texelFetch(grey_tex,ivec2(s,t),0).r)/255.0;
  color=vec3(y,y,y);
}
