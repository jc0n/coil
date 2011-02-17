/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include <string.h>

#include "struct.h"
#include "struct_private.h"

#define DEFAULT_MAX 63 // max number of buckets (2^n - 1)
#define HASH_BYTE(hash, byte) (hash * 33 + (byte))

static inline guint
hash_bytes(guint         hash,
           const guchar *byte,
           guint         n)
{
  g_return_val_if_fail(byte && *byte, 0);
  g_return_val_if_fail(n, 0);

  do
  {
    hash = HASH_BYTE(hash, *byte);
    byte++;
  } while (--n > 0);

  return hash;
}

inline guint
hash_absolute_path(const gchar *path,
                   guint8       path_len)
{
  g_return_val_if_fail(path && *path, 0);
  g_return_val_if_fail(*path == '@', 0); /* must be absolute */
  g_return_val_if_fail(path_len, 0);

  path += COIL_ROOT_PATH_LEN;
  path_len -= COIL_ROOT_PATH_LEN;

  return path_len ? hash_bytes(0, (guchar *)path, path_len) : 0;
}

inline guint
hash_relative_path(guint        container_hash,
                   const gchar *path,
                   guint8       path_len)
{
  g_return_val_if_fail(path && *path, 0);
  g_return_val_if_fail(path_len, 0);

  if (path[0] != COIL_PATH_DELIM)
    container_hash = HASH_BYTE(container_hash, COIL_PATH_DELIM);

  return hash_bytes(container_hash, (guchar *)path, path_len);
}

static inline guint
compute_size_mask(guint size)
{
  g_return_val_if_fail(size, 0);

#ifdef __GNUC__
  size = G_MAXUINT >> __builtin_clzl(size);
#else
  size |= size >> 1;
  size |= size >> 2;
  size |= size >> 4;
  size |= size >> 8;
  size |= size >> 16;
#endif

  return size | DEFAULT_MAX;
}

StructTable *
struct_table_new_sized(gsize size)
{
  g_return_val_if_fail(size, NULL);

  StructTable *t = g_new(StructTable, 1);

  /* max always 2^n-1 */
  t->max = compute_size_mask(size);
  t->bucket = g_new0(CoilStructEntry *, t->max + 1);
  t->ref_count = 1;
  t->size = 0;
  t->free = NULL;

  return t;
}

StructTable *
struct_table_new(void)
{
  return struct_table_new_sized(DEFAULT_MAX);
}

static void
struct_table_rehash(StructTable *t,
                    guint        new_max)
{
  g_return_if_fail(t);
  g_return_if_fail(new_max);

  if (t->max == new_max)
    return;

  /* dont pass garbage -- must be 2^n - 1 */
  g_assert(new_max == compute_size_mask(new_max));

  guint             n = t->max;
  CoilStructEntry **old, **new;

  old = t->bucket;
  new = g_new0(CoilStructEntry *, new_max + 1);

  do
  {
    while (*old)
    {
      guint i = (*old)->hash & new_max;
      CoilStructEntry *next = (*old)->next;
      (*old)->next = new[i];
      new[i] = *old;
      *old = next;
    }
    old++;
  } while (--n > 0);

  g_free(t->bucket);
  t->bucket = new;
  t->max = new_max;
}

guint
struct_table_get_size(const StructTable *t)
{
  g_return_val_if_fail(t, 0);

  return t->size;
}

void
struct_table_resize(StructTable *t,
                   guint         size)
{
  g_return_if_fail(t);
  g_return_if_fail(size);

  guint max = compute_size_mask(size);
  struct_table_rehash(t, max);
}

/** grow by factor of 2 */
static void
struct_table_grow(StructTable *t)
{
  g_return_if_fail(t);

  /* grow by power of 2 and prevent overflow issues */
  guint max = (t->max << 1) | t->max;
  struct_table_rehash(t, max);
}

static void
struct_table_shrink(StructTable *t)
{
  g_return_if_fail(t);

  /** shrink by factor of 2 */
  guint max = (t->max >> 1) | DEFAULT_MAX;
  struct_table_rehash(t, max);
}

static CoilStructEntry *
new_struct_entry(StructTable *t)
{
  g_return_val_if_fail(t, NULL);

  CoilStructEntry *e;

  if ((e = t->free))
    t->free = e->next;
  else
    e = g_new(CoilStructEntry, 1);

  e->next = NULL;
  return e;
}

static CoilStructEntry **
find_bucket(StructTable  *t,
            guint         hash,
            const gchar  *path,
            guint8        path_len)
{
  g_return_val_if_fail(t, NULL);
  g_return_val_if_fail(path && *path && *path == '@', NULL);
  g_return_val_if_fail(path_len, NULL);

  CoilStructEntry *e, **bp;

  for (bp = &t->bucket[hash & t->max], e = *bp;
       e; bp = &e->next, e = *bp)
  {
    const CoilPath *p = e->path;
    if (e->hash == hash
      && p->path_len == path_len
      && memcmp(p->path, path, path_len) == 0)
        break;
  }

  return bp;
}

