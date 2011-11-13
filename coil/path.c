/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include <string.h>

#include "strings_extra.h"
#include "error.h"
#include "path.h"

CoilPath _coil_root_path = {
    (gchar *)COIL_ROOT_PATH,
    (guint8)COIL_ROOT_PATH_LEN,
    (gchar *)NULL,
    (guint8)0,
    1,
    (COIL_STATIC_PATH | COIL_PATH_IS_ABSOLUTE),
    1,
};

/* TODO: proper error handling */

/* IMPLEMENTATION NOTES:
 *
 * TODO: Discuss reference counting
 * TODO: NUL-terminate all strings
 */

#define HASH_BYTE(hash, byte) hash = (hash * 33 + (byte))

static inline guint
hash_bytes(guint hash, const guchar *byte, guint n)
{
    g_return_val_if_fail(byte, 0);
    g_return_val_if_fail(*byte, 0);
    g_return_val_if_fail(n > 0, 0);

    do {
        HASH_BYTE(hash, *byte++);
    } while (--n > 0);

    return hash;
}

static inline guint
hash_absolute(const gchar *path, guint8 path_len)
{
    g_return_val_if_fail(path, 0);
    g_return_val_if_fail(*path == '@', 0); /* must be absolute */
    g_return_val_if_fail(path_len > 0, 0);

    path += COIL_ROOT_PATH_LEN;
    path_len -= COIL_ROOT_PATH_LEN;

    if (path_len > 0) {
        return hash_bytes(1, (guchar *)path, path_len);
    }
    return 0;
}

static inline guint
hash_relative(guint container_hash, const gchar *path, guint8 path_len)
{
    g_return_val_if_fail(path, 0);
    g_return_val_if_fail(*path, 0);
    g_return_val_if_fail(*path != '@', 0);
    g_return_val_if_fail(path_len > 0, 0);

    if (path[0] != COIL_PATH_DELIM) {
        HASH_BYTE(container_hash, COIL_PATH_DELIM);
    }
    return hash_bytes(container_hash, (guchar *)path, path_len);
}

guint
coil_path_get_hash(CoilPath *path)
{
    g_return_val_if_fail(path, 0);

    if (path->hash == 0) {
        path->hash = hash_absolute(path->str, path->len);
    }
    return path->hash;
}

static inline gboolean
compare_hash(CoilPath *a, CoilPath *b)
{
    /* check if both are absolute */
    if (!(a->flags & b->flags & COIL_PATH_IS_ABSOLUTE)) {
        /* a or b is a relative path and comparing them is meaningless */
        return FALSE;
    }
    if (a->hash == 0) {
        a->hash = hash_absolute(a->str, a->len);
    }
    if (b->hash == 0) {
        b->hash = hash_absolute(b->str, b->len);
    }
    return a->hash == b->hash;
}

static CoilPath *
path_alloc(void)
{
    CoilPath *path = g_slice_new0(CoilPath);
    path->ref_count = 1;
    return path;
}

static void
pathval_to_strval(const GValue *src, GValue *dst)
{
    g_return_if_fail(G_IS_VALUE(src));
    g_return_if_fail(G_IS_VALUE(dst));

    gchar *str;
    CoilPath *path = (CoilPath *)g_value_get_boxed(src);

    str = g_strndup(path->str, path->len);
    g_value_take_string(dst, str);
}

GType
coil_path_get_type(void)
{
    static GType type_id = 0;

    if (G_UNLIKELY(!type_id)) {
        type_id = g_boxed_type_register_static(g_intern_static_string("CoilPath"),
                (GBoxedCopyFunc)coil_path_ref,
                (GBoxedFreeFunc)coil_path_unref);

        g_value_register_transform_func(type_id, G_TYPE_STRING, pathval_to_strval);
    }
    return type_id;
}

void
coil_path_list_free(GList *list)
{
    while (list) {
        coil_path_unref(list->data);
        list = g_list_delete_link(list, list);
    }
}

