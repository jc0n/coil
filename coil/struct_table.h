/*
 * Copyright (C) 2012 John O'Connor
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
