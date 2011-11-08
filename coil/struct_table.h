/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#ifndef __COIL_STRUCT_PRIVATE
#define __COIL_STRUCT_PRIVATE

#include "path.h"

typedef struct _StructEntry StructEntry;
typedef struct _StructTable StructTable;

struct _StructTable
{
    guint         max;
    guint         size;

    volatile gint ref_count;

    StructEntry **bucket;
};

struct _StructEntry
{
    guint        hash;
    CoilPath    *path;
    GValue      *value;

    /* next in bucket */
    StructEntry *next;
};

G_BEGIN_DECLS

guint
hash_relative_path(guint        container_hash,
                   const gchar *path,
                   guint8       path_len);

guint
hash_absolute_path(const gchar *path,
                   guint8       path_len);

StructTable *
struct_table_new_sized(gsize size);

StructTable *
struct_table_new(void);

guint
struct_table_get_size(const StructTable *table);

void
struct_table_resize(StructTable *table,
                    guint        size);

StructEntry *
struct_table_insert(StructTable *table,
                    guint        hash,
                    CoilPath    *path,
                    GValue      *value);


void
struct_table_insert_entry(StructTable *table,
                          StructEntry *entry);


StructEntry *
struct_table_lookup(StructTable *table,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len);

gboolean
struct_table_lookup_full(StructTable  *table,
                         guint         hash,
                         const gchar  *path,
                         guint8        path_len,
                         StructEntry **entry);

StructEntry *
struct_table_remove(StructTable *table,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len);

StructEntry *
struct_table_remove_entry(StructTable *table,
                          StructEntry *entry);

void
struct_table_delete(StructTable *table,
                    guint        hash,
                    const gchar *path,
                    guint8       path_len);

void
struct_table_delete_entry(StructTable *table,
                          StructEntry *entry);

void
struct_table_destroy(StructTable *table);

void
struct_table_unref(StructTable *table);

StructTable *
struct_table_ref(StructTable *table);

G_END_DECLS
#endif
