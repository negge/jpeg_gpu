/* JPEG GPU project
Copyright (c) 2014-2016 JPEG GPU project contributors.  All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License"); you may not
 use this file except in compliance with the License.
You may obtain a copy of the License at:

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
 under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and limitations
 under the License. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jpeglib.h>
#include <getopt.h>
#define GLFW_INCLUDE_GLCOREARB
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include "jpeg_gpu.h"
#include "jpeg_wrap.h"
#include "logging.h"

#define NAME "jpeg_gpu"

#define NBUFFS_MAX (1)
#define NTEXTS_MAX (6)
#define NPROGS_MAX (3)

float GLJ_REAL_IDCT8X8_SCALES[8*8] = {
  0.12500000000000000000000000000000,  0.17337998066526843272770239894580,
  0.16332037060954706598208039667840,  0.14698445030241983962180838807641,
  0.12500000000000000000000000000000,  0.098211869798387772659737170957152,
  0.067649512518274623049965400670799, 0.034487422410367876541994695458672,
  0.17337998066526843272770239894580,  0.24048494156391084451602289867460,
  0.22653186158822196077562132671902,  0.20387328921222928506612844393770,
  0.17337998066526843272770239894580,  0.13622377669395466201616304326691,
  0.093832569379466311573889460303194, 0.047835429045636221466057498003800,
  0.16332037060954706598208039667840,  0.22653186158822196077562132671902,
  0.21338834764831844055010554526311,  0.19204443917785408423362663126035,
  0.16332037060954706598208039667840,  0.12831999178983418811588415576187,
  0.088388347648318440550105545263106, 0.045059988875434244611818243183939,
  0.14698445030241983962180838807641,  0.20387328921222928506612844393770,
  0.19204443917785408423362663126035,  0.17283542904563622146605749800380,
  0.14698445030241983962180838807641,  0.11548494156391084451602289867460,
  0.079547411285802121153812938642610, 0.040552918602682219084048047259306,
  0.12500000000000000000000000000000,  0.17337998066526843272770239894580,
  0.16332037060954706598208039667840,  0.14698445030241983962180838807641,
  0.12500000000000000000000000000000,  0.098211869798387772659737170957152,
  0.067649512518274623049965400670799, 0.034487422410367876541994695458672,
  0.098211869798387772659737170957152, 0.13622377669395466201616304326691,
  0.12831999178983418811588415576187,  0.11548494156391084451602289867460,
  0.098211869798387772659737170957152, 0.077164570954363778533942501996200,
  0.053151880922953528047918927773213, 0.027096593915592403965917353411492,
  0.067649512518274623049965400670799, 0.093832569379466311573889460303194,
  0.088388347648318440550105545263106, 0.079547411285802121153812938642610,
  0.067649512518274623049965400670799, 0.053151880922953528047918927773213,
  0.036611652351681559449894454736894, 0.018664458512585651505924232314542,
  0.034487422410367876541994695458672, 0.047835429045636221466057498003800,
  0.045059988875434244611818243183939, 0.040552918602682219084048047259306,
  0.034487422410367876541994695458672, 0.027096593915592403965917353411492,
  0.018664458512585651505924232314542, 0.0095150584360891554839771013254015
};

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
  if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GL_TRUE);
  }
}

static int window_width;

static int window_height;

static void size_callback(GLFWwindow *window, int width, int height) {
  (void)window;
  window_width = width;
  window_height = height;
}

/* Compile the shader fragment. */
static GLint load_shader(GLuint *_shad,GLenum _shader,const char *_src) {
  int len;
  GLuint shad;
  char info[8192];
  GLint  status;
  len = strlen(_src);
  shad = glCreateShader(_shader);
  glShaderSource(shad, 1, &_src, &len);
  glCompileShader(shad);
  glGetShaderInfoLog(shad, 8192, &len, info);
  if (len > 0) {
    printf("%s:%s", _src, info);
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

typedef enum texture_format {
  U8_1 = 0,
  U8_3 = 1,
  I16_1 = 2,
  I16_4 = 3,
  F32_1 = 4,
  F32_4 = 5
} texture_format;

typedef struct texture_format_info texture_format_info;

struct texture_format_info {
  GLint internal;
  GLenum format;
  GLenum type;
};

const texture_format_info TEXTURE_FORMATS[] = {
  { GL_R8UI,    GL_RED_INTEGER,  GL_UNSIGNED_BYTE },
  { GL_RGB8UI,  GL_RGB_INTEGER,  GL_UNSIGNED_BYTE },
  { GL_R16I,    GL_RED_INTEGER,  GL_SHORT },
  { GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT },
  { GL_R32F,    GL_RED,          GL_FLOAT },
  { GL_RGBA32F, GL_RGBA,         GL_FLOAT },
};

static GLint create_buffer(GLuint *buf, int length) {
  glGenBuffers(1, buf);
  glBindBuffer(GL_TEXTURE_BUFFER, *buf);
  glBufferData(GL_TEXTURE_BUFFER, length, NULL, GL_STATIC_DRAW);
  return GL_TRUE;
}

static void update_buffer(GLuint buf, int length, GLvoid *data) {
  glBindBuffer(GL_TEXTURE_BUFFER, buf);
  glBufferSubData(GL_TEXTURE_BUFFER, 0, length, data);
}

static GLint create_texture_buffer(GLuint *tex, int id, GLuint buf,
 texture_format fmt) {
  GLint internal;
  internal = TEXTURE_FORMATS[fmt].internal;
  glGenTextures(1, tex);
  glActiveTexture(GL_TEXTURE0 + id);
  glBindTexture(GL_TEXTURE_BUFFER, *tex);
  glTexBuffer(GL_TEXTURE_BUFFER, internal, buf);
  return GL_TRUE;
}

static GLint create_texture(GLuint *tex, int id, int width, int height,
 texture_format fmt) {
  GLint internal;
  GLenum format;
  GLenum type;
  internal = TEXTURE_FORMATS[fmt].internal;
  format = TEXTURE_FORMATS[fmt].format;
  type = TEXTURE_FORMATS[fmt].type;
  glGenTextures(1, tex);
  glActiveTexture(GL_TEXTURE0 + id);
  glBindTexture(GL_TEXTURE_2D, *tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, type,
   NULL);
  return GL_TRUE;
}

static void update_texture(GLuint tex, int id, int width, int height,
 texture_format fmt, GLvoid *data) {
  GLenum format;
  GLenum type;
  format = TEXTURE_FORMATS[fmt].format;
  type = TEXTURE_FORMATS[fmt].type;
  glActiveTexture(GL_TEXTURE0 + id);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, data);
}

static void print_texture(GLuint tex, int width, int height,
 texture_format fmt, void *buf) {
  int i, j;
  glBindTexture(GL_TEXTURE_2D, tex);
  printf("Texture %i:\n", tex);
  switch (fmt) {
    case U8_1 : {
      unsigned char *pixels;
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, buf);
      pixels = buf;
      for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
          printf("%s%4i", i > 0 ? ", " : "", pixels[j*width + i]);
        }
        printf("\n");
      }
      break;
    }
    case U8_3 : {
      unsigned char *pixels;
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, buf);
      pixels = buf;
      for (j = 0; j < height; j++) {
        for (i = 0; i < width*3; i++) {
          printf("%s%4i%s", i%3 == 0 ? "(" : "", pixels[j*width*3 + i],
           (i + 1)%3 == 0 ? ") " : ", ");
        }
        printf("\n");
      }
      break;
    }
    case I16_1 : {
      short *pixels;
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_SHORT, buf);
      pixels = buf;
      for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
          printf("%s%4i", i > 0 ? ", " : "", pixels[j*width + i]);
        }
        printf("\n");
      }
      break;
    }
    case I16_4 : {
      short *pixels;
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA_INTEGER, GL_SHORT, buf);
      pixels = buf;
      for (j = 0; j < height; j++) {
        for (i = 0; i < width*4; i++) {
          printf("%s%4i%s", i%4 == 0 ? "(" : "", pixels[j*width*4 + i],
           (i + 1)%4 == 0 ? ") " : ", ");
        }
        printf("\n");
      }
      break;
    }
    case F32_1 : {
      float *pixels;
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, buf);
      pixels = buf;
      for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
          printf("%s%f", i > 0 ? ", " : "", pixels[j*width + i]);
        }
        printf("\n");
      }
      break;
    }
    case F32_4 : {
      float *pixels;
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, buf);
      pixels = buf;
      for (j = 0; j < height; j++) {
        for (i = 0; i < width*4; i++) {
          printf("%s%f%s", i%4 == 0 ? "(" : "", pixels[j*width*4 + i],
           (i + 1)%4 == 0 ? ") " : ", ");
        }
        printf("\n");
      }
      break;
    }
  }
}

