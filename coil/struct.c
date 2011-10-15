/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"
#include "struct.h"
#include "struct_table.h"
#include "struct-private.h"
#include "link.h"
#include "list.h"
#include "include.h"

G_DEFINE_TYPE(CoilStruct, coil_struct, COIL_TYPE_OBJECT);

#define COIL_STRUCT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), COIL_TYPE_STRUCT, CoilStructPrivate))

struct _CoilStructPrivate
{
    StructTable *table;

    GQueue *entries;
    GQueue *dependencies;
    GList *unexpanded;

    guint size;

#if COIL_DEBUG
    guint version;
#endif

    gboolean is_prototype : 1;
    gboolean is_accumulating : 1;
};

typedef struct _ExpandNotify
{
    CoilObject *object;
    gulong handler_id;
} ExpandNotify;

typedef enum
{
    PROP_0,
    /* */
    PROP_IS_PROTOTYPE,
    PROP_LAST,
} StructProperties;

static GParamSpec *properties[PROP_LAST];

typedef enum
{
    CREATE,
    MODIFY,
    /*  DESTROY,*/
    /* TODO(jcon): INSERT */
    /* TODO(jcon): DELETE */
    /* TODO(jcon): EXPAND */
    ADD_DEPENDENCY,
    /* */
    LAST_SIGNAL
} StructSignals;

static guint
struct_signals[LAST_SIGNAL] = {0, };

static gboolean
iter_next_entry(CoilStructIter *iter, StructEntry **entry);

static void
struct_expand_notify(GObject *instance, gpointer data);

static void
prototype_cast_notify(GObject *instance, GParamSpec *arg1, gpointer data);

static void
struct_connect_expand_notify(CoilObject *self, CoilObject *parent);

static gboolean
struct_change_notify(CoilObject *self);

static gboolean
do_delete(CoilObject *o, CoilPath *path, gboolean strict, gboolean reset);

static CoilObject *
do_container_lookup(CoilObject *o, CoilPath *path);

static const GValue *
do_lookup(CoilObject *self, CoilPath *path, gboolean expand_value,
        gboolean expand_container);

static void
struct_build_string(CoilObject *self, GString *buffer, CoilStringFormat *format);

#if COIL_STRICT_CONTEXT
#define STRUCT_MERGE_STRICT(s, d) \
    coil_struct_merge_full(s, d, FALSE, TRUE)
#else
#define STRUCT_MERGE_STRICT(s, d) \
    coil_struct_merge(s, d)
#endif

/* really stupid hack to store root in a
 * gvalue without creating a reference cycle */
static void
destroy_root_entry(gpointer data, GObject *object, gboolean last_reference)
{
    g_return_if_fail(data);
    g_return_if_fail(COIL_IS_STRUCT(object));

    if (last_reference) {
        CoilStruct *self = COIL_STRUCT(object);

        g_object_ref(object);
        g_object_remove_toggle_ref(object, destroy_root_entry, data);
        struct_table_delete_entry(self->priv->table, (StructEntry *)data);
    }
}

static StructEntry *
lookup_root_entry(StructTable *table)
{
    StructEntry *res;

    res = struct_table_lookup(table, CoilRootPath);
    if (res == NULL) {
        g_error("No root entry in struct table.");
    }
    return res;
}

/* does not copy struct entries */
static void
become_root_struct(CoilObject *self)
{
    g_return_if_fail(COIL_IS_STRUCT(self));

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    GValue *value;
    StructEntry *entry;

    coil_object_set_container(self, NULL);
    coil_object_set_path(self, CoilRootPath);

    if (priv->table) {
        struct_table_unref(priv->table);
    }
    priv->table = struct_table_new();

    coil_value_init(value, COIL_TYPE_STRUCT, take_object, self);
    entry = struct_table_insert(priv->table, coil_path_ref(CoilRootPath), value);
    g_object_add_toggle_ref(G_OBJECT(self), destroy_root_entry, entry);
}

/**
 * change_container:
 *
 * Change the container of a struct.
 *
 * A few things happen here:
 *
 * 1) If the current struct is still in the entry table of the
 * prior container and `reset` is true, then we need to remove it.
 *
 * Note: If we're setting the new container to NULL, we make self @root
 *
 * 2) We need to update the path and hash of the struct and all entries.
 * 3) Minimize recomputing hash values and copying paths.
 *
 * @self: coil struct instance
 * @container: new container struct instance
 * @path: new struct path (if available)
 * @hash: new struct hash (if available)
 * @reset: whether or not to clear self from old container
 */
static gboolean
change_container(CoilObject *object, CoilObject *container,
                 CoilPath *path, gboolean reset)
{
    g_return_val_if_fail(COIL_IS_STRUCT(object), FALSE);
    g_return_val_if_fail(COIL_IS_STRUCT(container), FALSE);
    g_return_val_if_fail(object != container, FALSE);

    CoilStructPrivate *priv = COIL_STRUCT(object)->priv;
    CoilStructIter it;
    StructTable *old_table = priv->table;
    StructEntry *entry;

    /* remove struct from previous container */
    if (reset && object->container != NULL) {
        guint ref_count = coil_object_get_refcount(object);
        if (ref_count == 1) {
            g_error("Removing struct from previous container will"
                    "cause last struct reference to be removed");
        }
        if (!do_delete(object->container, object->path, FALSE, FALSE)) {
            goto error;
        }
    }
    if (container != NULL) {
        /* struct is NOT going to be changed to root */
        if (coil_struct_is_root(object)) {
            /* struct WAS root, delete root entry, the rest is handled below */
            entry = lookup_root_entry(priv->table);
            destroy_root_entry(entry, G_OBJECT(object), TRUE);
            entry = NULL;
        }
        if (path == NULL) {
            /* no path specified, build a new one */
            path = coil_path_join(container->path, object->path);
            if (path == NULL) {
                goto error;
            }
        }
        coil_object_set_path(object, path);
        coil_object_set_container(object, container);
        priv->table = struct_table_ref(COIL_STRUCT(container)->priv->table);
    }
    else {
        struct_table_ref(old_table);
        become_root_struct(object);
    }
    /* iterate through key-values and update paths entry table */
    coil_struct_iter_init(&it, object);
    while (iter_next_entry(&it, &entry)) {
        /* XXX: Each entry must be removed from the hash table because the hash
         * changes with the path. The path is changed, then the entry is inserted
         * back into the hash table.
         */

        struct_table_remove_entry(old_table, entry);

        path = coil_path_join(object->path, entry->path);
        if (path == NULL) {
            goto error;
        }
        coil_path_unref(entry->path);
        entry->path = path;

        struct_table_insert_entry(priv->table, entry);

        /* update containers and paths for entry objects */
        if (entry->value && G_VALUE_HOLDS(entry->value, COIL_TYPE_OBJECT)) {
            CoilObject *entryobj = COIL_OBJECT(g_value_get_object(entry->value));
            if (COIL_IS_STRUCT(entryobj) &&
                    !change_container(entryobj, object, entry->path, TRUE)) {
                goto error;
            }
            else {
                coil_object_set_path(entryobj, entry->path);
                coil_object_set_container(entryobj, object);
            }
        }
    }
    struct_table_unref(old_table);
    return TRUE;

error:
    if (old_table)
        struct_table_unref(old_table);

    return FALSE;
}

COIL_API(GQuark)
coil_struct_prototype_quark(void)
{
    static GQuark result = 0;

    if (G_UNLIKELY(!result)) {
        result = g_quark_from_static_string("prototype");
    }

    return result;
}

/**
 * coil_struct_empty:
 * @o: #CoilStruct instance
 *
 * Clears all entries in a struct
 */
COIL_API(void)
coil_struct_empty(CoilObject *o)
{
    g_return_if_fail(COIL_IS_STRUCT(o));

    CoilStructPrivate *priv = COIL_STRUCT(o)->priv;
    CoilObject *depobj;
    StructEntry *entry;

    while ((depobj = g_queue_pop_head(priv->dependencies))) {
        coil_object_unref(depobj);
    }
    while ((entry = g_queue_pop_head(priv->entries))) {
        struct_table_delete_entry(priv->table, entry);
    }
    priv->size = 0;
#if COIL_DEBUG
    priv->version = 0;
#endif
}

