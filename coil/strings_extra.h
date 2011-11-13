/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#ifndef __COIL_STRINGS_H
#define __COIL_STRINGS_H

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
#define str2cmp(s, c0, c1) \
    (s)[0] == (c0) && (s)[1] == (c1)

#define str4cmp(s, c0, c1, c2, c3) \
    (s)[0] == (c0) && (s)[1] == (c1) && (s)[2] == (c2) && (s)[3] == (c3)
#else
#define str2cmp(s, c0, c1) \
    *(uint16_t *)(s) == (((c1) << 8) | (c0))

#define str4cmp(s, c0, c1, c2, c3) \
    *(uint32_t *)(s) == (((c3) << 24) | ((c2) << 16) | ((c1) << 8) | (c0))
#endif

#define str3cmp(s, c0, c1, c2) \
    str2cmp(s, c0, c1) && (s)[2] == (c2)

#define str5cmp(s, c0, c1, c2, c3, c4) \
    str4cmp(s, c0, c1, c2, c3) && (s)[4] == (c4)

#define str6cmp(s, c0, c1, c2, c3, c4, c5) \
    str4cmp(s, c0, c1, c2, c3) && str2cmp(&s[4], c4, c5)

enum {
    TRUNCATE_LEFT,
    TRUNCATE_RIGHT,
    TRUNCATE_CENTER
};

char *
strtrunc(const char *delim, gint mode, guint max, const char *str, guint len);

#endif
