/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
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
