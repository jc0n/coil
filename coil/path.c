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

/* TODO: proper error handling */

static CoilPath *
path_alloc(void)
{
  CoilPath *path = g_new0(CoilPath, 1);
  path->ref_count = 1;
  return path;
}

static void
pathval_to_strval(const GValue *pathval,
                        GValue *strval)
{
  g_return_if_fail(G_IS_VALUE(pathval));
  g_return_if_fail(G_IS_VALUE(strval));

  CoilPath *path;
  gchar    *string;

  path = (CoilPath *)g_value_get_boxed(pathval);
  string = g_strndup(path->path, path->path_len);
  g_value_take_string(strval, string);
}

GType
coil_path_get_type(void)
{
  static GType type_id = 0;

  if (G_UNLIKELY(!type_id))
  {
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
  while (list)
  {
    coil_path_unref(list->data);
    list = g_list_delete_link(list, list);
  }
}

void
path_length_error(const gchar *path,
                  guint        path_len,
                  GError     **error)
{
  g_return_if_fail(path && *path);
  g_return_if_fail(error == NULL || *error == NULL);

  gchar prefix[16], suffix[16];

  memcpy(prefix, path, 15);
  prefix[15] = '\0';

  memcpy(suffix, path + path_len - 15, 15);
  suffix[15] = '\0';

  g_set_error(error,
              COIL_ERROR,
              COIL_ERROR_PATH,
              "Length of path %s...%s too long (%d). Max path length is %d",
              prefix, suffix, path_len, COIL_PATH_LEN);
}

void
key_length_error(const gchar *key,
                 guint        key_len,
                 GError     **error)
{
  g_return_if_fail(key && *key);
  g_return_if_fail(error == NULL || *error == NULL);

  gchar prefix[16], suffix[16];

  memcpy(prefix, key, 15);
  prefix[15] = '\0';

  memcpy(suffix, &key[key_len] - 15, 15);
  suffix[15] = '\0';

  g_set_error(error,
              COIL_ERROR,
              COIL_ERROR_KEY,
              "Length of key %s...%s too long (%d). Max key length is %d",
              prefix, suffix, key_len,
              (gint)(COIL_PATH_LEN - COIL_ROOT_PATH_LEN - 1));
}

COIL_API(CoilPath *)
coil_path_take_strings(gchar         *path,
                       guint8         path_len,
                       gchar         *key,
                       guint8         key_len,
                       CoilPathFlags  flags)

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
  p->path = path;
  p->path_len = path_len ? path_len : strlen(path);
  p->flags = flags;

  if (!(flags & COIL_PATH_IS_BACKREF)
      && G_LIKELY(path_len > 2) /* check for backref */
      && path[0] == COIL_PATH_DELIM
      && path[1] == COIL_PATH_DELIM)
    p->flags |= COIL_PATH_IS_BACKREF;
  else if (flags & COIL_PATH_IS_ROOT
      || (path_len == COIL_ROOT_PATH_LEN /* check for root */
        && (path == (const gchar *)&COIL_ROOT_PATH
          || strncasecmp(path, COIL_ROOT_PATH, COIL_ROOT_PATH_LEN) == 0)))
  {
    p->flags |= COIL_PATH_IS_ROOT | COIL_PATH_IS_ABSOLUTE;
    p->flags &= ~COIL_STATIC_KEY;
    p->key = NULL;
    p->key_len = 0;
    return p;
  }
  else if (flags & COIL_PATH_IS_ABSOLUTE
      || path[0] == COIL_SPECIAL_CHAR)
  {
    p->flags |= COIL_PATH_IS_ABSOLUTE;
    if (key == NULL)
    {
      p->key = memrchr(p->path + COIL_ROOT_PATH_LEN,
                       COIL_PATH_DELIM,
                       p->path_len - COIL_ROOT_PATH_LEN);
      p->key_len = p->path_len - (++p->key - p->path);
      p->flags |= COIL_STATIC_KEY;
      return p;
    }
    goto have_key;
  }

  if (key)
  {
have_key:
    p->key = key;
    p->key_len = key_len;
  }
  else
  {
    p->key = memrchr(path, COIL_PATH_DELIM, path_len);
    if (p->key == NULL)
    {
      g_assert(!(p->flags & COIL_PATH_IS_ABSOLUTE));
      p->key = p->path;
      p->key_len = p->path_len;
    }
    else
      p->key_len = p->path_len - (++p->key - p->path);
  }

  if (p->key >= p->path && p->key <= p->path + p->path_len)
    p->flags |= COIL_STATIC_KEY;

  g_assert(p->key);
  g_assert(p->key_len);

  return p;
}

