/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef __COIL_PATH_H
#define __COIL_PATH_H

#include "stdarg.h"
#include "stdint.h"

typedef enum {
    COIL_STATIC_PATH         = 1 << 0,
    COIL_STATIC_KEY          = 1 << 1,
    COIL_STATIC_PATH_STRINGS = COIL_STATIC_PATH | COIL_STATIC_KEY,
    COIL_PATH_IS_ABSOLUTE    = 1 << 2,
    COIL_PATH_IS_ROOT        = 1 << 3,
    COIL_PATH_IS_BACKREF     = 1 << 4,
} CoilPathFlags;

typedef struct _CoilPath
{
    gchar *str;
    guint8 len;
    gchar *key;
    guint8 key_len;
    guint hash; /* reserved */
    CoilPathFlags flags;
    volatile gint ref_count;
} CoilPath;

extern CoilPath _coil_root_path;

#define CoilRootPath ((CoilPath *)(&_coil_root_path))

#define COIL_PATH_LEN 255
#define COIL_PATH_BUFLEN (COIL_PATH_LEN + 1) /* +1 for '\0' */

#define COIL_PATH_MAX_PARTS \
  ((gint)((COIL_PATH_LEN - COIL_ROOT_PATH_LEN) / 2))

#define COIL_SPECIAL_CHAR '@'
#define COIL_SPECIAL_CHAR_S "@"

#define COIL_ROOT_PATH \
        COIL_SPECIAL_CHAR_S "root"

#define COIL_ROOT_PATH_LEN \
        (sizeof(COIL_ROOT_PATH)-1)

#define COIL_PATH_DELIM '.'
#define COIL_PATH_DELIM_S "."

#define COIL_PATH_IS_RELATIVE(p) \
        (!((p)->flags & COIL_PATH_IS_ABSOLUTE))

#define COIL_PATH_IS_ABSOLUTE(p) \
        ((p)->flags & COIL_PATH_IS_ABSOLUTE)

#define COIL_PATH_IS_ROOT(p) \
        (((p) == CoilRootPath) \
         || (p)->flags & COIL_PATH_IS_ROOT)

#define COIL_PATH_IS_BACKREF(p) \
        ((p)->flags & COIL_PATH_IS_BACKREF)

#define COIL_PATH_CONTAINER_LEN(p) \
  (((p)->len - (p->key_len)) - 1)

#define COIL_KEY_REGEX "-*[a-zA-Z_][\\w-]*"

#define COIL_PATH_REGEX                                       \
        "(" COIL_SPECIAL_CHAR_S "|\\.\\.+)?"                  \
        COIL_KEY_REGEX "(\\." COIL_KEY_REGEX ")*"

#define COIL_TYPE_PATH (coil_path_get_type())

#define COIL_PATH_QUICK_BUFFER(buf, blen, ctr, clen, key, klen) \
    G_STMT_START \
{ \
    g_assert(sizeof(blen) == sizeof(guint8)); \
    g_assert(sizeof(clen) == sizeof(guint8)); \
    g_assert(sizeof(klen) == sizeof(guint8)); \
    g_assert(((guint32)(klen + clen + 1)) <= COIL_PATH_LEN); \
    register gchar *__p; \
    if (*key == COIL_PATH_DELIM) { \
        key++; \
        klen--; \
        g_assert(*key != COIL_PATH_DELIM); \
        g_assert(klen > 0); \
    } \
    blen = clen + klen + 1; \
    buf = g_alloca(blen + 1); \
    __p = mempcpy(buf, ctr, clen); \
    *__p++ = COIL_PATH_DELIM; \
    __p = mempcpy(__p, key, klen); \
    *__p = 0; \
} \
G_STMT_END\



G_BEGIN_DECLS

void
path_length_error(const gchar *path, guint path_len, GError **error);

GType
coil_path_get_type(void) G_GNUC_CONST;

void
coil_path_list_free(GList *list);

CoilPath *
coil_path_take_strings(gchar *path, guint8 path_len,
                       gchar *key, guint8 key_len,
                       CoilPathFlags flags);

CoilPath *
coil_path_new_len(const gchar *str, guint len, GError **error);

CoilPath *
coil_path_new(const gchar *str, GError **error);

CoilPath *
coil_path_copy(CoilPath *p);

gboolean
coil_path_equal(CoilPath *a, CoilPath *b);

gint
coil_path_compare(CoilPath *a, CoilPath *b);

void
coil_path_free(CoilPath *p);

CoilPath *
coil_path_ref(CoilPath *p);

void
coil_path_unref(CoilPath *p);

CoilPath *
coil_path_concat(CoilPath *container, CoilPath *key, GError **error)
    G_GNUC_WARN_UNUSED_RESULT;

CoilPath *
coil_build_path_valist(GError **error, const gchar *first_key, va_list args);

CoilPath *
coil_build_path(GError **error, const gchar *first_key, ...)
    G_GNUC_NULL_TERMINATED;

gboolean
coil_validate_path_len(const gchar *str, guint len);

gboolean
coil_validate_path(const gchar *path);

gboolean
coil_validate_key_len(const gchar *key, guint key_len);

gboolean
coil_validate_key(const gchar *key);

gboolean
coil_check_key(const gchar *key, guint key_len, GError **error);

gboolean
coil_check_path(const gchar *path, guint path_len, GError **error);

/* XXX: remove, paths will be immutable */
gboolean
coil_path_change_container(CoilPath **path_ptr, CoilPath *container,
        GError **error);

CoilPath *
coil_path_resolve(CoilPath *path, CoilPath *context, GError **error)
    G_GNUC_WARN_UNUSED_RESULT;

gboolean
coil_path_resolve_into(CoilPath **path, CoilPath *context, GError **error);

CoilPath *
coil_path_relativize(CoilPath *path, CoilPath *base)
    G_GNUC_WARN_UNUSED_RESULT;

gboolean
coil_path_has_container(CoilPath *path, CoilPath *container, gboolean strict);

CoilPath *
coil_path_pop(CoilPath *path);

G_END_DECLS
#endif

