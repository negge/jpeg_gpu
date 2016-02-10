#version 140
in vec3 in_pos;
in ivec2 in_tex;
out vec2 tex_coord;
void main() {
  gl_Position = vec4(in_pos.x, in_pos.y, in_pos.z, 1.0);
  tex_coord = vec2(in_tex);
}
