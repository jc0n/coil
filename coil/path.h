/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef __COIL_PATH_H
#define __COIL_PATH_H

typedef enum {
    COIL_STATIC_PATH         = 1 << 0,
    COIL_STATIC_KEY          = 1 << 1,
    COIL_STATIC_PATH_STRINGS = COIL_STATIC_PATH | COIL_STATIC_KEY,
    COIL_PATH_IS_ABSOLUTE    = 1 << 2,
    COIL_PATH_IS_BACKREF     = 1 << 3,
} CoilPathFlags;

typedef struct _CoilPath
{
    const gchar *str;
    const guint8 len;
    const gchar *key;
    const guint8 key_len;
    const guint hash; /* private */
    const CoilPathFlags flags; /* private */
} CoilPath;

extern CoilPath _coil_root_path;

#define CoilRootPath ((CoilPath *)(&_coil_root_path))

#define COIL_PATH_LEN 255
#define COIL_PATH_BUFLEN (COIL_PATH_LEN + 1) /* +1 for '\0' */

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
        ((p) == CoilRootPath) \

#define COIL_PATH_IS_BACKREF(p) \
        ((p)->flags & COIL_PATH_IS_BACKREF)

#define COIL_PATH_CONTAINER_LEN(p) \
  (((p)->len - (p->key_len)) - 1)

#define COIL_KEY_REGEX "-*[a-zA-Z_][\\w-]*"

#define COIL_PATH_REGEX                                       \
        "(" COIL_SPECIAL_CHAR_S "|\\.\\.+)?"                  \
        COIL_KEY_REGEX "(\\." COIL_KEY_REGEX ")*"

#define COIL_TYPE_PATH (coil_path_get_type())

G_BEGIN_DECLS

void
path_length_error(const gchar *path, guint path_len, GError **error);

GType
coil_path_get_type(void) G_GNUC_CONST;

void
coil_path_list_free(GList *list);

guint
coil_path_get_hash(CoilPath *path);

CoilPath *
coil_path_take_string_with_keyx(gchar *str, guint len, gchar *key, guint keylen,
        guint flags);

CoilPath *
coil_path_take_string_with_key(gchar *str, guint len, gchar *key, guint keylen);

CoilPath *
coil_path_take_string(gchar *str, guint len);

CoilPath *
coil_path_take_stringx(gchar *str, guint len, guint flags);


CoilPath *
coil_path_new_len(const gchar *str, guint len, GError **error);

CoilPath *
coil_path_new(const gchar *str, GError **error);

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
coil_path_join(CoilPath *container, CoilPath *key, GError **error)
    G_GNUC_WARN_UNUSED_RESULT;

CoilPath *
coil_path_concat(CoilPath *a, CoilPath *b, GError **err);

gboolean
coil_validate_path_len(const gchar *str, guint len);

gboolean
coil_validate_path(const gchar *path);

gboolean
coil_check_path(const gchar *path, guint path_len, GError **error);

CoilPath *
coil_path_resolve(CoilPath *path, CoilPath *context, GError **error)
    G_GNUC_WARN_UNUSED_RESULT;

gboolean
coil_path_resolve_inplace(CoilPath **path, CoilPath *context, GError **error);

CoilPath *
coil_path_relativize(CoilPath *path, CoilPath *base)
    G_GNUC_WARN_UNUSED_RESULT;

gboolean
coil_path_has_container(CoilPath *path, CoilPath *container, gboolean strict);

CoilPath *
coil_path_pop(CoilPath *path, int i);

gboolean
coil_path_pop_inplace(CoilPath **path, int i);

G_END_DECLS
#endif