COIL_API(CoilPath *)
coil_path_new_len(const gchar  *buffer,
                  guint         buf_len,
                  GError      **error)
{
  g_return_val_if_fail(buffer || *buffer, NULL);
  g_return_val_if_fail(buf_len > 0, NULL);

  if (!coil_check_path(buffer, buf_len, error))
    return NULL;

  if (buffer[0] == COIL_PATH_DELIM
     && buffer[1] != COIL_PATH_DELIM)
  {
    /** ignore single leading . for relative path */
    buffer++;
    buf_len--;
  }

  buffer = g_strndup(buffer, buf_len);
  return coil_path_take_strings((gchar *)buffer, buf_len, NULL, 0, 0);
}

COIL_API(CoilPath *)
coil_path_new(const gchar *buffer,
              GError     **error)
{
  return coil_path_new_len(buffer, strlen(buffer), error);
}

COIL_API(CoilPath *)
coil_path_copy(const CoilPath *p)
{
  g_return_val_if_fail(p, NULL);

  CoilPath *copy = path_alloc();

  copy->path = g_strndup(p->path, p->path_len);
  copy->path_len = p->path_len;
  copy->key = &copy->path[p->path_len - p->key_len];
  copy->key_len = p->key_len;
  copy->flags = (p->flags & ~COIL_STATIC_PATH) | COIL_STATIC_KEY;

  return copy;
}

COIL_API(gboolean)
coil_path_equal(const CoilPath *a,
                const CoilPath *b)
{
  g_return_val_if_fail(a, FALSE);
  g_return_val_if_fail(b, FALSE);

  if (a == b
    || a->path == b->path
    || a->flags & b->flags & COIL_PATH_IS_ROOT)
    return TRUE;

  if (a->path_len != b->path_len
    || a->key_len != b->key_len
      /* check same relativity */
    || (a->flags ^ b->flags) & COIL_PATH_IS_ABSOLUTE)
    return FALSE;

  return memcmp(a->path, b->path, b->path_len) == 0;
}

COIL_API(gint)
coil_path_compare(const CoilPath *a,
                  const CoilPath *b)
{
  g_return_val_if_fail(a, -1);
  g_return_val_if_fail(b, -1);

  if (a == b
    || a->path == b->path
    || a->flags & b->flags & COIL_PATH_IS_ROOT)
    return 0;

  return strcmp(a->path, b->path);
}


COIL_API(void)
coil_path_free(CoilPath *p)
{
  g_return_if_fail(p);

  CoilPathFlags flags = ~p->flags;

  if (flags & COIL_STATIC_KEY)
    g_free(p->key);

  if (flags & COIL_STATIC_PATH)
    g_free(p->path);

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

  if (g_atomic_int_dec_and_test(&p->ref_count))
    coil_path_free(p);
}

#ifdef COIL_DEBUG
void
coil_path_debug(CoilPath *p)
{
  g_debug("path @ %p = {\npath=%s\npath_len=%d\nkey=%s\nkey_len=%d\n"
          "key_overlap=%d\nis_root=%d\nis_abs=%d\nis_backref=%d\n"
          "static_path=%d\nstatic_key=%d}\n",
          p, p->path, p->path_len, p->key, p->key_len,
          p->key >= p->path && p->key <= p->path + p->path_len,
          (p->flags & COIL_PATH_IS_ROOT) > 0,
          (p->flags & COIL_PATH_IS_ABSOLUTE) > 0,
          (p->flags & COIL_PATH_IS_BACKREF) > 0,
          (p->flags & COIL_STATIC_PATH) > 0,
          (p->flags & COIL_STATIC_KEY) > 0);
}
#endif