/**
 * coil_struct_is_root:
 * @o: #CoilStruct instance
 * @Returns: %TRUE if @o is a root struct
 *
 * Returns %TRUE if @o is a root struct
 */
COIL_API(gboolean)
coil_struct_is_root(CoilObject *o)
{
    g_return_val_if_fail(COIL_IS_STRUCT(o), FALSE);

    return o->root == o;
}

/**
 * coil_struct_is_prototype
 * @o: #CoilStruct instance
 * @Returns: %TRUE if @o is a prototype struct
 *
 * Returns %TRUE if @o is a prototype struct
 */
COIL_API(gboolean)
coil_struct_is_prototype(CoilObject *o)
{
    g_return_val_if_fail(COIL_IS_STRUCT(o), FALSE);

    return COIL_STRUCT(o)->priv->is_prototype;
}

static gboolean
struct_needs_expand(CoilObject *o)
{
    g_return_val_if_fail(COIL_IS_STRUCT(o), FALSE);

    CoilStruct *self = COIL_STRUCT(o);
    CoilStructPrivate *priv = self->priv;

    if (g_queue_is_empty(priv->dependencies)) {
        return FALSE;
    }
    return priv->unexpanded != NULL;
}

static gboolean
struct_expand(CoilObject *self)
{
    return coil_object_expand(self, NULL, FALSE);
}

/**
 * coil_struct_is_empty:
 * @o: #CoilStruct instance
 * @Returns: %TRUE if struct @o is empty
 *
 * Returns %TRUE if struct @o is empty
 */
COIL_API(gboolean)
coil_struct_is_empty(CoilObject *o)
{
    g_return_val_if_fail(COIL_IS_STRUCT(o), FALSE);

    CoilStructPrivate *priv = COIL_STRUCT(o)->priv;

    if (priv->size > 0) {
        return FALSE;
    }
    if (!struct_needs_expand(o) && !struct_expand(o)) {
        return FALSE;
    }
    return priv->size == 0;
}

/**
 * coil_struct_is_ancestor:
 * @a: #CoilStruct instance
 * @b: #CoilStruct instance
 * @Returns: %TRUE if @a is an ancestor of @b
 *
 * If @a is an ancestor of @b, it means that @a contains @b,
 * or contains another struct which contains @b.
 *
 * Returns %TRUE if @a is an ancestor of @b
 */
COIL_API(gboolean)
coil_struct_is_ancestor(CoilObject *a, CoilObject *b)
{
    g_return_val_if_fail(COIL_IS_STRUCT(a), FALSE);
    g_return_val_if_fail(COIL_IS_STRUCT(b), FALSE);
    g_return_val_if_fail(a != b, FALSE);

    CoilObject *container = b;

    while ((container = container->container) != NULL) {
        if (container == a) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * coil_struct_is_descendent:
 * @a: #CoilStruct instance
 * @b: #CoilStruct instance
 * @Returns: %TRUE if @a is a descendent of @b
 *
 * If @a is a descendent of @b, it implies that @b contains @a or
 * contains another struct which contains @a.
 *
 * Returns %TRUE if @a is a descendent of @b
 */
COIL_API(gboolean)
coil_struct_is_descendent(CoilObject *a, CoilObject *b)
{
    g_return_val_if_fail(COIL_IS_STRUCT(a), FALSE);
    g_return_val_if_fail(COIL_IS_STRUCT(b), FALSE);
    g_return_val_if_fail(a != b, FALSE);

    return coil_struct_is_ancestor(b, a);
}

COIL_API(void)
coil_struct_foreach_ancestor(CoilObject *o,
                             gboolean include_self,
                             CoilStructFunc func,
                             gpointer user_data)
{
    g_return_if_fail(COIL_IS_STRUCT(o));
    g_return_if_fail(func);

    CoilObject *c;
    gboolean keep_going = TRUE;

    if (include_self) {
        keep_going = (*func)(o, user_data);
    }
    c = o->container;
    while (keep_going && c != NULL) {
        keep_going = (*func)(c, user_data);
        c = c->container;
    }
}

void
coil_struct_set_prototype(CoilObject *self, gboolean prototype)
{
    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;

    priv->is_prototype = prototype;
#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 6
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_IS_PROTOTYPE]);
#else
    g_object_notify(G_OBJECT(self), properties[PROP_IS_PROTOTYPE]->name);
#endif
}

void
coil_struct_set_accumulate(CoilObject *self, gboolean accumulate)
{
    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;

    priv->is_accumulating = accumulate;
}

gboolean
finalize_prototype(CoilObject *self, gpointer unused)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);

    if (coil_struct_is_prototype(self)) {
        coil_struct_set_prototype(self, FALSE);
        return TRUE;
    }
    return FALSE;
}

static gboolean
override_entry(CoilObject *o, StructEntry *entry, CoilPath *path,
               GValue *value, /* steals */
               gboolean replace)
{
    /* entry exists for path (value may be null) */
    if (entry->value != NULL && G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT)) {
        CoilObject *dst = COIL_OBJECT(g_value_get_object(entry->value));

        if (coil_struct_is_prototype(dst)) {
            CoilObject *src;

            if (!G_VALUE_HOLDS(value, COIL_TYPE_STRUCT)) {
                /* XXX: old value is struct prototype, new value is not struct */
                coil_struct_error(o,
                        "Attempting to overwrite struct prototype with non-struct value."
                        "This implies struct is referenced but never defined.");
                return FALSE;
            }
            src = COIL_OBJECT(g_value_get_object(value));
            /* XXX: Overwriting a prototype.
             * Merge the items from struct we're trying to set now
             * and destroy it (leaving the prototype in place but casting to
             * non-prototype).
             */
            if (!coil_struct_merge_full(src, dst, TRUE, FALSE)) {
                return FALSE;
            }
            if (value) {
                coil_value_free(value);
            }
            return TRUE;
        }
    }
    else if (!replace) {
        coil_struct_error(o,
                "Attempting to insert value at path '%s' "
                "which already exists.",
                path->str);
        return FALSE;
    }
    /* entry exists, overwrite value */
    if (entry->value) {
        coil_value_free(entry->value);
    }
    entry->value = value;

    if (entry->path) {
        coil_path_unref(entry->path);
    }
    entry->path = coil_path_ref(path);

    return TRUE;
}

static gboolean
insert_internal(CoilObject *self, CoilPath *path,
                GValue *value, /* steals */
                gboolean replace, gboolean check_exists)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(path, FALSE);
    g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(path), FALSE);
    g_return_val_if_fail(!COIL_PATH_IS_ROOT(path), FALSE);

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    StructEntry *entry = NULL;
    gboolean value_is_struct = FALSE;

    if (!struct_change_notify(self)) {
        goto error;
    }
    /* TODO(jcon): dont check value for struct more than once */
    if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT)) {
        CoilObject *valueobj = COIL_OBJECT(g_value_get_object(value));
        if (coil_struct_is_ancestor(valueobj, self)) {
            coil_struct_error(self,
                    "Attempting to insert ancestor struct '%s' into '%s'.",
                    valueobj->path->str, self->path->str);
            goto error;
        }
        value_is_struct = TRUE;
    }

    /* XXX: add option to skip this lookup */
    if (check_exists) {
        entry = struct_table_lookup(priv->table, path);
    }
    if (entry == NULL) {
        entry = struct_table_insert(priv->table, path, value);
        g_queue_push_tail(priv->entries, entry);
        priv->size++;
        if (!struct_change_notify(self)) {
            goto error;
        }
    }
    else if (!override_entry(self, entry, path, value, replace)) {
        goto error;
    }
#if COIL_DEBUG
    priv->version++;