static GLint create_framebuffer(GLuint *fbo, int att, int off, GLuint *tex) {
  int i;
  GLenum buf[2];
  GLenum status;
  glGenFramebuffers(1, fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, *fbo);
  for (i = 0; i < att; i++) {
   glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + off + i,
    GL_TEXTURE_2D, tex[i], 0);
    buf[i] = GL_COLOR_ATTACHMENT0 + off + i;
  }
  glDrawBuffers(att, buf);
  status=glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
  if (status!=GL_FRAMEBUFFER_COMPLETE) {
    printf("Error creating framebuffer object: %i\n", status);
    return GL_FALSE;
  }
  return GL_TRUE;
}

/* This program creates a VBO / VAO and binds the texture coodinates for a
    shader program that assumes the TEX_VS vertex shader. */
static GLint create_tex_rect(GLuint *vao, GLuint *vbo, GLuint prog, int width,
 int height) {
  GLint in_pos;
  GLint in_tex;
  vertex v[4];

  in_pos = glGetAttribLocation(prog, "in_pos");
  if (in_pos < 0) {
    fprintf(stderr, "Error finding attribute 'in_pos' in program %i\n", prog);
    return GL_FALSE;
  }

  in_tex = glGetAttribLocation(prog, "in_tex");
  if (in_tex < 0) {
    fprintf(stderr, "Error finding attribute 'in_tex' in program %i\n", prog);
    return GL_FALSE;
  }

  /* Set the vertex world positions */
  v[0].x =  1.0; v[0].y =  1.0; v[1].z = 0.0;
  v[1].x =  1.0; v[1].y = -1.0; v[2].z = 0.0;
  v[2].x = -1.0; v[2].y =  1.0; v[0].z = 0.0;
  v[3].x = -1.0; v[3].y = -1.0; v[3].z = 0.0;

  /* Set the vertex texture coordinates */
  v[0].s = width; v[0].t = 0;
  v[1].s = width; v[1].t = height;
  v[2].s = 0;     v[2].t = 0;
  v[3].s = 0;     v[3].t = height;

  glGenVertexArrays(1, vao);
  glBindVertexArray(*vao);

  glGenBuffers(1, vbo);
  glBindBuffer(GL_ARRAY_BUFFER, *vbo);

  glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

  glVertexAttribPointer(in_pos, 3, GL_FLOAT, GL_FALSE, sizeof(vertex),
   (void *)0);

  glVertexAttribIPointer(in_tex, 2, GL_INT, sizeof(vertex), (void *)12);

  glEnableVertexAttribArray(in_pos);
  glEnableVertexAttribArray(in_tex);

  return GL_TRUE;
}

