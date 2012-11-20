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

#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#include "struct.h"
#include "list.h"
#include "parser_defs.h"
#include "include.h"

/*
 * FIXME: Remove file_value property and use item[0] in imports list
 * FIXME: String formatting
 */

G_DEFINE_TYPE(CoilInclude, coil_include, COIL_TYPE_OBJECT);

#define COIL_INCLUDE_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), COIL_TYPE_INCLUDE, \
         CoilIncludePrivate))

#define IMPORT_LIST(obj) (COIL_INCLUDE((obj))->priv->imports)

static gboolean
process_import_list(CoilObject *self, CoilObject *import_list);

struct _CoilIncludePrivate
{
    CoilValue   *file_value; /* (object -> string) OR string */
    CoilObject  *imports; /* list of (object -> string) or strings */

    CoilObject  *root; /* root of the included file */
    CoilObject  *namespace;
    CoilValue   *namespace_value;

    gboolean    is_expanded : 1;
};

typedef enum
{
    PROP_0,
    PROP_FILE_VALUE,
    PROP_IMPORTS,
    PROP_NAMESPACE,
} CoilIncludeProperties;

#if COIL_STRICT_FILE_CONTEXT
  /*
   * This setting causes objects to expand in context of the file being
   * imported. This is done before merging names into the destination container.
   */
#define MERGE_NAMESPACE(ns, dst) \
    coil_struct_merge_full(ns, dst, FALSE, TRUE)

#else

#define MERGE_NAMESPACE(ns, dst) \
    coil_struct_merge(ns, dst)

#endif


/* TODO(jcon): consider moving caching into separate file and
 * explore smarter cache coherency approaches */

#if COIL_INCLUDE_CACHING

static GHashTable *include_cache = NULL;

typedef struct _CacheEntry
{
    gchar        *filepath;
    CoilObject   *root;
    time_t        m_time;
    volatile gint ref_count;
} CacheEntry;

static void
cache_init(void)
{
    if (include_cache == NULL) {
        include_cache = g_hash_table_new(g_str_hash, g_str_equal);
    }
}

static void
cache_entry_free(CacheEntry *entry)
{
    g_return_if_fail(entry);

    g_hash_table_remove(include_cache, entry->filepath);
    CLEAR(entry->root, coil_object_unref);
    g_free(entry->filepath);
    g_free(entry);
}

static void
cache_gc_notify(gpointer data, GObject *object_address)
{
    g_return_if_fail(data != NULL);

    gchar *filepath = (gchar *)data;
    CacheEntry *entry = g_hash_table_lookup(include_cache, filepath);

    if (entry && g_atomic_int_dec_and_test(&entry->ref_count)) {
        cache_entry_free(entry);
    }
}

static void
cache_save(GObject *object, const gchar *filepath, CoilObject *root)
{
    g_return_if_fail(G_IS_OBJECT(object));
    g_return_if_fail(filepath != NULL);
    g_return_if_fail(COIL_IS_STRUCT(root));

    CacheEntry *entry;
    struct stat st;

    entry = g_hash_table_lookup(include_cache, filepath);
    if (entry) {
        g_atomic_int_inc(&entry->ref_count);
    }
    else if (stat(filepath, &st) == 0) {
        entry = g_new(CacheEntry, 1);
        entry->ref_count = 1;
        entry->filepath = g_strdup(filepath);
        entry->root = coil_object_ref(root);
        entry->m_time = st.st_mtime;
        g_hash_table_insert(include_cache, entry->filepath, entry);
    }
    g_object_weak_ref(object, cache_gc_notify, entry->filepath);
}