#endif
    /* XXX: if value is struct, path will change based on container. */
    /* TODO(jcon): implement set_container in object */
    if (value_is_struct) {
        CoilObject *valueobj = COIL_OBJECT(g_value_get_object(value));

        if (valueobj->container != self &&
            !change_container(valueobj, self, path, TRUE)) {
            goto error;
        }
        /* XXX: inserting a prototype does not provoke a cast */
        if (!coil_struct_is_prototype(valueobj)) {
            coil_struct_foreach_ancestor(self, TRUE, finalize_prototype, NULL);
        }
        return TRUE;
    }

    if (G_VALUE_HOLDS(value, COIL_TYPE_OBJECT)) {
        CoilObject *valueobj = COIL_OBJECT(g_value_get_object(value));
        coil_object_set_container(valueobj, self);
        /* XXX: maybe set path here */
    }
    else if (G_VALUE_HOLDS(value, COIL_TYPE_LIST)) {
        guint i;
        GValue *item;
        GValueArray *arr;
        /* update containers for expandable nested in lists */
        arr = (GValueArray *)g_value_get_boxed(value);
        for (i = 0; i < arr->n_values; i++) {
            item = g_value_array_get_nth(arr, i);
            if (G_VALUE_HOLDS(item, COIL_TYPE_OBJECT)) {
                CoilObject *o = (CoilObject *)g_value_get_object(item);
                coil_object_set_container(o, self);
                /* XXX: maybe set path here */
            }
        }
    }

    coil_struct_foreach_ancestor(self, TRUE, finalize_prototype, NULL);
    return TRUE;

error:
    if (value) {
        coil_value_free(value);
    }
    return FALSE;
}

/* light weight struct allocator without all the extra checks */
static CoilObject *
struct_alloc(CoilObject *container, CoilPath *path, gboolean prototype)
{
    g_return_val_if_fail(container, NULL);
    g_return_val_if_fail(path, NULL);

    CoilObject *self = COIL_OBJECT(g_object_new(COIL_TYPE_STRUCT, NULL));
    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    GValue *value;

    self->container = container;
    self->root = container->root;
    self->path = coil_path_ref(path);

    priv->is_prototype = prototype;
    priv->table = struct_table_ref(COIL_STRUCT(container)->priv->table);

    coil_value_init(value, COIL_TYPE_STRUCT, set_object, self);
    if (!insert_internal(container, self->path, value, TRUE, FALSE)) {
        coil_object_unref(self);
        return NULL;
    }
    g_signal_emit(self, struct_signals[CREATE],
            prototype ? coil_struct_prototype_quark() : 0);

    return self;
}

/* Leave this private, but available to parser */
CoilObject *
_coil_create_containers(CoilObject *self, CoilPath *path,
        gboolean prototype, gboolean has_lookup)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
    g_return_val_if_fail(path, NULL);

    CoilObject *container;
    CoilPath *container_path;
    CoilStructPrivate *priv;
    GPtrArray *missing = NULL;

    if (COIL_PATH_IS_ROOT(path)) {
        return self->root;
    }
    container = self->root;
    container_path = coil_path_ref(path);
    missing = g_ptr_array_new_with_free_func((GDestroyNotify)coil_path_unref);
    if (has_lookup) {
        /* first key was already searched, skip */
        g_ptr_array_add(missing, container_path);
        container_path = coil_path_pop(container_path, -1);
        if (container_path == NULL) {
            goto err;
        }
    }
    priv = COIL_STRUCT(self)->priv;
    while (!COIL_PATH_IS_ROOT(container_path)) {
        StructEntry *entry;

        entry = struct_table_lookup(priv->table, container_path);
        if (entry != NULL) {
            if (entry->value == NULL) {
                coil_struct_error(self,
                    "Attempting to insert value for a previously deleted key %s",
                    container_path->str);
                goto err;
            }
            if (!G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT)) {
                coil_struct_error(self,
                    "Attempting to assign value in non-struct object %s type '%s'.",
                    container_path->str, G_VALUE_TYPE_NAME(entry->value));
                goto err;
            }
            container = COIL_OBJECT(g_value_get_object(entry->value));
            break;
        }
        g_ptr_array_add(missing, container_path);
        container_path = coil_path_pop(container_path, -1);
        if (container_path == NULL) {
            goto err;
        }
    }
    coil_path_unref(container_path);
    if (!prototype && !coil_struct_is_root(container)) {
        coil_struct_foreach_ancestor(container, TRUE, finalize_prototype, NULL);
    }
    if (missing->len > 0) {
        guint n = missing->len;
        while (n-- > 0) {
            container_path = (CoilPath *)g_ptr_array_index(missing, n);
            container = struct_alloc(container, container_path, prototype);
        }
    }
    g_ptr_array_free(missing, TRUE);
    return container;

err:
    if (missing) {
        g_ptr_array_free(missing, TRUE);
    }
    if (container_path) {
        coil_path_unref(container_path);
    }
    return NULL;
}

COIL_API(gboolean)
coil_struct_insert_path(CoilObject *self, CoilPath *path,
                        GValue *value, /* steals */ /*XXX: change when possible */
                        gboolean replace)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(path, FALSE);
    g_return_val_if_fail(value, FALSE);

    CoilObject *container;
    CoilPath *container_path;
    gboolean res;

    coil_path_ref(path);
    if (!coil_path_resolve_inplace(&path, self->path)) {
        goto error;
    }
    if (COIL_PATH_IS_ROOT(path)) {
        coil_struct_error(self, "Cannot assign a value directly to @root.");
        goto error;
    }
    container_path = coil_path_pop(path, -1);
    if (container_path == NULL) {
        goto error;
    }
    container = _coil_create_containers(self, container_path, TRUE, FALSE);
    coil_path_unref(container_path);
    if (container == NULL) {
        goto error;
    }
    res = insert_internal(container, path, value, replace, TRUE);
    coil_path_unref(path);
    return res;
error:
    if (path) {
        coil_path_unref(path);
    }
    if (value) {
        coil_value_free(value);
    }
    return FALSE;
}

COIL_API(gboolean)
coil_struct_insert(CoilObject *self, const gchar *str, guint len,
        GValue *value, gboolean replace)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(str, FALSE);
    g_return_val_if_fail(len > 0, FALSE);
    g_return_val_if_fail(value, FALSE);

    CoilPath *path;
    gboolean res;

    path = coil_path_new_len(str, len);
    if (path == NULL) {
        return FALSE;
    }
    res = coil_struct_insert_path(self, path, value, replace);
    coil_path_unref(path);
    return res;
}

static gboolean
struct_remove_entry(CoilObject *self, StructEntry *entry)
{
    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;

    g_queue_remove(priv->entries, (gpointer)entry);
#if COIL_DEBUG
    priv->version++;
#endif
    priv->size--;
    return TRUE;
}

static gboolean
struct_delete_entry(CoilObject *self, StructEntry *entry)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(entry, FALSE);

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;

    if (!struct_remove_entry(self, entry)) {
        return FALSE;
    }

    struct_table_delete_entry(priv->table, entry);
    return TRUE;
}

static gboolean
do_delete(CoilObject *self, CoilPath *path,
        gboolean strict, gboolean reset)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(!coil_struct_is_prototype(self), FALSE);
    g_return_val_if_fail(path, FALSE);

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    CoilObject *container;
    StructEntry *entry;

    entry = struct_table_lookup(priv->table, path);
    if (entry == NULL) {
        if (strict) {
            coil_struct_error(self,
                    "Attempting to delete non-existent path '%s'.", path->str);
            return FALSE;
        }
        return TRUE;
    }
    /* TODO: when/if values become CoilObjects we should store
     * the container with each value */
    container = do_container_lookup(self, path);
    if (container == NULL) {
        return FALSE;
    }
    if (reset && G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT)) {
        CoilObject *entryobj = COIL_OBJECT(g_value_get_object(entry->value));
        if (!change_container(entryobj, NULL, NULL, FALSE)) {
            return FALSE;
        }
    }
    return struct_delete_entry(container, entry);
}

