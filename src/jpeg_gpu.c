#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jpeglib.h>
#include <getopt.h>
#define GLFW_INCLUDE_GLCOREARB
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include "jpeg_wrap.h"

#define NAME "jpeg_gpu"

static const char TEX_VERT[]="\
#version 140\n\
in vec3 in_pos;\n\
in ivec2 in_tex;\n\
out vec2 tex_coord;\n\
void main() {\n\
  gl_Position = vec4(in_pos.x, in_pos.y, in_pos.z, 1.0);\n\
  tex_coord = vec2(in_tex);\n\
}";

static const char YUV_FRAG[]="\
#version 140\n\
in vec2 tex_coord;\n\
out vec3 color;\n\
uniform usampler2D y_tex;\n\
uniform usampler2D u_tex;\n\
uniform usampler2D v_tex;\n\
void main() {\n\
  int s=int(tex_coord.s);\n\
  int t=int(tex_coord.t);\n\
  float y=float(texelFetch(y_tex,ivec2(s,t),0).r);\n\
  float u=float(texelFetch(u_tex,ivec2(s>>1,t>>1),0).r);\n\
  float v=float(texelFetch(v_tex,ivec2(s>>1,t>>1),0).r);\n\
  float r=y+1.402*(v-128);\n\
  float g=y-0.34414*(u-128)-0.71414*(v-128);\n\
  float b=y+1.772*(u-128);\n\
  color=vec3(r,g,b)/255.0;\n\
}";

static const char RGB_FRAG[]="\
#version 140\n\
in vec2 tex_coord;\n\
out vec3 color;\n\
uniform usampler2D rgb_tex;\n\
void main() {\n\
  int s=int(tex_coord.s);\n\
  int t=int(tex_coord.t);\n\
  color=vec3(texelFetch(rgb_tex,ivec2(s,t),0).rgb)/255.0;\n\
}";

static const char GREY_FRAG[]="\
#version 140\n\
in vec2 tex_coord;\n\
out vec3 color;\n\
uniform usampler2D grey_tex;\n\
void main() {\n\
  int s=int(tex_coord.s);\n\
  int t=int(tex_coord.t);\n\
  float y=float(texelFetch(grey_tex,ivec2(s,t),0).r)/255.0;\n\
  color=vec3(y,y,y);\n\
}";

typedef struct vertex vertex;

struct vertex {
  GLfloat x, y, z;
  GLint s, t;
};

static void error_callback(int error, const char* description) {
  fprintf(stderr, "glfw error %i: %s\n", error, description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action,
 int mods) {
  (void)scancode;
  (void)mods;
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GL_TRUE);
  }
}

/* Compile the shader fragment. */
static GLint load_shader(GLuint *_shad,GLenum _shader,const char *_src) {
  int    len;
  GLuint shad;
  char info[8192];
  GLint  status;
  len = strlen(_src);
  shad = glCreateShader(_shader);
  glShaderSource(shad, 1, &_src, &len);
  glCompileShader(shad);
  glGetShaderInfoLog(shad, 8192, &len, info);
  if (len > 0) {
    printf("%s", info);
  }
  glGetShaderiv(shad, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    printf("Failed to compile fragment shader.\n");
    return GL_FALSE;
  }
  *_shad = shad;
  return GL_TRUE;
}

static GLint setup_shader(GLuint *_prog,const char *_vert,const char *_frag) {
  GLuint prog;
  int len;
  char info[8192];
  GLint  status;
  prog = glCreateProgram();
  if (_vert!=NULL) {
    GLuint vert;
    if (!load_shader(&vert, GL_VERTEX_SHADER, _vert)) {
      return GL_FALSE;
    }
    glAttachShader(prog, vert);
  }
  if (_frag!=NULL) {
    GLuint frag;
    if (!load_shader(&frag, GL_FRAGMENT_SHADER, _frag)) {
      return GL_FALSE;
    }
    glAttachShader(prog, frag);
  }
  glLinkProgram(prog);
  glGetProgramiv(prog, GL_LINK_STATUS, &status);
  glGetProgramInfoLog(prog, 8192, &len, info);
  if (len > 0) {
    printf("%s", info);
  }
  if (status != GL_TRUE) {
    printf("Failed to link program.\n");
    return GL_FALSE;
  }
  glUseProgram(prog);
  *_prog = prog;
  return GL_TRUE;
}

