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
#include <limits.h>
#include "internal.h"

void *glj_aligned_malloc(size_t _sz,size_t _align) {
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

void glj_aligned_free(void *_ptr) {
  unsigned char *p;
  p = (unsigned char *)_ptr;
  if (p != NULL) {
    int offs;
    offs = *--p;
    free(p - offs);
  }
}

int glj_ilog(unsigned int _v) {
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