void
path_length_error(const gchar *path, guint path_len, GError **error)
{
    g_return_if_fail(path && *path);
    g_return_if_fail(error == NULL || *error == NULL);

    gchar prefix[16], suffix[16];

    memcpy(prefix, path, 15);
    prefix[15] = '\0';

    memcpy(suffix, path + path_len - 15, 15);
    suffix[15] = '\0';

    g_set_error(error, COIL_ERROR, COIL_ERROR_PATH,
            "Length of path %s...%s too long (%d). Max path length is %d",
            prefix, suffix, path_len, COIL_PATH_LEN);
}

void
key_length_error(const gchar *key, guint key_len, GError **error)
{
    g_return_if_fail(key && *key);
    g_return_if_fail(error == NULL || *error == NULL);

    gchar prefix[16], suffix[16];

    memcpy(prefix, key, 15);
    prefix[15] = '\0';

    memcpy(suffix, &key[key_len] - 15, 15);
    suffix[15] = '\0';

    g_set_error(error, COIL_ERROR, COIL_ERROR_KEY,
            "Length of key %s...%s too long (%d). Max key length is %d",
            prefix, suffix, key_len,
            (gint)(COIL_PATH_LEN - COIL_ROOT_PATH_LEN - 1));
}

static const char valid_path_chars[128] = {
    /* 0x0-0x13  */0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x14-0x27 */0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x28-0x2C */0,0,0,0,0,

    /* [-.] */
    /* 0x2D-0x2E */1,1,

    /* 0x2F      */0,

    /* [0-9] */
    /* 0x30-0x39 */1,1,1,1,1,1,1,1,1,

    /* 0x3A-0x3F */0,0,0,0,0,0,

    /* [@A-Z] */
    /* 0x40-0x5A */1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,

    /* 0x5B-0x5E */0,0,0,0,

    /* [_]       */1,
    /* 0x60      */0,
    /* [a-z]     */1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    /* 0x7B      */0,0,0,0,0
};

/*
 * coil_path_take_string_with_keyx:
 * @str: a nul-terminated path string
 * @len: the length of @str
 * @key: a nul-terminated key string
 * @keylen: the length of @key
 * @flags: any of #CoilPathFlags
 * @Returns: a new #CoilPath object
 *
 * Creates a new #CoilPath object
 */
COIL_API(CoilPath *)
coil_path_take_string_with_keyx(gchar *str, guint len, gchar *key, guint keylen,
        guint flags)
{
    g_return_val_if_fail(str != NULL, NULL);
    g_return_val_if_fail(len > 0, NULL);

    CoilPath *path;

    if (len == COIL_ROOT_PATH_LEN && keylen == 0 &&
            memcmp(str, COIL_ROOT_PATH, COIL_ROOT_PATH_LEN) == 0) {
        if (!(flags & COIL_STATIC_PATH)) {
            g_free(str);
        }
        if (!(flags & COIL_STATIC_KEY)) {
            g_free(key);
        }
        coil_path_ref(CoilRootPath);
        return CoilRootPath;
    }
    if (str[0] == '@') {
        if (!(flags & COIL_PATH_IS_ABSOLUTE) &&
                memcmp(str, COIL_ROOT_PATH, COIL_ROOT_PATH_LEN) == 0) {
            flags |= COIL_PATH_IS_ABSOLUTE;
        }
    }
    else if (!(flags & COIL_PATH_IS_BACKREF) && str[0] == '.') {
        if (str[1] == '.') {
            flags |= COIL_PATH_IS_BACKREF;
        }
        else {
            /* path string begins with . */
            memmove(str, str + 1, len - 1);
        }
    }
    if (key == NULL) {
        key = memrchr(str, '.', len);
        if (key) {
            key++;
            keylen = len - (key - str);
            flags |= COIL_STATIC_KEY;
        }
    }

    path = path_alloc();
    path->str = str;
    path->len = len;
    path->key = key;
    path->key_len = keylen;
    path->hash = 0;
    path->flags = flags;

    return path;
}

/* coil_path_take_string_with_key:
 * @str: a nul-terminated path string
 * @len: the length of @str
 * @key: a nul-terminated key string
 * @keylen: the length of @key
 *
 * Creates a new #CoilPath object
 */
