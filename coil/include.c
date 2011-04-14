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

/* TODO(jcon): Make CoilInclude more of a user friendly api.
 *
 * Right now I'm just going to support
 * passing an argument list directly from the parser. In the future
 * it might be cool to do things like..
 *
 * CoilInclude *include = coil_include_new("somefile.coil", "a", "b", "c");
 *
 */
G_DEFINE_TYPE(CoilInclude, coil_include, COIL_TYPE_EXPANDABLE);

#define COIL_INCLUDE_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), COIL_TYPE_INCLUDE, \
         CoilIncludePrivate))

struct _CoilIncludePrivate
{
  GValue     *filepath_value; /* (expandable -> string) OR string */
  GList      *import_list; /* list of (expandable -> string) or strings */

  CoilStruct *root;

  gboolean    is_expanded : 1;
};

typedef enum
{
  PROP_0,
  PROP_FILEPATH_VALUE,
  PROP_IMPORT_LIST,
} CoilIncludeProperties;

/* TODO(jcon): consider moving caching into separate file and
 * explore smarter cache coherency approaches */

#ifdef COIL_INCLUDE_CACHING

static GHashTable *open_files = NULL;

typedef struct _IncludeCacheEntry IncludeCacheEntry;

struct _IncludeCacheEntry
{
  gchar        *filepath;
  CoilStruct   *cacheable;
  time_t        m_time;
  volatile gint ref_count;
};

static void
include_cache_init(void)
{
  if (open_files == NULL)
    open_files = g_hash_table_new(g_str_hash, g_str_equal);
}

static void
include_cache_delete_entry(IncludeCacheEntry *entry)
{
  g_return_if_fail(entry);

  g_hash_table_remove(open_files, entry->filepath);
  g_object_unref(entry->cacheable);
  g_free(entry->filepath);
  g_free(entry);
}