COIL_API(gboolean)
coil_struct_delete(CoilObject *self, const gchar *str, guint len,
        gboolean strict)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(str, FALSE);
    g_return_val_if_fail(len > 0, FALSE);

    CoilPath *path;
    gboolean res = FALSE;

    path = coil_path_new_len(str, len);
    if (path == NULL) {
        return FALSE;
    }
    if (!coil_path_resolve_inplace(&path, self->path)) {
        goto err;
    }
    if (COIL_PATH_IS_ROOT(path)) {
        coil_struct_error(self, "Cannot delete '@root' path.");
        goto err;
    }
    res = do_delete(self, path, strict, TRUE);
err:
    coil_path_unref(path);
    return res;
}

COIL_API(gboolean)
coil_struct_delete_path(CoilObject *self, CoilPath *path, gboolean strict)
{
    if (!coil_path_resolve_inplace(&path, self->path))
        return FALSE;
    if (COIL_PATH_IS_ROOT(path)) {
        coil_struct_error(self, "Cannot delete '@root' path.");
        return FALSE;
    }
    return do_delete(self, path, strict, TRUE);
}

static gboolean
mark_deleted(CoilObject *self, CoilPath *path, gboolean force)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(path, FALSE);
    g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(path), FALSE);

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    StructEntry *entry = struct_table_lookup(priv->table, path);

    if (entry && !force) {
        const gchar *msg;
        if (entry->value == NULL) {
            msg = "Attempting to delete '%s' twice.";
        }
        else {
            msg = "Attempting to delete first-order key '%s'.";
        }
        coil_struct_error(self, msg, path->key);
        return FALSE;
    }
    struct_table_insert(priv->table, path, NULL);
#if COIL_DEBUG
    priv->version++;
#endif
    return TRUE;
}

/* private to parser */
gboolean
coil_struct_mark_deleted_path(CoilObject *self, CoilPath *path,
        gboolean force)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(path, FALSE);

    CoilObject *container;
    CoilPath *container_path;
    gboolean res = FALSE;

    coil_path_ref(path);
    if (!coil_path_resolve_inplace(&path, self->path)) {
        goto err;
    }
    if (COIL_PATH_IS_ROOT(path)) {
        coil_struct_error(self, "Cannot mark @root as deleted.");
        goto err;
    }
    container_path = coil_path_pop(path, -1);
    if (container_path == NULL) {
        goto err;
    }
    container = _coil_create_containers(self, container_path, TRUE, FALSE);
    coil_path_unref(container_path);
    if (container == NULL) {
        goto err;
    }
    res = mark_deleted(container, path, force);
err:
    coil_path_unref(path);
    return res;
}

static gboolean
check_parent_sanity(CoilObject *self, CoilObject *parent)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(COIL_IS_STRUCT(parent), FALSE);

    if (self == parent) {
        coil_struct_error(self, "cannot extend self.");
        return FALSE;
    }
    if (coil_struct_is_root(parent)) {
        coil_struct_error(self, "cannot be extended.");
        return FALSE;
    }
    if (coil_struct_is_ancestor(parent, self)) {
        coil_struct_error(self, "cannot inherit from parent containers.");
        return FALSE;
    }
    if (coil_struct_is_descendent(parent, self)) {
        coil_struct_error(self, "cannot inherit from children.");
        return FALSE;
    }
    if (self->root != parent->root) {
        coil_struct_error(self, "cannot inherit outside of root.");
        return FALSE;
    }
    return TRUE;
}

static gboolean
struct_check_dependency_type(GType type)
{
    return g_type_is_a(type, COIL_TYPE_STRUCT)
        || g_type_is_a(type, COIL_TYPE_LINK)
        || g_type_is_a(type, COIL_TYPE_INCLUDE);
}

gboolean
coil_struct_add_dependency(CoilObject *self, CoilObject *dep)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(COIL_IS_OBJECT(dep), FALSE);
    g_return_val_if_fail(self != dep, FALSE);

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    GType type = G_OBJECT_TYPE(dep);

    if (coil_struct_is_root(self) && COIL_IS_STRUCT(dep)) {
        coil_struct_error(self, "cannot inherit from other structs.");
        return FALSE;
    }
    if (struct_needs_expand(self)) {
        coil_struct_error(self, "Cannot add dependencies after struct expansion");
        return FALSE;
    }
    if (!struct_check_dependency_type(type)) {
        g_error("Adding invalid dependency type '%s'.",
                G_OBJECT_TYPE_NAME(dep));
    }

    g_signal_emit(G_OBJECT(self), struct_signals[ADD_DEPENDENCY],
            g_type_qname(type), dep);

    if (coil_error_occurred()) {
        return FALSE;
    }
    if (COIL_IS_STRUCT(dep)) {
        if (G_UNLIKELY(!check_parent_sanity(self, dep))) {
            return FALSE;
        }
        struct_connect_expand_notify(self, dep);
    }
    else if (self->root != dep->root) {
        coil_struct_error(self, "Cannot add dependency in a different @root.",
                self->path->str);
        return FALSE;
    }
    coil_object_ref(dep);
    g_queue_push_tail(priv->dependencies, dep);
#if COIL_DEBUG
    priv->version++;
#endif
    if (coil_struct_is_prototype(self)) {
        coil_struct_set_prototype(self, FALSE);
    }
    return TRUE;
}

static void
expand_notify_free(gpointer data, GClosure *closure)
{
    g_return_if_fail(data);
    g_free(data);
}

/**
 * @instance - parent struct
 * @data - expand notify
 */
void
struct_expand_notify(GObject *instance, gpointer data)
{
    g_return_if_fail(COIL_IS_STRUCT(instance));
    g_return_if_fail(data);

    ExpandNotify *notify = (ExpandNotify *)data;
    CoilObject *self = notify->object;

    g_return_if_fail(COIL_IS_STRUCT(self));

    g_signal_handler_disconnect(instance, notify->handler_id);
    /* notify = NULL */

    struct_expand(self);
}

static void
prototype_cast_notify(GObject *instance, /* parent */
                      GParamSpec *unused, /* ignore argument */
                      gpointer data) /* ExpandNotify */
{
    g_return_if_fail(COIL_IS_STRUCT(instance));
    g_return_if_fail(data);

    ExpandNotify *notify = (ExpandNotify *)data;
    CoilObject *self = notify->object;
    CoilObject *parent = COIL_OBJECT(instance);

    if (!coil_struct_is_prototype(parent)) {
        g_signal_handler_disconnect(instance, notify->handler_id);
        notify = g_new(ExpandNotify, 1);
        notify->object = self;
        notify->handler_id = g_signal_connect_data(instance, "modify",
                G_CALLBACK(struct_expand_notify),
                notify, expand_notify_free, 0);
    }
}

static void
struct_connect_expand_notify(CoilObject *self, CoilObject *parent)
{
    g_return_if_fail(COIL_IS_STRUCT(self));
    g_return_if_fail(COIL_IS_STRUCT(parent));

    ExpandNotify *notify = g_new(ExpandNotify, 1);
    const gchar *detailed_signal;
    GCallback callback;

    notify->object = self;

    if (coil_struct_is_prototype(parent)) {
        detailed_signal = "notify::is-prototype";
        callback = G_CALLBACK(prototype_cast_notify);
    }
    else {
        detailed_signal = "modify";
        callback = G_CALLBACK(struct_expand_notify);
    }
    notify->handler_id = g_signal_connect_data(parent, detailed_signal,
            callback, notify, expand_notify_free, 0);
}

gboolean
coil_struct_extend_path(CoilObject *self, CoilPath *path,
        CoilObject *context)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(path, FALSE);
    g_return_val_if_fail(context == NULL || COIL_IS_STRUCT(context), FALSE);

    CoilObject *dependency;
    const GValue *value;

    if (context == NULL) {
        context = self;
    }
    path = coil_path_resolve(path, context->path);
    if (path == NULL) {
        goto err;
    }
    value = do_lookup(context, path, FALSE, FALSE);
    if (value) {
        GType type = G_VALUE_TYPE(value);

        if (!struct_check_dependency_type(type)) {
            coil_struct_error(self,
                    "parent '%s' must be a struct, found type '%s'.",
                    path->str, g_type_name(type));
            goto err;
        }
        dependency = COIL_OBJECT(g_value_get_object(value));
    }
    else {
        if (coil_error_occurred()) {
            goto err;
        }
        dependency = _coil_create_containers(context, path, TRUE, TRUE);
        if (dependency == NULL) {
            goto err;
        }
    }
    coil_path_unref(path);
    return coil_struct_add_dependency(self, dependency);

