#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "../src/dct.h"
#include "../src/internal.h"
#include "../src/logging.h"
#include "../src/test.h"

/*The true forward 8-point type-II DCT basis, to 32-digit (100 bit) precision.
  The inverse is merely the transpose.*/
static const double GLJ_DCT8_BASIS[8][8] = {
  {
    0.35355339059327376220042218105242,  0.35355339059327376220042218105242,
    0.35355339059327376220042218105242,  0.35355339059327376220042218105242,
    0.35355339059327376220042218105242,  0.35355339059327376220042218105242,
    0.35355339059327376220042218105242,  0.35355339059327376220042218105242
  },
  {
     0.49039264020161522456309111806712,  0.41573480615127261853939418880895,
     0.27778511650980111237141540697427,  0.097545161008064133924142434238511,
    -0.097545161008064133924142434238511,-0.27778511650980111237141540697427,
    -0.41573480615127261853939418880895, -0.49039264020161522456309111806712
  },
  {
     0.46193976625574337806409159469839,  0.19134171618254488586422999201520,
    -0.19134171618254488586422999201520, -0.46193976625574337806409159469839,
    -0.46193976625574337806409159469839, -0.19134171618254488586422999201520,
     0.19134171618254488586422999201520,  0.46193976625574337806409159469839
  },
  {
     0.41573480615127261853939418880895, -0.097545161008064133924142434238511,
    -0.49039264020161522456309111806712, -0.27778511650980111237141540697427,
     0.27778511650980111237141540697427,  0.49039264020161522456309111806712,
     0.097545161008064133924142434238511,-0.41573480615127261853939418880895
  },
  {
     0.35355339059327376220042218105242, -0.35355339059327376220042218105242,
    -0.35355339059327376220042218105242,  0.35355339059327376220042218105242,
     0.35355339059327376220042218105242, -0.35355339059327376220042218105242,
    -0.35355339059327376220042218105242,  0.35355339059327376220042218105242
  },
  {
     0.27778511650980111237141540697427, -0.49039264020161522456309111806712,
     0.097545161008064133924142434238511, 0.41573480615127261853939418880895,
    -0.41573480615127261853939418880895, -0.097545161008064133924142434238511,
     0.49039264020161522456309111806712, -0.27778511650980111237141540697427
  },
  {
     0.19134171618254488586422999201520, -0.46193976625574337806409159469839,
     0.46193976625574337806409159469839, -0.19134171618254488586422999201520,
    -0.19134171618254488586422999201520,  0.46193976625574337806409159469839,
    -0.46193976625574337806409159469839,  0.19134171618254488586422999201520
  },
  {
     0.097545161008064133924142434238511,-0.27778511650980111237141540697427,
     0.41573480615127261853939418880895, -0.49039264020161522456309111806712,
     0.49039264020161522456309111806712, -0.41573480615127261853939418880895,
     0.27778511650980111237141540697427, -0.097545161008064133924142434238511
  }
};

# define IEEE1180_NRANGES (3)
# define IEEE1180_NBLOCKS (10000)

# define IEEE1180_TEST(cond) ((cond) ? "meets" : "FAILS")

static const int IEEE1180_L[IEEE1180_NRANGES] = { -256, -5, -300 };
static const int IEEE1180_H[IEEE1180_NRANGES] = {  255,  5,  300 };

static int ieee1180_randx;

static void ieee1180_srand(int seed) {
  ieee1180_randx = seed;
}

static int ieee1180_random(int low, int high) {
  double x;
  ieee1180_randx = ieee1180_randx*1103515245U + 12345;
  x = (ieee1180_randx&0x7ffffffe)/((double)0x7fffffff)*(high - low + 1);
  return (int)x + low;
}

static void fdct8(double *y, const double *x, int xstride) {
  int j;
  int i;
  for (j = 0; j < 8; j++) {
    y[j] = 0;
    for (i = 0; i < 8; i++) {
      y[j] += GLJ_DCT8_BASIS[j][i]*x[i*xstride];
    }
  }
}