COIL_API(CoilPath *)
coil_path_concat(const CoilPath  *container,
                 const CoilPath  *key,
                 GError         **error)
{
  g_return_val_if_fail(container, NULL);
  g_return_val_if_fail(key, NULL);

  gchar    *p;
  CoilPath *path;
  guint     path_len = container->path_len + key->key_len + 1;

  if (path_len > COIL_PATH_LEN)
  {
    p = g_strjoin(COIL_PATH_DELIM_S, container->path, key->key, NULL);
    path_length_error(p, path_len, error);
    g_free(p);

    return NULL;
  }

  path = path_alloc();
  path->path_len = path_len;
  path->path = g_new(gchar, path_len + 1);

  p = mempcpy(path->path, container->path, container->path_len);
  *p++ = COIL_PATH_DELIM;
  path->key = p;
  path->key_len = key->key_len;
  p = mempcpy(p, key->key, key->key_len);
  *p = '\0';

  path->flags = (container->flags & COIL_PATH_IS_ABSOLUTE)
              | COIL_STATIC_KEY;

  return path;
}

COIL_API(CoilPath *)
coil_build_path_valist(GError     **error,
                       const gchar *first_key,
                       va_list      args)
{
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  gchar       *buffer, *p;
  const gchar *key = first_key;
  guint        path_len = 0, key_len = 0, n = 64;

  p = buffer = (gchar *)g_malloc(n);

  do
  {
    key_len = strlen(key);
    path_len += key_len + 1;

    if (path_len >= n)
    {
      buffer = (gchar *)g_realloc(buffer, 2 * n);
      p = &buffer[path_len - key_len + 1];
    }

    p = (gchar *)mempcpy(p, key, key_len);
    *p++ = COIL_PATH_DELIM;

    key = va_arg(args, gchar *);
  } while (key);

  buffer[--path_len] = '\0';

  if (!coil_check_path(buffer, path_len, error))
  {
    g_free(buffer);
    return NULL;
  }

  key = &buffer[path_len - key_len];

  return coil_path_take_strings(buffer, path_len,
                                (gchar *)key, (guint8)key_len,
                                COIL_STATIC_KEY);
}

