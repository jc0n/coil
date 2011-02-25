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

CoilPath *
coil_path_alloc(void)
{
  CoilPath *path = g_new0(CoilPath, 1);
  path->ref_count = 1;
  return path;
}

GType
coil_path_get_type(void)
{
  static GType type_id = 0;

  if (G_UNLIKELY(!type_id))
    type_id = g_boxed_type_register_static(g_intern_static_string("CoilPath"),
                                           (GBoxedCopyFunc)coil_path_copy,
                                           (GBoxedFreeFunc)coil_path_unref);
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

COIL_API(CoilPath *)
coil_path_take_strings(gchar         *path,
                       guint8         path_len,
                       gchar         *key,
                       guint8         key_len,
                       CoilPathFlags  flags)

{
  g_return_val_if_fail(path && *path, NULL);
  g_return_val_if_fail(path_len, NULL);
  g_return_val_if_fail((key && key_len) || !(key || key_len), NULL);
  g_return_val_if_fail((!(flags & COIL_PATH_IS_ROOT)
      || (flags & COIL_PATH_IS_ABSOLUTE && flags & COIL_PATH_IS_ROOT)), NULL);
  g_return_val_if_fail(key || !(flags & COIL_STATIC_KEY), NULL);

  CoilPath *p = coil_path_alloc();
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
                  guint8        buf_len)
{
  g_return_val_if_fail(buffer || *buffer, NULL);
  g_return_val_if_fail(buf_len, NULL);

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
coil_path_new(const gchar *buffer)
{
  return coil_path_new_len(buffer, strlen(buffer));
}

COIL_API(CoilPath *)
coil_path_copy(const CoilPath *p)
{
  g_return_val_if_fail(p, NULL);

  CoilPath *copy = coil_path_alloc();

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
coil_path_concat(const CoilPath *container,
                 const CoilPath *key)
{
  g_return_val_if_fail(container, NULL);
  g_return_val_if_fail(key, NULL);

  gchar    *p;
  CoilPath *path = coil_path_alloc();

  path->path_len = container->path_len + key->key_len + 1;
  path->path = g_new(gchar, path->path_len + 1);

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

/*
 * Builds a path based on a variable number of key arguments
 *
 * @param base the first key in the path
 * @return A valid coil path string assuming the keys are valid
 */
COIL_API(CoilPath *)
coil_path_build_new_vlen(guint8       path_len,
                         const gchar *base,
                         va_list      args)
{
  g_return_val_if_fail(base, NULL);

  va_list        a;
  const gchar   *path, *sp, *kp;
  gchar         *pp;

  if (path_len < 1)
  {
    sp = base;
    va_copy(a, args);
    do
    {
      path_len += strlen(sp) + 1; /* +1 for path '.' delimiter */
    } while ((sp = va_arg(a, gchar *)));
    va_end(a);

    if (path_len > COIL_PATH_LEN)
      g_error("Path length %d is longer than "
          "maximum length %d.", path_len, COIL_PATH_LEN);
  }

  path = g_new(gchar, path_len + 1);
  pp = (gchar *)path;
  sp = base;
  va_copy(a, args);
  do
  {
    kp = pp;
    pp = g_stpcpy(pp, sp);
    *pp++ = COIL_PATH_DELIM;
  } while ((sp = va_arg(a, gchar *)));
  va_end(a);
  *--pp = 0;

  return coil_path_take_strings((gchar *)path, path_len,
                                (gchar *)kp, pp - kp, COIL_STATIC_KEY);
}

COIL_API(CoilPath *)
coil_path_build_new_len(guint8       path_len,
                        const gchar *base,
                        ...)
{
  g_return_val_if_fail(base, NULL);
  g_return_val_if_fail(path_len, NULL);

  va_list   args;
  CoilPath *path;

  va_start(args, base);
  path = coil_path_build_new_vlen(path_len, base, args);
  va_end(args);

  return path;
}

COIL_API(CoilPath *)
coil_path_build_new(const gchar *base,
                    ...)
{
  g_return_val_if_fail(base, NULL);

  va_list   args;
  CoilPath *path;

  va_start(args, base);
  path = coil_path_build_new_vlen(0, base, args);
  va_end(args);

  return path;
}

/**
 * Check that a path is valid
 *
 * @param a coil path string
 * @return TRUE if path is valid.
 */
COIL_API(gboolean)
coil_validate_path_strn(const gchar *path,
                        guint8       path_len)
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
coil_validate_path_str(const gchar *path)
{
  return coil_validate_path_strn(path, strlen(path));
}

COIL_API(gboolean)
coil_validate_path(const CoilPath *path)
{
  return coil_validate_path_strn(path->path, path->path_len);
}

/**
 * Check that a key is valid
 *
 * @param a coil key string
 * @return TRUE if key is valid
 */
COIL_API(gboolean)
coil_validate_key_strn(const gchar *key,
                       guint8       key_len)
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
coil_validate_key_str(const gchar *key)
{
  return coil_validate_key_strn(key, strlen(key));
}

COIL_API(void)
coil_path_change_container(CoilPath      **path_ptr,
                           const CoilPath *container)
{
  g_return_if_fail(path_ptr);
  g_return_if_fail(*path_ptr);
  g_return_if_fail(container);
  g_return_if_fail(COIL_PATH_IS_ABSOLUTE(container));

  CoilPath *path = *path_ptr;
  guint8    len = container->path_len + path->key_len + 1;
  gchar    *p;

  g_return_if_fail(path != container);
  g_return_if_fail(!COIL_PATH_IS_ROOT(path));
  g_return_if_fail(!COIL_PATH_IS_BACKREF(path));
  g_return_if_fail(path->key && path->key_len);

  if (path->ref_count > 1 || path->flags & COIL_STATIC_PATH)
  {
    CoilPath *old = path;

    path = coil_path_alloc();
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
}

COIL_API(CoilPath *) /* new reference */
coil_path_resolve(const CoilPath *path, /* any path */
                  const CoilPath *prefix, /* absolute path */
                  GError        **error)
{
  g_return_val_if_fail(path && path->path && path->path_len, NULL);
  g_return_val_if_fail(prefix && prefix->path && prefix->path_len, NULL);
  g_return_val_if_fail(path != prefix, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (G_UNLIKELY(COIL_PATH_IS_RELATIVE(prefix)))
    g_error("Error resolving path '%s', "
        "Prefix path argument '%s', must be an absolute path.",
        path->path, prefix->path);

  if (COIL_PATH_IS_ABSOLUTE(path))
    return coil_path_ref((CoilPath *)path);

  const gchar *qualifier, *e;
  guint8       path_len, prefix_len, qualifier_len;

  qualifier = path->path;
  e = prefix->path + prefix->path_len;

  if (*qualifier == COIL_PATH_DELIM)
  {
    while (*++qualifier == COIL_PATH_DELIM)
      while (*--e != COIL_PATH_DELIM)
        if (G_UNLIKELY(prefix->path > e))
        {
          coil_path_error(error, path,
              "reference passed root while resolving against '%s'.",
              prefix->path);

          return NULL;
        }

    prefix_len = e - prefix->path;
    qualifier_len = path->path_len - (qualifier - path->path);
    path_len = (qualifier_len) ? prefix_len + qualifier_len + 1 : prefix_len;
  }
  else
  {
    prefix_len = prefix->path_len;
    qualifier_len = path->path_len;
    path_len = prefix_len + qualifier_len + 1;
  }

  if (G_UNLIKELY(path_len > COIL_PATH_LEN))
  {
    coil_path_error(error, path,
          "length %d too long resolving against '%s'. "
          "A path can contain a maximum of %d characters.",
          path_len, prefix->path, COIL_PATH_LEN);

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

  /*g_debug("%s (%s : %s) to ...", __FUNCTION__, prefix->path, path->path);*/

  CoilPath *resolved = coil_path_alloc();

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

    p = mempcpy(resolved->path, prefix->path, prefix_len);
    *p++ = COIL_PATH_DELIM;
    memcpy(p, qualifier, qualifier_len);

    resolved->key = &resolved->path[path_len - path->key_len];
    resolved->path[path_len] = 0;
    resolved->path_len = path_len;
    resolved->key_len = path->key_len;
    resolved->flags = COIL_PATH_IS_ABSOLUTE
                    | COIL_STATIC_KEY;
  }

  /*g_debug("%s resolved_path=%s", __FUNCTION__, resolved->path);*/

  return resolved;
}

/*
 * Compute the relative path from two absolute paths.
 *
 * This function takes two absolute paths and computes the optimal
 * relative path to reach the second given the first. Where <i>path</i> is
 * the destination and <i>base</i> is the source.
 *
 * Example output:
 *
 * prefix: @root.asdf.bxd          (*p1)
 * path: @root.asdf.bhd.xxx.yyy  (*p2)
 * result: ..bhd.xxx.yyy
 *
 *
 * prefix: @root.asdf.bxd.xxx.yyy  (*p1)
 * path: @root.asdf.bhd          (*p2)
 * result: ....bhd
 *
 * prefix: @root.asdf.bhd          (!*p1)
 * path: @root.asdf.bhd.xyz      (*p2)
 * result: xyz
 *
 *
 * prefix: @root.asdf.bhd.xyz      (*p1)
 * path: @root.asdf.bhd          (!*p2)
 * result: ...bhd
 *
 * prefix: @root.asdf.asdf         (!*p1)
 * path: @root.asdf.asdf         (!*p2)
 * result: ..asdf
 *
 * prefix: @root.asdf (*p1)
 * path: @root.xyz  (*p2)
 * result: ..xyz
 *
 * prefix: @root.asdf (!*p1)
 * path: @root.asdf (!*p2)
 * result: ..asdf
 *
 * @param an absolute path and source of the result
 * @param an absolute path and destination of the result relative to <i>base</i>
 * @return a relative path to reach <i>path</i> from <i>base</i>
 *
 */
COIL_API(CoilPath *) /* new reference */
coil_path_relativize(const CoilPath *path, /* absolute path */
                     const CoilPath *prefix) /* absolute path */
{
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(prefix, NULL);
  g_return_val_if_fail(!COIL_PATH_IS_ROOT(path), NULL);
  g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(prefix), NULL);

  if (COIL_PATH_IS_RELATIVE(path))
    return coil_path_ref((CoilPath *)path);

  CoilPath    *relative = coil_path_alloc();
  guint8       prefix_len, num_dots;
  const gchar *delim_ptr, *prefix_ptr, *path_ptr;

  delim_ptr = prefix->path + COIL_ROOT_PATH_LEN;
  prefix_ptr = delim_ptr + 1;
  path_ptr = path->path + COIL_ROOT_PATH_LEN + 1;

  if (path == prefix)
  {
    num_dots = 2;
    prefix_len = prefix->path_len;
    goto backref;
  }

  /* find the first differing character
   * which marks the end of the prefix and
   * possibly the end of the path */
  while (*prefix_ptr != '\0'
     && *prefix_ptr == *path_ptr)
  {
    /* keep track of the lask delim in prefix */
    if (*prefix_ptr == COIL_PATH_DELIM)
      delim_ptr = prefix_ptr;

    prefix_ptr++;
    path_ptr++;
  }

  prefix_len = delim_ptr - prefix->path;

  /* the only case where there are no dots is
   * if <prefix> is a prefix of path */
  if (*prefix_ptr == '\0' && *path_ptr == COIL_PATH_DELIM)
  {
    /* just move the keys to the front
     * no need to mess with the allocation */
    relative->path_len = (path->path_len - prefix_len) - 1;
    relative->path = g_strndup(path_ptr + 1, relative->path_len);
  }
  else
  {
    num_dots = 2;
    /* count # of parts to remove from prefix path to get to
     * path ie. number of dots to add to relative path */
    while ((delim_ptr = strchr(delim_ptr, COIL_PATH_DELIM)))
      num_dots++;

backref:
    {
      guint8 tail_len = path->path_len - prefix_len - 1;
      relative->path_len = tail_len + num_dots;
      relative->path = g_new(gchar, relative->path_len + 1);

      memset(relative->path, COIL_PATH_DELIM, num_dots);
      memcpy(relative->path + num_dots, path_ptr + 1, tail_len);
    }
  }

  relative->key = &relative->path[relative->path_len - path->key_len];
  relative->key_len = path->key_len;
  relative->path[relative->path_len] = 0;
  relative->flags = COIL_STATIC_KEY;

  return relative;
}

/*
static gboolean
path_has_container(const gchar *path,
                   const gchar *base,
                   gboolean     strict)
{
  register const gchar *p1, *p2;
  gboolean has_prefix = FALSE;

  g_return_val_if_fail(path != NULL, FALSE);
  g_return_val_if_fail(base != NULL, FALSE);

  if (COIL_PATH_IS_ROOT(base))
    return !COIL_PATH_IS_ROOT(path);

  p1 = path;
  p2 = base;

  while (*p1 != 0
    && *p1 == *p2)
  {
    p1++;
    p2++;
  }

  has_prefix = (*p1 == COIL_PATH_DELIM && *p2 == 0);

  if (strict)
    has_prefix &= (strchr(p1, COIL_PATH_DELIM) == NULL);

  return has_prefix;
}

COIL_API(gboolean)
coil_path_is_descendent(const gchar *path,
                        const gchar *maybe_container)
{
  g_return_val_if_fail(path != NULL, FALSE);
  g_return_val_if_fail(maybe_container != NULL, FALSE);

  return path_has_container(path, maybe_container, FALSE);
}

COIL_API(gboolean)
coil_path_has_container(const gchar *path,
                        const gchar *maybe_container)
{
  g_return_val_if_fail(path != NULL, FALSE);
  g_return_val_if_fail(maybe_container != NULL, FALSE);

  return path_has_container(path, maybe_container, TRUE);
}
*/
