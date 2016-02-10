#version 140
in vec2 tex_coord;
out vec3 color;
uniform usampler2D rgb_tex;
void main() {
  int s=int(tex_coord.s);
  int t=int(tex_coord.t);
  color=vec3(texelFetch(rgb_tex,ivec2(s,t),0).rgb)/255.0;
}