static CoilObject *
cache_load(gpointer _notify, const gchar *filepath)
{
    g_return_val_if_fail(G_IS_OBJECT(_notify), NULL);
    g_return_val_if_fail(filepath != NULL, NULL);

    GObject *notify = G_OBJECT(_notify);
    CacheEntry *entry;
    CoilObject *root = NULL;
    struct stat st;

    entry = g_hash_table_lookup(include_cache, filepath);
    if (entry == NULL) {
        root = coil_parse_file(filepath);
        if (coil_error_occurred()) {
            goto error;
        }
        cache_save(notify, filepath, root);
        return root;
    }
    if (stat(filepath, &st) < 0) {
        coil_set_error(COIL_ERROR_INTERNAL, NULL,
                "Cannot stat include file '%s'.", filepath);
        goto error;
    }
    if (st.st_mtime != entry->m_time) {
        root = coil_parse_file(filepath);
        if (coil_error_occurred()) {
            cache_entry_free(entry);
            goto error;
        }
        coil_object_unref(root);
        entry->root = root;
        entry->m_time = st.st_mtime;
    }
    return coil_object_ref(entry->root);

error:
    CLEAR(root, coil_object_unref);
    return NULL;
}
#define CACHE_INIT() cache_init()

#define CACHE_LOAD(notify, path) \
    cache_load(notify, path)

#else

#define CACHE_INIT()
#define CACHE_LOAD(notify, path) coil_parse_file(path)

#endif

static gboolean
include_is_expanded(CoilObject *object)
{
    CoilInclude *self = COIL_INCLUDE(object);
    return self->priv->is_expanded;
}

/* search for the filename in multiple paths */
static gchar *
find_include_path(CoilObject *self, const gchar *filename)
{
    gchar *dirpath, *path, *origpath;
    guint flags = G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR;

    origpath = self->location.filepath;
    if (g_path_is_absolute(filename)) {
        if (origpath != NULL && strcmp(filename, origpath) == 0) {
            coil_include_error(self, "a file should not import itself");
            return NULL;
        }
        return g_strdup(filename);
    }
    if (origpath != NULL) {
        dirpath = g_dirname(origpath);
        path = g_build_filename(dirpath, filename, NULL);
        g_free(dirpath);
        if (g_file_test(path, flags)) {
            return path;
        }
        g_free(path);
    }

    dirpath = g_get_current_dir();
    path = g_build_filename(dirpath, filename, NULL);
    g_free(dirpath);
    if (g_file_test(path, flags)) {
        return path;
    }
    g_free(path);

    dirpath = (gchar *)g_get_home_dir();
    path = g_build_filename(dirpath, filename, NULL);
    g_free(dirpath);
    if (g_file_test(path, flags)) {
        return path;
    }
    g_free(path);

    coil_include_error(self, "include file path '%s' not found.", filename);
    return NULL;
}

static const gchar *
expand_file_value(CoilObject *self)
{
    CoilIncludePrivate *priv = COIL_INCLUDE(self)->priv;
    CoilValue *file_value = priv->file_value;
    gchar *filepath = NULL;

    if (file_value == NULL) {
        coil_object_error(COIL_ERROR_INTERNAL, self,
                "No value set for filepath");
        return NULL;
    }
    if (G_VALUE_HOLDS(file_value, COIL_TYPE_OBJECT)) {
        CoilObject *ob = COIL_OBJECT(g_value_dup_object(file_value));
        const CoilValue *return_value = NULL;

        coil_object_set(ob, "container", self->container, NULL);
        if (!coil_object_expand(ob, &return_value, TRUE)) {
            coil_object_unref(ob);
            return NULL;
        }
        if (return_value == NULL) {
            coil_object_error(COIL_ERROR_INTERNAL, self,
                    "No value returned expanding include file path value");
            coil_object_unref(ob);
            return NULL;
        }
        g_value_unset(file_value);
        g_value_init(file_value, G_VALUE_TYPE(return_value));
        g_value_copy(return_value, file_value);
        coil_object_unref(ob);
    }

    if (G_VALUE_HOLDS(file_value, G_TYPE_GSTRING)) {
        GString *buf = g_value_get_boxed(file_value);
        filepath = buf->str;
    }
    else if (G_VALUE_HOLDS(file_value, G_TYPE_STRING)) {
        filepath = (gchar *)g_value_get_string(file_value);
    }
    else {
        coil_include_error(self,
                "include path must expand to string type. "
                "Found type '%s'.", G_VALUE_TYPE_NAME(priv->file_value));
        return NULL;
    }

    filepath = find_include_path(self, filepath);
    if (filepath == NULL) {
        return NULL;
    }

    g_value_unset(file_value);
    g_value_init(file_value, G_TYPE_STRING);
    g_value_take_string(file_value, (gchar *)filepath);

    return filepath;
}