static void idct8(double *x, int xstride, const double *y) {
  int j;
  int i;
  for (j = 0; j < 8; j++) {
    x[j*xstride] = 0;
    for (i = 0; i < 8; i++) {
      x[j*xstride] += GLJ_DCT8_BASIS[i][j]*y[i];
    }
  }
}

static void fdct8x8(double *y, int ystride, const double *x, int xstride) {
  double t[8*8];
  int i;
  for (i = 0; i < 8; i++) fdct8(t + 8*i, x + i, xstride);
  for (i = 0; i < 8; i++) fdct8(y + ystride*i, t + i, 8);
}

static void idct8x8(double *x, int xstride, const double *y, int ystride) {
  double t[8*8];
  int i;
  for (i = 0; i < 8; i++) idct8(t + i, 8, y + ystride*i);
  for (i = 0; i < 8; i++) idct8(x + i, xstride, t + 8*i);
}

static void ieee1180_block(long pme[8*8], long pmse[8*8], int ppe[8*8],
 int low, int high, int sign) {
  short img[8*8];
  double dct[8*8];
  short ref[8*8];
  short test[8*8];
  int j;
  int i;
  for (j = 0; j < 8; j++) {
    for (i = 0; i < 8; i++) {
      img[8*j + i] = ieee1180_random(low, high)*sign;
      dct[8*j + i] = img[8*j + i];
    }
  }
  fdct8x8(dct, 8, dct, 8);
  for (j = 0; j < 8; j++) {
    for (i = 0; i < 8; i++) {
      img[8*j + i] = GLJ_CLAMPI(-2048, (int)floor(dct[8*j + i] + 0.5), 2047);
      dct[8*j + i] = img[8*j + i];
    }
  }
  idct8x8(dct, 8, dct, 8);
  glj_real_idct8x8(img, 8, img, 8);
  for (j = 0; j < 8; j++) {
    for (i = 0; i < 8; i++) {
      ref[8*j + i] = GLJ_CLAMPI(-256, (int)floor(dct[8*j + i] + 0.5), 255);
      test[8*j + i] = GLJ_CLAMPI(-256, img[8*j + i], 255);
    }
  }
  for (j = 0; j < 8; j++) {
    for (i = 0; i < 8; i++) {
      int err;
      err = test[8*j + i] - ref[8*j + i];
      pme[8*j + i] += err;
      pmse[8*j + i] += err*err;
      ppe[8*j + i] = GLJ_MAXI(ppe[8*j + i], GLJ_ABSI(err));
    }
  }
}

