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

#endif