err:
    if (path) {
        coil_path_unref(path);
    }
    return FALSE;
}

gboolean
coil_struct_extend_paths(CoilObject *self, GList *list, CoilObject *context)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(list, FALSE);
    g_return_val_if_fail(context == NULL || COIL_IS_STRUCT(context), FALSE);

    while (list) {
        CoilPath *path = (CoilPath *)list->data;

        if (!coil_struct_extend_path(self, path, context)) {
            return FALSE;
        }
        list = g_list_next(list);
    }
    return TRUE;
}

static void
iter_init(CoilStructIter *iter, CoilObject *self, gboolean reversed)
{
    g_return_if_fail(COIL_IS_STRUCT(self));
    g_return_if_fail(!coil_struct_is_prototype(self));
    g_return_if_fail(iter);

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;

#if COIL_DEBUG
    iter->version = priv->version;
#endif
    iter->node = self;
    iter->reversed = reversed;
    if (iter->reversed)
        iter->position = g_queue_peek_tail_link(priv->entries);
    else
        iter->position = g_queue_peek_head_link(priv->entries);
}

COIL_API(void)
coil_struct_iter_init(CoilStructIter *iter, CoilObject *self)
{
    iter_init(iter, self, FALSE);
}

COIL_API(void)
coil_struct_iter_init_reversed(CoilStructIter *iter, CoilObject *self)
{
    iter_init(iter, self, TRUE);
}

static gboolean
iter_next_entry(CoilStructIter *iter, StructEntry **entry)
{
    g_return_val_if_fail(!iter->reversed, FALSE);

    if (!iter->position) {
        return FALSE;
    }
    *entry = (StructEntry *)iter->position->data;
    iter->position = g_list_next(iter->position);
    return TRUE;
}

static gboolean
iter_prev_entry(CoilStructIter *iter, StructEntry **entry)
{
    g_return_val_if_fail(iter->reversed, FALSE);

    if (!iter->position) {
        return FALSE;
    }
    *entry = (StructEntry *)iter->position->data;
    iter->position = g_list_previous(iter->position);
    return TRUE;
}

COIL_API(gboolean)
coil_struct_iter_next(CoilStructIter *iter, CoilPath **path, const GValue **value)
{
    g_return_val_if_fail(iter, FALSE);
    g_return_val_if_fail(path || value, FALSE);

    StructEntry *entry;
    gboolean res;

    if (iter->reversed)
        res = iter_prev_entry(iter, &entry);
    else
        res = iter_next_entry(iter, &entry);

    if (!res)
        return FALSE;

    if (path)
        *path = entry->path;

    if (value)
        *value = entry->value;

    return TRUE;
}

/* Call before making implicit or hidden changes in struct to notify ancestry
 * XXX: this should probably hook into the MODIFY signal */
static gboolean
struct_change_notify(CoilObject *self)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;

    if (priv->is_accumulating) {
        return TRUE;
    }
    g_signal_emit(self, struct_signals[MODIFY], 0);
    if (coil_error_occurred()) {
        return FALSE;
    }
    if (self->container == NULL) {
        return TRUE;
    }
    return struct_change_notify(self->container);
}

static gboolean
merge_item(CoilObject *self, CoilPath *item_path, const GValue *item_value,
        gboolean overwrite, gboolean force_expand)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(item_path, FALSE);
    g_return_val_if_fail(G_IS_VALUE(item_value), FALSE);

    CoilObject *src, *dst;
    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    StructEntry *entry;
    CoilPath *path = NULL;
    GValue *value;

    path = coil_path_join(self->path, item_path);
    if (path == NULL) {
        goto error;
    }
    entry = struct_table_lookup(priv->table, path);
    if (entry && !overwrite) {
        /* merge values if old and new entries are structs */
        if (entry->value &&
            G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT) &&
            G_VALUE_HOLDS(item_value, COIL_TYPE_STRUCT)) {

            src = COIL_OBJECT(g_value_get_object(item_value));
            dst = COIL_OBJECT(g_value_get_object(entry->value));
            if (!coil_struct_merge_full(src, dst, overwrite, force_expand)) {
                goto error;
            }
            if (coil_struct_is_prototype(dst)) {
                coil_struct_foreach_ancestor(dst, TRUE, finalize_prototype, NULL);
            }
        }
        coil_path_unref(path);
        /* entry exists, dont overwrite
         * includes keys marked deleted */
        return TRUE;
    }
    if (G_VALUE_HOLDS(item_value, COIL_TYPE_STRUCT)) {
        src = COIL_OBJECT(g_value_get_object(item_value));
        dst = struct_alloc(self, path, FALSE);
        if (!coil_struct_merge_full(src, dst, overwrite, force_expand)) {
            goto error;
        }
        coil_value_init(value, COIL_TYPE_STRUCT, take_object, dst);
    }
    else if (force_expand && G_VALUE_HOLDS(item_value, COIL_TYPE_OBJECT)) {
        const GValue *real_value = NULL;

        if (!struct_change_notify(self)) {
            goto error;
        }
        if (!coil_expand_value(item_value, &real_value, TRUE)) {
            goto error;
        }
        if (real_value == NULL) {
            g_error("Expecting return value from expansion of %s '%s'",
                    G_VALUE_TYPE_NAME(item_value), path->str);
        }
        value = coil_value_copy(real_value);
    }
    else if (G_VALUE_HOLDS(item_value, COIL_TYPE_OBJECT)) {
        src = COIL_OBJECT(g_value_get_object(item_value));
        dst = coil_object_copy(src,
            "container", self,
            "path", path,
            "location", self->location,
            NULL);
        if (dst == NULL) {
            goto error;
        }
        coil_value_init(value, G_VALUE_TYPE(item_value), take_object, dst);
    }
    else {
        value = coil_value_copy(item_value);
    }

    if (!insert_internal(self, path, value, TRUE, entry == NULL)) {
        goto error;
    }
    coil_path_unref(path);
    return TRUE;

error:
    if (path) {
        coil_path_unref(path);
    }
    return FALSE;
}

COIL_API(gboolean)
coil_struct_merge_full(CoilObject *src, CoilObject *dst,
        gboolean overwrite, gboolean force_expand)
{
    g_return_val_if_fail(COIL_IS_STRUCT(src), FALSE);
    g_return_val_if_fail(COIL_IS_STRUCT(dst), FALSE);
    g_return_val_if_fail(src != dst, FALSE);

    CoilStructIter it;
    CoilPath *path;
    const GValue *value;

    if (!struct_expand(src)) {
        return FALSE;
    }
    coil_struct_set_accumulate(dst, TRUE);
    coil_struct_iter_init(&it, src);
    while (coil_struct_iter_next(&it, &path, &value)) {
        if (!merge_item(dst, path, value, overwrite, force_expand)) {
            coil_struct_set_accumulate(dst, FALSE);
            return FALSE;
        }
    }
    coil_struct_set_accumulate(dst, FALSE);
    return TRUE;
}

COIL_API(gboolean)
coil_struct_merge(CoilObject *src, CoilObject *dst)
{
    return coil_struct_merge_full(src, dst, FALSE, FALSE);
}