COIL_API(CoilPath *)
coil_path_take_string_with_key(gchar *str, guint len, gchar *key, guint keylen)
{
    return coil_path_take_string_with_keyx(str, len, key, keylen, 0);
}

/*
 * coil_path_take_string:
 * @str: a nul-terminated path string
 * @len: the length of @str
 * @Returns: a new #CoilPath object
 *
 * Creates a new #CoilPath object
 */
COIL_API(CoilPath *)
coil_path_take_string(gchar *str, guint len)
{
    return coil_path_take_string_with_keyx(str, len, NULL, 0, 0);
}

/* coil_path_take_stringx:
 * @str: a nul-terminated path string
 * @len: the length of @str
 * @flags: flags from #CoilPathFlags
 * @Returns: a new #CoilPath object
 *
 * Creates a new #CoilPath object
 */
COIL_API(CoilPath *)
coil_path_take_stringx(gchar *str, guint len, guint flags)
{
    return coil_path_take_string_with_keyx(str, len, NULL, 0, flags);
}

/* coil_path_new_with_key:
 * @str: a nul-terminated path string
 * @len: the length of @str
 * @key: a nul-terminated key string
 * @keylen: the length of @key
 * @Returns: a new #CoilPath object
 *
 * Returns a new #CoilPath object
 */
COIL_API(CoilPath *)
coil_path_new_with_key(const gchar *str, guint len,
        const gchar *key, guint keylen, GError **error)
{
    g_return_val_if_fail(str, NULL);
    g_return_val_if_fail(len > 0, NULL);

    if (len == COIL_ROOT_PATH_LEN && keylen == 0 &&
            memcmp(str, COIL_ROOT_PATH, COIL_ROOT_PATH_LEN) == 0) {
        coil_path_ref(CoilRootPath);
        return CoilRootPath;
    }
    if (!coil_check_path(str, len, error)) {
        return NULL;
    }
    if (key) {
        if (!coil_check_key(key, keylen, error)) {
            return NULL;
        }
        key = g_strndup(key, keylen);
    }
    if (str[0] == COIL_PATH_DELIM && str[1] != COIL_PATH_DELIM) {
        /** ignore single leading . for relative path */
        str = g_strndup(str + 1, len - 1);
    }
    else {
        str = g_strndup(str, len);
    }
    return coil_path_take_string_with_key((char *)str, len, (char *)key, keylen);
}

/*
 * coil_path_new_len:
 * @str: a nul-terminated path string
 * @len: the length of @str
 * @Returns: a new #CoilPath object
 *
 * Creates a new #CoilPath object
 */
COIL_API(CoilPath *)
coil_path_new_len(const gchar *str, guint len, GError **error)
{
    g_return_val_if_fail(str, NULL);
    g_return_val_if_fail(len > 0, NULL);

    return coil_path_new_with_key(str, len, NULL, 0, error);
}

/*
 * coil_path_new:
 * @str: a nul-terminated path string
 * @Returns: a new #CoilPath object
 *
 * Creates a new #CoilPath object
 */
COIL_API(CoilPath *)
coil_path_new(const gchar *str, GError **error)
{
    g_return_val_if_fail(str, NULL);

    return coil_path_new_with_key(str, strlen(str), NULL, 0, error);
}

/*
 * coil_path_equal:
 * @a: a #CoilPath object
 * @b: a #CoilPath object
 * @Returns: %TRUE if @a and @b are equal
 *
 * Compares two paths @a and @b for equality.
 */
COIL_API(gboolean)
coil_path_equal(CoilPath *a, CoilPath *b)
{
    g_return_val_if_fail(a, FALSE);
    g_return_val_if_fail(b, FALSE);

    if (!compare_hash(a, b)) {
        return FALSE;
    }
    if (a == b || a->str == b->str) {
        return TRUE;
    }
    if (a->len != b->len || a->key_len != b->key_len) {
        return FALSE;
    }
    /* check if one path is relative and the other is absolute */
    if ((a->flags ^ b->flags) & COIL_PATH_IS_ABSOLUTE) {
        return FALSE;
    }
    /* check if both are absolute */
    if ((a->flags & b->flags) & COIL_PATH_IS_ABSOLUTE) {
        return memcmp(a->str + COIL_ROOT_PATH_LEN,
                b->str + COIL_ROOT_PATH_LEN,
                b->len - COIL_ROOT_PATH_LEN) == 0;
    }
    return memcmp(a->str, b->str, b->len) == 0;
}

