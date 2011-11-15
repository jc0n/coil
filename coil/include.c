/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#include "struct.h"
#include "list.h"
#include "parser_defs.h"
#include "include.h"

G_DEFINE_TYPE(CoilInclude, coil_include, COIL_TYPE_OBJECT);

#define COIL_INCLUDE_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), COIL_TYPE_INCLUDE, \
         CoilIncludePrivate))

struct _CoilIncludePrivate
{
    GValue      *file_value; /* (object -> string) OR string */
    GValueArray *imports; /* list of (object -> string) or strings */

    CoilObject  *namespace;

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

static GHashTable *namespace_cache = NULL;

typedef struct _CacheEntry
{
    gchar        *filepath;
    CoilObject   *namespace;
    time_t        m_time;
    volatile gint ref_count;
} CacheEntry;

static void
cache_init(void)
{
    if (namespace_cache == NULL) {
        namespace_cache = g_hash_table_new(g_str_hash, g_str_equal);
    }
}

static void
cache_entry_free(CacheEntry *entry)
{
    g_return_if_fail(entry);

    g_hash_table_remove(namespace_cache, entry->filepath);
    g_object_unref(entry->namespace);
    g_free(entry->filepath);
    g_free(entry);
}

static void
cache_gc_notify(gpointer data, GObject *object_address)
{
    g_return_if_fail(data != NULL);

    gchar *filepath = (gchar *)data;
    CacheEntry *entry = g_hash_table_lookup(namespace_cache, filepath);

    if (entry && g_atomic_int_dec_and_test(&entry->ref_count))
        cache_entry_free(entry);
}

static void
cache_save(GObject *object, const gchar *filepath, CoilObject *namespace)
{
    g_return_if_fail(G_IS_OBJECT(object));
    g_return_if_fail(filepath != NULL);
    g_return_if_fail(COIL_IS_STRUCT(namespace));

    CacheEntry *entry;
    struct stat st;

    entry = g_hash_table_lookup(namespace_cache, filepath);
    if (entry) {
        g_atomic_int_inc(&entry->ref_count);
        g_object_weak_ref(object, cache_gc_notify, entry->filepath);
    }
    else if (stat(filepath, &st) == 0) {
        entry = g_new(CacheEntry, 1);
        entry->ref_count = 1;
        entry->filepath = g_strdup(filepath);
        entry->namespace = g_object_ref(namespace);
        entry->m_time = st.st_mtime;

        g_hash_table_insert(namespace_cache, entry->filepath, entry);
        g_object_weak_ref(object, cache_gc_notify, entry->filepath);
    }
}

static CoilObject *
cache_load(gpointer _notify, const gchar *filepath)
{
    g_return_val_if_fail(G_IS_OBJECT(_notify), NULL);
    g_return_val_if_fail(filepath != NULL, NULL);

    GObject *notify = G_OBJECT(_notify);
    CacheEntry *entry;
    CoilObject *namespace;
    struct stat st;

    entry = g_hash_table_lookup(namespace_cache, filepath);
    if (entry == NULL) {
        namespace = coil_parse_file(filepath);
        if (namespace) {
            cache_save(notify, filepath, namespace);
        }
        return namespace;
    }
    if (stat(filepath, &st) < 0) {
        coil_set_error(COIL_ERROR_INTERNAL, NULL,
                "Cannot stat include file '%s'.", filepath);
        return NULL;
    }
    if (st.st_mtime != entry->m_time) {
        namespace = coil_parse_file(filepath);
        if (namespace == NULL) {
            cache_entry_free(entry);
            return NULL;
        }
        g_object_unref(entry->namespace);
        entry->namespace = namespace;
        entry->m_time = st.st_mtime;
    }
    return g_object_ref(entry->namespace);
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
    GValue *file_value = priv->file_value;
    gchar *filepath = NULL;

    if (file_value == NULL) {
        coil_object_error(COIL_ERROR_INTERNAL, self,
                "No value set for filepath");
        return NULL;
    }
    if (G_VALUE_HOLDS(file_value, COIL_TYPE_OBJECT)) {
        CoilObject *ob = COIL_OBJECT(g_value_dup_object(file_value));
        const GValue *return_value = NULL;

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
    if (filepath == NULL)
        return NULL;

    g_value_unset(file_value);
    g_value_init(file_value, G_TYPE_STRING);
    g_value_take_string(file_value, (gchar *)filepath);

    return filepath;
}

static int
load_namespace(CoilObject *self)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);

    CoilIncludePrivate *priv = COIL_INCLUDE(self)->priv;
    CoilObject *namespace;
    const gchar *filepath;

    if (priv->is_expanded) {
        if (priv->namespace == NULL) {
            g_error("Include is expanded but no namespace is available.");
        }
        return 0;
    }
    if (priv->namespace) {
        coil_object_unref(priv->namespace);
        priv->namespace = NULL;
    }

    filepath = expand_file_value(self);
    if (filepath == NULL)
        return -1;

    namespace = CACHE_LOAD(self->root, filepath);
    if (namespace == NULL)
        return -1;

    priv->namespace = namespace;
    return 0;
}