static CoilObject *
get_root(CoilObject *self)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);

    CoilIncludePrivate *priv = COIL_INCLUDE(self)->priv;
    CoilObject *root;
    const gchar *filepath;

    if (priv->root) {
        return coil_object_ref(priv->root);
    }
    if (priv->is_expanded) {
        g_error("Include is expanded but no root is available.");
    }
    filepath = expand_file_value(self);
    if (filepath == NULL) {
        return NULL;
    }
    root = CACHE_LOAD(self->root, filepath);
    if (root == NULL) {
        return NULL;
    }
    priv->root = root;
    return coil_object_ref(root);
}

static gboolean
expand_import_value(CoilValue *value)
{
    CoilObject *importobj;
    const CoilValue *returnval;

    if (!G_VALUE_HOLDS(value, COIL_TYPE_OBJECT)) {
        return TRUE;
    }
    returnval = NULL;
    importobj = COIL_OBJECT(g_value_dup_object(value));
    if (!coil_object_expand(importobj, &returnval, TRUE)) {
        coil_object_unref(importobj);
        return FALSE;
    }
    g_assert(returnval);
    g_value_unset(value);
    g_value_init(value, G_VALUE_TYPE(returnval));
    g_value_copy(returnval, value);
    coil_object_unref(importobj);
    return TRUE;
}

static CoilPath *
get_path_from_import_value(CoilValue *value)
{
    GType type;

    if (!expand_import_value(value)) {
        return NULL;
    }
    type = G_VALUE_TYPE(value);
    if (type == COIL_TYPE_PATH) {
        return (CoilPath *)g_value_dup_boxed(value);
    }
    if (type == G_TYPE_STRING) {
        return coil_path_new(g_value_get_string(value));
    }
    if (type == G_TYPE_GSTRING) {
        GString *g = (GString *)g_value_get_boxed(value);
        return coil_path_new_len(g->str, g->len);
    }
    g_error("Invalid include argument type '%s' in import argument list. "
            "All import arguments are treated as strings.",
            G_VALUE_TYPE_NAME(value));
}

static gboolean
copy_into_namespace(CoilObject *self, CoilPath *path, const CoilValue *value)
{
    CoilObject *obj = NULL;
    CoilObject *namespace = COIL_INCLUDE(self)->priv->namespace;
    gboolean res = FALSE;

    if (!G_VALUE_HOLDS(value, COIL_TYPE_OBJECT)) {
        /* regular non-object value */
        CoilValue *value_copy = coil_value_copy(value);
        if (value_copy) {
            res = coil_struct_insert(namespace, path->key, path->key_len,
                    value_copy, FALSE);
        }
        return res;
    }
    obj = COIL_OBJECT(g_value_dup_object(value));
    if (!COIL_IS_STRUCT(obj)) {
        CoilValue *copyval;
        CoilObject *copyobj;
        CoilPath *copypath = coil_path_new_len(path->key, path->key_len);
        if (copypath == NULL) {
            goto end;
        }
        if (!coil_path_resolve_inplace(&copypath, namespace->path)) {
            coil_path_unref(copypath);
            goto end;
        }
        copyobj = coil_object_copy(obj,
                "container", namespace,
                "path", copypath,
                "location", self->location,
                NULL);
        if (copyobj == NULL) {
            coil_path_unref(copypath);
            goto end;
        }
        coil_value_init(copyval, G_OBJECT_TYPE(obj), take_object, copyobj);
        res = coil_struct_insert_path(namespace, copypath, copyval, FALSE);
        coil_path_unref(copypath);
        if (!res) {
            coil_value_free(copyval);
            goto end;
        }
    }
    else {
        res = MERGE_NAMESPACE(obj, namespace);
    }
end:
    coil_object_unref(obj);
    return res;
}