static const char *OPTSTRING = "hi:o:dH";

static const struct option OPTIONS[] = {
  { "help", no_argument, NULL, 'h' },
  { "no-cpu", no_argument, NULL, 0 },
  { "no-gpu", no_argument, NULL, 0 },
  { "impl", required_argument, NULL, 'i' },
  { "out", required_argument, NULL, 'o' },
  { "dump", no_argument, NULL, 'd' },
  { "header", no_argument, NULL, 'H' },
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
   "                                 pack => RLC zero packed and quantized.\n"
   "                                 quant => quantized but de-zigzaged.\n"
   "                                 dct => DCT (12-bit dequantized)\n"
   "                                 yuv (default) => YUV (4:4:4 or 4:2:0)\n"
   "                                 rgb => RGB (4:4:4)\n"
   "  -d --dump                      Dump jpeg data in the output format.\n"
   "  -H --header                    Print the jpeg header.\n\n"
   " %s accepts only 8-bit non-hierarchical JPEG files.\n\n", NAME, NAME);
}

int main(int argc, char *argv[]) {
  jpeg_decode_ctx_vtbl vtbl;
  jpeg_decode_out out;
  int no_cpu;
  int no_gpu;
  int dump;
  int head;
  jpeg_info info;
  jpeg_header header;
  image img;
  no_cpu = 0;
  no_gpu = 0;
  dump = 0;
  head = 0;
  glj_log_init(NULL);
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
          else if (strcmp(OPTIONS[loi].name, "dump") == 0) {
            dump = 1;
          }
          else if (strcmp(OPTIONS[loi].name, "header") == 0) {
            head = 1;
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
          if (strcmp("pack", optarg) == 0) {
            out = JPEG_DECODE_PACK;
          }
          else if (strcmp("quant", optarg) == 0) {
            out = JPEG_DECODE_QUANT;
          }
          else if (strcmp("dct", optarg) == 0) {
            out = JPEG_DECODE_DCT;
          }
          else if (strcmp("yuv", optarg) == 0) {
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
        case 'd' : {
          dump = 1;
          break;
        }
        case 'H' : {
          head = 1;
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
    dec = (*vtbl.decode_alloc)(&info);
    (*vtbl.decode_header)(dec, &header);
    if (head) {
      int i, j;
      printf("Image Size         : %ix%i\n", header.width, header.height);
      printf("Bits Per Pixel     : %i\n", header.bits);
      printf("Components         : %i\n", header.ncomps);
      printf("Chroma Subsampling : %s\n", JPEG_SUBSAMP_NAMES[header.subsamp]);
      printf("Minimum Coded Unit : ");
      for (i = 0; i < header.ncomps; i++) {
        printf("%s%ix%i", i > 0 ? " " : "", header.comp[i].hsamp,
         header.comp[i].vsamp);
      }
      printf("\n");
      printf("Restart Interval   : %i\n", header.restart_interval);
      for (i = 0; i < NQUANT_MAX; i++) {
        if (header.quant[i].valid) {
          printf("Quant Table %i Bits : %i\n", i, header.quant[i].bits);
          for (j = 1; j <= 64; j++) {
            printf("%4i%s", header.quant[i].tbl[j - 1], j & 0x7 ? "" : "\n");
          }
        }
      }
      return EXIT_SUCCESS;
    }
    if (image_init(&img, &header) != EXIT_SUCCESS) {
      fprintf(stderr, "Error initializing image\n");
      return EXIT_FAILURE;
    }
    if (dump) {
      int i, j, k;
      (*vtbl.decode_image)(dec, &img, out);
      for (i = 0; i < img.nplanes; i++) {
        image_plane *plane;
        plane = &img.plane[i];
        printf("Plane %i\n", i);
        switch (out) {
          case JPEG_DECODE_QUANT :
          case JPEG_DECODE_DCT : {
            for (k = 0; k < plane->height; k++) {
              for (j = 0; j < plane->width; j++) {
                printf("%4i ", plane->coef[k*plane->width + j]);
              }
              printf("\n");
            }
            break;
          }
          case JPEG_DECODE_YUV : {
            for (k = 0; k < plane->height; k++) {
              for (j = 0; j < plane->width; j++) {
                printf("%4i ", plane->data[k*plane->width + j]);
              }
              printf("\n");
            }
            break;
          }
          case JPEG_DECODE_RGB : {
            for (k = 0; k < img.height; k++) {
              for (j = 0; j < img.width; j++) {
                printf("%4i ", img.pixels[(k*plane->width + j)*3 + i]);
              }
              printf("\n");
            }
            break;
          }
          default : {
            fprintf(stderr, "Unsupported output '%s'.\n",
             JPEG_DECODE_OUT_NAMES[out]);
            return EXIT_FAILURE;
          }
        }
        printf("\n");
      }
      return EXIT_SUCCESS;
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
    GLuint buf[NBUFFS_MAX];
    GLuint tex[NTEXTS_MAX];
    GLuint fbo[NPROGS_MAX];
    GLuint prog[NPROGS_MAX];
    GLuint vbo[NPROGS_MAX];
    GLuint vao[NPROGS_MAX];
    double last;
    int frames;
    int i, j;
    int pixels;

    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
      return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    /* These global variables are better than the overhead of calling
        glfwGetFramebufferSize() to get the current window in repaint loop
        before the call to glViewport(). */
    window_width = img.width;
    window_height = img.height;
    window = glfwCreateWindow(window_width, window_height, NAME, NULL, NULL);
    if (!window) {
      glfwTerminate();
      return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowSizeCallback(window, size_callback);
    glfwSwapInterval(0);

    printf("  OpenGL: %s\n",glGetString(GL_VERSION));
    printf("    GLSL: %s\n",glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("Renderer: %s\n",glGetString(GL_RENDERER));

    switch (out) {
      case JPEG_DECODE_QUANT : {
        int width;
        int height;
        width = img.plane[0].width;
        height = 0;
        for (i = 0; i < img.nplanes; i++) {
          height += img.plane[i].cstride;
        }
        switch (img.nplanes) {
          case 1 : {
            if (!setup_shader(&prog[0], TEX_VS, HORZ_QUANT_GREY_FS)) {
              return EXIT_FAILURE;
            }
            if (!create_buffer(&buf[0], 64*sizeof(float))) {
              return EXIT_FAILURE;
            }
            break;
          }
          case 3 : {
            if (!setup_shader(&prog[0], TEX_VS, HORZ_QUANT_YUV_FS)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "u_cstride", img.plane[1].cstride*8)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "v_cstride", img.plane[2].cstride*8)) {
              return EXIT_FAILURE;
            }
            if (!create_buffer(&buf[0], 3*64*sizeof(float))) {
              return EXIT_FAILURE;
            }
          }
        }
        if (!create_texture_buffer(&tex[0], 0, buf[0], F32_1)) {
          return EXIT_FAILURE;
        }
        if (!create_texture(&tex[1], 1, width*8, height, I16_1)) {
          return EXIT_FAILURE;
        }
        if (!bind_int1(prog[0], "quant", 0)) {
          return EXIT_FAILURE;
        }
        if (!bind_int1(prog[0], "tex", 1)) {
          return EXIT_FAILURE;
        }
        if (!create_tex_rect(&vao[0], &vbo[0], prog[0], width/8, height*8)) {
          return EXIT_FAILURE;
        }
        if (!create_texture(&tex[2], 2, width/8, height*8, F32_4)) {
          return EXIT_FAILURE;
        }
        if (!create_texture(&tex[3], 3, width/8, height*8, F32_4)) {
          return EXIT_FAILURE;
        }
        if (!setup_shader(&prog[1], TEX_VS, VERT_FS)) {
          return EXIT_FAILURE;
        }
        if (!bind_int1(prog[1], "h_low", 2)) {
           return EXIT_FAILURE;
        }
        if (!bind_int1(prog[1], "h_high", 3)) {
           return EXIT_FAILURE;
        }
        if (!create_tex_rect(&vao[1], &vbo[1], prog[1], width, height)) {
          return EXIT_FAILURE;
        }
        if (!create_texture(&tex[4], 4, width, height, I16_4)) {
          return EXIT_FAILURE;
        }
        if (!create_texture(&tex[5], 5, width, height, I16_4)) {
          return EXIT_FAILURE;
        }
        switch (img.nplanes) {
          case 1 : {
            if (!setup_shader(&prog[2], TEX_VS, UNGREY_FS)) {
              return EXIT_FAILURE;
            }
            break;
          }
          case 3 : {
            if (!setup_shader(&prog[2], TEX_VS, UNYUV_FS)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "y_width", img.plane[0].width)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "y_height", img.plane[0].height)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "u_xdec", img.plane[1].xdec)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "u_ydec", img.plane[1].ydec)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "v_xdec", img.plane[2].xdec)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "v_ydec", img.plane[2].ydec)) {
              return EXIT_FAILURE;
            }
            break;
          }
        }
        if (!bind_int1(prog[2], "low", 4)) {
           return EXIT_FAILURE;
        }
        if (!bind_int1(prog[2], "high", 5)) {
           return EXIT_FAILURE;
        }
        if (!create_tex_rect(&vao[2], &vbo[2], prog[2], img.width,
         img.height)) {
          return EXIT_FAILURE;
        }
        glBindFragDataLocation(prog[2], 0, "color");
        if (!create_framebuffer(&fbo[0], 2, 0, &tex[2])) {
          return EXIT_FAILURE;
        }
        if (!create_framebuffer(&fbo[1], 2, 2, &tex[4])) {
          return EXIT_FAILURE;
        }
        break;
      }
      case JPEG_DECODE_DCT : {
        int width;
        int height;
        width = img.plane[0].width;
        height = 0;
        for (i = 0; i < img.nplanes; i++) {
          height += img.plane[i].cstride;
        }
        if (!setup_shader(&prog[0], TEX_VS, HORZ_FS)) {
          return EXIT_FAILURE;
        }
        if (!create_texture(&tex[0], 0, width*8, height, I16_1)) {
          return EXIT_FAILURE;
        }
        if (!bind_int1(prog[0], "tex", 0)) {
          return EXIT_FAILURE;
        }
        if (!create_tex_rect(&vao[0], &vbo[0], prog[0], width/8, height*8)) {
          return EXIT_FAILURE;
        }
        if (!create_texture(&tex[1], 1, width/8, height*8, F32_4)) {
          return EXIT_FAILURE;
        }
        if (!create_texture(&tex[2], 2, width/8, height*8, F32_4)) {
          return EXIT_FAILURE;
        }
        if (!setup_shader(&prog[1], TEX_VS, VERT_FS)) {
          return EXIT_FAILURE;
        }
        if (!bind_int1(prog[1], "h_low", 1)) {
           return EXIT_FAILURE;
        }
        if (!bind_int1(prog[1], "h_high", 2)) {
           return EXIT_FAILURE;
        }
        if (!create_tex_rect(&vao[1], &vbo[1], prog[1], width, height)) {
          return EXIT_FAILURE;
        }
        if (!create_texture(&tex[3], 3, width, height, I16_4)) {
          return EXIT_FAILURE;
        }
        if (!create_texture(&tex[4], 4, width, height, I16_4)) {
          return EXIT_FAILURE;
        }
        switch (img.nplanes) {
          case 1 : {
            if (!setup_shader(&prog[2], TEX_VS, UNGREY_FS)) {
              return EXIT_FAILURE;
            }
            break;
          }
          case 3 : {
            if (!setup_shader(&prog[2], TEX_VS, UNYUV_FS)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "y_width", img.plane[0].width)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "y_height", img.plane[0].height)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "u_xdec", img.plane[1].xdec)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "u_ydec", img.plane[1].ydec)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "v_xdec", img.plane[2].xdec)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[2], "v_ydec", img.plane[2].ydec)) {
              return EXIT_FAILURE;
            }
            break;
          }
        }
        if (!bind_int1(prog[2], "low", 3)) {
           return EXIT_FAILURE;
        }
        if (!bind_int1(prog[2], "high", 4)) {
           return EXIT_FAILURE;
        }
        if (!create_tex_rect(&vao[2], &vbo[2], prog[2], img.width,
         img.height)) {
          return EXIT_FAILURE;
        }
        glBindFragDataLocation(prog[2], 0, "color");
        if (!create_framebuffer(&fbo[0], 2, 0, &tex[1])) {
          return EXIT_FAILURE;
        }
        if (!create_framebuffer(&fbo[1], 2, 2, &tex[3])) {
          return EXIT_FAILURE;
        }
        break;
      }
      case JPEG_DECODE_YUV : {
        for (i = 0; i < img.nplanes; i++) {
          image_plane *plane;
          plane = &img.plane[i];
          if (!create_texture(&tex[i], i, plane->width, plane->height, U8_1)) {
            return EXIT_FAILURE;
          }
        }
        switch (img.nplanes) {
          case 1 : {
            if (!setup_shader(&prog[0], TEX_VS, GREY_FS)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "grey_tex", 0)) {
              return EXIT_FAILURE;
            }
            break;
          }
          case 3 : {
            if (!setup_shader(&prog[0], TEX_VS, YUV_FS)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "y_tex", 0)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "u_tex", 1)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "u_xdec", img.plane[1].xdec)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "u_ydec", img.plane[1].ydec)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "v_tex", 2)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "v_xdec", img.plane[2].xdec)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "v_ydec", img.plane[2].ydec)) {
              return EXIT_FAILURE;
            }
            break;
          }
        }
        if (!create_tex_rect(vao, vbo, prog[0], img.width, img.height)) {
          return EXIT_FAILURE;
        }
        glBindFragDataLocation(prog[0], 0, "color");
        break;
      }
      case JPEG_DECODE_RGB : {
        switch (img.nplanes) {
          case 1 : {
            if (!create_texture(tex, 0, img.width, img.height, U8_1)) {
              return EXIT_FAILURE;
            }
            if (!setup_shader(&prog[0], TEX_VS, GREY_FS)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "grey_tex", 0)) {
              return EXIT_FAILURE;
            }
            break;
          }
          case 3 : {
            if (!create_texture(tex, 0, img.width, img.height, U8_3)) {
              return EXIT_FAILURE;
            }
            if (!setup_shader(&prog[0], TEX_VS, RGB_FS)) {
              return EXIT_FAILURE;
            }
            if (!bind_int1(prog[0], "rgb_tex", 0)) {
              return EXIT_FAILURE;
            }
            break;
          }
        }
        if (!create_tex_rect(vao, vbo, prog[0], img.width, img.height)) {
          return EXIT_FAILURE;
        }
        glBindFragDataLocation(prog[0], 0, "color");
        break;
      }
      default : {
        fprintf(stderr, "Unsupported output '%s'.\n",
         JPEG_DECODE_OUT_NAMES[out]);
        return EXIT_FAILURE;
      }
    }

    glUseProgram(prog[0]);

    dec = (*vtbl.decode_alloc)(&info);

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
        (*vtbl.decode_reset)(dec, &info);
        (*vtbl.decode_header)(dec, &header);
        if ((*vtbl.decode_image)(dec, &img, out) != EXIT_SUCCESS) {
         break;
        }
      }

      if (!no_gpu) {
        switch (out) {
          case JPEG_DECODE_QUANT : {
            int width;
            int height;
            width = img.plane[0].width;
            height = 0;
            for (i = 0; i < img.nplanes; i++) {
              height += img.plane[i].cstride;
            }
            switch (img.nplanes) {
              case 1 : {
                float quant[64];
                for (i = 0; i < 64; i++) {
                  quant[i] = GLJ_REAL_IDCT8X8_SCALES[i]*header.quant[0].tbl[i];
                }
                update_buffer(buf[0], 64*sizeof(float), quant);
                break;
              }
              case 3 : {
                float quant[3*64];
                for (j = 0; j < 3; j++) {
                  for (i = 0; i < 64; i++) {
                    quant[j*64 + i] =
                     GLJ_REAL_IDCT8X8_SCALES[i]*header.comp[j].quant->tbl[i];
                  }
                }
                update_buffer(buf[0], 3*64*sizeof(float), quant);
              }
            }
            /* Update the texture with DCT coefficients */
            update_texture(tex[1], 1, width*8, height, I16_1, img.coef);
            /* Perform the horizontal IDCT */
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[0]);
            glViewport(0, 0, width/8, height*8);
            glUseProgram(prog[0]);
            glBindVertexArray(vao[0]);
            glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            /* Perform the vertical IDCT */
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[1]);
            glViewport(0, 0, width, height);
            glUseProgram(prog[1]);
            glBindVertexArray(vao[1]);
            glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            /* Unpack the coefficients and display them */
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, window_width, window_height);
            glUseProgram(prog[2]);
            glBindVertexArray(vao[2]);
            glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            break;
          }
          case JPEG_DECODE_DCT : {
            int width;
            int height;
            width = img.plane[0].width;
            height = 0;
            for (i = 0; i < img.nplanes; i++) {
              height += img.plane[i].cstride;
            }
            /* Update the texture with DCT coefficients */
            update_texture(tex[0], 0, width*8, height, I16_1, img.coef);
            /* Perform the horizontal IDCT */
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[0]);
            glViewport(0, 0, width/8, height*8);
            glUseProgram(prog[0]);
            glBindVertexArray(vao[0]);
            glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            /* Perform the vertical IDCT */
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[1]);
            glViewport(0, 0, width, height);
            glUseProgram(prog[1]);
            glBindVertexArray(vao[1]);
            glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            /* Unpack the coefficients and display them */
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, window_width, window_height);
            glUseProgram(prog[2]);
            glBindVertexArray(vao[2]);
            glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            break;
          }
          case JPEG_DECODE_YUV : {
            for (i = 0; i < img.nplanes; i++) {
              image_plane *pl;
              pl = &img.plane[i];
              update_texture(tex[i], i, pl->width, pl->height, U8_1, pl->data);
            }
            glViewport(0, 0, window_width, window_height);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            break;
          }
          case JPEG_DECODE_RGB : {
            switch (img.nplanes) {
              case 1 : {
                update_texture(tex[0], 0, img.width, img.height, U8_1,
                 img.pixels);
                break;
              }
              case 3 : {
                update_texture(tex[0], 0, img.width, img.height, U8_3,
                 img.pixels);
                break;
              }
            }
            glViewport(0, 0, window_width, window_height);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            break;
          }
          default : {
            fprintf(stderr, "Unsupported output '%s'.\n",
             JPEG_DECODE_OUT_NAMES[out]);
            return EXIT_FAILURE;
          }
        }

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
