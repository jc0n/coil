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
#include "common.h"

#if !HAVE_MEMCHR
void *
memchr(const void *src, unsigned char c, size_t n)
{
    const unsigned char *p;

    for (p = src; n > 0; n--, p++) {
        if (*p == c) {
            return (void *)p;
        }
    }
    return NULL;
}
#endif

#if !HAVE_MEMRCHR
void *
memrchr(const void *src, unsigned char c, size_t n)
{
    const unsigned char *p;

    for (p = src + n; n > 0; n--, p--) {
        if (*p == c) {
            return (void *)p;
        }
    }
    return NULL;
}
#endif

#if !HAVE_MEMPCPY
void *
mempcpy(void *dst, const unsigned char *src, size_t n)
{
    const unsigned char *p;

    for (p = src; n > 0; n--) {
        *dst++ = *sp++;
    }
    return (char *)dp;
}
#endif

#if !HAVE_MEMMEM
void *
memmem(void *haystack, size_t n,
       void *needle, size_t m)
{
    const unsigned char *p;

    for (p = haystack; n > 0; n--) {
        if (memcmp(p, needle, m) == 0)
            return (void *)p;
        p++;
    }
    return NULL;
}
#endif

char *
strtrunc(const char *delim, gint mode, guint max, const char *str, guint len)
{
    g_return_val_if_fail(delim, NULL);
    g_return_val_if_fail(str, NULL);
    g_return_val_if_fail(len > 0, NULL);
    g_return_val_if_fail(max > 0, NULL);

    char *p, *new;
    guint ndelim, m;

    if (len <= max) {
        return g_strndup(str, len);
    }
    ndelim = strlen(delim);
    if (ndelim >= max) {
        return g_strndup(delim, max);
    }
    new = g_new(gchar, max + 1);
    /* m is the number of chars from str we intend to write */
    m = max - ndelim;
    if (mode == TRUNCATE_LEFT) {
        p = mempcpy(new, delim, ndelim);
        p = mempcpy(p, &str[len - m], m);
    }
    else if (mode == TRUNCATE_RIGHT) {
        p = mempcpy(new, str, m);
        p = mempcpy(p, delim, ndelim);
    }
    else if (mode == TRUNCATE_CENTER) {
        guint n = m / 2;
        /* if m is odd, write one more char on front */
        p = mempcpy(new, str, n + (m % 2));
        p = mempcpy(p, delim, ndelim);
        p = mempcpy(p, &str[len - n], n);
    }
    else {
        g_error("Unknown truncate mode.");
    }
    *p = '\0';
    return new;
}

