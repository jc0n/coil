/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include <string.h>

#include "struct.h"
#include "struct_private.h"

#define DEFAULT_MAX 255 // max number of buckets (2^n - 1)

#define HASH_BYTE(hash, byte) hash = (hash * 33 + (byte))

static inline guint
hash_bytes(guint         hash,
           const guchar *byte,
           guint         n)
{
  g_return_val_if_fail(byte, 0);
  g_return_val_if_fail(*byte, 0);
  g_return_val_if_fail(n > 0, 0);

  do
  {
    HASH_BYTE(hash, *byte++);
  } while (--n > 0);

  return hash;
}

inline guint
hash_absolute_path(const gchar *path,
                   guint8       path_len)
{
  g_return_val_if_fail(path, 0);
  g_return_val_if_fail(*path == '@', 0); /* must be absolute */
  g_return_val_if_fail(path_len > 0, 0);

  path += COIL_ROOT_PATH_LEN;
  path_len -= COIL_ROOT_PATH_LEN;

  if (path_len > 0)
    return hash_bytes(0, (guchar *)path, path_len);

  return 0;
}

inline guint
hash_relative_path(guint        container_hash,
                   const gchar *path,
                   guint8       path_len)
{
  g_return_val_if_fail(path, 0);
  g_return_val_if_fail(*path, 0);
  g_return_val_if_fail(path_len > 0, 0);

  if (path[0] != COIL_PATH_DELIM)
    HASH_BYTE(container_hash, COIL_PATH_DELIM);

  return hash_bytes(container_hash, (guchar *)path, path_len);
}

/**
 * Compute next highest power of 2 minus 1
 *
 * Satisfies requirement for table max size to be 2^n - 1
 */
static guint
compute_real_max(guint size)
{
  g_return_val_if_fail(size > 0, 0);

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
  g_return_val_if_fail(size > 0, NULL);

  StructTable *table = g_new(StructTable, 1);

  /* max always 2^n-1 */
  table->max = compute_real_max(size);
  table->bucket = g_new0(CoilStructEntry *, table->max + 1);
  table->ref_count = 1;
  table->size = 0;
  table->free = NULL;

  return table;
}

StructTable *
struct_table_new(void)
{
  return struct_table_new_sized(DEFAULT_MAX);
}

static void
struct_table_rehash(StructTable *table,
                    guint        max)
{
  g_return_if_fail(table);
  g_return_if_fail(max > 0);
  g_return_if_fail(max == compute_real_max(max));

  if (table->max == max)
    return;

  CoilStructEntry **new = g_new0(CoilStructEntry *, max + 1);

  if (table->size > 0)
  {
    guint            n;
    CoilStructEntry *entry, *next, **old;

    for (n = table->max, old = &table->bucket[n];
         n-- > 0; old = &table->bucket[n])
    {
      for (entry = *old;
           entry; entry = next)
      {
        guint idx = entry->hash & max;

        next = entry->next;
        entry->next = new[idx];
        new[idx] = entry;
      }
    }
  }

  g_free(table->bucket);
  table->bucket = new;
  table->max = max;
}

guint
struct_table_get_size(const StructTable *table)
{
  g_return_val_if_fail(table, 0);

  return table->size;
}

void
struct_table_resize(StructTable *table,
                   guint         size)
{
  g_return_if_fail(table);
  g_return_if_fail(size > 0);

  guint max;

  max = compute_real_max(size);
  struct_table_rehash(table, max);
}

/** grow by factor of 2 */
static void
struct_table_grow(StructTable *table)
{
  g_return_if_fail(table);

  guint max;

  /* grow by power of 2 and prevent overflow issues */
  max = (table->max << 1) | table->max;
  struct_table_rehash(table, max);
}

static void
struct_table_shrink(StructTable *table)
{
  g_return_if_fail(table);

  guint max;

  /** shrink by factor of 2 */
  max = (table->max >> 1) | DEFAULT_MAX;
  struct_table_rehash(table, max);
}

static CoilStructEntry *
new_entry(StructTable *table)
{
  g_return_val_if_fail(table, NULL);

  CoilStructEntry *entry = table->free;

  /* XXX: new entry memory is not zero'd */

  if (entry)
    table->free = entry->next;
  else
    entry = g_new(CoilStructEntry, 1);

  entry->next = NULL;

  return entry;
}