static GLint bind_int1(GLuint prog,const char *name, int val) {
  GLint loc;
  loc = glGetUniformLocation(prog, name);
  if (loc < 0) {
    printf("Error finding uniform '%s' in program %i\n", name, prog);
    return GL_FALSE;
  }
  glUniform1i(loc, val);
  return GL_TRUE;
}

static const char *OPTSTRING = "hi:o:";

static const struct option OPTIONS[] = {
  { "help", no_argument, NULL, 'h' },
  { "no-cpu", no_argument, NULL, 0 },
  { "no-gpu", no_argument, NULL, 0 },
  { "impl", required_argument, NULL, 'i' },
  { "out", required_argument, NULL, 'o' },
  { NULL, 0, NULL, 0 }
};

static void usage() {
  fprintf(stderr,
   "Usage: %s [options] jpeg_file\n\n"
   "Options:\n\n"
   "  -h --help                      Display this help and exit.\n"
   "     --no-cpu                    Disable CPU decoding in main loop.\n"
   "     --no-gpu                    Disable GPU decoding in main loop.\n"
   "  -i --impl <decoder>            Software decoder to use.\n"
   "                                 libjpeg (default) => platform libjpeg\n"
   "                                 xjpeg => project decoder\n"
   "  -o --out <format>              Format software decoder should output\n"
   "                                  and send to the GPU for display.\n"
   "                                 yuv (default) => YUV (4:4:4 or 4:2:0)\n"
   "                                 rgb => RGB (4:4:4)\n\n"
   " %s accepts only 8-bit non-hierarchical JPEG files.\n\n", NAME, NAME);
}