static gboolean
process_import(CoilObject *self, CoilValue *import)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);
    g_return_val_if_fail(import != NULL, FALSE);
    g_return_val_if_fail(G_IS_VALUE(import), FALSE);

    CoilObject *root;
    CoilPath *path;
    const CoilValue *value;

    if (!expand_import_value(import)) {
        return FALSE;
    }
    if (G_VALUE_HOLDS(import, COIL_TYPE_LIST)) {
        return process_import_list(self, coil_value_get_object(import));
    }
    root = get_root(self);
    if (root == NULL) {
        goto err;
    }
    if (self->container && G_VALUE_HOLDS(import, COIL_TYPE_OBJECT)) {
        CoilObject *obj;

        obj = coil_value_dup_object(import);
        coil_object_set_container(obj, self->container);
        if (coil_error_occurred()) {
            coil_object_unref(obj);
            goto err;
        }
        coil_object_unref(obj);
    }
    path = get_path_from_import_value(import);
    if (path == NULL) {
        goto err;
    }
    /* lookup the path in the root */
    value = coil_struct_lookupx(root, path, TRUE);
    if (value == NULL) {
        if (!coil_error_occurred()) {
            coil_include_error(self, "import path '%s' does not exist.",
                    path->str);
        }
        goto err;
    }
    if (!copy_into_namespace(self, path, value)) {
        goto err;
    }
    CLEAR(root, coil_object_unref);
    CLEAR(path, coil_path_unref);
    return TRUE;

err:
    CLEAR(root, coil_object_unref);
    CLEAR(path, coil_path_unref);
    return FALSE;
}

static gboolean
process_import_list(CoilObject *self, CoilObject *import_list)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);
    g_return_val_if_fail(import_list != NULL, FALSE);
    g_return_val_if_fail(COIL_IS_LIST(import_list), FALSE);

    guint i, len;

    len = coil_list_length(import_list);
    for (i = 0; i < len; i++) {
        CoilValue *import_value;

        import_value = coil_list_get_index(import_list, i);
        if (!process_import(self, import_value)) {
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean
include_expand(CoilObject *self, const CoilValue **return_value)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);

    CoilIncludePrivate *priv = COIL_INCLUDE(self)->priv;
    CoilObject *import_list = IMPORT_LIST(self);
    CoilObject *root = NULL;

    if (priv->is_expanded) {
        g_assert(priv->namespace_value);
        g_assert(G_IS_VALUE(priv->namespace_value));
        *return_value = priv->namespace_value;
        return TRUE;
    }
    if (coil_list_length(import_list) > 0) {
        /* selective imports */
        priv->namespace = coil_struct_new(NULL, NULL);
        coil_value_init(priv->namespace_value, COIL_TYPE_STRUCT,
                set_object, priv->namespace);
        if (!process_import_list(self, import_list)) {
            goto error;
        }
    }
    else {
        /* import everything */
        root = get_root(self);
        if (coil_error_occurred()) {
            goto error;
        }
        priv->namespace = root;
        coil_value_init(priv->namespace_value, COIL_TYPE_STRUCT,
                set_object, priv->namespace);
    }
    priv->is_expanded = TRUE;
    *return_value = priv->namespace_value;
    return TRUE;
error:
    *return_value = NULL;
    priv->is_expanded = FALSE;
    CLEAR(priv->namespace_value, coil_value_free);
    CLEAR(priv->namespace, coil_object_unref);
    CLEAR(root, coil_object_unref);
    return FALSE;
}

static void
build_legacy_string(CoilObject *self, GString *buffer, CoilStringFormat *format)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(COIL_IS_INCLUDE(self));
    g_return_if_fail(buffer);
    g_return_if_fail(format);

    CoilIncludePrivate *priv = COIL_INCLUDE(self)->priv;
    CoilObject *import_list = IMPORT_LIST(self);
    gsize i, num_imports;

    num_imports = coil_list_length(import_list);
    for (i = 0; i < num_imports; i++) {
        CoilValue *import;

        g_string_append_len(buffer, "@file: [", 8);

        coil_value_build_string(priv->file_value, buffer, format);
        if (coil_error_occurred()) {
            return;
        }
        g_string_append_c(buffer, ' ');

        import = coil_list_get_index(import_list, i);
        coil_value_build_string(import, buffer, format);
        if (coil_error_occurred()) {
            return;
        }
        g_string_append_c(buffer, ']');
    }
}