static CoilStruct *
include_cache_lookup(const gchar *filepath,
                     GError     **error)
{
  g_return_val_if_fail(filepath != NULL, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  IncludeCacheEntry *entry = g_hash_table_lookup(open_files, filepath);

  if (entry)
  {
    struct stat buf;

    if (stat(filepath, &buf) == -1)
    {
      g_set_error(error,
                  COIL_ERROR,
                  COIL_ERROR_INTERNAL,
                  "Cannot stat file include file '%s'.",
                  filepath);

      return NULL;
    }

    if (buf.st_mtime != entry->m_time)
    {
      CoilStruct *root = coil_parse_file(filepath, error);

      if (root == NULL)
      {
        /* parse error */
        include_cache_delete_entry(entry);
        return NULL;
      }

      g_object_unref(entry->cacheable);
      entry->cacheable = root;
      entry->m_time = buf.st_mtime;
    }

    return entry->cacheable;
  }

  return NULL;
}

static void
include_cache_gc_notify(gpointer data,
                        GObject *addr)
{
  g_return_if_fail(data != NULL);
  g_return_if_fail(G_IS_OBJECT(addr));

  gchar             *filepath = (gchar *)data;
  IncludeCacheEntry *entry = g_hash_table_lookup(open_files, filepath);

  if (entry && g_atomic_int_dec_and_test(&entry->ref_count))
      include_cache_delete_entry(entry);
}

static void
include_cache_save(GObject     *object,
                   const gchar *filepath,
                   CoilStruct  *cacheable)
{
  g_return_if_fail(G_IS_OBJECT(object));
  g_return_if_fail(filepath != NULL);
  g_return_if_fail(COIL_IS_STRUCT(cacheable));

  IncludeCacheEntry *entry = g_hash_table_lookup(open_files, filepath);
  struct stat        buf;

  if (entry)
  {
    g_atomic_int_inc(&entry->ref_count);
    g_object_weak_ref(object,
                      (GWeakNotify)include_cache_gc_notify,
                      (gpointer)entry->filepath);

  }
  else if (stat(filepath, &buf) == 0)
  {
    entry = g_new(IncludeCacheEntry, 1);
    entry->ref_count = 1;
    entry->filepath = g_strdup(filepath);
    entry->cacheable = g_object_ref(cacheable);
    entry->m_time = buf.st_mtime;

    g_hash_table_insert(open_files, entry->filepath, entry);
    g_object_weak_ref(object,
                      (GWeakNotify)include_cache_gc_notify,
                      (gpointer)entry->filepath);
  }
}

#endif

static gboolean
include_is_expanded(gconstpointer object)
{
  CoilInclude *self = COIL_INCLUDE(object);
  return self->priv->is_expanded;
}

static CoilPath *
make_path_from_import_arg(CoilInclude  *self,
                          const GValue *path_value,
                          GError      **error)
{
  g_return_val_if_fail(COIL_IS_INCLUDE(self), NULL);
  g_return_val_if_fail(G_IS_VALUE(path_value), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilPath *path;
#if 0
  gchar *str;
  GValue strvalue = {0, };

  g_value_init(&strvalue, G_TYPE_STRING);
  if (g_value_transform(argvalue, &strvalue))
  {
    str = (gchar *)g_value_get_string(&strvalue);
    path = coil_path_take_strings(str, 0, NULL, 0, 0);
  }
#else
  if (G_VALUE_HOLDS(path_value, COIL_TYPE_PATH))
  {
    path = (CoilPath *)g_value_dup_boxed(path_value);
  }
  else if (G_VALUE_HOLDS(path_value, G_TYPE_GSTRING))
  {
    const GString *gstring = (GString *)g_value_get_boxed(path_value);
    gchar *str = g_strndup(gstring->str, gstring->len);
    guint8 len = gstring->len;

    path = coil_path_take_strings(str, len, NULL, 0, 0);
  }
  else if (G_VALUE_HOLDS(path_value, G_TYPE_STRING))
  {
    gchar *str = g_value_dup_string(path_value);
    path = coil_path_take_strings(str, 0, NULL, 0, 0);
  }
#endif
  else
    g_error("Invalid include argument type '%s' in import argument list. "
            "All import arguments are treated as strings.",
            G_VALUE_TYPE_NAME(path_value));

  return path;
}

static void
expand_import_arg(GValue  *arg,
                  GError **error)
{
  g_return_if_fail(G_IS_VALUE(arg));

  /* XXX: bubble up through g_list_foreach call on error */
  if (G_UNLIKELY(error != NULL && *error != NULL))
    return;

  if (G_VALUE_HOLDS(arg, COIL_TYPE_EXPANDABLE))
  {
    /* XXX: dup reference -- bc. the expanded list doesnt own any refs
     * and we need to replace the value below (which will cause an unref of the
     * object).
     */
    GObject      *object = g_value_dup_object(arg);
    const GValue *return_value = NULL;

    if (!coil_expand(object, &return_value, TRUE, error))
    {
      g_object_unref(object);
      return;
    }

    if (G_UNLIKELY(!return_value))
      g_error("Expecting return value from expansion of type '%s'.",
              G_OBJECT_TYPE_NAME(object));

    g_value_unset(arg);
    g_value_init(arg, G_VALUE_TYPE(return_value));
    g_value_copy(return_value, arg);
    g_object_unref(object);
  }
}

static gboolean
process_import_arg(CoilInclude *self,
                   CoilStruct  *root,
                   GValue      *arg,
                   GError     **error)
{
  CoilPath     *path;
  CoilStruct   *source;
  CoilStruct   *container = COIL_EXPANDABLE(self)->container;
  GError       *internal_error = NULL;
  const GValue *import_value;
  gboolean      result = FALSE;

  g_assert(!G_VALUE_HOLDS(arg, COIL_TYPE_EXPANDABLE));

  path = make_path_from_import_arg(self, arg, error);

  if (G_UNLIKELY(path == NULL))
    goto done;

  import_value = coil_struct_lookup_path(root, path,
                                         FALSE, &internal_error);

  if (import_value == NULL)
  {
    if (G_UNLIKELY(internal_error))
      g_propagate_error(error, internal_error);
    else
      coil_include_error(error, self,
                         "import path '%s' does not exist.",
                         path->path);
    goto done;
  }

  /* TODO(jcon): support importing non struct paths */
  if (!G_VALUE_HOLDS(import_value, COIL_TYPE_STRUCT))
  {
    coil_include_error(error, self,
                       "import path '%s' type must be struct.",
                        path->path);
    goto done;
  }

#if 0
  value_copy = value_alloc();

  if (G_VALUE_HOLDS(import_value, COIL_TYPE_EXPANDABLE))
  {
    GObject *object = g_value_get_object(import_value);
    CoilExpandable *object_copy = coil_expandable_copy(object, container, &internal_error);

    if (G_UNLIKELY(internal_error != NULL))
    {
      g_propagate_error(error, internal_error);
      goto done;
    }

    g_value_init(value_copy, G_VALUE_TYPE(import_value));
    g_value_take_object(value_copy, object_copy);
  }
  else
  {
    g_value_init(value_copy, G_VALUE_TYPE(import_value));
    g_value_copy(import_value, value_copy);
  }

  result = coil_struct_insert_key(container, path->key, path->key_len,
                                  value_copy, FALSE, error);

#endif

  source = COIL_STRUCT(g_value_dup_object(import_value));

#ifdef COIL_STRICT_FILE_CONTEXT
  result = coil_struct_merge_full(source, container, FALSE, TRUE, error);
#else
  result = coil_struct_merge(source, container, error);
#endif

  g_object_unref(source);

done:
  coil_path_free(path);
  return result;
}

static GList *
expand_import_arglist(const GList   *list,
                      GError       **error)
{
  GError *internal_error = NULL;
  GList  *lp;

  list = copy_value_list(list);

  for (lp = (GList *)list;
       lp; lp = g_list_next(lp))
  {
    expand_import_arg((GValue *)lp->data, &internal_error);

    if (G_UNLIKELY(internal_error))
    {
      free_value_list((GList *)list);
      g_propagate_error(error, internal_error);
      return NULL;
    }
  }

  return (GList *)list;
}

static gboolean
process_import_list(CoilInclude  *self,
                    CoilStruct   *root,
                    GError      **error)
{
  g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(root), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilIncludePrivate *priv = self->priv;
  GList              *list, *expanded_list;
  GError             *internal_error = NULL;

  expanded_list = expand_import_arglist(priv->import_list,
                                        &internal_error);

  if (G_UNLIKELY(internal_error))
    goto error;

  for (list = expanded_list;
       list != NULL;
       list = g_list_delete_link(list, list))
  {
    GValue *value = (GValue *)list->data;
/*
    if (G_VALUE_HOLDS(value, COIL_TYPE_LIST))
    {
      GList *inner_list = (GList *)g_value_get_boxed(value);

      inner_list = expand_import_arglist(inner_list, &internal_error);
      expanded_list = g_list_concat(expanded_list, inner_list);

      if (G_UNLIKELY(internal_error))
        goto error;
    }
    else */

    if (!process_import_arg(self, root, value, error))
      goto error;

    free_value(value);
  }

  return TRUE;

error:
  if (expanded_list)
    free_value_list(expanded_list);

  if (internal_error)
    g_propagate_error(error, internal_error);

  return FALSE;
}


static const gchar *
expand_filepath(CoilInclude *self,
                GError     **error)
{
  CoilIncludePrivate *const priv = self->priv;
  const gchar        *this_filepath, *filepath = NULL;

  /* TODO(jcon): I can't wait to refactor locations so
   *            I dont have to do stuff like this */
  this_filepath = COIL_EXPANDABLE(self)->location.filepath;

  if (G_VALUE_HOLDS(priv->filepath_value, COIL_TYPE_EXPANDABLE))
  {
    const GValue *return_value = NULL;

    if (!coil_expand_value(priv->filepath_value, &return_value,
                           TRUE, error))
      return FALSE;

    g_value_unset(priv->filepath_value);
    g_value_init(priv->filepath_value, G_VALUE_TYPE(return_value));
    g_value_copy(return_value, priv->filepath_value);
  }

  if (G_VALUE_HOLDS(priv->filepath_value, G_TYPE_GSTRING))
  {
    GString *buf = g_value_get_boxed(priv->filepath_value);
    filepath = (gchar *)buf->str;
  }
  else if (G_VALUE_HOLDS(priv->filepath_value, G_TYPE_STRING))
  {
    filepath = (gchar *)g_value_get_string(priv->filepath_value);
  }
#if 0
  else if (g_value_type_transformable(priv->filepath_value, G_TYPE_STRING))
  {

  }
#endif
  else
  {
    coil_include_error(error, self,
                       "include path must expand to type string. "
                       "Found type '%s'.",
                       G_VALUE_TYPE_NAME(priv->filepath_value));
    return FALSE;
  }

  if (this_filepath)
  {
    if (G_UNLIKELY(strcmp(filepath, this_filepath) == 0))
    {
      coil_include_error(error, self, "a file cannot import itself");
      return FALSE;
    }

    if (!g_path_is_absolute(filepath))
    {
      gchar *dirname, *fullpath;

      dirname = g_path_get_dirname(this_filepath);
      fullpath = g_build_filename(dirname, filepath, NULL);

      g_free(dirname);

      g_value_unset(priv->filepath_value);
      g_value_init(priv->filepath_value, G_TYPE_STRING);
      g_value_take_string(priv->filepath_value, fullpath);

      filepath = fullpath;
    }
  }

  return filepath;
}

static gboolean
include_load_root(CoilInclude *self,
                  GError     **error)
{
  g_return_val_if_fail(COIL_IS_INCLUDE(self), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilIncludePrivate *priv = self->priv;
  CoilExpandable     *const super = COIL_EXPANDABLE(self);
  CoilStruct         *container = super->container;
  CoilStruct         *root;
  const gchar        *filepath;
  GError             *internal_error = NULL;

  if (priv->is_expanded)
    return TRUE;

  if (!(filepath = expand_filepath(self, error)))
    return FALSE;

  if (G_UNLIKELY(!g_file_test(filepath,
                              G_FILE_TEST_IS_REGULAR |
                              G_FILE_TEST_EXISTS)))
  {
    coil_include_error(error, self,
                       "include path '%s' does not exist.",
                       filepath);

    goto error;
  }

#ifdef COIL_INCLUDE_CACHING
  root = include_cache_lookup(filepath, &internal_error);

  if (G_UNLIKELY(internal_error))
    goto error;

  if (root)
    g_object_ref(root);
  else
  {
    GObject *notify_object;

    root = coil_parse_file(filepath, &internal_error);

    if (G_UNLIKELY(internal_error))
      goto error;

    notify_object = G_OBJECT(coil_struct_get_root(container));
    include_cache_save(notify_object, filepath, root);
  }
#else
  root = coil_parse_file(filepath, &internal_error);

  if (G_UNLIKELY(internal_error))
    goto error;
#endif

  priv->root = root;
  return TRUE;

error:
  if (internal_error)
    g_propagate_error(error, internal_error);

  priv->root = NULL;
  return FALSE;
}

COIL_API(CoilStruct *)
coil_include_get_root_node(CoilInclude *self,
                           GError     **error)
{
  g_return_val_if_fail(COIL_IS_INCLUDE(self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilIncludePrivate *priv = self->priv;

  if (priv->root == NULL
    && !include_load_root(self, error))
    return NULL;

  return priv->root;
}

COIL_API(CoilStruct *)
coil_include_dup_root_node(CoilInclude *self,
                           GError     **error)
{
  g_return_val_if_fail(COIL_IS_INCLUDE(self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStruct *root;

  root = coil_include_get_root_node(self, error);
  if (root)
    g_object_ref(root);

  return root;
}

static gboolean
include_expand(gconstpointer   include,
               const GValue  **return_value,
               GError        **error)
{
  g_return_val_if_fail(COIL_IS_INCLUDE(include), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilInclude        *const self = COIL_INCLUDE(include);
  CoilIncludePrivate *const priv = self->priv;
  CoilStruct         *root;
  GError             *internal_error = NULL;

  root = coil_include_dup_root_node(self, &internal_error);

  if (G_UNLIKELY(internal_error))
    goto error;

  if (priv->import_list == NULL)
  {
    CoilExpandable *const super = COIL_EXPANDABLE(self);
    CoilStruct     *container = super->container;

#ifdef COIL_STRICT_FILE_CONTEXT
    /*
     * XXX: this expands objects in the file root context
     * before merging into container context.
     *
     * ie. links and references to root resolve against 'root'
     * rather than root of 'container'
     */
    if (!coil_struct_merge_full(root,
                                container,
                                FALSE,
                                TRUE,
                                &internal_error))
      goto error;
#else
    if (!coil_struct_merge(root, container, &internal_error))
      goto error;
#endif
  }
  else if (!process_import_list(self, root, error))
    goto error;

  g_object_unref(root);
  priv->is_expanded = TRUE;

  return TRUE;

error:
  if (root)
    g_object_unref(root);

  if (internal_error)
    g_propagate_error(error, internal_error);

  return FALSE;
}

COIL_API(gboolean)
coil_include_equals(gconstpointer   e1,
                    gconstpointer   e2,
                    GError        **error)
{
  COIL_NOT_IMPLEMENTED(FALSE);
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
  GList              *import_list = priv->import_list;
  GError             *internal_error = NULL;

  g_assert(import_list);

  do
  {
    g_string_append_len(buffer, COIL_STATIC_STRLEN("@file: ["));

    coil_value_build_string(priv->filepath_value, buffer,
                            format, &internal_error);

    if (G_UNLIKELY(internal_error))
      goto error;

    g_string_append_c(buffer, ' ');

    coil_value_build_string((GValue *)import_list->data, buffer,
                            format, &internal_error);

    if (G_UNLIKELY(internal_error))
      goto error;

    g_string_append_c(buffer, ']');
  } while ((import_list = g_list_next(import_list)));

  return;

error:
  g_propagate_error(error, internal_error);
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
  GList              *import_list = priv->import_list;
  GError             *internal_error = NULL;
  gboolean            whitespace = !(format->options & COMPACT);

  if (format->options & LEGACY)
  {
    guint num_imports;

    num_imports = g_list_length((GList *)import_list);
    if (num_imports > 1)
    {
      build_legacy_string(self, buffer, format, error);
      return;
    }
  }

  g_string_append_len(buffer, COIL_STATIC_STRLEN("@file:"));

  if (whitespace)
    g_string_append_c(buffer, ' ');

  if (priv->import_list)
  {
    GList *argument_list;

    argument_list = g_list_append(NULL, priv->filepath_value);
    argument_list = g_list_concat(argument_list, import_list);

    coil_list_build_string(argument_list, buffer, format, &internal_error);

    if (G_UNLIKELY(internal_error))
      goto error;

    import_list = g_list_delete_link(argument_list, argument_list);
  }
  else
  {
    coil_value_build_string(priv->filepath_value, buffer,
                            format, &internal_error);

    if (G_UNLIKELY(error))
      goto error;
  }

  return;

error:
  g_propagate_error(error, internal_error);
}

COIL_API(gchar *)
coil_include_to_string(CoilInclude      *self,
                       CoilStringFormat *format,
                       GError          **error)
{
  g_return_val_if_fail(COIL_IS_INCLUDE(self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  GString *buffer = g_string_sized_new(128);
  coil_include_build_string(self, buffer, format, error);

  return g_string_free(buffer, FALSE);
}

static void
coil_include_dispose(GObject *object)
{
  CoilInclude        *self = COIL_INCLUDE(object);
  CoilIncludePrivate *priv = self->priv;

  free_value(priv->filepath_value);
  free_value_list(priv->import_list);

  if (priv->root)
    g_object_unref(priv->root);

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

  switch (property_id)
  {
    case PROP_FILEPATH_VALUE: /* XXX: steals */
    {
      if (priv->filepath_value)
        free_value(priv->filepath_value);

      priv->filepath_value = (GValue *)g_value_get_pointer(value);
      break;
    }

    case PROP_IMPORT_LIST: /* XXX: steals */
    {
      if (priv->import_list)
        free_value_list(priv->import_list);

//      priv->import_list = (GList *)g_value_get_boxed(value);
      priv->import_list = (GList *)g_value_get_pointer(value);
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
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

  switch(property_id)
  {
    case PROP_FILEPATH_VALUE:
      g_value_set_pointer(value, priv->filepath_value);
      break;

    case PROP_IMPORT_LIST:
      g_value_set_pointer(value, priv->import_list);
//      g_value_set_boxed(value, priv->import_list);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
coil_include_init(CoilInclude *self)
{
  self->priv = COIL_INCLUDE_GET_PRIVATE(self);
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

  g_object_class_install_property(gobject_class, PROP_FILEPATH_VALUE,
      g_param_spec_pointer("filepath_value",
                           "Filepath value",
                           "Path of file to import from.",
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(gobject_class, PROP_IMPORT_LIST,
      g_param_spec_pointer("import_list",
                         "Import List",
                         "List of paths to import from file.",
                         /*COIL_TYPE_LIST,*/
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY));

#ifdef COIL_INCLUDE_CACHING
  include_cache_init();
#endif
}