COIL_API(CoilPath *)
coil_build_path(GError     **error,
                const gchar *first_key,
                ...)
{
  g_return_val_if_fail(first_key, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilPath *result;
  va_list   args;

  va_start(args, first_key);
  result = coil_build_path_valist(error, first_key, args);
  va_end(args);

  return result;
 }

COIL_API(gboolean)
coil_validate_path_len(const gchar *path,
                       guint        path_len)
{
  static GRegex *path_regex = NULL;

  g_return_val_if_fail(path != NULL, FALSE);

  if (G_UNLIKELY(!path_regex))
    path_regex = g_regex_new("^"COIL_PATH_REGEX"$",
                             G_REGEX_OPTIMIZE,
                             G_REGEX_MATCH_NOTEMPTY,
                             NULL);

  return g_regex_match_full(path_regex, path, path_len,
                            0, 0, NULL, NULL);
}

COIL_API(gboolean)
coil_validate_path(const gchar *path)
{
  return coil_validate_path_len(path, strlen(path));
}

COIL_API(gboolean)
coil_validate_key_len(const gchar *key,
                      guint        key_len)
{
  static GRegex *key_regex = NULL;

  g_return_val_if_fail(key, FALSE);

  if (G_UNLIKELY(!key_regex))
    key_regex = g_regex_new("^"COIL_KEY_REGEX"$",
                            G_REGEX_OPTIMIZE,
                            G_REGEX_MATCH_NOTEMPTY,
                            NULL);

  return g_regex_match_full(key_regex, key, key_len,
                            0, 0, NULL, NULL);
}

COIL_API(gboolean)
coil_validate_key(const gchar *key)
{
  return coil_validate_key_len(key, strlen(key));
}


COIL_API(gboolean)
coil_check_key(const gchar *key,
               guint        key_len,
               GError     **error)
{
  g_return_val_if_fail(key, FALSE);
  g_return_val_if_fail(key_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (key_len > (COIL_PATH_LEN - COIL_ROOT_PATH_LEN - 1))
  {
    key_length_error(key, key_len, error);
    return FALSE;
  }

  if (!coil_validate_key_len(key, key_len))
  {
    g_set_error(error,
                COIL_ERROR,
                COIL_ERROR_KEY,
                "The key '%s' contains invalid characters",
                key);

    return FALSE;
  }

  return TRUE;
}

COIL_API(gboolean)
coil_check_path(const gchar *path,
                guint        path_len,
                GError     **error)
{
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (path_len > COIL_PATH_LEN)
  {
    path_length_error(path, path_len, error);
    return FALSE;
  }

  if (!coil_validate_path_len(path, path_len))
  {
    g_set_error(error,
                COIL_ERROR,
                COIL_ERROR_PATH,
                "The path '%s' contains invalid characters.",
                path);

    return FALSE;
  }

  return TRUE;
}

COIL_API(gboolean)
coil_path_change_container(CoilPath      **path_ptr,
                           const CoilPath *container,
                           GError        **error)
{
  g_return_val_if_fail(path_ptr && *path_ptr, FALSE);
  g_return_val_if_fail(container, FALSE);
  g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(container), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilPath *path;
  guint     len;
  gchar    *p;

  path = *path_ptr;
  len = container->path_len + path->key_len + 1;

  if (len > COIL_PATH_LEN)
  {
    if (error)
    {
     /* quickly make a path to show in the error message */
      p = g_strjoin(".", container->path, path->key, NULL);
      path_length_error(p, len, error);
      g_free(p);
    }

    return FALSE;
  }

  g_return_val_if_fail(path != container, FALSE);
  g_return_val_if_fail(!COIL_PATH_IS_ROOT(path), FALSE);
  g_return_val_if_fail(!COIL_PATH_IS_BACKREF(path), FALSE);
  g_return_val_if_fail(path->key && path->key_len, FALSE);

  if (path->ref_count > 1
    || path->flags & COIL_STATIC_PATH)
  {
    CoilPath *old = path;

    path = path_alloc();
    path->path = g_new(gchar, len + 1);

    p = mempcpy(path->path, container->path, container->path_len);
    *p++ = COIL_PATH_DELIM;

    path->key = p;
    path->key_len = old->key_len;

    p = mempcpy(p, old->key, old->key_len);
    *p = '\0';

    coil_path_unref(old);
    *path_ptr = path;
  }
  else
  {
    if (len > path->path_len)
      path->path = g_realloc(path->path, len + 1);

    if (path->flags & COIL_STATIC_KEY)
    {
      p = path->path + container->path_len;
      path->key = memmove(p + 1, path->key, path->key_len);
      *p = COIL_PATH_DELIM;
      memcpy(path->path, container->path, container->path_len);
    }
    else
    {
      gchar *key;

      p = mempcpy(path->path,
                  container->path,
                  container->path_len);
      *p++ = COIL_PATH_DELIM;
      key = p;
      p = mempcpy(p, path->key, path->key_len);
      *p = '\0';

      g_free(path->key);
      path->key = key;
    }
  }

  path->path[len] = '\0';
  path->path_len = len;
  path->flags |= COIL_PATH_IS_ABSOLUTE | COIL_STATIC_KEY;
  return TRUE;
}

COIL_API(CoilPath *) /* new reference */
coil_path_resolve(const CoilPath *path, /* any path */
                  const CoilPath *context, /* absolute path */
                  GError        **error)
{
  g_return_val_if_fail(path && path->path && path->path_len, NULL);
  g_return_val_if_fail(context && context->path && context->path_len, NULL);
  g_return_val_if_fail(path != context, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (G_UNLIKELY(COIL_PATH_IS_RELATIVE(context)))
    g_error("Error resolving path '%s', "
        "Prefix path argument '%s', must be an absolute path.",
        path->path, context->path);

  if (COIL_PATH_IS_ABSOLUTE(path))
    return coil_path_ref((CoilPath *)path);

  const gchar *qualifier, *e;
  guint8       context_len, qualifier_len;
  guint16      path_len;

  qualifier = path->path;
  e = context->path + context->path_len;

  if (*qualifier == COIL_PATH_DELIM)
  {
    while (*++qualifier == COIL_PATH_DELIM)
      while (*--e != COIL_PATH_DELIM)
        if (G_UNLIKELY(e < context->path))
        {
          coil_path_error(error, path,
              "reference passed root while resolving against '%s'.",
              context->path);

          return NULL;
        }

    context_len = e - context->path;
    qualifier_len = path->path_len - (qualifier - path->path);
    path_len = (qualifier_len) ? context_len + qualifier_len + 1 : context_len;
  }
  else
  {
    context_len = context->path_len;
    qualifier_len = path->path_len;
    path_len = context_len + qualifier_len + 1;
  }

  if (G_UNLIKELY(path_len > COIL_PATH_LEN))
  {
    gchar *p;
    p = g_strjoin(COIL_PATH_DELIM_S, context->path, qualifier, NULL);
    path_length_error(p, path_len, error);
    g_free(p);

    return NULL;
  }
  else if (G_UNLIKELY(path_len == 0))
  {
    coil_path_error(error, path,
        "references must contain at least one key "
        "ie. '..a'. Where as just '%c' are not valid.",
        COIL_PATH_DELIM);

    return NULL;
  }

  CoilPath *resolved = path_alloc();

  if (path_len == COIL_ROOT_PATH_LEN)
  {
    resolved->path = COIL_ROOT_PATH;
    resolved->path_len = COIL_ROOT_PATH_LEN;
    resolved->key = NULL;
    resolved->key_len = 0;
    resolved->flags = COIL_STATIC_PATH
                    | COIL_PATH_IS_ABSOLUTE
                    | COIL_PATH_IS_ROOT;
  }
  else
  {
    register gchar *p;

    resolved->path = g_new(gchar, path_len + 1);

    p = mempcpy(resolved->path, context->path, context_len);
    *p++ = COIL_PATH_DELIM;
    memcpy(p, qualifier, qualifier_len);

    resolved->key = &resolved->path[path_len - path->key_len];
    resolved->path[path_len] = 0;
    resolved->path_len = path_len;
    resolved->key_len = path->key_len;
    resolved->flags = COIL_PATH_IS_ABSOLUTE
                    | COIL_STATIC_KEY;
  }

  return resolved;
}

COIL_API(gboolean)
coil_path_resolve_into(CoilPath      **path,
                       const CoilPath *context,
                       GError        **error)
{
  g_return_val_if_fail(path || *path, FALSE);
  g_return_val_if_fail(context, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilPath *abspath;

  abspath = coil_path_resolve(*path, context, error);
  if (abspath == NULL)
    return FALSE;

  coil_path_unref(*path);
  *path = abspath;

  return TRUE;
}


COIL_API(CoilPath *) /* new reference */
coil_path_relativize(const CoilPath *target, /* absolute path */
                     const CoilPath *container) /* absolute path */
{
  g_return_val_if_fail(target, NULL);
  g_return_val_if_fail(container, NULL);
  g_return_val_if_fail(!COIL_PATH_IS_ROOT(target), NULL);
  g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(container), NULL);

  if (COIL_PATH_IS_RELATIVE(target))
    return coil_path_ref((CoilPath *)target);

  CoilPath    *relative = path_alloc();
  guint8       prefix_len, tail_len, num_dots;
  const gchar *delim, *prefix, *path;

  /* both paths are absolute, start checking after @root */
  prefix = delim = container->path + COIL_ROOT_PATH_LEN;
  path = target->path + COIL_ROOT_PATH_LEN;

  if (target == container)
  {
    num_dots = 2;
    prefix_len = container->path_len;

    goto backref;
  }

  /* find the first differing character
   * which marks the end of the prefix and
   * possibly the end of the path */
  while (*prefix != '\0' && *prefix == *path)
  {
    /* keep track of the lask delim in prefix */
    if (*prefix == COIL_PATH_DELIM)
      delim = prefix;

    prefix++;
    path++;
  }

  prefix_len = delim - container->path;

  /* the only case where there are no dots is
   * if <prefix> is a prefix of path */
  if (*prefix == '\0' && *path == COIL_PATH_DELIM)
  {
    /* just move the keys to the front
     * no need to mess with the allocation */
    relative->path_len = (target->path_len - prefix_len) - 1;
    relative->path = g_strndup(path + 1, relative->path_len);
  }
  else
  {
    num_dots = 2;
    /* count # of parts to remove from prefix path to get to
     * path ie. number of dots to add to relative path */
    while ((delim = strchr(delim + 1, COIL_PATH_DELIM)) != NULL)
      num_dots++;

    g_assert(num_dots < COIL_PATH_MAX_PARTS);

backref:
    tail_len = target->path_len - prefix_len - 1;
    relative->path_len = tail_len + num_dots;
    relative->path = g_new(gchar, relative->path_len + 1);

    memset(relative->path, COIL_PATH_DELIM, num_dots);
    memcpy(relative->path + num_dots, path, tail_len);
  }

  relative->key = &relative->path[relative->path_len - target->key_len];
  relative->key_len = target->key_len;
  relative->path[relative->path_len] = 0;
  relative->flags = COIL_STATIC_KEY;

  return relative;
}

