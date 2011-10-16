/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "struct.h"
#include "list.h"
#include "parser_defs.h"
#include "include.h"

G_DEFINE_TYPE(CoilInclude, coil_include, COIL_TYPE_EXPANDABLE);

#define COIL_INCLUDE_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), COIL_TYPE_INCLUDE, \
         CoilIncludePrivate))

struct _CoilIncludePrivate
{
    GValue      *file_value; /* (expandable -> string) OR string */
    GValueArray *imports; /* list of (expandable -> string) or strings */

    CoilStruct  *namespace;

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
#define MERGE_NAMESPACE(ns, dst, err) \
    coil_struct_merge_full(ns, dst, FALSE, TRUE, err)

#else

#define MERGE_NAMESPACE(ns, dst, err) \
    coil_struct_merge(ns, dst, err)

#endif


/* TODO(jcon): consider moving caching into separate file and
 * explore smarter cache coherency approaches */

#if COIL_INCLUDE_CACHING

static GHashTable *namespace_cache = NULL;

typedef struct _CacheEntry
{
    gchar        *filepath;
    CoilStruct   *namespace;
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
cache_save(GObject *object, const gchar *filepath, CoilStruct *namespace)
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

static CoilStruct *
cache_load(gpointer _notify, const gchar *filepath, GError **error)
{
    g_return_val_if_fail(G_IS_OBJECT(_notify), NULL);
    g_return_val_if_fail(filepath != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GObject *notify = G_OBJECT(_notify);
    CacheEntry *entry;
    CoilStruct *namespace;
    struct stat st;

    entry = g_hash_table_lookup(namespace_cache, filepath);
    if (entry == NULL) {
        namespace = coil_parse_file(filepath, error);
        if (namespace) {
            cache_save(notify, filepath, namespace);
        }
        return namespace;
    }

    if (stat(filepath, &st) < 0) {
        g_set_error(error, COIL_ERROR, COIL_ERROR_INTERNAL,
                "Cannot stat include file '%s'.", filepath);
        return NULL;
    }

    if (st.st_mtime != entry->m_time) {
        namespace = coil_parse_file(filepath, error);
        if (namespace == NULL) {
            /* parse error */
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

#define CACHE_LOAD(notify, path, err) \
    cache_load(notify, path, err)

#else

#define CACHE_INIT()
#define CACHE_LOAD(notify, path, err) coil_parse_file(path, err)

#endif

static gboolean
include_is_expanded(gconstpointer object)
{
    CoilInclude *self = COIL_INCLUDE(object);
    return self->priv->is_expanded;
}

/* search for the filename in multiple paths */
static gchar *
find_include_path(CoilInclude *self, const gchar *filename, GError **error)
{
    gchar *dirpath, *inpath;
    const gchar *origpath = COIL_EXPANDABLE(self)->location.filepath;
    guint tflags = G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR;

    if (g_path_is_absolute(filename)) {
        if (origpath != NULL && strcmp(filename, origpath) == 0) {
            coil_include_error(error, self, "a file should not import itself");
            return NULL;
        }
        return g_strdup(filename);
    }

    if (origpath != NULL) {
        dirpath = g_dirname(origpath);
        inpath = g_build_filename(dirpath, filename, NULL);

        g_free(dirpath);
        if (g_file_test(inpath, tflags))
            return inpath;
        g_free(inpath);
    }

    dirpath = g_get_current_dir();
    inpath = g_build_filename(dirpath, filename, NULL);
    g_free(dirpath);

    if (g_file_test(inpath, tflags))
        return inpath;
    g_free(inpath);

    dirpath = (gchar *)g_get_home_dir();
    inpath = g_build_filename(dirpath, filename, NULL);

    if (g_file_test(inpath, tflags))
        return inpath;
    g_free(inpath);

    coil_include_error(error, self,
            "include file path '%s' not found.", filename);
    return NULL;
}

static const gchar *
expand_file_value(CoilInclude *self, GError **error)
{
    CoilExpandable *ex = COIL_EXPANDABLE(self);
    CoilIncludePrivate *const priv = self->priv;
    gchar *filepath = NULL;
    GValue *file_value = priv->file_value;

    if (file_value == NULL) {
        coil_expandable_error(error, COIL_ERROR_INTERNAL, self,
                "No value set for filepath");
        return NULL;
    }

    if (G_VALUE_HOLDS(file_value, COIL_TYPE_EXPANDABLE)) {
        GObject *ob;
        const GValue *return_value = NULL;

        ob = g_value_dup_object(file_value);
        g_object_set(ob, "container", ex->container, NULL);

        if (!coil_expand(ob, &return_value, TRUE, error)) {
            g_object_unref(ob);
            return NULL;
        }
        if (return_value == NULL) {
            coil_expandable_error(error, COIL_ERROR_INTERNAL, self,
                    "No value returned expanding include file path value");
            g_object_unref(ob);
            return NULL;
        }
        g_value_unset(file_value);
        g_value_init(file_value, G_VALUE_TYPE(return_value));
        g_value_copy(return_value, file_value);
        g_object_unref(ob);
    }

    if (G_VALUE_HOLDS(file_value, G_TYPE_GSTRING)) {
        GString *buf = g_value_get_boxed(file_value);
        filepath = buf->str;
    }
    else if (G_VALUE_HOLDS(file_value, G_TYPE_STRING)) {
        filepath = (gchar *)g_value_get_string(file_value);
    }
    else {
        coil_include_error(error, self,
                "include path must expand to string type. "
                "Found type '%s'.", G_VALUE_TYPE_NAME(priv->file_value));
        return NULL;
    }

    filepath = find_include_path(self, filepath, error);
    if (filepath == NULL)
        return NULL;


    g_value_unset(file_value);
    g_value_init(file_value, G_TYPE_STRING);
    g_value_take_string(file_value, (gchar *)filepath);

    return filepath;
}

static int
load_namespace(CoilInclude *self, GError **error)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilIncludePrivate *priv = self->priv;
    CoilExpandable *const super = COIL_EXPANDABLE(self);
    CoilStruct *root, *namespace;
    const gchar *filepath;
    GError *internal_error = NULL;

    if (priv->is_expanded) {
        if (priv->namespace == NULL) {
            g_error("Include is marked 'expanded'"
                    " but no namespace is available.");
        }
        return 0;
    }
    if (priv->namespace) {
        g_object_unref(priv->namespace);
        priv->namespace = NULL;
    }

    filepath = expand_file_value(self, error);
    if (filepath == NULL)
        return -1;


    root = coil_struct_get_root(super->container);
    namespace = CACHE_LOAD(root, filepath, &internal_error);
    if (namespace == NULL)
        return -1;

    priv->namespace = namespace;
    return 0;
}

static CoilStruct *
get_namespace(CoilInclude *self, GError **error)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    CoilIncludePrivate *priv = self->priv;

    if (priv->namespace == NULL && load_namespace(self, error) < 0)
        return NULL;

    return priv->namespace;
}

static int
expand_import(GValue *import,
              GError **error)
{
    g_return_val_if_fail(G_IS_VALUE(import), -1);

    GObject *ob;
    const GValue *returnval = NULL;

    if (!G_VALUE_HOLDS(import, COIL_TYPE_EXPANDABLE))
        return 0;

    ob = g_value_dup_object(import);
    if (!coil_expand(ob, &returnval, TRUE, error)) {
        g_object_unref(ob);
        return -1;
    }
    if (returnval == NULL) {
        g_error("Expecting return value from expansion of type '%s'.",
                G_OBJECT_TYPE_NAME(ob));
    }
    g_value_unset(import);
    g_value_init(import, G_VALUE_TYPE(returnval));
    g_value_copy(returnval, import);
    g_object_unref(ob);
    return 0;
}

static CoilPath *
get_path_from_import(GValue *import, GError **error)
{
    g_return_val_if_fail(G_IS_VALUE(import), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    CoilPath *path;

    if (expand_import(import, error) < 0)
        return NULL;

    if (G_VALUE_HOLDS(import, COIL_TYPE_PATH)) {
        path = (CoilPath *)g_value_dup_boxed(import);
    }
    else if (G_VALUE_HOLDS(import, G_TYPE_GSTRING)) {
        GString *g = (GString *)g_value_get_boxed(import);
        path = coil_path_new_len(g->str, g->len, error);
    }
    else if (G_VALUE_HOLDS(import, G_TYPE_STRING)) {
        const gchar *str = g_value_get_string(import);
        path = coil_path_new(str, error);
    }
    else {
        g_error("Invalid include argument type '%s' in import argument list. "
                "All import arguments are treated as strings.",
                G_VALUE_TYPE_NAME(import));
    }
    return path;
}

static int
process_import(CoilInclude *self, GValue *import, GError **error)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), -1);
    g_return_val_if_fail(G_IS_VALUE(import), -1);
    g_return_val_if_fail(error == NULL || *error == NULL, -1);

    CoilPath *path = NULL;
    CoilStruct *namespace = NULL;
    CoilStruct *source, *container;
    GError *internal_error = NULL;
    const GValue *value;
    gboolean res;

    namespace = get_namespace(self, error);
    if (namespace == NULL)
        goto err;


    /* make sure the import's container is set */
    container = COIL_EXPANDABLE(self)->container;
    g_assert(container);

    if (G_VALUE_HOLDS(import, COIL_TYPE_EXPANDABLE)) {
        GObject *ob = g_value_get_object(import);
        g_object_set(ob, "container", container, NULL);
    }

    path = get_path_from_import(import, error);
    if (path == NULL)
        goto err;


    /* lookup the path in the namespace */
    value = coil_struct_lookup_path(namespace, path, FALSE, &internal_error);
    if (value == NULL) {
        if (internal_error == NULL) {
            coil_include_error(error, self,
                    "import path '%s' does not exist.", path->path);
        }
        goto err;
    }
    /* TODO(jcon): support importing non struct paths */
    if (!G_VALUE_HOLDS(value, COIL_TYPE_STRUCT)) {
        coil_include_error(error, self,
                "import path '%s' type must be struct.", path->path);
        goto err;
    }

    source = COIL_STRUCT(g_value_dup_object(value));

    res = MERGE_NAMESPACE(source, container, error);
    g_object_unref(source);
    coil_path_unref(path);
    return (res) ? 0 : -1;

err:
    if (path)
        coil_path_unref(path);

    return -1;
}

static int
process_individual_imports(CoilInclude  *self, GError **error)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GValue *import;
    GValueArray *imports;
    guint i, n;

    imports = self->priv->imports;
    if (imports == NULL)
        return 0;

    n = imports->n_values;
    for (i = 0; i < n; i++) {
        import = g_value_array_get_nth(imports, i);
        if (process_import(self, import, error) < 0)
            return -1;
    }

    return 0;
}

static gboolean
include_expand(gconstpointer include, const GValue **return_value, GError **error)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(include), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilInclude *const self = COIL_INCLUDE(include);
    CoilIncludePrivate *const priv = self->priv;
    CoilStruct *namespace = NULL, *container;

