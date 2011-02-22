/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#ifndef __COIL_STRUCT_PRIVATE
#define __COIL_STRUCT_PRIVATE

#include "path.h"

typedef struct _CoilStructEntry CoilStructEntry;
typedef struct _StructTable StructTable;

struct _StructTable
{
  guint   max;
  guint   size;

  volatile gint ref_count;

  CoilStructEntry **bucket;
  CoilStructEntry  *free;
} StructTable;

G_BEGIN_DECLS

guint
hash_relative_path(guint container_hash,
                   const gchar  *path,
                   guint8        path_len);

guint
hash_absolute_path(const gchar  *path,
                   guint8        path_len);

void
struct_table_prealloc(StructTable *t,
                      guint        n);

StructTable *
struct_table_new_sized(gsize size);

StructTable *
struct_table_new(void);

guint
struct_table_get_size(const StructTable *t);

void
struct_table_resize(StructTable *t,
                    guint        size);

CoilStructEntry *
struct_table_insert(StructTable *t,
                    guint        hash,
                    CoilPath    *path,
                    GValue      *value);


void
struct_table_insert_entry(StructTable *t,
                          CoilStructEntry *e);


CoilStructEntry *
struct_table_lookup(StructTable *t,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len);

gboolean
struct_table_lookup_full(StructTable  *t,
                         guint         hash,
                         const gchar  *path,
                         guint8        path_len,
                         CoilStructEntry **entry);

CoilStructEntry *
struct_table_remove(StructTable *t,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len);

CoilStructEntry *
struct_table_remove_entry(StructTable *t,
                          CoilStructEntry *e);

void
struct_table_delete(StructTable *t,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len);

void
struct_table_delete_entry(StructTable *t,
                          CoilStructEntry *e);

void
struct_table_destroy(StructTable *h);

void
struct_table_unref(StructTable *h);

StructTable *
struct_table_ref(StructTable *h);

G_END_DECLS
#endif