/*
 * coil_path_compare:
 * @a: a #CoilPath object
 * @b: a #CoilPath object
 * @Returns: 0 if @a and @b are equal, 1 if @a is greater and -1 otherwise
 *
 * Compares two paths @a and @b.
 */
COIL_API(gint)
coil_path_compare(CoilPath *a, CoilPath *b)
{
    g_return_val_if_fail(a, -1);
    g_return_val_if_fail(b, -1);

    if (a == b || a->str == b->str) {
        return 0;
    }
    if (a->len != b->len) {
        return (a->len > b->len) ? 1 : -1;
    }
    if (a->key_len != b->key_len) {
        return (a->key_len > b->key_len) ? 1 : -1;
    }
    if ((a->flags & b->flags) & COIL_PATH_IS_ABSOLUTE) {
        return memcmp(a->str + COIL_ROOT_PATH_LEN,
                b->str + COIL_ROOT_PATH_LEN,
                b->len - COIL_ROOT_PATH_LEN);
    }
    return memcmp(a->str, b->str, b->len);
}

/*
 * coil_path_free:
 * @path: a #CoilPath object
 *
 * Frees all memory associated with @path
 */
COIL_API(void)
coil_path_free(CoilPath *path)
{
    g_return_if_fail(path);

    CoilPathFlags iflags = ~path->flags;

    if (iflags & COIL_STATIC_KEY) {
        g_free(path->key);
    }
    if (iflags & COIL_STATIC_PATH) {
        g_free(path->str);
    }
    /* try to be nice if others still have a reference */
    if (g_atomic_int_get(&path->ref_count) > 1) {
        memset(path, 0, sizeof(*path));
        return;
    }
    g_slice_free(CoilPath, path);
}

/*
 * coil_path_ref:
 * @path: a #CoilPath object
 * @Returns: a new reference to @path
 *
 * Increments the reference count of @path
 */
COIL_API(CoilPath *)
coil_path_ref(CoilPath *path)
{
    g_return_val_if_fail(path, NULL);

    g_atomic_int_inc(&path->ref_count);
    return path;
}

/* coil_path_unref:
 * @path: a #CoilPath object
 *
 * Decrements the reference count of @path
 */
COIL_API(void)
coil_path_unref(CoilPath *path)
{
    g_return_if_fail(path);

    if (g_atomic_int_dec_and_test(&path->ref_count)) {
        coil_path_free(path);
    }
}

/* coil_path_join:
 * @a: a #CoilPath object
 * @b: a #CoilPath object
 *
 * Returns a new #CoilPath using the container from @a and
 * the key from @b.
 */
COIL_API(CoilPath *)
coil_path_join(CoilPath *a, CoilPath *b, GError **error)
{
    g_return_val_if_fail(a, NULL);
    g_return_val_if_fail(b, NULL);

    guint len = a->len - b->key_len - 1;

    return coil_path_new_with_key(a->str, len, b->key, b->key_len, error);
}

/*
 * coil_path_concat:
 * @a: a #CoilPath object
 * @b: a #CoilPath object
 * @Returns: a new #CoilPath object
 *
 * Returns a new #CoilPath object with @a and @b concatenated.
 */
COIL_API(CoilPath *)
coil_path_concat(CoilPath *a, CoilPath *b, GError **error)
{
    g_return_val_if_fail(a, NULL);
    g_return_val_if_fail(b, NULL);
    g_return_val_if_fail(COIL_PATH_IS_RELATIVE(b), NULL);

    gchar *str, *p;
    guint len;
    CoilPath *res;

    len = a->len + b->len + 1;
    if (len > COIL_PATH_LEN) {
        p = g_strjoin(".", a->str, b->str, NULL);
        path_length_error(p, len, error);
        g_free(p);
        return NULL;
    }

    str = g_new(gchar, len + 1);
    p = mempcpy(str, a->str, a->len);
    *p++ = COIL_PATH_DELIM;
    p = mempcpy(p, b->str, b->len);
    *p = '\0';

    res = coil_path_take_string(str, len);
/*    if (COIL_PATH_IS_ABSOLUTE(a) && a->hash > 0) {
        res->hash = hash_relative(a->hash, b->str, b->len);
    }
*/
    return res;
}

