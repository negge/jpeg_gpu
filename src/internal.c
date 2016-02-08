#include <stdlib.h>
#include <limits.h>
#include "internal.h"

void *od_aligned_malloc(size_t _sz,size_t _align) {
  unsigned char *p;
  if (_align - 1 > UCHAR_MAX || (_align & (_align - 1))
   || _sz > ~(size_t)0 - _align) {
    return NULL;
  }
  p = (unsigned char *)malloc(_sz + _align);
  if (p != NULL) {
    int offs;
    offs = ((p - (unsigned char *)0) - 1) & (_align - 1);
    p[offs] = offs;
    p += offs + 1;
  }
  return p;
}

void od_aligned_free(void *_ptr) {
  unsigned char *p;
  p = (unsigned char *)_ptr;
  if (p != NULL) {
    int offs;
    offs = *--p;
    free(p - offs);
  }
}

int od_ilog(unsigned int _v) {
  /*On a Pentium M, this branchless version tested as the fastest on
     1,000,000,000 random 32-bit integers, edging out a similar version with
     branches, and a 256-entry LUT version.*/
  int ret;
  int m;
  ret = !!_v;
  m = !!(_v&0xFFFF0000)<<4;
  _v >>= m;
  ret |= m;
  m = !!(_v&0xFF00)<<3;
  _v >>= m;
  ret |= m;
  m = !!(_v&0xF0)<<2;
  _v >>= m;
  ret |= m;
  m = !!(_v&0xC)<<1;
  _v >>= m;
  ret |= m;
  ret += !!(_v&0x2);
  return ret;
}