static CoilStructEntry **
find_bucket_with_entry(StructTable     *t,
                       CoilStructEntry *entry)
{
  g_return_val_if_fail(t, NULL);
  g_return_val_if_fail(entry, NULL);

  CoilStructEntry *e, **bp;

  for (bp = &t->bucket[entry->hash & t->max], e = *bp;
       e; bp = &e->next, e = *bp)
    if (e == entry)
      break;

  return bp;
}

static void
struct_table_calibrate(StructTable *t)
{
  g_return_if_fail(t);

  if (t->size > t->max)
    struct_table_grow(t);
  else if (t->size > DEFAULT_MAX && t->size <= (t->max >> 1))
    struct_table_shrink(t);
}

void
clear_struct_entry(CoilStructEntry *e)
{
  g_return_if_fail(e);

  if (G_LIKELY(e->path))
    coil_path_unref(e->path);

  if (e->value)
    free_value(e->value);
}

CoilStructEntry *
struct_table_insert(StructTable *t,
                    guint        hash,
                    CoilPath    *path, /* steals */
                    GValue      *value) /* steals */
{
  g_return_val_if_fail(t, NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(path->flags & COIL_PATH_IS_ABSOLUTE, NULL);

  CoilStructEntry *e, **bp = find_bucket(t, hash, path->path, path->path_len);

  if (*bp == NULL)
    *bp = new_struct_entry(t);
  else
    clear_struct_entry(*bp);

  e = *bp;
  e->hash = hash;
  e->path = path;
  e->value = value;

  t->size++;
  struct_table_calibrate(t);

  return e;
}

void
struct_table_insert_entry(StructTable *t,
                          CoilStructEntry *e)
{
  g_return_if_fail(t);
  g_return_if_fail(e && e->hash && e->path);

  const CoilPath *p = e->path;
  CoilStructEntry **bp = find_bucket(t, e->hash, p->path, p->path_len);
  e->next = NULL;
  *bp = e;
  t->size++;
}

CoilStructEntry *
struct_table_lookup(StructTable *t,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len)
{
  g_return_val_if_fail(t, NULL);
  g_return_val_if_fail(path && *path && *path == '@', NULL);
  g_return_val_if_fail(path_len, NULL);

  return *find_bucket(t, hash, path, path_len);
}

CoilStructEntry *
struct_table_remove(StructTable *t,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len)
{
  g_return_val_if_fail(t, NULL);
  g_return_val_if_fail(path && *path && *path == '@', NULL);
  g_return_val_if_fail(path_len, NULL);

  CoilStructEntry **bp = find_bucket(t, hash, path, path_len);

  if (*bp)
  {
    CoilStructEntry *e = *bp;
    *bp = e->next;
    e->next = NULL;
    t->size--;
    return e;
  }

  return NULL;
}

CoilStructEntry *
struct_table_remove_entry(StructTable     *t,
                          CoilStructEntry *e)
{
  g_return_val_if_fail(t, NULL);
  g_return_val_if_fail(e, NULL);

  CoilStructEntry **bp = find_bucket_with_entry(t, e);

  if (*bp)
  {
    *bp = e->next;
    e->next = NULL;
    t->size--;
    return e;
  }

  return NULL;
}

void
struct_table_delete(StructTable *t,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len)
{
  g_return_if_fail(t);
  g_return_if_fail(path && *path && *path == '@');
  g_return_if_fail(path_len);

  CoilStructEntry *e = struct_table_remove(t, hash, path, path_len);
  clear_struct_entry(e);
  e->next = t->free;
  t->free = e;
}

void
struct_table_delete_entry(StructTable *t,
                          CoilStructEntry *e)
{
  g_return_if_fail(t);
  g_return_if_fail(e);

  struct_table_remove_entry(t, e);
  clear_struct_entry(e);
  e->next = t->free;
  t->free = e;
}

void
struct_table_destroy(StructTable *t)
{
  g_return_if_fail(t);
  g_return_if_fail(t->ref_count <= 1);

  CoilStructEntry *e, **bp = t->bucket;

  do
  {
    while (*bp)
    {
      e = (*bp)->next;
      clear_struct_entry(*bp);
      g_free(*bp);
      *bp = e;
    }
    bp++;
  } while(t->max-- > 0);
  g_free(t->bucket);

  while ((e = t->free) != NULL)
  {
    t->free = e->next;
    g_free(e);
  }
  g_free(t);
}

void
struct_table_unref(StructTable *t)
{
  g_return_if_fail(t);

  if (g_atomic_int_dec_and_test(&t->ref_count))
    struct_table_destroy(t);
}

StructTable *
struct_table_ref(StructTable *t)
{
  g_return_val_if_fail(t, NULL);

  g_atomic_int_inc(&t->ref_count);
  return t;
}