/*
 * coil_validate_path_len:
 * @str: a nul-terminated path string
 * @len: the length of @str
 * @Returns: %TRUE if @str is a valid path string
 *
 * Returns %TRUE if @str is a valid path string
 */
COIL_API(gboolean)
coil_validate_path_len(const gchar *str, guint len)
{
    static GRegex *path_regex = NULL;

    g_return_val_if_fail(str != NULL, FALSE);

    if (G_UNLIKELY(!path_regex)) {
        path_regex = g_regex_new("^"COIL_PATH_REGEX"$",
                G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);
    }
    return g_regex_match_full(path_regex, str, len, 0, 0, NULL, NULL);
}

/*
 * coil_validate_path:
 * @str: a nul-terminated path string
 * @Returns: %TRUE if @str is a valid path string
 *
 * Returns %TRUE if @str is a valid path string
 */
COIL_API(gboolean)
coil_validate_path(const gchar *str)
{
    return coil_validate_path_len(str, strlen(str));
}

/*
 * coil_validate_key_len:
 * @str: a nul-termianted key string
 * @len: the length of @str
 * @Returns: %TRUE if @str is a valid key
 *
 * Returns %TRUE if @str is a valid key
 */
COIL_API(gboolean)
coil_validate_key_len(const gchar *str, guint len)
{
    static GRegex *key_regex = NULL;

    g_return_val_if_fail(str, FALSE);

    if (G_UNLIKELY(!key_regex)) {
        key_regex = g_regex_new("^"COIL_KEY_REGEX"$",
                G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);
    }
    return g_regex_match_full(key_regex, str, len, 0, 0, NULL, NULL);
}

/*
 * coil_validate_key:
 * @str: a nul-terminated key string
 * @Returns: %TRUE if @str is a valid key
 *
 * Returns %TRUE if @str is a valid key
 */
COIL_API(gboolean)
coil_validate_key(const gchar *str)
{
    return coil_validate_key_len(str, strlen(str));
}

/*
 * coil_check_key:
 * @str: a nul-terminated key string
 * @len: the length of @str
 * @Returns: %FALSE with an error set if @str is an invalid key
 *
 * Returns %FALSE with an error set if @str is an invalid key.
 */