static gboolean
expand_dependency(CoilObject *self, CoilObject *dependency)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(COIL_IS_OBJECT(dependency), FALSE);

    const GValue *expanded_value = NULL;

    if (!coil_object_expand(dependency, &expanded_value, TRUE)) {
        return FALSE;
    }
    /* structs are sanity checked before being added as a dependency */
    if (!COIL_IS_STRUCT(dependency)) {
        /* dependency handled its own expansion
         * there is no expanded value to merge */
        if (expanded_value == NULL) {
            return TRUE;
        }
        if (!G_VALUE_HOLDS(expanded_value, COIL_TYPE_STRUCT)) {
            coil_struct_error(self,
                    "Invalid struct dependency type '%s', expecting '%s' type.",
                    G_VALUE_TYPE_NAME(expanded_value),
                    g_type_name(COIL_TYPE_STRUCT));
            return FALSE;
        }
        dependency = COIL_OBJECT(g_value_get_object(expanded_value));
        if (COIL_IS_LINK(dependency) &&
            !check_parent_sanity(self, dependency)) {
            return FALSE;
        }
    }
    else if (coil_struct_is_root(self)) {
        coil_struct_error(self, "cannot inherit from other structs.");
        return FALSE;
    }
    if (coil_struct_is_prototype(dependency)) {
        coil_struct_error(self, "dependency struct '%s' is still a prototype"
                "(referenced but never defined).",
                dependency->path->str);
        return FALSE;
    }
    if (!STRUCT_MERGE_STRICT(dependency, self)) {
        return FALSE;
    }
    /* if the parent changes after now we dont care */
    g_signal_handlers_disconnect_matched(dependency,
            G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
            struct_signals[MODIFY], 0, NULL, G_CALLBACK(struct_expand_notify),
            self);

    return TRUE;
}

static gboolean
struct_expand_internal(CoilObject *self, const GValue **return_value)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    CoilObject *dependency;
    GList *list;

    g_return_val_if_fail(!priv->is_prototype, FALSE);

    if (struct_needs_expand(self)) {
        return TRUE;
    }
    if (priv->unexpanded == NULL) {
        list = g_queue_peek_head_link(priv->dependencies);
    }
    else {
        list = g_list_next(priv->unexpanded);
    }

    while (list) {
        dependency = COIL_OBJECT(list->data);
        priv->unexpanded = list;
        if (!expand_dependency(self, dependency)) {
            return FALSE;
        }
        list = g_list_next(list);
    }
#if COIL_DEBUG
    priv->version++;
#endif
    return TRUE;
}


gboolean
coil_struct_expand_items(CoilObject *self, gboolean recursive)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);

    CoilStructIter it;
    const GValue *value;

    if (!struct_expand(self)) {
        return FALSE;
    }

    coil_struct_iter_init(&it, self);
    while (coil_struct_iter_next(&it, NULL, &value)) {
        if (G_VALUE_HOLDS(value, COIL_TYPE_OBJECT)) {
            CoilObject *object = COIL_OBJECT(g_value_get_object(value));

            if (self->container && !struct_change_notify(self->container)) {
                return FALSE;
            }
            if (!coil_object_expand(object, NULL, TRUE)) {
                return FALSE;
            }
            if (recursive && COIL_IS_STRUCT(object) &&
                    !coil_struct_expand_items(object, TRUE)) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

static const GValue *
maybe_expand_value(CoilObject *self, const GValue *value)
{
    g_return_val_if_fail(value, NULL);

    if (G_VALUE_HOLDS(value, COIL_TYPE_OBJECT)) {
        if (!struct_change_notify(self)) {
            return NULL;
        }
        if (!coil_expand_value(value, &value, TRUE)) {
            return NULL;
        }
    }
    return value;
}

/**
 * Find the CoilStruct which directly contains @path
 */
static CoilObject *
do_container_lookup(CoilObject *self, CoilPath *path)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
    g_return_val_if_fail(path, NULL);

    CoilPath *container_path;
    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    const StructEntry *entry;

    container_path = coil_path_pop(path, -1);
    if (container_path == NULL) {
        return NULL;
    }
    if (COIL_PATH_IS_ROOT(container_path)) {
        coil_path_unref(container_path);
        return self->root;
    }
    entry = struct_table_lookup(priv->table, container_path);
    coil_path_unref(container_path);
    if (entry == NULL || entry->value == NULL) {
        return NULL;
    }
    if (!G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT)) {
        coil_struct_error(self, "Attempting to lookup value in entry '%s'"
                " which is not a struct. Entry is of type '%s'.",
                path->str, G_VALUE_TYPE_NAME(entry->value));
        return NULL;
    }
    return COIL_OBJECT(g_value_get_object(entry->value));
}

static gboolean
do_lookup_expand(CoilObject *self, CoilPath *path)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
    g_return_val_if_fail(path, FALSE);

    CoilObject *container;
    CoilPath *container_path = coil_path_ref(path);
    const GValue *val;

    do {
        if (!coil_path_pop_inplace(&container_path, -1)) {
            goto err;
        }
        val = do_lookup(self, container_path, FALSE, FALSE);
        if (val != NULL) {
            if (G_VALUE_HOLDS(val, COIL_TYPE_LINK)) {
                if (!coil_expand_value(val, &val, TRUE)) {
                    goto err;
                }
            }
            if (!G_VALUE_HOLDS(val, COIL_TYPE_STRUCT)) {
                coil_struct_error(self,
                        "item at '%s' is type %s, expected type %s",
                        container_path->str, G_VALUE_TYPE_NAME(val),
                        g_type_name(COIL_TYPE_STRUCT));
                goto err;
            }
            container = COIL_OBJECT(g_value_get_object(val));
            coil_struct_foreach_ancestor(container, TRUE,
                    (CoilStructFunc)struct_expand, NULL);
            if (coil_error_occurred()) {
                goto err;
            }
            break;
        }
    } while (!COIL_PATH_IS_ROOT(container_path));
    coil_path_unref(container_path);
    if (!struct_expand(self->root)) {
        goto err;
    }
    return TRUE;

err:
   coil_path_unref(container_path);
   return FALSE;
}

static const GValue *
do_lookup(CoilObject *self, CoilPath *path,
        gboolean expand_value, gboolean expand_container)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
    g_return_val_if_fail(path, NULL);

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    StructEntry *entry;
    const GValue *result;

    entry = struct_table_lookup(priv->table, path);
    if (entry == NULL) {
        if (!expand_container) {
            return NULL;
        }
        if (!do_lookup_expand(self, path)) {
            return NULL;
        }
        entry = struct_table_lookup(priv->table, path);
        result = (entry) ? entry->value : NULL;
    }
    else {
        result = entry->value;
    }

    if (expand_value && result) {
        return maybe_expand_value(self, result);
    }
    return result;
}

COIL_API(const GValue *)
coil_struct_lookupx(CoilObject *self, CoilPath *path,
        gboolean expand_value)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
    g_return_val_if_fail(path, NULL);

    const GValue *res;

    path = coil_path_resolve(path, self->path);
    if (path == NULL) {
        return FALSE;
    }
    res = do_lookup(self, path, expand_value, TRUE);
    coil_path_unref(path);
    return res;
}

COIL_API(const GValue *)
coil_struct_lookup(CoilObject *self, const gchar *str, guint len,
                   gboolean expand_value)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
    g_return_val_if_fail(!coil_struct_is_prototype(self), NULL);
    g_return_val_if_fail(str, NULL);
    g_return_val_if_fail(len > 0, NULL);

    const GValue *res;
    CoilPath *path = coil_path_new_len(str, len);

    if (path == NULL) {
        return FALSE;
    }
    res = coil_struct_lookupx(self, path, expand_value);
    coil_path_unref(path);
    return res;
}

COIL_API(GList *)
coil_struct_get_paths(CoilObject *self)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);

    CoilStructIter it;
    CoilPath *path;
    GQueue q = G_QUEUE_INIT;

    if (!struct_expand(self)) {
        return NULL;
    }

    coil_struct_iter_init(&it, self);
    while (coil_struct_iter_next(&it, &path, NULL)) {
        g_queue_push_tail(&q, (gpointer)path);
    }
    return g_queue_peek_head_link(&q);
}

COIL_API(GList *)
coil_struct_get_values(CoilObject *self)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);

    CoilStructIter it;
    const GValue *value;
    GQueue q = G_QUEUE_INIT;

    if (!struct_expand(self)) {
        return NULL;
    }

    coil_struct_iter_init(&it, self);
    while (coil_struct_iter_next(&it, NULL, &value)) {
        g_queue_push_tail(&q, (gpointer)value);
    }
    return g_queue_peek_head_link(&q);
}

