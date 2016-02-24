#include <stdlib.h>
#include "../src/image.h"
#include "../src/jpeg_info.h"
#include "../src/test.h"

static void test_image_init_8bit_420(void *ctx) {
  jpeg_header header;
  image img;
  (void)ctx;
  header.bits = 8;
  header.width = 32;
  header.height = 24;
  header.ncomps = 3;
  header.comp[0].hblocks = 4;
  header.comp[0].vblocks = 4;
  header.comp[0].hsamp = 2;
  header.comp[0].vsamp = 2;
  header.comp[1].hblocks = 2;
  header.comp[1].vblocks = 2;
  header.comp[1].hsamp = 1;
  header.comp[1].vsamp = 1;
  header.comp[2].hblocks = 2;
  header.comp[2].vblocks = 2;
  header.comp[2].hsamp = 1;
  header.comp[2].vsamp = 1;
  image_init(&img, &header);
  GLJ_TEST(img.plane[0].xdec == 0);
  GLJ_TEST(img.plane[0].ydec == 0);
  GLJ_TEST(img.plane[0].xstride == 1);
  GLJ_TEST(img.plane[0].ystride == 32);
  GLJ_TEST(img.plane[1].xdec == 1);
  GLJ_TEST(img.plane[1].ydec == 1);
  GLJ_TEST(img.plane[1].xstride == 1);
  GLJ_TEST(img.plane[1].ystride == 16);
  GLJ_TEST(img.plane[2].xdec == 1);
  GLJ_TEST(img.plane[2].ydec == 1);
  GLJ_TEST(img.plane[2].xstride == 1);
  GLJ_TEST(img.plane[2].ystride == 16);
  image_clear(&img);
}

static glj_test TESTS[] = {
 { "Image Init 8-bit 4:2:0 Test", test_image_init_8bit_420, 0, 0 }
};

static glj_test_suite IMAGE_TEST_SUITE = {
  NULL,
  NULL,
  TESTS,
  sizeof(TESTS)/sizeof(*TESTS)
};

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  if (glj_test_suite_run(&IMAGE_TEST_SUITE, NULL) != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