static CoilObject *
get_namespace(CoilObject *self)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), NULL);

    CoilIncludePrivate *priv = COIL_INCLUDE(self)->priv;

    if (priv->namespace == NULL && load_namespace(self) < 0)
        return NULL;

    return priv->namespace;
}

static int
expand_import(GValue *import)
{
    g_return_val_if_fail(G_IS_VALUE(import), -1);

    CoilObject *o;
    const GValue *returnval = NULL;

    if (!G_VALUE_HOLDS(import, COIL_TYPE_OBJECT))
        return 0;

    o = COIL_OBJECT(g_value_dup_object(import));
    if (!coil_object_expand(o, &returnval, TRUE)) {
        coil_object_unref(o);
        return -1;
    }
    if (returnval == NULL) {
        g_error("Expecting return value from expansion of type '%s'.",
                G_OBJECT_TYPE_NAME(o));
    }
    g_value_unset(import);
    g_value_init(import, G_VALUE_TYPE(returnval));
    g_value_copy(returnval, import);
    coil_object_unref(o);
    return 0;
}

static CoilPath *
get_path_from_import(GValue *import)
{
    g_return_val_if_fail(G_IS_VALUE(import), NULL);

    CoilPath *path;

    if (expand_import(import) < 0)
        return NULL;

    if (G_VALUE_HOLDS(import, COIL_TYPE_PATH)) {
        path = (CoilPath *)g_value_dup_boxed(import);
    }
    else if (G_VALUE_HOLDS(import, G_TYPE_GSTRING)) {
        GString *g = (GString *)g_value_get_boxed(import);
        path = coil_path_new_len(g->str, g->len);
    }
    else if (G_VALUE_HOLDS(import, G_TYPE_STRING)) {
        const gchar *str = g_value_get_string(import);
        path = coil_path_new(str);
    }
    else {
        g_error("Invalid include argument type '%s' in import argument list. "
                "All import arguments are treated as strings.",
                G_VALUE_TYPE_NAME(import));
    }
    return path;
}

static int
process_import(CoilObject *self, GValue *import)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), -1);
    g_return_val_if_fail(G_IS_VALUE(import), -1);

    CoilPath *path = NULL;
    CoilObject *source, *namespace;
    const GValue *value;
    gboolean res;

    namespace = get_namespace(self);
    if (namespace == NULL) {
        goto err;
    }
    g_assert(self->container);
    if (G_VALUE_HOLDS(import, COIL_TYPE_OBJECT)) {
        CoilObject *ob = COIL_OBJECT(g_value_get_object(import));
        coil_object_set(ob, "container", self->container, NULL);
    }
    path = get_path_from_import(import);
    if (path == NULL) {
        goto err;
    }
    /* lookup the path in the namespace */
    value = coil_struct_lookupx(namespace, path, FALSE);
    if (value == NULL) {
        if (!coil_error_occurred()) {
            coil_include_error(self, "import path '%s' does not exist.",
                    path->str);
        }
        goto err;
    }
    /* TODO(jcon): support importing non struct paths */
    if (!G_VALUE_HOLDS(value, COIL_TYPE_STRUCT)) {
        coil_include_error(self, "import path '%s' type must be struct.",
                path->str);
        goto err;
    }
    source = COIL_OBJECT(g_value_dup_object(value));
    res = MERGE_NAMESPACE(source, self->container);
    coil_object_unref(source);
    coil_path_unref(path);
    return (res) ? 0 : -1;

err:
    if (path) {
        coil_path_unref(path);
    }
    return -1;
}

static int
process_import_array(CoilObject *self, GValueArray *arr)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), -1);
    g_return_val_if_fail(arr != NULL, -1);

    GValue *import;
    gsize i, n = arr->n_values;

    for (i = 0; i < n; i++) {
        import = g_value_array_get_nth(arr, i);
        if (expand_import(import) < 0) {
            return -1;
        }
        if (G_VALUE_HOLDS(import, COIL_TYPE_LIST)) {
            GValueArray *subarr = (GValueArray *)g_value_get_boxed(import);
            if (process_import_array(self, subarr) < 0) {
                return -1;
            }
        }
        else if (process_import(self, import) < 0) {
            return -1;
        }
    }
    return 0;
}

static int
process_all_imports(CoilObject *self)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);

    GValueArray *imports = COIL_INCLUDE(self)->priv->imports;
    if (imports == NULL) {
        return 0;
    }
    return process_import_array(self, imports);
}

static gboolean
include_expand(CoilObject *self, const GValue **return_value)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);

    CoilIncludePrivate *priv = COIL_INCLUDE(self)->priv;
    CoilObject *namespace = NULL;

    if (priv->is_expanded) {
        return TRUE;
    }
    if (priv->imports != NULL && priv->imports->n_values > 0) {
        if (process_all_imports(self) < 0) {
            goto error;
        }
        priv->is_expanded = TRUE;
        return TRUE;
    }
    /* import everything */
    namespace = get_namespace(self);
    if (namespace == NULL) {
        goto error;
    }
    coil_object_ref(namespace);
    if (!MERGE_NAMESPACE(namespace, self->container)) {
        goto error;
    }
    coil_object_unref(namespace);
    priv->is_expanded = TRUE;
    return TRUE;

