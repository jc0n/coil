/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include "strings_extra.h"

#if !HAVE_MEMCHR
void *
memchr(const void *src, unsigned char c, size_t n)
{
  const unsigned char *p;

  for (p = src; n > 0; n--, p++)
    if (*p == c)
      return (void *)p;

  return NULL;
}
#endif

#if !HAVE_MEMRCHR
void *
memrchr(const void *src, unsigned char c, size_t n)
{
  const unsigned char *p;

  for (p = src + n; n > 0; n--, p--)
    if (*p == c)
      return (void *)p;

  return NULL;
}
#endif

#if !HAVE_MEMPCPY
void *
mempcpy(void *dst, const unsigned char *src, size_t n)
{
  const unsigned char *p;

  for (p = src; n > 0; n--)
    *dst++ = *sp++;

  return (char *)dp;
}
#endif