static void
include_build_string(CoilObject *self, GString *buffer, CoilStringFormat *format)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(COIL_IS_INCLUDE(self));
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(format != NULL);

    CoilIncludePrivate *priv = COIL_INCLUDE(self)->priv;
    CoilValue *file_value = priv->file_value;
    CoilObject *import_list = IMPORT_LIST(self);
    CoilObject *tmp;
    guint i, num_imports;

    num_imports = coil_list_length(import_list);
    if ((format->options & LEGACY) && num_imports > 1) {
        build_legacy_string(self, buffer, format);
        return;
    }

    g_string_append_len(buffer, COIL_STATIC_STRLEN("@file:"));

    if (!(format->options & COMPACT))
        g_string_append_c(buffer, ' ');

    if (num_imports == 0) {
        coil_value_build_string(file_value, buffer, format);
        return;
    }

    /* XXX: this code must GO */
    /* FIXME FIXME */
    tmp = coil_list_new_sized(num_imports + 1);
    if (tmp == NULL) {
        return;
    }
    coil_list_append(tmp, coil_value_copy(file_value));
    for (i = 0; i < num_imports; i++) {
        CoilValue *v = coil_list_dup_index(import_list, i);
        coil_list_append(tmp, v);
    }
    coil_object_build_string(import_list, buffer, format);
    coil_object_unref(tmp);
}

static void
coil_include_dispose(GObject *object)
{
    CoilIncludePrivate *priv = COIL_INCLUDE(object)->priv;

    CLEAR(priv->root, coil_object_unref);
    CLEAR(priv->file_value, coil_value_free);
    CLEAR(priv->imports, coil_object_unref);
    CLEAR(priv->namespace, coil_object_unref);
    CLEAR(priv->namespace_value, coil_value_free);

    G_OBJECT_CLASS(coil_include_parent_class)->dispose(object);
}

static void
coil_include_set_property(GObject      *object,
                          guint         property_id,
                          const CoilValue *value,
                          GParamSpec   *pspec)
{
    CoilIncludePrivate *priv = COIL_INCLUDE(object)->priv;

    switch (property_id) {
        case PROP_FILE_VALUE: { /* XXX: steals reference */
            CLEAR(priv->file_value, coil_value_free);
            priv->file_value = (CoilValue *)g_value_get_pointer(value);
            break;
        }
        case PROP_IMPORTS: {
            CLEAR(priv->imports, coil_object_unref);
            priv->imports = coil_value_dup_object(value);
            break;
        }
        default: {
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
        }
    }
}

static void
coil_include_get_property(GObject    *object,
                          guint       property_id,
                          CoilValue     *value,
                          GParamSpec *pspec)
{
    CoilInclude        *self = COIL_INCLUDE(object);
    CoilIncludePrivate *priv = self->priv;

    switch (property_id) {
        case PROP_FILE_VALUE: /* does not increment reference */
            g_value_set_pointer(value, priv->file_value);
            break;
        case PROP_IMPORTS: /* *increments* reference count */
            g_value_set_object(value, priv->imports);
            break;
        case PROP_NAMESPACE: /* *increments* reference count */
            g_value_set_object(value, priv->namespace);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
coil_include_init(CoilInclude *self)
{
    CoilIncludePrivate *priv = COIL_INCLUDE_GET_PRIVATE(self);
    self->priv = priv;
}

static void
coil_include_class_init(CoilIncludeClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    CoilObjectClass *object_class = COIL_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(CoilIncludePrivate));

    gobject_class->set_property = coil_include_set_property;
    gobject_class->get_property = coil_include_get_property;
    gobject_class->dispose = coil_include_dispose;

    object_class->is_expanded = include_is_expanded;
    object_class->expand = include_expand;
    object_class->build_string = include_build_string;

    g_object_class_install_property(gobject_class, PROP_FILE_VALUE,
            g_param_spec_pointer("file_value",
                "File value",
                "Path on filesystem to import from.",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class, PROP_IMPORTS,
            g_param_spec_object("imports",
                "Imports",
                "List of coil paths to import from file.",
                COIL_TYPE_LIST,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class, PROP_NAMESPACE,
            g_param_spec_object("namespace",
                "Namespace",
                "Namespace of the resulting imports from included file",
                COIL_TYPE_STRUCT,
                G_PARAM_READABLE));

    CACHE_INIT();
}