error:
    if (namespace) {
        coil_object_unref(namespace);
    }
    return FALSE;
}

static void
build_legacy_string(CoilObject *self, GString *buffer, CoilStringFormat *format)
{
    g_return_if_fail(COIL_IS_INCLUDE(self));
    g_return_if_fail(buffer);
    g_return_if_fail(format);

    CoilIncludePrivate *priv = COIL_INCLUDE(self)->priv;
    GValue *import;
    GValueArray *imports;
    gsize i, n;

    g_assert(priv->imports);

    imports = priv->imports;
    n = imports->n_values;

    for (i = 0; i < n; i++) {
        g_string_append_len(buffer, "@file: [", 8);

        coil_value_build_string(priv->file_value, buffer, format);
        if (coil_error_occurred()) {
            return;
        }
        g_string_append_c(buffer, ' ');

        import = g_value_array_get_nth(imports, i);
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
    g_return_if_fail(COIL_IS_INCLUDE(self));
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(format != NULL);

    CoilIncludePrivate *priv = COIL_INCLUDE(self)->priv;
    GValue *file_value = priv->file_value;
    GValueArray *imports = priv->imports, *args;
    guint n;

    n = (imports) ? imports->n_values : 0;
    if ((format->options & LEGACY) && n > 1) {
        build_legacy_string(self, buffer, format);
        return;
    }

    g_string_append_len(buffer, COIL_STATIC_STRLEN("@file:"));

    if (!(format->options & COMPACT))
        g_string_append_c(buffer, ' ');

    if (n == 0) {
        coil_value_build_string(file_value, buffer, format);
        return;
    }

    args = g_value_array_new(n + 1);
    g_value_array_insert(args, 0, file_value);
    memcpy(args->values + 1, imports->values, n);

    coil_list_build_string(args, buffer, format);
    g_value_array_free(args);
}

static gchar *
include_to_string(CoilObject *self, CoilStringFormat *format)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), NULL);

    GString *buffer;

    buffer = g_string_sized_new(128);
    include_build_string(self, buffer, format);

    return g_string_free(buffer, FALSE);
}

static void
coil_include_dispose(GObject *object)
{
    CoilIncludePrivate *priv = COIL_INCLUDE(object)->priv;

    coil_value_free(priv->file_value);
    priv->file_value = NULL;

    if (priv->imports != NULL)
       g_value_array_free(priv->imports);
    priv->imports = NULL;

    if (priv->namespace)
        coil_object_unref(priv->namespace);
    priv->namespace = NULL;

    G_OBJECT_CLASS(coil_include_parent_class)->dispose(object);
}

static void
coil_include_set_property(GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    CoilIncludePrivate *priv = COIL_INCLUDE(object)->priv;

    switch (property_id) {
        case PROP_FILE_VALUE: { /* XXX: steals reference */
            if (priv->file_value)
                coil_value_free(priv->file_value);

            priv->file_value = (GValue *)g_value_get_pointer(value);
            break;
        }
        case PROP_IMPORTS: {
            if (priv->imports)
                g_value_array_free(priv->imports);

            priv->imports = (GValueArray *)g_value_dup_boxed(value);
            break;
        }
        case PROP_NAMESPACE: {
            if (priv->namespace)
                coil_object_unref(priv->namespace);

            priv->namespace = g_value_dup_object(value);
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
                          GValue     *value,
                          GParamSpec *pspec)
{
    CoilInclude        *self = COIL_INCLUDE(object);
    CoilIncludePrivate *priv = self->priv;

    switch (property_id) {
        case PROP_FILE_VALUE: /* does not increment reference */
            g_value_set_pointer(value, priv->file_value);
            break;
        case PROP_IMPORTS: /* does not increment reference */
            g_value_take_boxed(value, priv->imports);
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

    priv->file_value = NULL;
    priv->imports = NULL;
    priv->namespace = NULL;
}

static void
coil_include_class_init(CoilIncludeClass *klass)
{
    g_type_class_add_private(klass, sizeof(CoilIncludePrivate));

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = coil_include_set_property;
    gobject_class->get_property = coil_include_get_property;
    gobject_class->dispose = coil_include_dispose;

    CoilObjectClass *object_class = COIL_OBJECT_CLASS(klass);

    object_class->is_expanded = include_is_expanded;
    object_class->expand = include_expand;
    object_class->build_string = include_build_string;

    g_object_class_install_property(gobject_class, PROP_FILE_VALUE,
            g_param_spec_pointer("file_value",
                "File value",
                "Path on filesystem to import from.",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class, PROP_IMPORTS,
            g_param_spec_boxed("imports",
                "Imports",
                "List of coil paths to import from file.",
                G_TYPE_VALUE_ARRAY,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class, PROP_NAMESPACE,
            g_param_spec_object("namespace",
                "Namespace",
                "Namespace of the resulting imports from included file",
                COIL_TYPE_STRUCT,
                G_PARAM_READABLE));

    CACHE_INIT();
}

