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
    guint max;
    guint size;

    volatile gint ref_count;

    StructEntry **bucket;
};

struct _StructEntry
{
    CoilPath *path;
    GValue *value;
    /* next in bucket */
    StructEntry *next;
};

G_BEGIN_DECLS

StructTable *
struct_table_new_sized(gsize size);

StructTable *
struct_table_new(void);

guint
struct_table_get_size(const StructTable *table);

void
struct_table_resize(StructTable *table, guint size);

StructEntry *
struct_table_insert(StructTable *table, CoilPath *path, GValue *value);

void
struct_table_insert_entry(StructTable *table, StructEntry *entry);

StructEntry *
struct_table_lookup(StructTable *table, CoilPath *path);

gboolean
struct_table_lookup_full(StructTable *table, CoilPath *path, StructEntry **entry);

StructEntry *
struct_table_remove(StructTable *table, CoilPath *path);

StructEntry *
struct_table_remove_entry(StructTable *table, StructEntry *entry);

void
struct_table_delete(StructTable *table, CoilPath *path);

void
struct_table_delete_entry(StructTable *table, StructEntry *entry);

void
struct_table_destroy(StructTable *table);

void
struct_table_unref(StructTable *table);

StructTable *
struct_table_ref(StructTable *table);

G_END_DECLS
#endif