/* TODO(jcon): allow de-duplicating branches */
COIL_API(GNode *)
coil_struct_dependency_treev(CoilObject *self, GNode *tree, guint ntypes,
        GType *allowed_types)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);

    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;
    CoilStructIter it;
    StructEntry *entry;
    GList *list;

    if (tree == NULL) {
        tree = g_node_new(NULL);
    }

    list = g_queue_peek_head_link(priv->dependencies);

    while (list) {
        GNode *branch;
        CoilObject *object = COIL_OBJECT(list->data);

        if (ntypes > 0) {
            guint i;
            for (i = 0; i < ntypes; i++)
                if (G_TYPE_CHECK_INSTANCE_TYPE(object, allowed_types[i]))
                    goto found;

            goto next;
        }
found:
        branch = g_node_append_data(tree, object);

        /* TODO(jcon): consider implementing dependency protocol
         * for CoilObjects */
        if (COIL_IS_STRUCT(object)) {
            coil_struct_dependency_treev(object, branch, ntypes,
                    allowed_types);
            if (coil_error_occurred()) {
                return NULL;
            }
        }
        else if (COIL_IS_INCLUDE(object)) {
            CoilObject *namespace;

            /* TODO(jcon): encapsulate this in include dependency protocol */
            if (!coil_object_expand(object, NULL, FALSE)) {
                return NULL;
            }
            coil_object_get(object, "namespace", &namespace, NULL);
            coil_struct_dependency_treev(namespace, branch, ntypes, allowed_types);
            if (coil_error_occurred()) {
                return NULL;
            }
        }
next:
        list = g_list_next(list);
    }

    coil_struct_iter_init(&it, self);
    while (iter_next_entry(&it, &entry)) {
        if (G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT)) {
            CoilObject *node = COIL_OBJECT(g_value_get_object(entry->value));
            coil_struct_dependency_treev(node, tree, ntypes, allowed_types);
            if (coil_error_occurred()) {
                return NULL;
            }
        }
    }
    return tree;
}

COIL_API(GNode *)
coil_struct_dependency_tree_valist(CoilObject *self, guint ntypes, va_list args)
{
    GType *allowed_types;
    GNode *result;

    if (ntypes > 0) {
        guint i;
        allowed_types = g_new(GType, ntypes);
        for (i = 0; i < ntypes; i++) {
            allowed_types[i] = va_arg(args, GType);
        }
    }
    else {
        allowed_types = NULL;
    }
    result = coil_struct_dependency_treev(self, NULL, ntypes, allowed_types);
    g_free(allowed_types);
    return result;
}

/**
 * Varargs are GType, ...
 */
COIL_API(GNode *)
coil_struct_dependency_tree(CoilObject *self, guint ntypes, ...)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);

    GNode *result;
    va_list args;

    va_start(args, ntypes);
    result = coil_struct_dependency_tree_valist(self, ntypes, args);
    va_end(args);

    return result;
}

COIL_API(gint)
coil_struct_get_size(CoilObject *self)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), -1);

    if (!struct_needs_expand(self) && !struct_expand(self)) {
        return -1;
    }
    return COIL_STRUCT(self)->priv->size;
}

/* Only call from struct_build_string() */
static void
build_flat_string(CoilObject *self, GString *buffer, CoilStringFormat *format)
{
    g_return_if_fail(COIL_IS_STRUCT(self));
    g_return_if_fail(buffer);
    g_return_if_fail(format);
    g_return_if_fail(format->context);
    g_return_if_fail(!coil_struct_is_prototype(self));

    CoilStructIter it;
    const GValue *value;
    CoilPath *path, *context_path = format->context->path;
    guint path_offset = context_path->len + 1;

    coil_struct_iter_init(&it, self);
    while (coil_struct_iter_next(&it, &path, &value)) {
        if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT)) {
            CoilObject *node = COIL_OBJECT(g_value_get_object(value));
            struct_build_string(node, buffer, format);
        }
        else {
            g_string_append_printf(buffer, "%s: ", path->str + path_offset);
            coil_value_build_string(value, buffer, format);
            g_string_append_c(buffer, '\n');
        }
        if (coil_error_occurred()) {
            return;
        }
    }
}

/* Only call from struct_build_string() */
static void
build_scoped_string(CoilObject *self, GString *buffer, CoilStringFormat *format)
{
    g_return_if_fail(COIL_IS_STRUCT(self));
    g_return_if_fail(buffer);
    g_return_if_fail(format);
    g_return_if_fail(!coil_struct_is_prototype(self));

    CoilStructIter  it;
    CoilPath *path;
    const GValue *value;
    gboolean brace_on_blank_line, with_braces;
    gchar *newline_after_brace, *newline_after_struct;
    gchar indent[128], brace_indent[128];
    gint indent_len, brace_indent_len;

    if (format->context != self && !coil_struct_is_root(self))
        with_braces = TRUE;
    else
        with_braces = FALSE;

    brace_on_blank_line = (format->options & BRACE_ON_BLANK_LINE);
    newline_after_brace = (format->options & BLANK_LINE_AFTER_BRACE) ? "\n" : "";
    newline_after_struct = (format->options & BLANK_LINE_AFTER_STRUCT) ? "\n" : "";

    indent_len = MIN(format->indent_level, sizeof(indent));
    memset(indent, ' ', indent_len);
    indent[indent_len] = '\0';

    brace_indent_len = MIN(format->brace_indent, sizeof(brace_indent));
    memset(brace_indent, ' ', brace_indent_len);
    brace_indent[brace_indent_len] = '\0';

    format->indent_level += format->block_indent;

    if (with_braces) {
        if (brace_on_blank_line) {
            g_string_append_printf(buffer, "\n%s%s{%s",
                    indent, brace_indent, newline_after_brace);
        }
        else {
            g_string_append_printf(buffer, "%s{%s",
                    brace_indent, newline_after_brace);
        }
    }

    coil_struct_iter_init(&it, self);
    while (coil_struct_iter_next(&it, &path, &value)) {
        g_string_append_printf(buffer, "%s%s: ", indent, path->key);
        coil_value_build_string(value, buffer, format);
        if (coil_error_occurred()) {
            format->indent_level -= format->block_indent;
            return;
        }
        g_string_append_c(buffer, '\n');
    }

    if (buffer->len > 0) {
        g_string_truncate(buffer, buffer->len - 1);
        if (buffer->str[buffer->len - 1] == '\n')
            g_string_truncate(buffer, buffer->len - 1);
    }

    format->indent_level -= format->block_indent;

    if (with_braces) {
        indent_len = MAX((indent_len - format->block_indent), 0);
        indent[indent_len] = '\0';
        g_string_append_printf(buffer, "\n%s%s}%s",
                indent, brace_indent, newline_after_struct);
    }
}

static void
struct_build_string(CoilObject *self, GString *buffer, CoilStringFormat *format)
{
    g_return_if_fail(COIL_IS_STRUCT(self));
    g_return_if_fail(buffer);
    g_return_if_fail(format);
    g_return_if_fail(!coil_struct_is_prototype(self));

    CoilStringFormat format_ = *format;

    if (format_.context == NULL) {
        format_.context = self;
    }

    if (!coil_struct_expand_items(self, TRUE)) {
        return;
    }

    if (format->options & FLATTEN_PATHS) {
        build_flat_string(self, buffer, &format_);
    }
    else {
        build_scoped_string(self, buffer, &format_);
    }
}

COIL_API(CoilObject *)
struct_copy_valist(CoilObject *self, const gchar *first_property_name,
        va_list properties)
{
    g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
    g_return_val_if_fail(!coil_struct_is_prototype(self), NULL);

    CoilObject *copy = coil_struct_new_valist(first_property_name, properties);
    if (copy == NULL) {
        return NULL;
    }
    if (!coil_struct_expand_items(self, TRUE)) {
        goto error;
    }
    if (!coil_struct_merge(self, copy)) {
        goto error;
    }
    return copy;

error:
    coil_object_unref(copy);
    return NULL;
}