    if (priv->is_expanded)
        return TRUE;

    if (priv->imports != NULL) {
        if (priv->imports->n_values > 0) {
            if (process_individual_imports(self, error) < 0)
                goto error;
            priv->is_expanded = TRUE;
            return TRUE;
        }
    }

    /* import everything */
    namespace = get_namespace(self, error);
    if (namespace == NULL)
        goto error;

    g_object_ref(namespace);
    container = COIL_EXPANDABLE(self)->container;

    if (!MERGE_NAMESPACE(namespace, container, error))
        goto error;

    g_object_unref(namespace);
    priv->is_expanded = TRUE;
    return TRUE;

error:
    if (namespace)
        g_object_unref(namespace);

    return FALSE;
}

COIL_API(gboolean)
coil_include_equals(gconstpointer   e1,
                    gconstpointer   e2,
                    GError        **error)
{
    g_assert(0);
    return FALSE;
}

static void
include_build_string(gconstpointer     include,
                     GString          *const buffer,
                     CoilStringFormat *format,
                     GError          **error)
{
  g_return_if_fail(COIL_IS_INCLUDE(include));
  g_return_if_fail(buffer);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);

  coil_include_build_string(COIL_INCLUDE(include), buffer, format, error);
}

static void
build_legacy_string(CoilInclude      *self,
                    GString          *const buffer,
                    CoilStringFormat *format,
                    GError          **error)
{
    g_return_if_fail(COIL_IS_INCLUDE(self));
    g_return_if_fail(buffer);
    g_return_if_fail(format);
    g_return_if_fail(error == NULL || *error == NULL);

    CoilIncludePrivate *priv = self->priv;
    GValue *import;
    GValueArray *imports;
    GError *internal_error = NULL;
    gsize i, n;

    g_assert(priv->imports);

    imports = priv->imports;
    n = imports->n_values;

    for (i = 0; i < n; i++) {
        g_string_append_len(buffer, "@file: [", 8);

        coil_value_build_string(priv->file_value, buffer, format, &internal_error);
        if (internal_error) {
            g_propagate_error(error, internal_error);
            return;
        }
        g_string_append_c(buffer, ' ');

        import = g_value_array_get_nth(imports, i);
        coil_value_build_string(import, buffer, format, &internal_error);
        if (internal_error) {
            g_propagate_error(error, internal_error);
            return;
        }
        g_string_append_c(buffer, ']');
    }
}