static CoilStructEntry **
find_bucket(StructTable  *table,
            guint         hash,
            const gchar  *path,
            guint8        path_len)
{
  g_return_val_if_fail(table, NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(*path == '@', NULL);
  g_return_val_if_fail(path_len > 0, NULL);

  CoilStructEntry *e, **bp;

  for (bp = &table->bucket[hash & table->max], e = *bp;
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
find_bucket_with_entry(StructTable     *table,
                       CoilStructEntry *entry)
{
  g_return_val_if_fail(table, NULL);
  g_return_val_if_fail(entry, NULL);

  CoilStructEntry *e, **ep;

  for (ep = &table->bucket[entry->hash & table->max], e = *ep;
       e; ep = &e->next, e = *ep)
  {
    if (e == entry)
      break;
  }

  return ep;
}

static void
struct_table_calibrate(StructTable *table)
{
  g_return_if_fail(table);

  if (table->size > table->max)
    struct_table_grow(table);
  else if (table->size <= table->max >> 1
      && table->size > DEFAULT_MAX)
    struct_table_shrink(table);
}

void
clear_struct_entry(CoilStructEntry *entry)
{
  g_return_if_fail(entry);

  if (G_LIKELY(entry->path))
    coil_path_unref(entry->path);

  if (entry->value)
    free_value(entry->value);
}

CoilStructEntry *
struct_table_insert(StructTable *table,
                    guint        hash,
                    CoilPath    *path, /* steals */
                    GValue      *value) /* steals */
{
  g_return_val_if_fail(table, NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(path->flags & COIL_PATH_IS_ABSOLUTE, NULL);

  CoilStructEntry *entry, **ep;

  ep = find_bucket(table, hash, path->path, path->path_len);

  if (*ep == NULL)
    *ep = new_entry(table);
  else
    clear_struct_entry(*ep);

  entry = *ep;
  entry->hash = hash;
  entry->path = path;
  entry->value = value;

  table->size++;
  struct_table_calibrate(table);

  return entry;
}

void
struct_table_insert_entry(StructTable     *table,
                          CoilStructEntry *entry)
{
  g_return_if_fail(table);
  g_return_if_fail(entry);
  g_return_if_fail(entry->hash);
  g_return_if_fail(entry->path);

  CoilStructEntry **ep;

  ep = find_bucket(table,
                   entry->hash,
                   entry->path->path,
                   entry->path->path_len);

  entry->next = NULL;
  *ep = entry;

  table->size++;
}

CoilStructEntry *
struct_table_lookup(StructTable *table,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len)
{
  g_return_val_if_fail(table, NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(*path == '@', NULL);
  g_return_val_if_fail(path_len > 0, NULL);

  return *find_bucket(table, hash, path, path_len);
}

CoilStructEntry *
struct_table_remove(StructTable *table,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len)
{
  g_return_val_if_fail(table, NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(*path == '@', NULL);
  g_return_val_if_fail(path_len > 0, NULL);

  CoilStructEntry *entry, **ep;

  ep = find_bucket(table, hash, path, path_len);
  entry = *ep;

  if (entry)
  {
    *ep = entry->next;
    entry->next = NULL;
    table->size--;

    return entry;
  }

  return NULL;
}

CoilStructEntry *
struct_table_remove_entry(StructTable     *table,
                          CoilStructEntry *entry)
{
  g_return_val_if_fail(table, NULL);
  g_return_val_if_fail(entry, NULL);

  CoilStructEntry **ep;

  ep = find_bucket_with_entry(table, entry);

  /* TODO(jcon): de-duplicate code */
  if (*ep)
  {
    g_assert(entry == *ep);

    *ep = entry->next;
    entry->next = NULL;
    table->size--;

    return entry;
  }

  return NULL;
}

void
struct_table_delete(StructTable *table,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len)
{
  g_return_if_fail(table);
  g_return_if_fail(path);
  g_return_if_fail(*path == '@');
  g_return_if_fail(path_len);

  CoilStructEntry *entry;

  entry = struct_table_remove(table, hash, path, path_len);
  clear_struct_entry(entry);

  entry->next = table->free;
  table->free = entry;
}

void
struct_table_delete_entry(StructTable     *table,
                          CoilStructEntry *entry)
{
  g_return_if_fail(table);
  g_return_if_fail(entry);

  struct_table_remove_entry(table, entry);
  clear_struct_entry(entry);

  entry->next = table->free;
  table->free = entry;
}

void
struct_table_destroy(StructTable *table)
{
  g_return_if_fail(table);
  g_return_if_fail(table->ref_count <= 1);

  guint             n;
  CoilStructEntry **bucket, *entry, *next;

  if (table->size > 0)
  {
    /* clear entries in all buckets */
    for (n = table->max, bucket = &table->bucket[n];
         n-- > 0; bucket = &table->bucket[n])
    {
      for (entry = *bucket; entry; entry = next)
      {
        next = entry->next;
        clear_struct_entry(entry);
        g_free(entry);
      }
    }
  }

  /* clear free list */
  for (entry = table->free; entry; entry = next)
  {
    next = entry->next;
    g_free(entry);
  }

  g_free(table->bucket);
  g_free(table);
}

void
struct_table_unref(StructTable *table)
{
  g_return_if_fail(table);

  if (g_atomic_int_dec_and_test(&table->ref_count))
    struct_table_destroy(table);
}

StructTable *
struct_table_ref(StructTable *table)
{
  g_return_val_if_fail(table, NULL);

  g_atomic_int_inc(&table->ref_count);

  return table;
}