int main(int argc, char *argv[]) {
  jpeg_decode_ctx_vtbl vtbl;
  jpeg_decode_out out;
  int no_cpu;
  int no_gpu;
  jpeg_info info;
  image img;
  no_cpu = 0;
  no_gpu = 0;
  vtbl = LIBJPEG_DECODE_CTX_VTBL;
  out = JPEG_DECODE_YUV;
  {
    int c;
    int loi;
    while ((c = getopt_long(argc, argv, OPTSTRING, OPTIONS, &loi)) != EOF) {
      switch (c) {
        case 0 : {
          if (strcmp(OPTIONS[loi].name, "no-cpu") == 0) {
            no_cpu = 1;
          }
          else if (strcmp(OPTIONS[loi].name, "no-gpu") == 0) {
            no_gpu = 1;
          }
          break;
        }
        case 'i' : {
          if (strcmp("libjpeg", optarg) == 0) {
            vtbl = LIBJPEG_DECODE_CTX_VTBL;
          }
          else if (strcmp("xjpeg", optarg) == 0) {
            vtbl = XJPEG_DECODE_CTX_VTBL;
          }
          else {
            fprintf(stderr, "Invalid decoder implementation: %s\n", optarg);
            usage();
            return EXIT_FAILURE;
          }
          break;
        }
        case 'o' : {
          if (strcmp("yuv", optarg) == 0) {
            out = JPEG_DECODE_YUV;
          }
          else if (strcmp("rgb", optarg) == 0) {
            out = JPEG_DECODE_RGB;
          }
          else {
            fprintf(stderr, "Invalid decoder output format: %s\n", optarg);
            usage();
            return EXIT_FAILURE;
          }
          break;
        }
        case 'h' :
        default : {
          usage();
          return EXIT_FAILURE;
        }
      }
    }
    /*Assume anything following the options is a file name.*/
    info.buf = NULL;
    for (; optind < argc; optind++) {
      jpeg_info_init(&info, argv[optind]);
    }
  }
  if (info.buf == NULL) {
    usage();
    return EXIT_FAILURE;
  }

  /* Decompress the jpeg header and allocate memory for the image planes.
     We will directly decode into these buffers and upload them to the GPU. */
  {
    jpeg_decode_ctx *dec;
    jpeg_header header;
    dec = (*vtbl.decode_alloc)(&info);
    (*vtbl.decode_header)(dec, &header);
    if (image_init(&img, &header) != EXIT_SUCCESS) {
      fprintf(stderr, "Error initializing image\n");
      return EXIT_FAILURE;
    }
    (*vtbl.decode_free)(dec);
  }

  /* Open a glfw context and run the entire jpeg decoder inside the main loop.
     We decode only as far as the 8-bit YUV values and then upload these as
      textures to the GPU for the color conversion step.
     This should only upload half as much data as an RGB texture for 4:2:0
      images. */
  {
    jpeg_decode_ctx *dec;
    GLFWwindow *window;
    GLuint tex[NPLANES_MAX];
    GLuint prog;
    GLuint vbo;
    GLuint vao;
    double last;
    int frames;
    int i;
    int first;
    int pixels;

    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
      return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    window = glfwCreateWindow(img.width, img.height, NAME, NULL, NULL);
    if (!window) {
      glfwTerminate();
      return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSwapInterval(0);

    printf("  OpenGL: %s\n",glGetString(GL_VERSION));
    printf("    GLSL: %s\n",glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("Renderer: %s\n",glGetString(GL_RENDERER));

    glViewport(0, 0, img.width, img.height);

    switch (out) {
      case JPEG_DECODE_YUV : {
        glGenTextures(img.nplanes, tex);
        for (i = 0; i < img.nplanes; i++) {
          image_plane *plane;
          plane = &img.plane[i];
          printf("Texture %i: %i\n", i, tex[i]);
          glActiveTexture(GL_TEXTURE0 + i);
          glBindTexture(GL_TEXTURE_2D, tex[i]);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
          glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, plane->width, plane->height,
           0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, plane->data);
        }
        switch (img.nplanes) {
          case 1 : {
            if (!setup_shader(&prog, TEX_VERT, GREY_FRAG)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog, "grey_tex", 0)) {
              return EXIT_FAILURE;
            }
            break;
          }
          case 3 : {
            if (!setup_shader(&prog, TEX_VERT, YUV_FRAG)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog, "y_tex", 0)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog, "u_tex", 1)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog, "v_tex", 2)) {
              return EXIT_FAILURE;
            }
            break;
          }
        }
        break;
      }
      case JPEG_DECODE_RGB : {
        glGenTextures(1, tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        switch (img.nplanes) {
          case 1 : {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, img.width, img.height,
             0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, img.pixels);
            if (!setup_shader(&prog, TEX_VERT, GREY_FRAG)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog, "grey_tex", 0)) {
              return EXIT_FAILURE;
            }
            break;
          }
          case 3 : {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8UI, img.width, img.height,
             0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, img.pixels);
            if (!setup_shader(&prog, TEX_VERT, RGB_FRAG)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog, "rgb_tex", 0)) {
              return EXIT_FAILURE;
            }
            break;
          }
        }
        break;
      }
      default : {
        fprintf(stderr, "Unsupported output %i\n", out);
        return EXIT_FAILURE;
      }
    }

    /* Create the vertex buffer object */
    {
      GLint in_pos;
      GLint in_tex;
      vertex v[4];

      in_pos = glGetAttribLocation(prog, "in_pos");
      printf("in_pos %i\n", in_pos);

      in_tex = glGetAttribLocation(prog, "in_tex");
      printf("in_tex %i\n", in_tex);

      /* Set the vertex world positions */
      v[0].x =  1.0; v[0].y =  1.0; v[1].z = 0.0;
      v[1].x =  1.0; v[1].y = -1.0; v[2].z = 0.0;
      v[2].x = -1.0; v[2].y =  1.0; v[0].z = 0.0;
      v[3].x = -1.0; v[3].y = -1.0; v[3].z = 0.0;

      /* Set the vertex texture coordinates */
      v[0].s = img.width; v[0].t = 0;
      v[1].s = img.width; v[1].t = img.height;
      v[2].s = 0;         v[2].t = 0;
      v[3].s = 0;         v[3].t = img.height;

      glGenVertexArrays(1, &vao);
      glBindVertexArray(vao);

      glGenBuffers(1, &vbo);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);

      glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

      glVertexAttribPointer(in_pos, 3, GL_FLOAT, GL_FALSE, sizeof(vertex),
       (void *)0);

      glVertexAttribIPointer(in_tex, 2, GL_INT, sizeof(vertex), (void *)12);

      glEnableVertexAttribArray(in_pos);
      glEnableVertexAttribArray(in_tex);
    }

    glBindFragDataLocation(prog, 0, "color");
    glUseProgram(prog);

    dec = (*vtbl.decode_alloc)(&info);

    first = 0;
    last = glfwGetTime();
    frames = 0;
    /* TODO Compute this based on out */
    pixels = 0;
    for (i = 0; i < img.nplanes; i++) {
      image_plane *plane;
      plane = &img.plane[i];
      pixels += (plane->width >> plane->xdec)*(plane->height >> plane->ydec);
    }
    while (!glfwWindowShouldClose(window)) {
      int i;
      double time;

      if (!no_cpu) {
        jpeg_header header;

        (*vtbl.decode_reset)(dec, &info);
        (*vtbl.decode_header)(dec, &header);
        if ((*vtbl.decode_image)(dec, &img, out) != EXIT_SUCCESS) {
         break;
        }
      }

      if (first) {
        for (i = 0; i < img.nplanes; i++) {
          image_plane *plane;
          int j, k;
          plane = &img.plane[i];
          printf("plane %i\n", i);
          for (k = 0; k < plane->height; k++) {
            for (j = 0; j < plane->width; j++) {
              printf("%i ", plane->data[k*plane->width + j]);
            }
            printf("\n");
          }
          printf("\n");
        }
        first = 0;
      }

      if (!no_gpu) {
        switch (out) {
          case JPEG_DECODE_YUV : {
            for (i = 0; i < img.nplanes; i++) {
              image_plane *plane;
              plane = &img.plane[i];
              glActiveTexture(GL_TEXTURE0+i);
              glBindTexture(GL_TEXTURE_2D, tex[i]);
              glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, plane->width, plane->height,
               0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, plane->data);
            }
            break;
          }
          case JPEG_DECODE_RGB : {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex[0]);
            switch (img.nplanes) {
              case 1 : {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, img.width, img.height,
                 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, img.pixels);
                break;
              }
              case 3 : {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8UI, img.width, img.height,
                 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, img.pixels);
                break;
              }
            }
            break;
          }
          default : {
            fprintf(stderr, "Unsupported output %i\n", out);
            return EXIT_FAILURE;
          }
        }

        glClear(GL_COLOR_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);
      }

      frames++;
      time = glfwGetTime();
      if (time - last >= 1.0) {
        double avg;
        char title[255];
        avg = 1000*(time - last)/frames;
        if (no_gpu) {
          sprintf(title, "%s - %4i FPS (%0.3f ms)", NAME, frames, avg);
        }
        else {
          sprintf(title, "%s - %4i FPS (%0.3f ms) %0.3f MBps", NAME, frames,
           avg, frames*(pixels/1000000.0));
        }
        glfwSetWindowTitle(window, title);
        frames = 0;
        last = time;
      }

      glfwPollEvents();
    }
    glfwDestroyWindow(window);

    glDeleteTextures(img.nplanes, tex);
    (*vtbl.decode_free)(dec);
  }

  jpeg_info_clear(&info);
  image_clear(&img);
  return EXIT_SUCCESS;
}
