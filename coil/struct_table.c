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
#include "common.h"

#include "struct.h"
#include "struct_table.h"

#define DEFAULT_MAX 255 // max number of buckets (2^n - 1)

/**
 * Compute next highest power of 2 minus 1
 *
 * Satisfies requirement for table max size to be 2^n - 1
 */
static guint
size_mask(guint size)
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

    StructTable *table;

    table = g_new(StructTable, 1);
    table->max = size_mask(size); /* max always (2^n)-1 */
    table->bucket = g_new0(StructEntry *, table->max + 1);
    table->ref_count = 1;
    table->size = 0;

    return table;
}

StructTable *
struct_table_new(void)
{
    return struct_table_new_sized(DEFAULT_MAX);
}

static void
do_resize(StructTable *table, guint max)
{
    g_return_if_fail(table);
    g_return_if_fail(max > 0);
    g_return_if_fail(max == size_mask(max));

    guint n;
    StructEntry *entry, *next, **old, **new;

    if (table->max == max) {
        return;
    }
    new = g_new0(StructEntry *, max + 1);
    if (table->size > 0) {
        for (n = table->max, old = &table->bucket[n];
             n-- > 0; old = &table->bucket[n]) {
            for (entry = *old; entry; entry = next) {
                guint idx, hash;

                hash = coil_path_get_hash(entry->path);
                idx = hash & max;

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
struct_table_resize(StructTable *table, guint size)
{
    g_return_if_fail(table);
    g_return_if_fail(size > 0);

    guint max = size_mask(size);
    do_resize(table, max);
}

/** grow by factor of 2 */
static void
table_grow(StructTable *table)
{
    g_return_if_fail(table);

    guint max;

    /* grow by power of 2 and prevent overflow issues */
    max = (table->max << 1) | table->max;
    do_resize(table, max);
}

static void
table_shrink(StructTable *table)
{
    g_return_if_fail(table);

    guint max;

    /** shrink by factor of 2 */
    max = (table->max >> 1) | DEFAULT_MAX;
    do_resize(table, max);
}

static StructEntry *
alloc_entry(StructTable *table)
{
    g_return_val_if_fail(table, NULL);

    StructEntry *entry;

    entry = g_slice_new(StructEntry);
    entry->next = NULL;

    return entry;
}

static void
clear_entry(StructEntry *entry)
{
    g_return_if_fail(entry);

    if (G_LIKELY(entry->path)) {
        coil_path_unref(entry->path);
        entry->path = NULL;
    }
    if (entry->value) {
        coil_value_free(entry->value);
        entry->value = NULL;
    }
    entry->next = NULL;
}

static void
destroy_entry(StructEntry *entry)
{
    g_return_if_fail(entry);

    clear_entry(entry);
    g_slice_free(StructEntry, entry);
}

static StructEntry **
find_bucket(StructTable *table, CoilPath *path)
{
    g_return_val_if_fail(table, NULL);
    g_return_val_if_fail(path, NULL);

    StructEntry *entry, **bucket;
    guint hash, idx;

    hash = coil_path_get_hash(path);
    idx = hash & table->max;

    for (bucket = &table->bucket[idx], entry = *bucket; entry;
            bucket = &entry->next, entry = *bucket) {
        if (coil_path_equal(entry->path, path)) {
            break;
        }
    }
    return bucket;
}

static StructEntry **
find_bucket_with_entry(StructTable *table, StructEntry *entry)
{
    g_return_val_if_fail(table, NULL);
    g_return_val_if_fail(entry, NULL);

    StructEntry *e, **bucket;
    guint idx, hash;

    hash = coil_path_get_hash(entry->path);
    idx = hash & table->max;

    for (bucket = &table->bucket[idx], e = *bucket; e;
            bucket = &e->next, e = *bucket) {
        if (e == entry) {
            break;
        }
    }
    return bucket;
}

static void
table_recalibrate(StructTable *table)
{
    g_return_if_fail(table);

    if (table->size > table->max) {
        table_grow(table);
    }
    else if (table->size <= table->max >> 1 && table->size > DEFAULT_MAX) {
        table_shrink(table);
    }
}

/* XXX: steals value (this should change) */
StructEntry *
struct_table_insert(StructTable *table, CoilPath *path, GValue *value)
{
    g_return_val_if_fail(table, NULL);
    g_return_val_if_fail(path, NULL);
    g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(path), NULL);

    StructEntry *entry, **bucket;

    bucket = find_bucket(table, path);
    if (*bucket == NULL) {
        *bucket = alloc_entry(table);
    }
    else {
        clear_entry(*bucket);
    }
    entry = *bucket;
    entry->path = coil_path_ref(path);
    entry->value = value;

    table->size++;
    table_recalibrate(table);
    return entry;
}

void
struct_table_insert_entry(StructTable *table, StructEntry *entry)
{
    g_return_if_fail(table);
    g_return_if_fail(entry);
    g_return_if_fail(entry->path);

    StructEntry **bucket;

    bucket = find_bucket(table, entry->path);

    entry->next = NULL;
    *bucket = entry;
    table->size++;
}

StructEntry *
struct_table_lookup(StructTable *table, CoilPath *path)
{
    g_return_val_if_fail(table, NULL);
    g_return_val_if_fail(path, NULL);

    return *find_bucket(table, path);
}

static StructEntry *
remove_bucket_entry(StructTable *table, StructEntry **bucket)
{
    g_return_val_if_fail(table, NULL);
    g_return_val_if_fail(bucket, NULL);

    StructEntry *entry = *bucket;

    if (entry) {
        *bucket = entry->next;
        entry->next = NULL;
        table->size--;
        return entry;
    }
    return NULL;
}

StructEntry *
struct_table_remove(StructTable *table, CoilPath *path)
{
    g_return_val_if_fail(table, NULL);
    g_return_val_if_fail(path, NULL);

    StructEntry **bucket = find_bucket(table, path);

    return remove_bucket_entry(table, bucket);
}

StructEntry *
struct_table_remove_entry(StructTable *table, StructEntry *entry)
{
    g_return_val_if_fail(table, NULL);
    g_return_val_if_fail(entry, NULL);

    StructEntry **bucket = find_bucket_with_entry(table, entry);

    return remove_bucket_entry(table, bucket);
}

void
struct_table_delete(StructTable *table, CoilPath *path)
{
    g_return_if_fail(table);
    g_return_if_fail(path);

    StructEntry *entry = struct_table_remove(table, path);

    destroy_entry(entry);
}

void
struct_table_delete_entry(StructTable *table, StructEntry *entry)
{
    g_return_if_fail(table);
    g_return_if_fail(entry);

    struct_table_remove_entry(table, entry);
    destroy_entry(entry);
}

void
struct_table_destroy(StructTable *table)
{
    g_return_if_fail(table);
    g_return_if_fail(table->ref_count <= 1);

    guint n;
    StructEntry **bucket, *entry, *next;

    /* clear entries in all buckets */
    if (table->size > 0) {
        for (n = table->max, bucket = &table->bucket[n]; n-- > 0;
                bucket = &table->bucket[n]) {
            for (entry = *bucket; entry; entry = next) {
                next = entry->next;
                destroy_entry(entry);
            }
        }
    }
    g_free(table->bucket);
    g_free(table);
}

void
struct_table_unref(StructTable *table)
{
    g_return_if_fail(table);

    if (g_atomic_int_dec_and_test(&table->ref_count)) {
        struct_table_destroy(table);
    }
}

StructTable *
struct_table_ref(StructTable *table)
{
    g_return_val_if_fail(table, NULL);

    g_atomic_int_inc(&table->ref_count);
    return table;
}
