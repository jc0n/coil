/*
 * Copyright (C) 2012 John O'Connor
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __COIL_STRINGS_H
#define __COIL_STRINGS_H

#include "stdint.h"

#if !HAVE_MEMCHR
void *
memchr(const void *src, int c, size_t n);
#endif

#if !HAVE_MEMRCHR
void *
memrchr(const void *src, int c, size_t n);
#endif

#if !HAVE_MEMPCPY
void *
mempcpy(void *dst, const unsigned char *src, size_t n);
#endif

#ifdef WORDS_BIGENDIAN
#define coil_str2cmp(s, c0, c1) \
    ((s)[0] == (c0) && (s)[1] == (c1))

#define coil_str4cmp(s, c0, c1, c2, c3) \
    ((s)[0] == (c0) && (s)[1] == (c1) && (s)[2] == (c2) && (s)[3] == (c3))
#else
#define coil_str2cmp(s, c0, c1) \
    (*(uint16_t *)(s) == (((c1) << 8) | (c0)))

#define coil_str4cmp(s, c0, c1, c2, c3) \
    (*(uint32_t *)(s) == (((c3) << 24) | ((c2) << 16) | ((c1) << 8) | (c0)))
#endif

#define coil_str3cmp(s, c0, c1, c2) \
    (coil_str2cmp(s, c0, c1) && (s)[2] == (c2))

#define coil_str5cmp(s, c0, c1, c2, c3, c4) \
    (coil_str4cmp(s, c0, c1, c2, c3) && (s)[4] == (c4))

#define coil_str6cmp(s, c0, c1, c2, c3, c4, c5) \
    (coil_str4cmp(s, c0, c1, c2, c3) && coil_str2cmp(&s[4], c4, c5))

#define coil_memrchr(_r, _s, _c, _n)                                         \
G_STMT_START {                                                               \
    char c = (_c);                                                           \
    size_t n = (_n);                                                         \
    char *s = (char *)(_s);                                                  \
    if (n <= 16) {                                                           \
        while (n-- > 0) {                                                    \
            if (s[n] == c) {                                                 \
                _r = &s[n];                                                  \
                break;                                                       \
            }                                                                \
        }                                                                    \
    } else {                                                                 \
        _r = memrchr(_s, _c, _n);                                            \
    }                                                                        \
} G_STMT_END

enum {
    TRUNCATE_LEFT,
    TRUNCATE_RIGHT,
    TRUNCATE_CENTER
};

char *
strtrunc(const char *delim, gint mode, guint max, const char *str, guint len);

#endif