static gint
compare_entry_key(const StructEntry *x, const StructEntry *y)
{
    g_return_val_if_fail(x, -1);
    g_return_val_if_fail(y, -1);

    CoilPath *xp = x->path;
    CoilPath *yp = y->path;

    if (x == y || xp == yp) {
        return 0;
    }
    if (xp->key_len != yp->key_len) {
        return (xp->key_len > yp->key_len) ? 1 : -1;
    }
    return memcmp(xp->key, yp->key, yp->key_len);
}

static gboolean
compare_entry(const StructEntry *x, const StructEntry *y)
{
    g_return_val_if_fail(x, FALSE);
    g_return_val_if_fail(y, FALSE);

    if (compare_entry_key(x, y) != 0)
        return FALSE;

    return coil_value_compare(x->value, y->value) == 0;

}

gboolean
struct_equals(CoilObject *x, CoilObject *y)
{
    g_return_val_if_fail(COIL_IS_STRUCT(x), FALSE);
    g_return_val_if_fail(COIL_IS_STRUCT(y), FALSE);

    CoilStructPrivate *xpriv = COIL_STRUCT(x)->priv;
    CoilStructPrivate *ypriv = COIL_STRUCT(y)->priv;
    GList *lx = NULL, *ly = NULL;

    if (x == y)
        return TRUE;

    if (coil_struct_is_descendent(x, y))
        return FALSE;

    if (coil_struct_is_descendent(y, x))
        return FALSE;

    if (!coil_struct_expand_items(x, TRUE))
        return FALSE;

    if (!coil_struct_expand_items(y, TRUE))
        return FALSE;

    if (xpriv->size != ypriv->size)
        return FALSE;

    /* Sort the keys since they are gauranteed to be first-order
     * after the expansion above. This means struct equality does not depend
     * on the order of the keys.
     */
    lx = g_queue_peek_head_link(xpriv->entries);
    ly = g_queue_peek_head_link(ypriv->entries);

    lx = g_list_sort(g_list_copy(lx), (GCompareFunc)compare_entry_key);
    ly = g_list_sort(g_list_copy(ly), (GCompareFunc)compare_entry_key);

    while (lx && ly) {
        if (!compare_entry(lx->data, ly->data)) {
            goto done;
        }
        lx = g_list_delete_link(lx, lx);
        ly = g_list_delete_link(ly, ly);
    }
    g_assert(lx == NULL && ly == NULL);
    return TRUE;

done:
    if (lx) {
        g_list_free(lx);
    }
    if (ly) {
        g_list_free(ly);
    }
    return FALSE;
}

static void
coil_struct_init(CoilStruct *self)
{
    g_return_if_fail(COIL_IS_STRUCT(self));

    CoilStructPrivate *priv = COIL_STRUCT_GET_PRIVATE(self);

    self->priv = priv;
    priv->entries = g_queue_new();
    priv->dependencies = g_queue_new();
}

COIL_API(CoilObject *)
coil_struct_new(const gchar *first_property_name, ...)
{
    va_list properties;
    CoilObject *object;

    va_start(properties, first_property_name);
    object = coil_struct_new_valist(first_property_name, properties);
    va_end(properties);

    return object;
}

COIL_API(CoilObject *)
coil_struct_new_valist(const gchar *first_property_name, va_list properties)
{
    CoilObject *self, *container;
    CoilStructPrivate *priv;
    GValue *value;

    self = COIL_OBJECT(g_object_new_valist(COIL_TYPE_STRUCT,
                first_property_name, properties));
    priv = COIL_STRUCT(self)->priv;

    if (self->path && !COIL_PATH_IS_ROOT(self->path)) {
        CoilPath *container_path;

        if (!self->container) {
            g_error("path specified with no container");
        }
        container = self->container;
        if (self->root == NULL) {
            self->root = container->root;
        }
        priv->table = struct_table_ref(COIL_STRUCT(container)->priv->table);
        if (!coil_path_has_container(self->path, container->path, FALSE)) {
            coil_struct_error(self,
                    "Invalid path '%s' for struct with container '%s'",
                    self->path->str, container->path->str);
            goto error;
        }
        container_path = coil_path_pop(self->path, -1);
        if (container_path == NULL) {
            goto error;
        }
        /* container may be an ancestor, we want the direct container */
        container = _coil_create_containers(self, container_path,
                priv->is_prototype, FALSE);
        coil_path_unref(container_path);
        if (container == NULL) {
            goto error;
        }
        /* TODO XXX: change_container() should probably fire as notify when "container"
         * changes, except here because we already know the container */
        self->container = container;
        if (!coil_path_resolve_inplace(&self->path, container->path)) {
            goto error;
        }
        /* XXX: add self to the tree now */
        coil_value_init(value, COIL_TYPE_STRUCT, set_object, self);
        if (!insert_internal(container, self->path, value, TRUE, FALSE)) {
            goto error;
        }
    }
    else if (!self->container) {
        become_root_struct(self);
    }
    else {
        g_error("container specified with no path or root path");
    }

    g_signal_emit(self, struct_signals[CREATE],
            priv->is_prototype ? coil_struct_prototype_quark() : 0);

    return self;

error:
    g_object_unref(self);
    return NULL;
}

static void
coil_struct_dispose(GObject *object)
{
    g_return_if_fail(COIL_IS_STRUCT(object));

    CoilObject *self = COIL_OBJECT(object);
    CoilStructPrivate *priv = COIL_STRUCT(self)->priv;

    coil_struct_empty(self);
    g_queue_free(priv->entries);
    g_queue_free(priv->dependencies);

    G_OBJECT_CLASS(coil_struct_parent_class)->dispose(object);
}

/*
 * coil_struct_finalize: Finalize function for CoilStruct
 */
static void
coil_struct_finalize(GObject *object)
{
    g_return_if_fail(COIL_IS_STRUCT(object));

    CoilStructPrivate *priv = COIL_STRUCT(object)->priv;

    struct_table_unref(priv->table);

    G_OBJECT_CLASS(coil_struct_parent_class)->finalize(object);
}

/*
 * coil_struct_set_property:
 *
 * Setter for #CoilStruct properties
 */
static void
coil_struct_set_property(GObject *object, guint property_id,
        const GValue *value, GParamSpec *pspec)
{
    CoilStructPrivate *priv = COIL_STRUCT(object)->priv;

    switch (property_id) {
        case PROP_IS_PROTOTYPE:
            priv->is_prototype = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

/*
 * coil_struct_get_property:
 *
 * Getter for #CoilStruct properties.
 */
static void
coil_struct_get_property(GObject *object, guint property_id,
        GValue *value, GParamSpec *pspec)
{
    CoilStructPrivate *priv = COIL_STRUCT(object)->priv;

    switch (property_id) {
        case PROP_IS_PROTOTYPE:
            g_value_set_boolean(value, priv->is_prototype);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

/*
 * coil_struct_class_init:
 * @klass: A #CoilStructClass
 *
 * Class initializer for #CoilStruct
 */
static void
coil_struct_class_init(CoilStructClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(CoilStructPrivate));

    /* GObject class methods */
    gobject_class->set_property = coil_struct_set_property;
    gobject_class->get_property = coil_struct_get_property;
    gobject_class->dispose = coil_struct_dispose;
    gobject_class->finalize = coil_struct_finalize;

    /* override object methods */
    CoilObjectClass *object_class = COIL_OBJECT_CLASS(klass);
    object_class->copy = struct_copy_valist;
    object_class->is_expanded = struct_needs_expand;
    object_class->expand = struct_expand_internal;
    object_class->equals = struct_equals;
    object_class->build_string = struct_build_string;

    properties[PROP_IS_PROTOTYPE] = g_param_spec_boolean("is-prototype",
            "Is Prototype",
            "TRUE if struct is referenced but not defined.",
            FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_property(gobject_class, PROP_IS_PROTOTYPE,
            properties[PROP_IS_PROTOTYPE]);

    struct_signals[CREATE] =
        g_signal_newv("create",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_DETAILED,
                NULL, NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0, NULL);

    struct_signals[MODIFY] =
        g_signal_newv("modify",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_NO_RECURSE,
                NULL, NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0, NULL);

    struct_signals[ADD_DEPENDENCY] =
        g_signal_new("add-dependency",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_DETAILED,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1, G_TYPE_OBJECT);
}

