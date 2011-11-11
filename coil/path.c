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
    0,
    (COIL_STATIC_PATH |
     COIL_PATH_IS_ROOT |
     COIL_PATH_IS_ABSOLUTE),
    1,
};

/* TODO: proper error handling */

static CoilPath *
path_alloc(void)
{
    CoilPath *path = g_new0(CoilPath, 1);
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
                (GBoxedCopyFunc)coil_path_copy,
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

COIL_API(CoilPath *)
coil_path_take_strings(gchar *path, guint8 path_len, gchar *key, guint8 key_len,
                       CoilPathFlags flags)

{
    g_return_val_if_fail(path, NULL);
    g_return_val_if_fail(*path, NULL);
    g_return_val_if_fail(path_len > 0, NULL);
    g_return_val_if_fail(path_len <= COIL_PATH_LEN, NULL);
    g_return_val_if_fail((key && key_len) || !(key || key_len), NULL);
    g_return_val_if_fail((!(flags & COIL_PATH_IS_ROOT)
                || (flags & COIL_PATH_IS_ABSOLUTE && flags & COIL_PATH_IS_ROOT)), NULL);
    g_return_val_if_fail(key || !(flags & COIL_STATIC_KEY), NULL);

    CoilPath *p = path_alloc();
    p->str = path;
    p->len = path_len ? path_len : strlen(path);
    p->flags = flags;

    if (!(flags & COIL_PATH_IS_BACKREF)
            && G_LIKELY(path_len > 2) /* check for backref */
            && path[0] == COIL_PATH_DELIM
            && path[1] == COIL_PATH_DELIM)
        p->flags |= COIL_PATH_IS_BACKREF;
    else if (flags & COIL_PATH_IS_ROOT
            || (path_len == COIL_ROOT_PATH_LEN /* check for root */
                && (path == (const gchar *)&COIL_ROOT_PATH
                    || strncasecmp(path, COIL_ROOT_PATH, COIL_ROOT_PATH_LEN) == 0))) {
        p->flags |= COIL_PATH_IS_ROOT | COIL_PATH_IS_ABSOLUTE;
        p->flags &= ~COIL_STATIC_KEY;
        p->key = NULL;
        p->key_len = 0;
        return p;
    }
    else if (flags & COIL_PATH_IS_ABSOLUTE
            || path[0] == COIL_SPECIAL_CHAR) {
        p->flags |= COIL_PATH_IS_ABSOLUTE;
        if (key == NULL) {
            p->key = memrchr(p->str + COIL_ROOT_PATH_LEN,
                    COIL_PATH_DELIM,
                    p->len - COIL_ROOT_PATH_LEN);
            p->key_len = p->len - (++p->key - p->str);
            p->flags |= COIL_STATIC_KEY;
            return p;
        }
        goto have_key;
    }

    if (key) {
have_key:
        p->key = key;
        p->key_len = key_len;
    }
    else {
        p->key = memrchr(path, COIL_PATH_DELIM, path_len);
        if (p->key == NULL) {
            g_assert(!(p->flags & COIL_PATH_IS_ABSOLUTE));
            p->key = p->str;
            p->key_len = p->len;
        }
        else {
            p->key_len = p->len - (++p->key - p->str);
        }
    }

    if (p->key >= p->str && p->key <= p->str + p->len) {
        p->flags |= COIL_STATIC_KEY;
    }

    g_assert(p->key);
    g_assert(p->key_len);
    return p;
}

COIL_API(CoilPath *)
coil_path_new_len(const gchar *str, guint len, GError **error)
{
    g_return_val_if_fail(str || *str, NULL);
    g_return_val_if_fail(len > 0, NULL);

    if (!coil_check_path(str, len, error))
        return NULL;

    if (str[0] == COIL_PATH_DELIM && str[1] != COIL_PATH_DELIM) {
        /** ignore single leading . for relative path */
        str++;
        len--;
    }
    str = g_strndup(str, len);
    return coil_path_take_strings((gchar *)str, len, NULL, 0, 0);
}

COIL_API(CoilPath *)
coil_path_new(const gchar *str, GError **error)
{
    return coil_path_new_len(str, strlen(str), error);
}

COIL_API(CoilPath *)
coil_path_copy(CoilPath *p)
{
    g_return_val_if_fail(p, NULL);

    CoilPath *copy = path_alloc();

    copy->str = g_strndup(p->str, p->len);
    copy->len = p->len;
    copy->key = &copy->str[p->len - p->key_len];
    copy->key_len = p->key_len;
    copy->flags = (p->flags & ~COIL_STATIC_PATH) | COIL_STATIC_KEY;
    return copy;
}

COIL_API(gboolean)
coil_path_equal(CoilPath *a, CoilPath *b)
{
    g_return_val_if_fail(a, FALSE);
    g_return_val_if_fail(b, FALSE);

    if (a->hash != b->hash) {
        return FALSE;
    }
    if (a == b || a->str == b->str ||
        a->flags & b->flags & COIL_PATH_IS_ROOT) {
        return TRUE;
    }
    if (a->len != b->len ||
        a->key_len != b->key_len ||
        /* check same relativity */
        (a->flags ^ b->flags) & COIL_PATH_IS_ABSOLUTE) {
        return FALSE;
    }
    return memcmp(a->str, b->str, b->len) == 0;
}

COIL_API(gint)
coil_path_compare(CoilPath *a, CoilPath *b)
{
    g_return_val_if_fail(a, -1);
    g_return_val_if_fail(b, -1);

    if (a == b || a->str == b->str ||
        a->flags & b->flags & COIL_PATH_IS_ROOT) {
        return 0;
    }
    if (a->len != b->len) {
        return (a->len > b->len) ? 1 : -1;
    }
    return memcmp(a->str, b->str, b->len);
}

COIL_API(void)
coil_path_free(CoilPath *p)
{
    g_return_if_fail(p);

    CoilPathFlags flags = ~p->flags;

    if (flags & COIL_STATIC_KEY)
        g_free(p->key);

    if (flags & COIL_STATIC_PATH)
        g_free(p->str);

    g_free(p);
}

COIL_API(CoilPath *)
coil_path_ref(CoilPath *p)
{
    g_return_val_if_fail(p, NULL);

    g_atomic_int_inc(&p->ref_count);
    return p;
}

COIL_API(void)
coil_path_unref(CoilPath *p)
{
    g_return_if_fail(p);

    if (g_atomic_int_dec_and_test(&p->ref_count)) {
        coil_path_free(p);
    }
}

COIL_API(CoilPath *)
coil_path_concat(CoilPath *container, CoilPath *key, GError **error)
{
    g_return_val_if_fail(container, NULL);
    g_return_val_if_fail(key, NULL);

    gchar *p;
    CoilPath *path;
    guint path_len = container->len + key->key_len + 1;

    if (path_len > COIL_PATH_LEN) {
        p = g_strjoin(COIL_PATH_DELIM_S, container->str, key->key, NULL);
        path_length_error(p, path_len, error);
        g_free(p);
        return NULL;
    }

    path = path_alloc();
    path->len = path_len;
    path->str = g_new(gchar, path_len + 1);

    p = mempcpy(path->str, container->str, container->len);
    *p++ = COIL_PATH_DELIM;
    path->key = p;
    path->key_len = key->key_len;
    p = mempcpy(p, key->key, key->key_len);
    *p = '\0';

    path->flags = (container->flags & COIL_PATH_IS_ABSOLUTE) | COIL_STATIC_KEY;

    return path;
}

COIL_API(CoilPath *)
coil_build_path_valist(GError **error, const gchar *first_key, va_list args)
{
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    gchar *buffer, *p;
    const gchar *key = first_key;
    guint path_len = 0, key_len = 0, n = 64;

    p = buffer = (gchar *)g_malloc(n);

    do {
        key_len = strlen(key);
        path_len += key_len + 1;
        if (path_len >= n) {
            buffer = (gchar *)g_realloc(buffer, 2 * n);
            p = &buffer[path_len - key_len + 1];
        }
        p = (gchar *)mempcpy(p, key, key_len);
        *p++ = COIL_PATH_DELIM;
        key = va_arg(args, gchar *);
    } while (key);

    buffer[--path_len] = '\0';

    if (!coil_check_path(buffer, path_len, error)) {
        g_free(buffer);
        return NULL;
    }

    key = &buffer[path_len - key_len];

    return coil_path_take_strings(buffer, path_len,
            (gchar *)key, (guint8)key_len,
            COIL_STATIC_KEY);
}

COIL_API(CoilPath *)
coil_build_path(GError **error, const gchar *first_key, ...)
{
    g_return_val_if_fail(first_key, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    CoilPath *result;
    va_list args;

    va_start(args, first_key);
    result = coil_build_path_valist(error, first_key, args);
    va_end(args);

    return result;
 }

COIL_API(gboolean)
coil_validate_path_len(const gchar *path, guint path_len)
{
    static GRegex *path_regex = NULL;

    g_return_val_if_fail(path != NULL, FALSE);

    if (G_UNLIKELY(!path_regex)) {
        path_regex = g_regex_new("^"COIL_PATH_REGEX"$",
                G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);
    }
    return g_regex_match_full(path_regex, path, path_len,
            0, 0, NULL, NULL);
}

COIL_API(gboolean)
coil_validate_path(const gchar *path)
{
    return coil_validate_path_len(path, strlen(path));
}

COIL_API(gboolean)
coil_validate_key_len(const gchar *key, guint key_len)
{
    static GRegex *key_regex = NULL;

    g_return_val_if_fail(key, FALSE);

    if (G_UNLIKELY(!key_regex)) {
        key_regex = g_regex_new("^"COIL_KEY_REGEX"$",
                G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);
    }
    return g_regex_match_full(key_regex, key, key_len,
            0, 0, NULL, NULL);
}

COIL_API(gboolean)
coil_validate_key(const gchar *key)
{
    return coil_validate_key_len(key, strlen(key));
}


COIL_API(gboolean)
coil_check_key(const gchar *key, guint key_len, GError **error)
{
    g_return_val_if_fail(key, FALSE);
    g_return_val_if_fail(key_len, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (key_len > (COIL_PATH_LEN - COIL_ROOT_PATH_LEN - 1)) {
        key_length_error(key, key_len, error);
        return FALSE;
    }
    if (!coil_validate_key_len(key, key_len)) {
        g_set_error(error, COIL_ERROR, COIL_ERROR_KEY,
                "The key '%.*s' is invalid or contains invalid characters",
                key_len, key);
        return FALSE;
    }
    return TRUE;
}

COIL_API(gboolean)
coil_check_path(const gchar *path, guint path_len, GError **error)
{
    g_return_val_if_fail(path, FALSE);
    g_return_val_if_fail(path_len, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (path_len > COIL_PATH_LEN) {
        path_length_error(path, path_len, error);
        return FALSE;
    }
    if (!coil_validate_path_len(path, path_len)) {
        g_set_error(error, COIL_ERROR, COIL_ERROR_PATH,
                "The path '%.*s' is invalid or contains invalid characters.",
                path_len, path);
        return FALSE;
    }
    return TRUE;
}

COIL_API(gboolean)
coil_path_change_container(CoilPath **path_ptr,
        CoilPath *container, GError **error)
{
    g_return_val_if_fail(path_ptr && *path_ptr, FALSE);
    g_return_val_if_fail(container, FALSE);
    g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(container), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilPath *path;
    guint len;
    gchar *p;

    path = *path_ptr;
    len = container->len + path->key_len + 1;

    if (len > COIL_PATH_LEN) {
        if (error) {
            /* quickly make a path to show in the error message */
            p = g_strjoin(".", container->str, path->key, NULL);
            path_length_error(p, len, error);
            g_free(p);
        }
        return FALSE;
    }

    g_return_val_if_fail(path != container, FALSE);
    g_return_val_if_fail(!COIL_PATH_IS_ROOT(path), FALSE);
    g_return_val_if_fail(!COIL_PATH_IS_BACKREF(path), FALSE);
    g_return_val_if_fail(path->key && path->key_len, FALSE);

    if (path->ref_count > 1 || path->flags & COIL_STATIC_PATH) {
        CoilPath *old = path;

        path = path_alloc();
        path->str = g_new(gchar, len + 1);

        p = mempcpy(path->str, container->str, container->len);
        *p++ = COIL_PATH_DELIM;

        path->key = p;
        path->key_len = old->key_len;

        p = mempcpy(p, old->key, old->key_len);
        *p = '\0';

        coil_path_unref(old);
        *path_ptr = path;
    }
    else {
        if (len > path->len) {
            path->str = g_realloc(path->str, len + 1);
        }
        if (path->flags & COIL_STATIC_KEY) {
            p = path->str + container->len;
            path->key = (gchar *)memmove(p + 1, path->key, path->key_len);
            *p = COIL_PATH_DELIM;
            memcpy(path->str, container->str, container->len);
        }
        else {
            gchar *key;

            p = mempcpy(path->str, container->str, container->len);
            *p++ = COIL_PATH_DELIM;
            key = p;
            p = mempcpy(p, path->key, path->key_len);
            *p = '\0';

            g_free(path->key);
            path->key = key;
        }
    }
    path->str[len] = '\0';
    path->len = len;
    path->hash = hash_relative_path(container->hash,
            path->str + container->len, path->len - container->len);
    path->flags |= COIL_PATH_IS_ABSOLUTE | COIL_STATIC_KEY;
    return TRUE;
}

COIL_API(CoilPath *)
coil_path_resolve(CoilPath *path, CoilPath *container, GError **error)
{
    g_return_val_if_fail(path && path->str && path->len, NULL);
    g_return_val_if_fail(container && container->str && container->len, NULL);
    g_return_val_if_fail(path != container, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    CoilPath *res;
    const gchar *suffix, *end;
    gchar *p;
    guint hash = 0, container_len, suffix_len, path_len;

    if (COIL_PATH_IS_ABSOLUTE(path)) {
        return coil_path_ref((CoilPath *)path);
    }
    if (COIL_PATH_IS_RELATIVE(container)) {
        g_error("Error resolving path '%s', "
                "Prefix path argument '%s', must be an absolute path.",
                path->str, container->str);
    }

    suffix = path->str;
    end = container->str + container->len;

    if (*suffix == COIL_PATH_DELIM) {
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
    }
    else {
        container_len = container->len;
        suffix_len = path->len;
        path_len = container_len + suffix_len + 1;
        hash = hash_relative_path(container->hash, suffix, suffix_len);
    }

    if (G_UNLIKELY(path_len > COIL_PATH_LEN)) {
        p = g_strjoin(COIL_PATH_DELIM_S, container->str, suffix, NULL);
        path_length_error(p, path_len, error);
        g_free(p);
        return NULL;
    }
    else if (G_UNLIKELY(path_len == 0)) {
        coil_path_error(error, path,
                "references must contain at least one key "
                "ie. '..a'. Where as just '%c' are not valid.",
                COIL_PATH_DELIM);
        return NULL;
    }
    if (path_len == COIL_ROOT_PATH_LEN) {
        return coil_path_ref(CoilRootPath);
    }

    res = path_alloc();
    res->str = g_new(gchar, path_len + 1);

    p = mempcpy(res->str, container->str, container_len);
    *p++ = COIL_PATH_DELIM;
    memcpy(p, suffix, suffix_len);

    res->key = &res->str[path_len - path->key_len];
    res->str[path_len] = 0;
    res->len = path_len;
    res->key_len = path->key_len;
    res->flags = COIL_PATH_IS_ABSOLUTE | COIL_STATIC_KEY;

    if (hash)
        res->hash = hash;
    else
        res->hash = hash_absolute_path(res->str, res->len);

    return res;
}

COIL_API(gboolean)
coil_path_resolve_into(CoilPath **path, CoilPath *context, GError **error)
{
    g_return_val_if_fail(path || *path, FALSE);
    g_return_val_if_fail(context, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilPath *abspath = coil_path_resolve(*path, context, error);
    if (abspath == NULL)
        return FALSE;

    coil_path_unref(*path);
    *path = abspath;

    return TRUE;
}

COIL_API(CoilPath *) /* new reference */
coil_path_relativize(CoilPath *target, /* absolute path */
                     CoilPath *container) /* absolute path */
{
    g_return_val_if_fail(target, NULL);
    g_return_val_if_fail(container, NULL);
    g_return_val_if_fail(!COIL_PATH_IS_ROOT(target), NULL);
    g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(container), NULL);

    CoilPath *relative;
    guint8 prefix_len, tail_len, num_dots = 2;
    const gchar *delim, *prefix, *path;

    if (COIL_PATH_IS_RELATIVE(target) || COIL_PATH_IS_ROOT(target)) {
        return coil_path_ref((CoilPath *)target);
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

COIL_API(CoilPath *)
coil_path_pop(CoilPath *path, int i)
{
    g_return_val_if_fail(path != NULL, NULL);

    char *p;

    if (i >= 0) {
        do {
            p = memchr(path->str, COIL_PATH_DELIM, path->len);
            if (p == NULL) {
                /* TODO error */
                return NULL;
            }
        } while (--i > 0);
    }
    else {
        while (i++ < 0) {
            p = memrchr(path->str, COIL_PATH_DELIM, path->len);
            if (p == NULL) {
                /* TODO error */
                return NULL;
            }
        }
    }

    return coil_path_new_len(path->str, p - path->str);
}