COIL_API(gboolean)
coil_check_key(const gchar *str, guint len, GError **error)
{
    g_return_val_if_fail(str, FALSE);
    g_return_val_if_fail(len, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (len > (COIL_PATH_LEN - COIL_ROOT_PATH_LEN - 1)) {
        key_length_error(str, len, error);
        return FALSE;
    }
    if (!coil_validate_key_len(str, len)) {
        g_set_error(error, COIL_ERROR, COIL_ERROR_KEY,
                "The key '%.*s' is invalid or contains invalid characters",
                len, str);
        return FALSE;
    }
    return TRUE;
}

/*
 * coil_check_path:
 * @str: a nul-terminated path string
 * @len: the length of @str
 * @Returns: %FALSE with an error set if @str is an invalid path.
 *
 * Returns %FALSE with an error set if @str is an invalid path.
 */
COIL_API(gboolean)
coil_check_path(const gchar *str, guint len, GError **error)
{
    g_return_val_if_fail(str, FALSE);
    g_return_val_if_fail(len, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (len > COIL_PATH_LEN) {
        path_length_error(str, len, error);
        return FALSE;
    }
    if (!coil_validate_path_len(str, len)) {
        g_set_error(error, COIL_ERROR, COIL_ERROR_PATH,
                "The str '%.*s' is invalid or contains invalid characters.",
                len, str);
        return FALSE;
    }
    return TRUE;
}

/* coil_path_resolve:
 * @path: a #CoilPath object
 * @container: a #CoilPath object
 *
 * Returns a new #CoilPath object using @container as the starting point to
 * resolve relative path @path.
 */
COIL_API(CoilPath *)
coil_path_resolve(CoilPath *path, CoilPath *container, GError **error)
{
    g_return_val_if_fail(path, NULL);
    g_return_val_if_fail(container, NULL);
    g_return_val_if_fail(path != container, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    gchar *suffix, *end, *str, *p;
    guint container_len, suffix_len, path_len;

    if (COIL_PATH_IS_RELATIVE(container)) {
        g_error("Error resolving path '%s', "
                "Prefix path argument '%s', must be an absolute path.",
                path->str, container->str);
    }
    if (COIL_PATH_IS_ABSOLUTE(path)) {
        return coil_path_ref(path);
    }
    if (!COIL_PATH_IS_BACKREF(path)) {
        return coil_path_concat(container, path, error);
    }
    suffix = path->str;
    end = container->str + container->len;
    while (*++suffix == COIL_PATH_DELIM) {
        while (*--end != COIL_PATH_DELIM) {
            if (G_UNLIKELY(end < container->str)) {
                coil_path_error(error, path,
                        "reference passed root while resolving "
                        "against '%s'.", container->str);
                return NULL;
            }
        }
    }
    container_len = end - container->str;
    suffix_len = path->len - (suffix - path->str);
    path_len = (suffix_len) ? container_len + suffix_len + 1 : container_len;

    if (G_UNLIKELY(path_len > COIL_PATH_LEN)) {
        gchar *p = g_strjoin(COIL_PATH_DELIM_S, container->str, suffix, NULL);
        path_length_error(p, path_len, error);
        g_free(p);
        return NULL;
    }
    g_assert(path_len > 0);
    if (path_len == COIL_ROOT_PATH_LEN && suffix_len == 0 &&
            memcmp(container->str, COIL_ROOT_PATH, COIL_ROOT_PATH_LEN) == 0) {
        return coil_path_ref(CoilRootPath);
    }

    str = g_new(gchar, path_len + 1);
    p = mempcpy(str, container->str, container_len);
    *p++ = COIL_PATH_DELIM;
    p = mempcpy(p, suffix, suffix_len);
    *p = '\0';

    return coil_path_take_string_with_keyx(str, path_len,
            &str[path_len - path->key_len], path->key_len,
            COIL_STATIC_KEY | COIL_PATH_IS_ABSOLUTE);
}

/*
 * coil_path_resolve_inplace:
 * @path: a pointer to a #CoilPath object
 * @container: a #CoilPath object
 *
 * Resolves a @path inplace using @container as the starting point.
 * See: #coil_path_resolve
 */
COIL_API(gboolean)
coil_path_resolve_inplace(CoilPath **path, CoilPath *container, GError **error)
{
    g_return_val_if_fail(path || *path, FALSE);
    g_return_val_if_fail(container, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilPath *abspath;

    abspath = coil_path_resolve(*path, container, error);
    if (abspath == NULL) {
        return FALSE;
    }
    coil_path_unref(*path);
    *path = abspath;
    return TRUE;
}

/*
 * coil_path_relativize:
 * @target: an #CoilPath object that contains an absolute path
 * @container: a #CoilPath object that contains an absolute path
 *
 * Returns a new relative path to @target starting from @container
 */
COIL_API(CoilPath *)
coil_path_relativize(CoilPath *target, CoilPath *container)
{
    g_return_val_if_fail(target, NULL);
    g_return_val_if_fail(container, NULL);
    g_return_val_if_fail(!COIL_PATH_IS_ROOT(target), NULL);
    g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(container), NULL);

    CoilPath *relative;
    guint8 prefix_len, tail_len, num_dots = 2;
    const gchar *delim, *prefix, *path;

    if (COIL_PATH_IS_RELATIVE(target) || COIL_PATH_IS_ROOT(target)) {
        return coil_path_ref(target);
    }
    relative = path_alloc();
    /* both paths are absolute, start checking after @root */
    prefix = delim = container->str + COIL_ROOT_PATH_LEN;
    path = target->str + COIL_ROOT_PATH_LEN;

    if (target == container) {
        tail_len = target->key_len;
        path = target->key;
        goto backref;
    }
    /* find the first differing character
     * which marks the end of the prefix and
     * possibly the end of the path */
    while (*prefix != '\0' && *prefix == *path) {
        /* keep track of the lask delim in prefix */
        if (*prefix == COIL_PATH_DELIM)
            delim = prefix;

        prefix++;
        path++;
    }
    /* the only case where there are no dots is
     * if <prefix> is a prefix of path */
    if (*prefix == '\0' && *path == COIL_PATH_DELIM) {
        /* just move the keys to the front
         * no need to mess with the allocation */
        prefix_len = ++path - target->str;
        relative->len = target->len - prefix_len;
        relative->str = g_strndup(path, relative->len);
    }
    else {
        prefix_len = delim - container->str;
        tail_len = target->len - prefix_len - 1;
        path = target->str + prefix_len + 1;

        /* count # of parts to remove from prefix path to get to
         * path ie. number of dots to add to relative path */
        while ((delim = strchr(delim + 1, COIL_PATH_DELIM)))
            num_dots++;

backref:
        relative->len = tail_len + num_dots;
        relative->str = g_new(gchar, relative->len + 1);

        memset(relative->str, COIL_PATH_DELIM, num_dots);
        memcpy(relative->str + num_dots, path, tail_len);
    }

    relative->key = &relative->str[relative->len - target->key_len];
    relative->key_len = target->key_len;
    relative->str[relative->len] = 0;
    relative->flags = COIL_STATIC_KEY;
    return relative;
}

/* XXX: maybe remove */
COIL_API(gboolean)
coil_path_has_container(CoilPath *path, CoilPath *container,
                        gboolean strict)
{
    g_return_val_if_fail(path, FALSE);
    g_return_val_if_fail(container, FALSE);

    guint key_len;

    if (container->len >= path->len ||
        path->str[container->len] != COIL_PATH_DELIM) {
        return FALSE;
    }
    if (!strict) {
        return g_str_has_prefix(path->str, container->str);
    }
    key_len = path->len - container->len - 1;

    return key_len == path->key_len &&
        memcmp(path->str, container->str, container->len) == 0;
}

/*
 * coil_path_pop:
 * @path: a #CoilPath object
 * @i: the number of keys to pop. can be negative.
 * @Returns: a new #CoilPath object
 *
 * Returns a new #CoilPath object with @i keys from the beginning of @path
 * if @i is positive. Otherwise returns a new #CoilPath with @i keys
 * removed from the end of @path.
 */
COIL_API(CoilPath *)
coil_path_pop(CoilPath *path, int i)
{
    g_return_val_if_fail(path != NULL, NULL);

    char *p;

    if (i >= 0) {
        if (COIL_PATH_IS_ABSOLUTE(path) && i == 0) {
            return coil_path_ref(CoilRootPath);
        }
        do {
            p = memchr(path->str, COIL_PATH_DELIM, path->len);
            if (p == NULL) {
                /* TODO error */
                return NULL;
            }
        } while (--i > 0);
    }
    else {
        if (i == -1) {
            if (COIL_PATH_IS_RELATIVE(path)) {
                return coil_path_new_len(path->key, path->key_len, NULL);
            }
            else {
                return coil_path_new_len(path->str,
                        path->len - path->key_len - 1, NULL);
            }
        }
        while (i++ < 0) {
            p = memrchr(path->str, COIL_PATH_DELIM, path->len);
            if (p == NULL) {
                /* TODO error */
                return NULL;
            }
        }
    }

    return coil_path_new_len(path->str, p - path->str, NULL);
}

COIL_API(gboolean)
coil_path_pop_inplace(CoilPath **path, int i)
{
    g_return_val_if_fail(path != NULL && *path != NULL, FALSE);

    CoilPath *res = coil_path_pop(*path, i);
    if (res == NULL) {
        return FALSE;
    }
    coil_path_unref(*path);
    *path = res;
    return TRUE;
}