COIL_API(void)
coil_include_build_string(CoilInclude      *self,
                          GString          *const buffer,
                          CoilStringFormat *format,
                          GError          **error)
{
    g_return_if_fail(COIL_IS_INCLUDE(self));
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(format != NULL);
    g_return_if_fail(error == NULL || *error == NULL);

    CoilIncludePrivate *priv = self->priv;
    GValue *file_value = priv->file_value;
    GValueArray *imports = priv->imports, *args;
    guint n;

    n = (imports) ? imports->n_values : 0;
    if ((format->options & LEGACY) && n > 1) {
        build_legacy_string(self, buffer, format, error);
        return;
    }

    g_string_append_len(buffer, COIL_STATIC_STRLEN("@file:"));

    if (!(format->options & COMPACT))
        g_string_append_c(buffer, ' ');

    if (n == 0) {
        coil_value_build_string(file_value, buffer, format, error);
        return;
    }

    args = g_value_array_new(n + 1);
    g_value_array_insert(args, 0, file_value);
    memcpy(args->values + 1, imports->values, n);

    coil_list_build_string(args, buffer, format, error);
    g_value_array_free(args);
}

COIL_API(gchar *)
coil_include_to_string(CoilInclude *self, CoilStringFormat *format, GError **error)
{
    g_return_val_if_fail(COIL_IS_INCLUDE(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GString *buffer;

    buffer = g_string_sized_new(128);
    coil_include_build_string(self, buffer, format, error);

    return g_string_free(buffer, FALSE);
}

static void
coil_include_dispose(GObject *object)
{
    CoilInclude        *self = COIL_INCLUDE(object);
    CoilIncludePrivate *priv = self->priv;

    coil_value_free(priv->file_value);
    priv->file_value = NULL;

    if (priv->imports != NULL)
       g_value_array_free(priv->imports);
    priv->imports = NULL;

    if (priv->namespace)
        g_object_unref(priv->namespace);
    priv->namespace = NULL;

    G_OBJECT_CLASS(coil_include_parent_class)->dispose(object);
}

static void
coil_include_set_property(GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    CoilInclude        *self = COIL_INCLUDE(object);
    CoilIncludePrivate *priv = self->priv;

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
                g_object_unref(priv->namespace);

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

    CoilExpandableClass *expandable_class = COIL_EXPANDABLE_CLASS(klass);

    expandable_class->is_expanded = include_is_expanded;
    expandable_class->expand = include_expand;
    expandable_class->equals = coil_include_equals;
    expandable_class->build_string = include_build_string;

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