static void ieee1180_test(long pme[8*8], long pmse[8*8], int ppe[8*8], int low,
 int high, int sign) {
  int j;
  int i;
  int m;
  double max;
  double total;
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG, "IEEE1180-1990 Test Results:"));
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG, "Input range: [%i,%i]", low, high));
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG, "Sign: %i", sign));
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG, "Iterations: %i", IEEE1180_NBLOCKS));
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG, "Peak absolute value of errors:"));
  m = 0;
  for (j = 0; j < 8; j++) {
    for (i = 0; i < 8; i++) {
      m = GLJ_MAXI(m, ppe[8*j + i]);
    }
    GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG, "%4i %4i %4i %4i %4i %4i %4i %4i",
     ppe[8*j + 0], ppe[8*j + 1], ppe[8*j + 2], ppe[8*j + 3],
     ppe[8*j + 4], ppe[8*j + 5], ppe[8*j + 6], ppe[8*j + 7]));
  }
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG,
   "Worst peak error = %i (%s spec limit 1)", m, IEEE1180_TEST(m <= 1)));
  GLJ_TEST(m <= 1);
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG, "Mean square errors:"));
  max = total = 0;
  for (j = 0; j < 8; j++) {
    double err[8];
    for (i = 0; i < 8; i++) {
      err[i] = pmse[j*8 + i]/(double)IEEE1180_NBLOCKS;
      total += err[i];
      max = GLJ_MAXF(max, err[i]);
    }
    GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG,
     "%8.4f%8.4f%8.4f%8.4f%8.4f%8.4f%8.4f%8.4f",
     err[0], err[1], err[2], err[3], err[4], err[5], err[6], err[7]));
  }
  total /= 8*8;
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG,
   "Worst mean square error = %.6f (%s spec limit 0.06)", max,
   IEEE1180_TEST(max <= 0.015)));
  GLJ_TEST(max <= 0.015);
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG,
   "Overall mean square error = %.6f (%s spec limit 0.02)", total,
   IEEE1180_TEST(max <= 0.02)));
  GLJ_TEST(max <= 0.02);
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG, "Mean errors:"));
  max = total = 0;
  for (j = 0; j < 8; j++) {
    double err[8];
    for (i = 0; i < 8; i++) {
      err[i] = pme[j*8 + i]/(double)IEEE1180_NBLOCKS;
      total += err[i];
      max = GLJ_MAXF(max, GLJ_ABSF(err[i]));
    }
    GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG,
     "%8.4f%8.4f%8.4f%8.4f%8.4f%8.4f%8.4f%8.4f",
     err[0], err[1], err[2], err[3], err[4], err[5], err[6], err[7]));
  }
  total /= 8*8;
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG,
   "Worst mean error = %.6f (%s spec limit 0.015)", max,
   IEEE1180_TEST(max <= 0.015)));
  GLJ_TEST(max <= 0.015);
  GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_DEBUG,
   "Overall mean error = %.6f (%s spec limit 0.0015)", total,
   IEEE1180_TEST(total <= 0.0015)));
  GLJ_TEST(total <= 0.0015);
}

static void test_idct8_ieee1180(void *ctx) {
  int i;
  long pme[8*8];
  long pmse[8*8];
  int ppe[8*8];
  int n;
  short dct[8*8];
  (void)ctx;
  ieee1180_srand(1);
  for (i = 0; i < IEEE1180_NRANGES; i++) {
    memset(pme, 0, sizeof(pme));
    memset(pmse, 0, sizeof(pmse));
    memset(ppe, 0, sizeof(ppe));
    for (n = 0; n < IEEE1180_NBLOCKS; n++) {
      ieee1180_block(pme, pmse, ppe, IEEE1180_L[i], IEEE1180_H[i], 1);
    }
    ieee1180_test(pme, pmse, ppe, IEEE1180_L[i], IEEE1180_H[i], 1);
  }
  ieee1180_srand(1);
  for (i = 0; i < IEEE1180_NRANGES; i++) {
    memset(pme, 0, sizeof(pme));
    memset(pmse, 0, sizeof(pmse));
    memset(ppe, 0, sizeof(ppe));
    for (n = 0; n < IEEE1180_NBLOCKS; n++) {
      ieee1180_block(pme, pmse, ppe, IEEE1180_L[i], IEEE1180_H[i], -1);
    }
    ieee1180_test(pme, pmse, ppe, IEEE1180_L[i], IEEE1180_H[i], -1);
  }
  memset(dct, 0, sizeof(dct));
  glj_real_idct8x8(dct, 8, dct, 8);
  for (n = 0, i = 0; i < 8*8; i++) n += GLJ_ABSI(dct[i]);
  GLJ_TEST(n == 0);
}

static glj_test TESTS[] = {
 { "iDCT IEEE-1180 Test", test_idct8_ieee1180, 0, 0 }
};

static glj_test_suite DCT_TEST_SUITE = {
  NULL,
  NULL,
  TESTS,
  sizeof(TESTS)/sizeof(*TESTS)
};

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  glj_log_init(NULL);
  if (glj_test_suite_run(&DCT_TEST_SUITE, NULL) != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
