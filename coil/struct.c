/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include <string.h>

#include "marshal.h"

#include "struct.h"

#include "link.h"
#include "include.h"

G_DEFINE_TYPE(CoilStruct, coil_struct, COIL_TYPE_EXPANDABLE);

#define COIL_STRUCT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), COIL_TYPE_STRUCT, CoilStructPrivate))

struct _CoilStructPrivate
{
  CoilStruct          *root;

  CoilPath            *path;

  StructTable         *table;

  GQueue               entries;
  GQueue               dependencies;
  GList               *expand_ptr;

  guint                size;
  guint                hash;

#if COIL_DEBUG
  guint                version;
#endif

  gboolean             is_prototype : 1;
  gboolean             is_accumulating : 1;
};

typedef struct _ExpandNotify
{
  CoilStruct *object;
  gulong      handler_id;
} ExpandNotify;

typedef enum
{
  PROP_0,
  /* */
  PROP_HASH,
  PROP_IS_PROTOTYPE,
  PROP_IS_ACCUMULATING,
  PROP_PATH,
  PROP_ROOT,
} StructProperties;

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

static CoilStruct *
struct_lookup_container_internal(CoilStruct  *self,
                                 const gchar *path,
                                 guint8       path_len,
                                 GError     **error);

static gboolean
struct_delete_internal(CoilStruct   *self,
                       guint         hash,
                       const gchar  *path,
                       guint8        path_len,
                       gboolean      strict,
                       gboolean      reset_container,
                       GError      **error);

static gboolean
struct_iter_next_entry(CoilStructIter *iter,
                       StructEntry   **entry);

static GError *
struct_expand_notify(GObject *instance,
                     gpointer data);

static void
prototype_cast_notify(GObject    *instance,
                        GParamSpec *arg1,
                        gpointer    data);

static void
struct_connect_expand_notify(CoilStruct *const self,
                             CoilStruct *const parent);

static gboolean
struct_change_notify(CoilStruct  *self,
                     GError     **error);

static const GValue *
struct_lookup_internal(CoilStruct     *self,
                       guint           hash,
                       const gchar    *path,
                       guint8          path_len,
                       gboolean        expand_value,
                       gboolean        expand_container,
                       GError        **error);

static void
struct_build_string_internal(CoilStruct       *self,
                             GString          *const buffer,
                             CoilStringFormat *format,
                             GError          **error);


/* really stupid hack to store root in a
 * gvalue without creating a reference cycle */
static void
destroy_root_struct(gpointer data,
                    GObject *object,
                    gboolean last_ref)
{
  g_return_if_fail(data);
  g_return_if_fail(G_IS_OBJECT(object));

  if (last_ref)
  {
    CoilStruct        *self = COIL_STRUCT(object);
    CoilStructPrivate *priv = self->priv;
    StructEntry       *entry = (StructEntry *)data;

    g_object_ref(object);
    g_object_remove_toggle_ref(object, (GToggleNotify)destroy_root_struct, data);
    struct_table_delete_entry(priv->table, entry);
  }
}

static void
become_root_struct(CoilStruct *self)
{
  g_return_if_fail(COIL_IS_STRUCT(self));

  CoilStructPrivate *priv = self->priv;
  GValue            *value;
  StructEntry       *entry;

  priv->hash = 0;

  if (priv->path)
    coil_path_unref(priv->path);
  priv->path = CoilRootPath;
  priv->root = self;

  if (priv->table)
    struct_table_unref(priv->table);
  priv->table = struct_table_new();

  g_object_set(G_OBJECT(self), "container", NULL, NULL);

  coil_value_init(value, COIL_TYPE_STRUCT, take_object, self);

  entry = struct_table_insert(priv->table, 0,
                              coil_path_ref(priv->path), value);

  g_object_add_toggle_ref(G_OBJECT(self),
                          (GToggleNotify)destroy_root_struct,
                          entry);
}

/**
 * Change the container of a struct.
 *
 * A few things need to happen here
 *
 * (1) If the current struct is still in the entry table of the
 * prior container we need to remove it.
 *
 * If we're setting new container to null we make self @root
 *
 * (2) We need to update the path and hash of the struct and its entries.
 *
 * (3) Minimize recomputing hash values and copying paths.
 */

static gboolean
struct_change_container(CoilStruct *self,
                        CoilStruct *container,
                        CoilPath   *path, /* NULL if do not have */
                        guint      *hash, /* NULL if do not have */
                        gboolean    reset_container,
                        GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(container == NULL || COIL_IS_STRUCT(container), FALSE);
  g_return_val_if_fail(self != container, FALSE);
  g_return_val_if_fail(!coil_struct_is_root(self) || path != NULL, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *const priv = self->priv;
  CoilExpandable    *const super = COIL_EXPANDABLE(self);
  CoilStructIter     it;
  CoilStruct        *old_container = super->container;
  StructTable       *old_table = NULL;
  StructEntry       *entry;
  GError            *internal_error = NULL;

  /* remove self from previous container */
  /* XXX: better have an extra reference */
  if (reset_container && old_container)
  {
    g_assert(G_OBJECT(self)->ref_count > 1);
    if (!struct_delete_internal(old_container, priv->hash,
                                priv->path->path, priv->path->path_len,
                                FALSE, FALSE, &internal_error))
      goto error;
  }

  if (container)
  {
    /* struct is NOT root */

    if (!old_container)
    {
      /* struct WAS root */
      StructEntry *entry;

      entry = struct_table_lookup(priv->table, 0,
                                  COIL_ROOT_PATH, COIL_ROOT_PATH_LEN);
      g_assert(entry);
      g_object_ref(self);
      g_object_remove_toggle_ref(G_OBJECT(self),
                                 (GToggleNotify)destroy_root_struct,
                                 entry);
      struct_table_delete_entry(priv->table, entry);
    }

    if (!path)
    {
      /* new path is not already built, we have to build one */
      const CoilPath *container_path;

      container_path = coil_struct_get_path(container);
      if (!coil_path_change_container(&path, container_path, &internal_error))
        goto error;
    }

    if (priv->path)
      coil_path_unref(priv->path);
    priv->path = coil_path_ref(path);

    if (hash)
      priv->hash = *hash;
    else
      priv->hash = hash_relative_path(container->priv->hash,
                                      priv->path->key,
                                      priv->path->key_len);

    old_table = priv->table;
    priv->table = struct_table_ref(container->priv->table);
    priv->root = coil_struct_get_root(container);

    g_object_set(G_OBJECT(self), "container", container, NULL);
  }
  else
  {
    old_table = struct_table_ref(priv->table);
    become_root_struct(self);
  }

  /* iterate through key-values and update paths entry table */
  coil_struct_iter_init(&it, self);

  while (struct_iter_next_entry(&it, &entry))
  {
    /* remove each entry from current entry table */
    struct_table_remove_entry(old_table, entry);

    /* update path with proper container */
    if (!coil_path_change_container(&entry->path, priv->path, &internal_error))
      goto error;

    /* add entry back into correct entry table, possibly new */
    entry->hash = hash_relative_path(priv->hash,
                                     entry->path->key,
                                     entry->path->key_len);

    struct_table_insert_entry(priv->table, entry);

    /* if value is struct recursively change containers */
    if (entry->value && G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT))
    {
      CoilStruct *object;

      object = COIL_STRUCT(g_value_get_object(entry->value));
      if (!struct_change_container(object, self,
                                   entry->path, &entry->hash,
                                   FALSE, &internal_error))
        goto error;
    }
  }

  struct_table_unref(old_table);

  return TRUE;

error:
  if (internal_error)
    g_propagate_error(error, internal_error);

  if (old_table)
    struct_table_unref(old_table);

  return FALSE;
}

COIL_API(GQuark)
coil_struct_prototype_quark(void)
{
  static GQuark result = 0;

  if (G_UNLIKELY(!result))
    result = g_quark_from_static_string("prototype");

  return result;
}

static CoilPath * /* resolved path */
struct_resolve_path(CoilStruct     *self,
                    const CoilPath *path, /* unresolved path */
                    guint          *return_hash, /* return ptr for hash of path */
                    GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(return_hash, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStructPrivate *const priv = self->priv;

  if (COIL_PATH_IS_ABSOLUTE(path))
  {
    *return_hash = hash_absolute_path(path->path, path->path_len);
    return coil_path_ref((CoilPath *)path);
  }

  if (COIL_PATH_IS_BACKREF(path))
  {
    CoilPath *resolved = coil_path_resolve(path, priv->path, error);

    if (resolved)
      *return_hash = hash_absolute_path(resolved->path, resolved->path_len);

    return resolved;
  }

  *return_hash = hash_relative_path(priv->hash, path->path, path->path_len);
  return coil_path_resolve(path, priv->path, error);
}

static gboolean
struct_resolve_path_into(CoilStruct *self,
                         CoilPath  **path,
                         guint      *return_hash,
                         GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path && *path, FALSE);
  g_return_val_if_fail(return_hash, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilPath *resolved;

  resolved = struct_resolve_path(self, *path, return_hash, error);
  if (resolved == NULL)
    return FALSE;

  coil_path_unref(*path);
  *path = resolved;

  return TRUE;
}

/*
 * coil_struct_empty: Clear all keys and values in CoilStruct
 *
 * @self: A CoilStruct instance.
 */
COIL_API(void)
coil_struct_empty(CoilStruct *self,
                  GError    **error)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  g_return_if_fail(error == NULL || *error == NULL);

  CoilStructPrivate *const priv = self->priv;

  /*
  GError            *internal_error = NULL;
  g_signal_emit(self, struct_signals[MODIFY], 0,
                (gpointer *)&internal_error);

  if (G_UNLIKELY(internal_error))
  {
    g_propagate_error(error, internal_error);
    return;
  }*/

  CoilExpandable *object;
  while ((object = g_queue_pop_head(&priv->dependencies)))
    g_object_unref(object);

  StructEntry *entry;
  while ((entry = g_queue_pop_head(&priv->entries)))
    struct_table_delete_entry(priv->table, entry);

  priv->size = 0;

#if COIL_DEBUG
  priv->version = 0;
#endif
}


COIL_API(gboolean)
coil_struct_is_root(const CoilStruct *self)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);

  return coil_struct_get_root(self) == self;
}

COIL_API(gboolean)
coil_struct_is_prototype(const CoilStruct *self)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);

  return self->priv->is_prototype;
}

static gboolean
struct_needs_expand(gconstpointer s)
{
  g_return_val_if_fail(COIL_IS_STRUCT(s), FALSE);

  CoilStruct        *self = COIL_STRUCT(s);
  CoilStructPrivate *priv = self->priv;
  GQueue            *deps = &priv->dependencies;

  if (g_queue_is_empty(deps))
    return FALSE;

  return priv->expand_ptr != NULL;
}

COIL_API(gboolean)
coil_struct_is_empty(CoilStruct *self,
                     GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);

  if (self->priv->size)
    return FALSE;

  if (!struct_needs_expand(self)
    && !coil_struct_expand(self, error))
    return FALSE;

  return self->priv->size == 0;
}

COIL_API(gboolean)
coil_struct_is_ancestor(const CoilStruct *ancestor,
                        const CoilStruct *descendent)
{
  g_return_val_if_fail(COIL_IS_STRUCT(ancestor), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(descendent), FALSE);
  g_return_val_if_fail(ancestor != descendent, FALSE);

  const CoilStruct *container = descendent;
  while ((container = coil_struct_get_container(container)))
  {
    if (container == ancestor)
      return TRUE;
  }

  return FALSE;
}

COIL_API(gboolean)
coil_struct_is_descendent(const CoilStruct *descendent,
                          const CoilStruct *ancestor)
{
  g_return_val_if_fail(COIL_IS_STRUCT(descendent), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(ancestor), FALSE);
  g_return_val_if_fail(ancestor != descendent, FALSE);

  return coil_struct_is_ancestor(ancestor, descendent);
}

COIL_API(CoilStruct *)
coil_struct_get_root(const CoilStruct *self)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);

  return self->priv->root;
}

COIL_API(CoilStruct *)
coil_struct_get_container(const CoilStruct *self)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);

  return COIL_EXPANDABLE(self)->container;
}

COIL_API(const CoilPath *)
coil_struct_get_path(const CoilStruct *self)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);

  return self->priv->path;
}

COIL_API(gboolean)
coil_struct_compare_root(const CoilStruct *a,
                          const CoilStruct *b)
{
  g_return_val_if_fail(COIL_IS_STRUCT(a), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(b), FALSE);

  if (a == b)
    return TRUE;

  if (coil_struct_is_root(a))
    return coil_struct_is_descendent(b, a);

  if (coil_struct_is_root(b))
    return coil_struct_is_descendent(a, b);

  return coil_struct_get_root(a) == coil_struct_get_root(b);
}

COIL_API(void)
coil_struct_foreach_ancestor(CoilStruct     *self,
                             gboolean        include_self,
                             CoilStructFunc  func,
                             gpointer        user_data)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  g_return_if_fail(func);

  CoilStruct *container;
  gboolean    keep_going = TRUE;

  if (include_self)
    keep_going = (*func)(self, user_data);

  container = coil_struct_get_container(self);
  while (keep_going && container)
  {
    keep_going = (*func)(container, user_data);
    container = coil_struct_get_container(container);
  }
}

gboolean
make_prototype_final(CoilStruct *self,
                     gpointer    unused)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);

  if (coil_struct_is_prototype(self))
  {
    g_object_set(self,
                 "is-prototype", FALSE,
                 NULL);

    return TRUE;
  }

  return FALSE;
}

static gboolean
insert_with_existing_entry(CoilStruct  *self,
                           StructEntry *entry,
                           CoilPath    *path, /* steals */
                           GValue      *value, /* steals */
                           gboolean     replace,
                           GError     **error)
{
  /* entry exists for path (value may be null) */
  GValue *old_value = entry->value;

  if (old_value != NULL
    && G_VALUE_HOLDS(old_value, COIL_TYPE_STRUCT))
  {
    CoilStruct *src, *dst;

    src = COIL_STRUCT(g_value_get_object(value));
    dst = COIL_STRUCT(g_value_get_object(old_value));

    if (src != dst && coil_struct_is_prototype(dst))
    {
      if (!G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
      {
        /* XXX: old value is struct prototype, new value is not struct */
        coil_struct_error(error, self,
                          "Attempting to overwrite struct prototype value "
                          "'%s' with non struct value. This implies struct "
                          "is used but never defined.",
                          path->path);

        return FALSE;
      }

      /* XXX: Overwriting a prototype.
       * Merge the items from struct we're trying to set now
       * and destroy it (leaving the prototype in place but casting to
       * non-prototype).
       */
      if (!coil_struct_merge_full(src, dst, TRUE, FALSE, error))
        return FALSE;

      if (path)
        coil_path_unref(path);

      if (value)
        coil_value_free(value);

      return TRUE;
    }
  }
  else if (!replace)
  {
     coil_struct_error(error, self,
                       "Attempting to insert value at path '%s' "
                       "which already exists.",
                       path->path);

     return FALSE;
  }

  /* entry exists, overwrite value */
  if (old_value)
    coil_value_free(old_value);
  entry->value = value;

  if (entry->path)
    coil_path_unref(entry->path);
  entry->path = path;

  return TRUE;
}

static gboolean
struct_insert_internal(CoilStruct     *self,
                       CoilPath       *path, /* steals */
                       GValue         *value, /* steals */
                       guint           hash,
                       gboolean        replace, /* TRUE to replace old value */
                       GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(path), FALSE);
  g_return_val_if_fail(!COIL_PATH_IS_ROOT(path), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *const priv = self->priv;
  StructEntry       *entry;
  GError            *internal_error = NULL;
  gboolean           value_is_struct = FALSE;

  g_return_val_if_fail(g_str_has_prefix(path->path, priv->path->path),
                       FALSE);

  g_return_val_if_fail(COIL_PATH_CONTAINER_LEN(path) == priv->path->path_len,
                       FALSE);

  if (!struct_change_notify(self, &internal_error))
    goto error;

  /* TODO(jcon): dont check value for struct more than once */
  if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
  {
    CoilStruct *object;

    object = COIL_STRUCT(g_value_get_object(value));
    if (coil_struct_is_ancestor(object, self))
    {
       coil_struct_error(error, self,
                         "Attempting to insert ancestor struct '%s' into '%s'.",
                         coil_struct_get_path(object)->path,
                         coil_struct_get_path(self)->path);
       goto error;
    }

    value_is_struct = TRUE;
  }

  entry = struct_table_lookup(priv->table, hash,
                              path->path, path->path_len);

  if (entry == NULL)
  {
    entry = struct_table_insert(priv->table, hash, path, value);
    g_queue_push_tail(&priv->entries, entry);
    priv->size++;

    if (!struct_change_notify(self, &internal_error))
      goto error;
  }
  else if (!insert_with_existing_entry(self, entry,
                                       path, value,
                                       replace, error))
    goto error;

#if COIL_DEBUG
  priv->version++;
#endif

  /* XXX: if value is struct, path will change based on container. */
  /* TODO(jcon): implement set_container in expandable */
  if (value_is_struct)
  {
    CoilStruct *object, *container;

    object = COIL_STRUCT(g_value_get_object(value));
    container = coil_struct_get_container(object);

    if (container != self
      && !struct_change_container(object, self,
                                  path, &hash,
                                  TRUE, &internal_error))
      goto error;

    /* XXX: inserting a prototype does not provoke a cast */
    if (!coil_struct_is_prototype(object))
      coil_struct_foreach_ancestor(self, TRUE, make_prototype_final, NULL);

    return TRUE;
  }

  if (G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE))
  {
    CoilExpandable *object;

    object = COIL_EXPANDABLE(g_value_get_object(value));
    g_object_set(G_OBJECT(object),
                 "container", self,
                 NULL);
  }

  coil_struct_foreach_ancestor(self, TRUE, make_prototype_final, NULL);
  return TRUE;

error:
  if (internal_error)
    g_propagate_error(error, internal_error);

  if (path)
    coil_path_unref(path);

  if (value)
    coil_value_free(value);

  return FALSE;
}

static void
compute_path_hash_with_len(const gchar *path,
                     guint8       path_len,
                     guint       *hashes,
                     guint8      *lens,
                     guint8      *num_parts)
{
  g_return_if_fail(path);
  g_return_if_fail(path_len > 0);
  g_return_if_fail(lens);
  g_return_if_fail(hashes);

  guint8       i;
  const gchar *end, *delim, *next;

  memset(lens, 0, sizeof(*lens) * COIL_PATH_MAX_PARTS);
  memset(hashes, 0, sizeof(*hashes) * COIL_PATH_MAX_PARTS);

  lens[0] = COIL_ROOT_PATH_LEN;
  hashes[0] = 0;

  end = path + path_len;
  delim = path + COIL_ROOT_PATH_LEN + 1;

  next = memchr(delim, COIL_PATH_DELIM, end - delim);
  for (i = 1; next != NULL; i++)
  {
    lens[i] = next - path;
    hashes[i] = hash_relative_path(hashes[i - 1], delim, next - delim);
    delim = next + 1;
    next = memchr(delim, COIL_PATH_DELIM, end - delim);
  }

  lens[i] = path_len;
  hashes[i] = hash_relative_path(hashes[i - 1], delim, end - delim);

  if (num_parts)
    *num_parts = i;
}

COIL_API(CoilStruct *)
coil_struct_create_containers_fast(CoilStruct     *self,
                                   const gchar    *path,
                                   guint8          path_len,
                                   gboolean        prototype,
                                   gboolean        has_lookup,
                                   GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(*path == '@', NULL);
  g_return_val_if_fail(path_len >= COIL_ROOT_PATH_LEN, NULL);
  g_return_val_if_fail(path_len <= COIL_PATH_LEN, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStructPrivate *priv = self->priv;
  CoilStruct        *container;
  guint              hashes[COIL_PATH_MAX_PARTS];
  guint8             lens[COIL_PATH_MAX_PARTS], i, missing_keys = 0;

  if (path_len == COIL_ROOT_PATH_LEN)
    return coil_struct_get_root(self);

  compute_path_hash_with_len(path, path_len, hashes, lens, &i);

  g_assert(i > 0);

  if (has_lookup)
  {
    i--;
    missing_keys++;
  }

  for (container = coil_struct_get_root(self);
       i > 0; missing_keys++, i--)
  {
    StructEntry *entry;

    entry = struct_table_lookup(priv->table, hashes[i], path, lens[i]);
    if (entry == NULL)
      continue;

    if (entry->value == NULL)
    {
      coil_struct_error(error, self,
                        "Attempting to insert value within a previously "
                        "deleted key %.*s",
                        lens[i], path);

      return NULL;
    }

    if (!G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT))
    {
      coil_struct_error(error, self,
                        "Attempting to assign values in non-struct "
                        "object %.*s type '%s'.",
                        lens[i], path,
                        G_VALUE_TYPE_NAME(entry->value));

      return NULL;
    }

    container = COIL_STRUCT(g_value_get_object(entry->value));
    break;
  }

  if (!prototype)
    coil_struct_foreach_ancestor(container, TRUE,
                                 make_prototype_final, NULL);

  for (i++; container && missing_keys > 0;
      missing_keys--, i++)
  {
    CoilPath   *p;
    gchar      *str, *key;
    guint8      key_len;
    CoilStruct *prev;

    prev = g_object_ref(container);
    str = g_strndup(path, lens[i]);
    key = &str[lens[i - 1]] + 1;
    key_len = (lens[i] - lens[i - 1]) - 1;

    p = coil_path_take_strings(str, lens[i], key, key_len,
                               COIL_PATH_IS_ABSOLUTE | COIL_STATIC_KEY);

    container = coil_struct_new(error,
                                "container", container,
                                "path", p,
                                "hash", hashes[i],
                                "is-prototype", prototype,
                                NULL);

    coil_path_unref(p);
    g_object_unref(prev);
  }

  return container;
}

COIL_API(CoilStruct *)
coil_struct_create_containers(CoilStruct  *self,
                              const gchar *path,
                              guint        path_len,
                              gboolean     prototype,
                              gboolean     has_lookup,
                              GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(path_len > 0, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (!coil_check_path(path, path_len, error))
    return NULL;

  return coil_struct_create_containers_fast(self,
                                            path, path_len,
                                            prototype, has_lookup,
                                            error);
}



COIL_API(gboolean)
coil_struct_insert_path(CoilStruct *self,
                        CoilPath   *path, /* steals */
                        GValue     *value, /* steals */
                        gboolean    replace,
                        GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(value, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStruct        *container;
  guint              hash = 0;

  if (!struct_resolve_path_into(self, &path, &hash, error))
    goto error;

  if (COIL_PATH_IS_ROOT(path))
  {
    coil_struct_error(error, self,
                      "Cannot assign a value directly to @root.");

    goto error;
  }

  container = coil_struct_create_containers(self, path->path,
                                            COIL_PATH_CONTAINER_LEN(path),
                                            TRUE, FALSE, error);

  if (G_UNLIKELY(container == NULL))
    goto error;

  return struct_insert_internal(container, path, value, hash, replace, error);

error:
  if (path)
    coil_path_unref(path);

  if (value)
    coil_value_free(value);

  return FALSE;
}

COIL_API(gboolean)
coil_struct_insert(CoilStruct  *self,
                   gchar       *path, /* steals */
                   guint        path_len,
                   GValue      *value, /* steals */
                   gboolean     replace,
                   GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path && *path, FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(value, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (!coil_check_path(path, path_len, error))
    return FALSE;

  return coil_struct_insert_fast(self, path, (guint8)path_len,
                                 value, replace, error);
}

COIL_API(gboolean)
coil_struct_insert_fast(CoilStruct *self,
                        gchar      *path,
                        guint8      path_len,
                        GValue     *value,
                        gboolean    replace,
                        GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path && *path, FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(value, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilPath *p = coil_path_take_strings(path, path_len, NULL, 0, 0);
  return coil_struct_insert_path(self, p, value, replace, error);
}

COIL_API(gboolean)
coil_struct_insert_key(CoilStruct   *self,
                       const gchar  *key,
                       guint         key_len,
                       GValue       *value,
                       gboolean      replace,
                       GError      **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(key && *key, FALSE);
  g_return_val_if_fail(key_len, FALSE);
  g_return_val_if_fail(value, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *priv = self->priv;
  CoilPath          *path;
  guint              hash;

  hash = hash_relative_path(priv->hash, key, key_len);

  path = coil_build_path(error, priv->path->path, key, NULL);
  if (path == NULL)
    return FALSE;

  return struct_insert_internal(self, path, value, hash, replace, error);
}

static gboolean
struct_remove_entry(CoilStruct            *self,
                    const StructEntry     *entry,
                    GError               **error)
{
  CoilStructPrivate *const priv = self->priv;

#if 0
  GError            *internal_error = NULL;
  g_signal_emit(self, struct_signals[MODIFY],
                0, (gpointer *)&internal_error);

  if (G_UNLIKELY(internal_error))
  {
    g_propagate_error(error, internal_error);
    return FALSE;
  }
#endif

  g_queue_remove(&priv->entries, (gpointer)entry);

#if COIL_DEBUG
  priv->version++;
#endif

  priv->size--;
  return TRUE;
}

static gboolean
struct_delete_entry(CoilStruct      *self,
                    StructEntry     *entry,
                    GError         **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(entry, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *const priv = self->priv;

  if (!struct_remove_entry(self, entry, error))
      return FALSE;

  struct_table_delete_entry(priv->table, entry);

  return TRUE;
}

static gboolean
struct_delete_internal(CoilStruct   *self,
                       guint         hash,
                       const gchar  *path,
                       guint8        path_len,
                       gboolean      strict,
                       gboolean      reset_container,
                       GError      **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(!coil_struct_is_prototype(self), FALSE);
  g_return_val_if_fail(path && *path && *path == '@', FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *const priv = self->priv;
  CoilStruct        *container;
  StructEntry       *entry;

  entry = struct_table_lookup(priv->table, hash, path, path_len);

  if (entry == NULL)
  {
    if (strict)
    {
      coil_struct_error(error, self,
                        "Attempting to delete non-existent path '%s'.",
                        path);

      return FALSE;
    }

    return TRUE;
  }

  /* TODO: when/if values become CoilObjects we should store
   * the container with each value */
  container = struct_lookup_container_internal(self, path, path_len, error);
  if (container == NULL)
    return FALSE;

  if (reset_container
    && G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT))
  {
    GObject *object = g_value_get_object(entry->value);
    if (!struct_change_container(COIL_STRUCT(object),
                                 NULL, NULL, NULL, FALSE,
                                 error))
      return FALSE;
  }

  return struct_delete_entry(container, entry, error);
}

COIL_API(gboolean)
coil_struct_delete_path(CoilStruct *self,
                        CoilPath   *path,
                        gboolean    strict,
                        GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  guint    hash = 0;
  gboolean result = FALSE;

  coil_path_ref(path);

  if (!struct_resolve_path_into(self, &path, &hash, error))
    goto error;

  if (COIL_PATH_IS_ROOT(path))
  {
    coil_struct_error(error, self,
                      "Cannot delete '@root' path.");

    goto error;
  }

  result = struct_delete_internal(self, hash,
                                  path->path, path->path_len,
                                  strict, TRUE, error);

error:
  coil_path_unref((CoilPath *)path);

  return result;
}

COIL_API(gboolean)
coil_struct_delete(CoilStruct  *self,
                   const gchar *path,
                   guint        path_len,
                   gboolean     strict,
                   GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path && *path, FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilPath *p;

  if (!coil_check_path(path, path_len, error))
    return FALSE;

  p = coil_path_take_strings((gchar *)path, path_len,
                             NULL, 0,
                             COIL_STATIC_PATH);

  return coil_struct_delete_path(self, p, strict, error);
}

COIL_API(gboolean)
coil_struct_delete_key(CoilStruct  *self,
                       const gchar *key,
                       guint        key_len,
                       gboolean     strict,
                       GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(key && *key, FALSE);
  g_return_val_if_fail(key_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail(!memchr(key, COIL_PATH_DELIM, key_len), FALSE);

  CoilStructPrivate *const priv = self->priv;
  const CoilPath    *const p = priv->path;
  guint              hash;
  gchar             *path;
  guint8             path_len;

  if (!coil_check_key(key, key_len, error))
    return FALSE;

  hash = hash_relative_path(priv->hash, key, key_len);

  COIL_PATH_QUICK_BUFFER(path, path_len,
                         p->path, p->path_len,
                         key, key_len);

  return struct_delete_internal(self, hash,
                                path, path_len,
                                strict, TRUE,
                                error);
}

static gboolean
struct_mark_deleted_internal(CoilStruct  *self,
                             CoilPath    *path, /* steal */
                             guint        hash,
                             gboolean     force,
                             GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(COIL_PATH_IS_ABSOLUTE(path), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *const priv = self->priv;
  StructEntry       *entry;

  entry = struct_table_lookup(priv->table, hash,
                              path->path, path->path_len);

  if (entry && !force)
  {
    if (entry->value == NULL)
       coil_struct_error(error, self,
         "Attempting to delete '%s' twice.",
         path->key);
    else
       coil_struct_error(error, self,
         "Attempting to delete first-order key '%s'.",
         path->key);

    goto error;
  }
/*
  GError *internal_error = NULL;
  g_signal_emit(self, struct_signals[MODIFY], 0,
      (gpointer *)&internal_error);

  if (G_UNLIKELY(internal_error))
  {
    g_propagate_error(error, internal_error);
    goto error;
  }
*/

  struct_table_insert(priv->table, hash, path, NULL);

#if COIL_DEBUG
  priv->version++;
#endif

  return TRUE;

error:
  coil_path_unref(path);
  return FALSE;
}

COIL_API(gboolean)
coil_struct_mark_deleted_path(CoilStruct     *self,
                              CoilPath       *path, /* steal */
                              gboolean        force,
                              GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStruct *container;
  guint       hash = 0;

  if (!struct_resolve_path_into(self, &path, &hash, error))
    return FALSE;

  if (COIL_PATH_IS_ROOT(path))
  {
    coil_struct_error(error, self,
        "Cannot mark @root as deleted.");

    return FALSE;
  }

  container = coil_struct_create_containers(self,
                                            path->path,
                                            COIL_PATH_CONTAINER_LEN(path),
                                            TRUE, FALSE,
                                            error);

  if (G_UNLIKELY(container == NULL))
    return FALSE;

  return struct_mark_deleted_internal(container, path, hash, force, error);
}

COIL_API(gboolean)
coil_struct_mark_deleted_fast(CoilStruct *self,
                              gchar      *path,
                              guint8      path_len,
                              gboolean    force,
                              GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path && *path, FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilPath *_path;

  _path = coil_path_take_strings(path, path_len, NULL, 0, 0);

  return coil_struct_mark_deleted_path(self, _path, force, error);
}

COIL_API(gboolean)
coil_struct_mark_deleted(CoilStruct  *self,
                         gchar       *path, /* steal */
                         guint        path_len,
                         gboolean     force,
                         GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path && *path, FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (!coil_check_path(path, path_len, error))
    return FALSE;

  return coil_struct_mark_deleted_fast(self, path, path_len, force, error);
}

COIL_API(gboolean)
coil_struct_mark_deleted_key(CoilStruct  *self,
                             const gchar *key,
                             gboolean     force,
                             GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(key && *key, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *priv = self->priv;
  CoilPath          *path;
  guint              hash;

  path = coil_build_path(error, priv->path->path, key, NULL);
  if (path == NULL)
    return FALSE;

  hash = hash_relative_path(priv->hash, path->key, path->key_len);

  return struct_mark_deleted_internal(self, path, hash, force, error);
}

static gboolean
check_parent_sanity(const CoilStruct *self,
                    const CoilStruct *parent,
                    GError          **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(parent), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (G_UNLIKELY(self == parent))
  {
    coil_struct_error(error, self,
      "cannot extend self.");

    return FALSE;
  }

  if (G_UNLIKELY(coil_struct_is_root(self)))
  {
    coil_struct_error(error, self,
        "@root cannot extend anything.");

    return FALSE;
  }

  if (G_UNLIKELY(coil_struct_is_root(parent)))
  {
    coil_struct_error(error, self,
      "@root cannot be extended.");

    return FALSE;
  }

  if (G_UNLIKELY(coil_struct_is_ancestor(parent, self)))
  {
    coil_struct_error(error, self,
      "cannot extend parent containers.");

    return FALSE;
  }

  if (G_UNLIKELY(coil_struct_is_descendent(parent, self)))
  {
    coil_struct_error(error, self,
      "cannot extend children.");

    return FALSE;
  }

  if (G_UNLIKELY(!coil_struct_compare_root(self, parent)))
  {
    G_BREAKPOINT();
    coil_struct_error(error, self,
      "cannot extend structs in disjoint roots.");

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

COIL_API(gboolean)
coil_struct_add_dependency(CoilStruct     *self,
                           gpointer        object,
                           GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(COIL_IS_EXPANDABLE(object), FALSE);
  g_return_val_if_fail(self != object, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *const priv = self->priv;
  GType              type = G_OBJECT_TYPE(object);
  GError            *internal_error = NULL;

  if (struct_needs_expand(self))
  {
    coil_struct_error(error, self,
        "Cannot add dependencies after struct expansion");
    return FALSE;
  }
  if (!struct_check_dependency_type(type))
  {
    g_error("Adding invalid dependency type '%s'.",
        G_OBJECT_TYPE_NAME(object));
  }

  g_signal_emit(self,
                struct_signals[ADD_DEPENDENCY],
                g_type_qname(type),
                object,
                &internal_error);

  if (G_UNLIKELY(internal_error))
  {
    g_propagate_error(error, internal_error);
    return FALSE;
  }

  if (COIL_IS_STRUCT(object))
  {
    CoilStruct *parent = COIL_STRUCT(object);

    if(G_UNLIKELY(!check_parent_sanity(self, parent, error)))
      return FALSE;

    struct_connect_expand_notify(self, parent);
  }
  else
  {
    CoilStruct *container;

    g_object_get(G_OBJECT(object),
                 "container", &container,
                 NULL);

    if (!coil_struct_compare_root(self, container))
    {
      coil_struct_error(error, self,
                        "Adding dependency in '%s' from a different @root.",
                        priv->path->path);

      g_object_unref(container);
      return FALSE;
    }

    g_object_unref(container);
  }

  g_object_ref(object);
  g_queue_push_tail(&priv->dependencies, object);

#if COIL_DEBUG
  priv->version++;
#endif

  if (coil_struct_is_prototype(self))
  {
    g_object_set(G_OBJECT(self),
                "is-prototype", FALSE,
                 NULL);
  }

  return TRUE;
}

static void
expand_notify_free(gpointer  data,
                   GClosure *closure)
{
  g_return_if_fail(data);
  g_free(data);
}

static GError *
struct_expand_notify(GObject *instance, /* parent */
                     gpointer data) /* ExpandNotify */
{
  g_return_val_if_fail(COIL_IS_STRUCT(instance), NULL);
  g_return_val_if_fail(data, NULL);

  ExpandNotify      *notify = (ExpandNotify *)data;
  CoilStruct        *self = notify->object;
  GError            *internal_error = NULL;

  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);

  g_signal_handler_disconnect(instance, notify->handler_id);
  /* notify = NULL */

  coil_struct_expand(self, &internal_error);

  return internal_error;
}

static void
prototype_cast_notify(GObject    *instance, /* parent */
                      GParamSpec *unused, /* ignore argument */
                      gpointer    data) /* ExpandNotify */
{
  g_return_if_fail(COIL_IS_STRUCT(instance));
  g_return_if_fail(data);

  CoilStruct   *self, *parent;
  ExpandNotify *notify = (ExpandNotify *)data;

  self = notify->object;
  parent = COIL_STRUCT(instance);

  if (!coil_struct_is_prototype(parent))
  {
    g_signal_handler_disconnect(instance, notify->handler_id);

    notify = g_new(ExpandNotify, 1);
    notify->object = self;
    notify->handler_id = g_signal_connect_data(instance, "modify",
                                               G_CALLBACK(struct_expand_notify),
                                               notify, expand_notify_free, 0);
  }
}

static void
struct_connect_expand_notify(CoilStruct *const self,
                             CoilStruct *const parent)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  g_return_if_fail(COIL_IS_STRUCT(parent));

  ExpandNotify *notify = g_new(ExpandNotify, 1);
  const gchar  *detailed_signal;
  GCallback     callback;

  notify->object = self;

  if (coil_struct_is_prototype(parent))
  {
    detailed_signal = "notify::is-prototype";
    callback = G_CALLBACK(prototype_cast_notify);
  }
  else
  {
    detailed_signal = "modify";
    callback = G_CALLBACK(struct_expand_notify);
  }

  notify->handler_id = g_signal_connect_data(parent, detailed_signal, callback,
                                             notify, expand_notify_free, 0);
}

COIL_API(gboolean)
coil_struct_extend(CoilStruct  *self,
                   CoilStruct  *parent,
                   GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(parent), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  return coil_struct_add_dependency(self, COIL_EXPANDABLE(parent), error);
}

COIL_API(gboolean)
coil_struct_extend_path(CoilStruct  *self,
                        CoilPath    *path,
                        CoilStruct  *context,
                        GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(context == NULL || COIL_IS_STRUCT(context), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilExpandable *dependency;
  const GValue   *value;
  guint           hash = 0;
  GError         *internal_error = NULL;

  coil_path_ref(path);

  if (context == NULL)
    context = self;

  if (!struct_resolve_path_into(context, &path, &hash, error))
    goto error;

  value = struct_lookup_internal(context, hash,
                                 path->path, path->path_len,
                                 FALSE, FALSE, &internal_error);

  if (G_UNLIKELY(internal_error))
    goto error;

  if (value)
  {
    GType type = G_VALUE_TYPE(value);

    if (G_UNLIKELY(!struct_check_dependency_type(type)))
    {
      coil_struct_error(error, self,
        "parent '%s' must be a struct, found type '%s'.",
        path->path, g_type_name(type));

      goto error;
    }

    dependency = COIL_EXPANDABLE(g_value_get_object(value));
  }
  else
  {
    CoilStruct *container;

    container = coil_struct_create_containers(context,
                                              path->path, path->path_len,
                                              TRUE, TRUE, &internal_error);

    if (G_UNLIKELY(container == NULL))
      goto error;

    dependency = COIL_EXPANDABLE(container);
  }

  coil_path_unref(path);

  return coil_struct_add_dependency(self, dependency, error);

error:
  if (path)
    coil_path_unref(path);

  if (internal_error)
    g_propagate_error(error, internal_error);

  return FALSE;
}

COIL_API(gboolean)
coil_struct_extend_paths(CoilStruct *self,
                         GList      *path_list, /* steals */
                         CoilStruct *context,
                         GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path_list, FALSE);
  g_return_val_if_fail(context == NULL || COIL_IS_STRUCT(context), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  while (path_list)
  {
    CoilPath *path = (CoilPath *)path_list->data;

    if (G_UNLIKELY(!coil_struct_extend_path(self, path, context, error)))
      return FALSE;

    path_list = g_list_next(path_list);
  }

  return TRUE;
}


COIL_API(void)
coil_struct_iter_init(CoilStructIter *iter,
                      CoilStruct     *self)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  g_return_if_fail(!coil_struct_is_prototype(self));
  g_return_if_fail(iter);

  CoilStructPrivate *const priv = self->priv;
  iter->node = self;
#if COIL_DEBUG
  iter->version = priv->version;
#endif
  iter->position = g_queue_peek_head_link(&priv->entries);
}

static gboolean
struct_iter_next_entry(CoilStructIter *iter,
                       StructEntry   **entry)
{
  g_return_val_if_fail(iter, FALSE);
  g_return_val_if_fail(entry, FALSE);

#if COIL_DEBUG
  const CoilStruct *self = iter->node;

  if (self == NULL
    || !G_IS_OBJECT(self))
      g_error("%s: Likely using uninitialized iterator", G_STRLOC);

  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(iter->version == self->priv->version, FALSE);
#endif

  if (!iter->position)
    return FALSE;

  *entry = (StructEntry *)iter->position->data;
  iter->position = g_list_next(iter->position);

  return TRUE;
}

COIL_API(gboolean)
coil_struct_iter_next(CoilStructIter  *iter,
                      const CoilPath **path,
                      const GValue   **value)
{
  g_return_val_if_fail(iter, FALSE);
  g_return_val_if_fail(path || value, FALSE);

  StructEntry *entry;

  if (!struct_iter_next_entry(iter, &entry))
    return FALSE;

  if (path)
    *path = entry->path;

  if (value)
    *value = entry->value;

  return TRUE;
}

COIL_API(gboolean)
coil_struct_iter_next_expand(CoilStructIter  *iter,
                             const CoilPath **path,
                             const GValue   **value,
                             gboolean         recursive,
                             GError         **error)
{
  g_return_val_if_fail(iter, FALSE);
  g_return_val_if_fail(value, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (!coil_struct_iter_next(iter, path, value))
    return FALSE;

  if (!struct_change_notify(iter->node, error))
    return FALSE;

  /* stop iteration on error */
  if (G_VALUE_HOLDS(*value, COIL_TYPE_EXPANDABLE))
    return coil_expand_value(*value, value, recursive, error);

  return TRUE;
}

/* Call before making implicit or hidden changes in struct to notify ancestry */
static gboolean
struct_change_notify(CoilStruct  *self,
                     GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStruct        *container;
  CoilStructPrivate *priv = self->priv;
  GError            *internal_error = NULL;

  if (priv->is_accumulating)
    return TRUE;

  g_signal_emit(self, struct_signals[MODIFY], 0, &internal_error);
  if (G_UNLIKELY(internal_error))
  {
    g_propagate_error(error, internal_error);
    return FALSE;
  }

  container = coil_struct_get_container(self);
  if (container == NULL)
    return TRUE;

  return struct_change_notify(container, error);
}

static gboolean
struct_merge_item(CoilStruct     *self,
                  const CoilPath *srcpath,
                  const GValue   *srcvalue,
                  gboolean        overwrite,
                  gboolean        force_expand,
                  GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(srcpath, FALSE);
  g_return_val_if_fail(G_IS_VALUE(srcvalue), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *priv = self->priv;
  StructEntry       *entry;
  CoilPath          *path = NULL;
  GValue            *value;
  guint              hash;
  GError            *internal_error = NULL;

  hash = hash_relative_path(priv->hash, srcpath->key, srcpath->key_len);

  path = coil_path_concat(priv->path, srcpath, &internal_error);
  if (path == NULL)
    goto error;

  entry = struct_table_lookup(priv->table, hash,
                              path->path, path->path_len);

  if (entry && !overwrite)
  {
    /* merge values if old and new entries are structs */
    if (entry->value
      && G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT)
      && G_VALUE_HOLDS(srcvalue, COIL_TYPE_STRUCT))
    {
      CoilStruct *src, *dst;

      src  = COIL_STRUCT(g_value_get_object(srcvalue));
      dst = COIL_STRUCT(g_value_get_object(entry->value));

      if (!coil_struct_merge_full(src, dst,
                                  overwrite, force_expand,
                                  &internal_error))
        goto error;

      if (coil_struct_is_prototype(dst))
      {
        CoilStructFunc fn = (CoilStructFunc)make_prototype_final;
        coil_struct_foreach_ancestor(dst, TRUE, fn, NULL);
      }
    }

    coil_path_unref(path);
    /* entry exists, dont overwrite
     * includes keys marked deleted */
    return TRUE;
  }

  if (G_VALUE_HOLDS(srcvalue, COIL_TYPE_STRUCT))
  {
    CoilStruct *obj, *new_obj;

    obj = COIL_STRUCT(g_value_get_object(srcvalue));
    new_obj = coil_struct_new(&internal_error,
                              "container", self,
                              "path", path,
                              NULL);

    if (new_obj == NULL)
      goto error;

    if (!coil_struct_merge_full(obj, new_obj,
                                overwrite, force_expand,
                                &internal_error))
      goto error;

    coil_value_init(value, COIL_TYPE_STRUCT, take_object, new_obj);
  }
  else if (force_expand
    && G_VALUE_HOLDS(srcvalue, COIL_TYPE_EXPANDABLE))
  {
    const GValue *real_value = NULL;

    if (!struct_change_notify(self, error))
      goto error;

    if (!coil_expand_value(srcvalue, &real_value, TRUE, error))
      goto error;

    if (real_value == NULL)
      g_error("Expecting return value from expansion of %s '%s'",
              G_VALUE_TYPE_NAME(srcvalue), path->path);

    value = coil_value_copy(real_value);
  }
  else if (G_VALUE_HOLDS(srcvalue, COIL_TYPE_EXPANDABLE))
  {
    CoilExpandable *obj, *obj_copy;

    obj = COIL_EXPANDABLE(g_value_get_object(srcvalue));
    obj_copy = coil_expandable_copy(obj, &internal_error,
                                    "container", self,
                                    NULL);

    if (G_UNLIKELY(obj_copy == NULL))
      goto error;

    coil_value_init(value, G_VALUE_TYPE(srcvalue), take_object, obj_copy);
  }
  else
    value = coil_value_copy(srcvalue);

  if (!struct_insert_internal(self, path, value, hash, TRUE, error))
    goto error;

  return TRUE;

error:
  if (path)
    coil_path_unref(path);

  if (internal_error)
    g_propagate_error(error, internal_error);

  return FALSE;
}

COIL_API(gboolean)
coil_struct_merge_full(CoilStruct  *src,
                       CoilStruct  *dst,
                       gboolean     overwrite,
                       gboolean     force_expand,
                       GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(src), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(dst), FALSE);
  g_return_val_if_fail(src != dst, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructIter   it;
  const CoilPath  *path;
  const GValue    *value;

  if (!coil_struct_expand(src, error))
    return FALSE;

  g_object_set(G_OBJECT(dst), "accumulate", TRUE, NULL);

  coil_struct_iter_init(&it, src);

  while (coil_struct_iter_next(&it, &path, &value))
  {
    if (!struct_merge_item(dst, path, value, overwrite, force_expand, error))
    {
      g_object_set(G_OBJECT(dst), "accumulate", FALSE, NULL);
      return FALSE;
    }
  }

  g_object_set(G_OBJECT(dst), "accumulate", FALSE, NULL);
  return TRUE;
}

COIL_API(gboolean)
coil_struct_merge(CoilStruct  *src,
                  CoilStruct  *dst,
                  GError     **error)
{
  return coil_struct_merge_full(src, dst, FALSE, FALSE, error);
}

static gboolean
struct_expand_dependency(CoilStruct     *self,
                         CoilExpandable *dependency,
                         GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(COIL_IS_EXPANDABLE(dependency), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  const GValue   *expanded_value = NULL;
  CoilStruct     *parent = NULL;

  if (!coil_expand(dependency, &expanded_value, TRUE, error))
    return FALSE;

  /* structs are sanity checked before being added as a dependency */
  if (!COIL_IS_STRUCT(dependency))
  {
    /* dependency handled its own expansion
     * there is no expanded value to merge */
    if (expanded_value == NULL)
      return TRUE;

    if (!G_VALUE_HOLDS(expanded_value, COIL_TYPE_STRUCT))
    {
      coil_struct_error(error, self,
          "Invalid struct dependency type '%s', expecting '%s' type.",
          G_VALUE_TYPE_NAME(expanded_value),
          g_type_name(COIL_TYPE_STRUCT));

      return FALSE;
    }

    parent = COIL_STRUCT(g_value_get_object(expanded_value));

    if (!check_parent_sanity(self, parent, error))
      return FALSE;
  }
  else
    parent = COIL_STRUCT(dependency);

  if (coil_struct_is_prototype(parent))
  {
    coil_struct_error(error, self,
      "dependency struct '%s' is still a prototype"
      "(inherited but never defined).",
        coil_struct_get_path(parent)->path);

    return FALSE;
  }

#if COIL_STRICT_CONTEXT
  if (!coil_struct_merge_full(parent, self, FALSE, TRUE, error))
    return FALSE;
#else
  if (!coil_struct_merge(parent, self, error))
    return FALSE;
#endif

  /* if the parent changes after now we dont care */
  g_signal_handlers_disconnect_matched(parent,
                                       G_SIGNAL_MATCH_ID |
                                       G_SIGNAL_MATCH_FUNC |
                                       G_SIGNAL_MATCH_DATA,
                                       struct_signals[MODIFY], 0, NULL,
                                       G_CALLBACK(struct_expand_notify),
                                       self);

  return TRUE;
}

/* This should not be called directly
 * Call indirectly by coil_struct_expand or coil_expand
 * */
/* TODO(jcon): think about expandable API in more detail. */
static gboolean
struct_expand(gconstpointer    object,
              const GValue   **return_value,
              GError         **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(object), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStruct        *self = COIL_STRUCT(object);
  CoilStructPrivate *priv = self->priv;
  CoilExpandable    *dependency;
  GList             *list;

  g_return_val_if_fail(!priv->is_prototype, FALSE);

  if (struct_needs_expand(self))
    return TRUE;

  /* Since we waited to expand we're not really changing anything
   * (theoretically). */
  /* TODO(jcon): remove this  -- handle in merge */
  g_object_set(self, "accumulate", TRUE, NULL);

  if (priv->expand_ptr == NULL)
    list = g_queue_peek_head_link(&priv->dependencies);
  else
    list = g_list_next(priv->expand_ptr);

  while (list)
  {
    dependency = COIL_EXPANDABLE(list->data);
    priv->expand_ptr = list;

    if (!struct_expand_dependency(self, dependency, error))
    {
      g_object_set(self, "accumulate", FALSE, NULL);
      return FALSE;
    }

    list = g_list_next(list);
  }

#if COIL_DEBUG
  priv->version++;
#endif

  /* resume reporting modifications -- particularly to dependents */
  g_object_set(self, "accumulate", FALSE, NULL);

  return TRUE;
}

COIL_API(gboolean)
coil_struct_expand(CoilStruct *self,
                   GError    **error)
{
  return coil_expand(self, NULL, FALSE, error);
}

COIL_API(gboolean)
coil_struct_expand_items(CoilStruct  *self,
                         gboolean     recursive,
                         GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructIter  it;
  CoilStruct     *container;
  const GValue   *value;

  if (!coil_struct_expand(self, error))
    return FALSE;

  container = coil_struct_get_container(self);

  coil_struct_iter_init(&it, self);
  while (coil_struct_iter_next(&it, NULL, &value))
  {
    if (G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE))
    {
      CoilExpandable *object;
      object = COIL_EXPANDABLE(g_value_get_object(value));

      if (container && !struct_change_notify(container, error))
        return FALSE;

      if (!coil_expand(object, NULL, TRUE, error))
        return FALSE;

      if (recursive
          && COIL_IS_STRUCT(object)
          && !coil_struct_expand_items(COIL_STRUCT(object), TRUE, error))
        return FALSE;
    }
  }

  return TRUE;
}

static const GValue *
maybe_expand_value(CoilStruct    *self,
                   const GValue  *value,
                   GError       **error)
{
  g_return_val_if_fail(value, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE))
  {
    if (!struct_change_notify(self, error))
      return NULL;

    if (!coil_expand_value(value, &value, TRUE, error))
      return NULL;
  }

  return value;
}

/**
 * Find CoilStruct who directly contains 'path'
 */
static CoilStruct *
struct_lookup_container_internal(CoilStruct  *self,
                                 const gchar *path,
                                 guint8       path_len,
                                 GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(path_len > 0, NULL);

  const StructEntry *entry;
  CoilStructPrivate *priv = self->priv;
  const gchar       *key, *end;
  guint              hash;
  guint8             container_path_len;

  end = path + path_len;
  key = memrchr(path, COIL_PATH_DELIM, path_len);

  g_assert(key != NULL);

  container_path_len = key - path;

  if (container_path_len == COIL_ROOT_PATH_LEN)
    return coil_struct_get_root(self);

  hash = hash_absolute_path(path, container_path_len);

  entry = struct_table_lookup(priv->table,
                              hash, /* container hash */
                              path, /* conatiner path */
                              container_path_len);

  if (entry == NULL || entry->value == NULL)
    return NULL;

  if (G_UNLIKELY(!G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT)))
  {
    coil_struct_error(error, self,
        "Attempting to lookup value in entry '%.*s' which is not a struct. "
        "Entry is of type '%s'.",
         path_len, path, G_VALUE_TYPE_NAME(entry->value));

    return NULL;
  }

  return COIL_STRUCT(g_value_get_object(entry->value));
}


static gboolean
lookup_internal_expand(CoilStruct  *self,
                       const gchar *path,
                       guint8       path_len,
                       GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(path_len > 0, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  const GValue *container_value;
  CoilStruct   *container, *root;
  guint8        lens[COIL_PATH_MAX_PARTS], i;
  guint         hashes[COIL_PATH_MAX_PARTS];
  GError       *internal_error = NULL;

  compute_path_hash_with_len(path, path_len, hashes, lens, &i);

  for (--i; i > 0; i--)
  {
    container_value = struct_lookup_internal(self,
                                             hashes[i],
                                             path,
                                             lens[i],
                                             FALSE,
                                             FALSE,
                                             error);

    if (container_value == NULL)
      continue;

    if (!G_VALUE_HOLDS(container_value, COIL_TYPE_STRUCT))
    {
      g_set_error(error,
                  COIL_ERROR,
                  COIL_ERROR_VALUE,
                  "<%s> the item at '%.*s' is type %s, expected %s",
                  coil_struct_get_path(self)->path,
                  path_len, path,
                  G_VALUE_TYPE_NAME(container_value),
                  g_type_name(COIL_TYPE_STRUCT));

      return FALSE;
    }

    container = COIL_STRUCT(g_value_get_object(container_value));
    coil_struct_foreach_ancestor(container, TRUE,
                                 (CoilStructFunc)coil_struct_expand,
                                 &internal_error);

    if (G_UNLIKELY(internal_error))
    {
      g_propagate_error(error, internal_error);
      return FALSE;
    }

    break;
  }

  root = coil_struct_get_root(self);
  if (!coil_struct_expand(root, error))
    return FALSE;

  return TRUE;
}

static const GValue *
struct_lookup_internal(CoilStruct     *self,
                       guint           hash,
                       const gchar    *path,
                       guint8          path_len,
                       gboolean        expand_value,
                       gboolean        expand_container,
                       GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(path && *path == '@', NULL);
  g_return_val_if_fail(path_len, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStructPrivate *const priv = self->priv;
  StructEntry       *entry;
  const GValue      *result;

  entry = struct_table_lookup(priv->table, hash, path, path_len);
  if (entry == NULL)
  {
    if (!expand_container)
      return NULL;

    if (!lookup_internal_expand(self, path, path_len, error))
      return NULL;

    entry = struct_table_lookup(priv->table, hash, path, path_len);
    result = (entry) ? entry->value : NULL;
  }
  else
    result = entry->value;

  if (expand_value && result)
    return maybe_expand_value(self, result, error);

  return result;
}

COIL_API(const GValue *)
coil_struct_lookup_path(CoilStruct *self,
                        CoilPath   *path,
                        gboolean    expand_value,
                        GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  const GValue *result = NULL;
  guint         hash = 0;

  coil_path_ref(path);

  if (!struct_resolve_path_into(self, &path, &hash, error))
  {
    coil_path_unref(path);
    return FALSE;
  }

  result = struct_lookup_internal(self, hash,
                                  path->path, path->path_len,
                                  expand_value, TRUE, error);

  coil_path_unref(path);
  return result;
}

COIL_API(const GValue *)
coil_struct_lookup_key_fast(CoilStruct  *self,
                            const gchar *key,
                            guint8       key_len,
                            gboolean     expand_value,
                            GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(!coil_struct_is_prototype(self), NULL);
  g_return_val_if_fail(key && *key, NULL);
  g_return_val_if_fail(key_len, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStructPrivate *const priv = self->priv;
  const CoilPath    *const p = priv->path;
  guint              hash = hash_relative_path(priv->hash, key, key_len);
  gchar             *path;
  guint8             path_len;

  COIL_PATH_QUICK_BUFFER(path, path_len,
                         p->path, p->path_len, /* container */
                         key, key_len);        /* key */

  return struct_lookup_internal(self, hash,
                                path, path_len,
                                expand_value, TRUE, error);
}

COIL_API(const GValue *)
coil_struct_lookup_key(CoilStruct  *self,
                       const gchar *key,
                       guint        key_len,
                       gboolean     expand_value,
                       GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(!coil_struct_is_prototype(self), NULL);
  g_return_val_if_fail(key && *key, NULL);
  g_return_val_if_fail(key_len, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (!coil_check_key(key, key_len, error))
    return FALSE;

  return coil_struct_lookup_key_fast(self,
                                     key, (guint8)key_len,
                                     expand_value, error);
}

COIL_API(const GValue *)
coil_struct_lookup_fast(CoilStruct  *self,
                        const gchar *path,
                        guint8       path_len,
                        gboolean     expand_value,
                        GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(!coil_struct_is_prototype(self), NULL);
  g_return_val_if_fail(path && *path, NULL);
  g_return_val_if_fail(path_len, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilPath     *p;
  const GValue *result;

  p = coil_path_take_strings((gchar *)path, path_len,
                             NULL, 0,
                             COIL_STATIC_PATH);

  result = coil_struct_lookup_path(self, p, expand_value, error);

  coil_path_unref(p);

  return result;
}

COIL_API(const GValue *)
coil_struct_lookup(CoilStruct  *self,
                   const gchar *path,
                   guint        path_len,
                   gboolean     expand_value,
                   GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(!coil_struct_is_prototype(self), NULL);
  g_return_val_if_fail(path && *path, NULL);
  g_return_val_if_fail(path_len, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (!coil_check_path(path, path_len, error))
    return FALSE;

  return coil_struct_lookup_fast(self,
                                 path, (guint8)path_len,
                                 expand_value, error);
}

COIL_API(GList *)
coil_struct_get_paths(CoilStruct *self,
                      GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStructIter  it;
  const CoilPath *path;
  GQueue          q = G_QUEUE_INIT;

  if (!coil_struct_expand(self, error))
    return NULL;

  coil_struct_iter_init(&it, self);

  while (coil_struct_iter_next(&it, &path, NULL))
    g_queue_push_tail(&q, (gpointer)path);

  return g_queue_peek_head_link(&q);
}

COIL_API(GList *)
coil_struct_get_values(CoilStruct *self,
                       GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStructIter     it;
  const GValue      *value;
  GQueue             q = G_QUEUE_INIT;

  if (!coil_struct_expand(self, error))
    return NULL;

  coil_struct_iter_init(&it, self);

  while (coil_struct_iter_next(&it, NULL, &value))
    g_queue_push_tail(&q, (gpointer)value);

  return g_queue_peek_head_link(&q);
}

/* TODO(jcon): allow de-duplicating branches */
COIL_API(GNode *)
coil_struct_dependency_treev(CoilStruct *self,
                             GNode      *tree,
                             guint       ntypes,
                             GType      *allowed_types,
                             GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStructPrivate *priv = self->priv;
  CoilStructIter     it;
  StructEntry       *entry;
  GList             *list;
  GError            *internal_error = NULL;

  if (tree == NULL)
    tree = g_node_new(NULL);

  list = g_queue_peek_head_link(&priv->dependencies);

  while (list)
  {
    GNode          *branch;
    CoilExpandable *object = COIL_EXPANDABLE(list->data);

    if (ntypes > 0)
    {
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
    if (COIL_IS_STRUCT(object))
    {
      CoilStruct *node = COIL_STRUCT(object);

      coil_struct_dependency_treev(node, branch,
                                   ntypes, allowed_types,
                                   &internal_error);

      if (G_UNLIKELY(internal_error))
        goto error;
    }
    else if (COIL_IS_INCLUDE(object))
    {
      CoilInclude *include = COIL_INCLUDE(object);
      CoilStruct  *namespace;

      /* TODO(jcon): encapsulate this in include dependency protocol */
      if (!coil_expand(include, NULL, FALSE, &internal_error))
        goto error;

      g_object_get(include, "namespace", &namespace, NULL);

      coil_struct_dependency_treev(namespace, branch,
                                   ntypes, allowed_types,
                                   &internal_error);

      if (G_UNLIKELY(internal_error))
        goto error;
    }

next:
    list = g_list_next(list);
  }

  coil_struct_iter_init(&it, self);

  while (struct_iter_next_entry(&it, &entry))
  {
    if (G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT))
    {
      CoilStruct *node;

      node = COIL_STRUCT(g_value_get_object(entry->value));
      coil_struct_dependency_treev(node, tree,
                                   ntypes, allowed_types,
                                   &internal_error);

      if (G_UNLIKELY(internal_error))
        goto error;
    }
  }

  return tree;

error:
  if (internal_error)
    g_propagate_error(error, internal_error);

  return NULL;
}

COIL_API(GNode *)
coil_struct_dependency_tree_valist(CoilStruct *self,
                                   guint       ntypes,
                                   va_list     args)
{
  GType   *allowed_types;
  GNode   *result;
  GError **error = NULL;

  if (ntypes > 0)
  {
    guint  i;
    allowed_types = g_new(GType, ntypes);

    for (i = 0; i < ntypes; i++)
      allowed_types[i] = va_arg(args, GType);
  }
  else
    allowed_types = NULL;

  error = va_arg(args, GError **);

  result = coil_struct_dependency_treev(self, NULL,
                                        ntypes, allowed_types,
                                        error);

  g_free(allowed_types);

  return result;
}

COIL_API(GNode *)
coil_struct_dependency_tree(CoilStruct *self,
                            guint       ntypes,
                            ...) /* GType, ... [GError **] */
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);

  GNode  *result;
  va_list args;

  va_start(args, ntypes);
  result = coil_struct_dependency_tree_valist(self, ntypes, args);
  va_end(args);

  return result;
}

COIL_API(gint)
coil_struct_get_size(CoilStruct *self,
                     GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), -1);
  g_return_val_if_fail(error == NULL || *error == NULL, -1);

  if (!struct_needs_expand(self)
      && !coil_struct_expand(self, error))
    return -1;

  return self->priv->size;
}

/* Only call from struct_build_string_internal() */
static void
_struct_build_flat_string(CoilStruct       *self,
                          GString          *const buffer,
                          CoilStringFormat *format,
                          GError          **error)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  g_return_if_fail(buffer);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);
  g_return_if_fail(!coil_struct_is_prototype(self));

  CoilStructIter  it;
  const GValue   *value;
  const CoilPath *path;
  const CoilPath *context_path = coil_struct_get_path(format->context);
  const guint     path_offset = context_path->path_len + 1;
  GError         *internal_error = NULL;

  coil_struct_iter_init(&it, self);

  while (coil_struct_iter_next(&it, &path, &value))
  {
    if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
    {
      CoilStruct *node = COIL_STRUCT(g_value_get_object(value));
      struct_build_string_internal(node, buffer, format, &internal_error);
    }
    else
    {
      g_string_append_printf(buffer, "%s: ", path->path + path_offset);
      coil_value_build_string(value, buffer, format, &internal_error);
      g_string_append_c(buffer, '\n');
    }

    if (G_UNLIKELY(internal_error))
    {
      g_propagate_error(error, internal_error);
      return;
    }
  }
}

/* Only call from struct_build_string_internal() */
static void
_struct_build_scoped_string(CoilStruct       *self,
                            GString          *const buffer,
                            CoilStringFormat *format,
                            GError          **error)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  g_return_if_fail(buffer);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);
  g_return_if_fail(!coil_struct_is_prototype(self));

  CoilStructIter  it;
  const CoilPath *path;
  const GValue   *value;
  gboolean        brace_on_blank_line, with_braces;
  gchar          *newline_after_brace, *newline_after_struct;
  gchar           indent[128], brace_indent[128];
  gint            indent_len, brace_indent_len;
  GError         *internal_error = NULL;

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

  if (with_braces)
  {
    if (brace_on_blank_line)
    {
      g_string_append_printf(buffer,
                             "\n%s%s{%s",
                             indent,
                             brace_indent,
                             newline_after_brace);
    }
    else
    {
      g_string_append_printf(buffer,
                             "%s{%s",
                             brace_indent,
                             newline_after_brace);
    }
  }

  coil_struct_iter_init(&it, self);

  while (coil_struct_iter_next(&it, &path, &value))
  {
    g_string_append_printf(buffer, "%s%s: ", indent, path->key);
    coil_value_build_string(value, buffer, format, &internal_error);

    if (G_UNLIKELY(internal_error))
    {
      format->indent_level -= format->block_indent;
      g_propagate_error(error, internal_error);
      return;
    }

    g_string_append_c(buffer, '\n');
  }

  if (buffer->len > 0)
  {
    g_string_truncate(buffer, buffer->len - 1);
    if (buffer->str[buffer->len - 1] == '\n')
      g_string_truncate(buffer, buffer->len - 1);
  }

  format->indent_level -= format->block_indent;

  if (with_braces)
  {
    indent_len = MAX((indent_len - format->block_indent), 0);
    indent[indent_len] = '\0';

    g_string_append_printf(buffer,
                           "\n%s%s}%s",
                           indent,
                           brace_indent,
                           newline_after_struct);
  }
}

static void
struct_build_string_internal(CoilStruct       *self,
                             GString          *const buffer,
                             CoilStringFormat *format,
                             GError          **error)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  g_return_if_fail(buffer);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);
  g_return_if_fail(!coil_struct_is_prototype(self));

  CoilStringFormat format_ = *format;

  if (format_.context == NULL)
    format_.context = self;

  if (!coil_struct_expand_items(self, TRUE, error))
    return;

  if (format->options & FLATTEN_PATHS)
    _struct_build_flat_string(self, buffer, &format_, error);
  else
    _struct_build_scoped_string(self, buffer, &format_, error);
}

/* expandable build string virtual function */
static void
_struct_build_string(gconstpointer     object,
                     GString          *buffer,
                     CoilStringFormat *format,
                     GError          **error)
{
  g_return_if_fail(COIL_IS_STRUCT(object));
  g_return_if_fail(buffer != NULL);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);

  struct_build_string_internal(COIL_STRUCT(object), buffer, format, error);
}

COIL_API(void)
coil_struct_build_string(CoilStruct       *self,
                         GString          *const buffer,
                         CoilStringFormat *format,
                         GError          **error)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  g_return_if_fail(buffer != NULL);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);

  struct_build_string_internal(self, buffer, format, error);
}

COIL_API(gchar *)
coil_struct_to_string(CoilStruct       *self,
                      CoilStringFormat *format,
                      GError          **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(format, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  GString *buffer;

  buffer = g_string_sized_new(512);
  struct_build_string_internal(self, buffer, format, error);

  return g_string_free(buffer, FALSE);
}

COIL_API(CoilStruct *)
coil_struct_copy(CoilStruct   *self,
                 GError      **error,
                 const gchar  *first_property_name,
                 ...)
{
  CoilStruct *copy;
  va_list     properties;

  va_start(properties, first_property_name);
  copy = coil_struct_copy_valist(self,
                                 first_property_name,
                                 properties,
                                 error);
  va_end(properties);

  return copy;
}

COIL_API(CoilStruct *)
coil_struct_copy_valist(CoilStruct  *self,
                        const gchar *first_property_name,
                        va_list      properties,
                        GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(!coil_struct_is_prototype(self), NULL);

  CoilStruct *copy;

  copy = coil_struct_new_valist(first_property_name, properties, error);
  if (copy == NULL)
    return NULL;

  if (!coil_struct_expand_items(self, TRUE, error))
    goto error;

  if (!coil_struct_merge(self, copy, error))
    goto error;

  return copy;

error:
  g_object_unref(copy);
  return NULL;
}

static CoilExpandable *
struct_copy_valist(gconstpointer     obj,
                   const gchar      *first_property_name,
                   va_list           properties,
                   GError           **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(obj), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStruct *copy = coil_struct_copy_valist(COIL_STRUCT(obj),
                                             first_property_name,
                                             properties,
                                             error);

  return COIL_EXPANDABLE(copy);
}

static gint
struct_entry_key_cmp(const StructEntry *self,
                     const StructEntry *other)
{
  g_return_val_if_fail(self, -1);
  g_return_val_if_fail(other, -1);

  if (self == other)
    return 0;

  const CoilPath *spath = self->path, *opath = other->path;

  return strcmp(spath->key, opath->key);
}

COIL_API(gboolean)
coil_struct_equals(gconstpointer   obj,
                   gconstpointer   other_obj,
                   GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(obj), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(other_obj), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStruct         *self = COIL_STRUCT(obj), *other = COIL_STRUCT(other_obj);
  CoilStructPrivate  *const spriv = self->priv, *const opriv = other->priv;
  register GList     *lp1 = NULL, *lp2 = NULL;

  if (self == other)
    return TRUE;

  if (coil_struct_is_descendent(self, other)
    || coil_struct_is_descendent(other, self)
    || !coil_struct_expand_items(self, TRUE, error)
    || !coil_struct_expand_items(other, TRUE, error)
    || spriv->size != opriv->size)
    return FALSE;

  // All keys are first-order ok to sort
  // XXX: compare should be order-independent
  lp1 = g_queue_peek_head_link(&spriv->entries);
  lp2 = g_queue_peek_head_link(&opriv->entries);

  lp1 = g_list_sort(g_list_copy(lp1), (GCompareFunc)struct_entry_key_cmp);
  lp2 = g_list_sort(g_list_copy(lp2), (GCompareFunc)struct_entry_key_cmp);

  // Loop foreach equal key
  while (lp1 != NULL && lp2 != NULL)
  {
    StructEntry    *e1, *e2;
    const CoilPath *p1, *p2;
    const GValue   *v1, *v2;

    e1 = (StructEntry *)lp1->data;
    e2 = (StructEntry *)lp2->data;

    p1 = e1->path;
    p2 = e2->path;

    // compare keys
    if (p1->key_len != p2->key_len
      || strcmp(p1->key, p2->key))
      goto done;

    v1 = e1->value;
    v2 = e2->value;

    if (coil_value_compare(v1, v2, error))
      goto done;

    lp1 = g_list_delete_link(lp1, lp1);
    lp2 = g_list_delete_link(lp2, lp2);
  }

  g_assert(lp1 == NULL && lp2 == NULL);

  return TRUE;

done:
  if (lp1)
    g_list_free(lp1);

  if (lp2)
    g_list_free(lp2);

  return FALSE;
}

static void
coil_struct_init (CoilStruct *self)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  self->priv = COIL_STRUCT_GET_PRIVATE(self);
}

COIL_API(CoilStruct *)
coil_struct_new(GError     **error,
                const gchar *first_property_name,
                ...)
{
  va_list     properties;
  CoilStruct *result;

  va_start(properties, first_property_name);
  result = coil_struct_new_valist(first_property_name, properties, error);
  va_end(properties);

  return result;
}

COIL_API(CoilStruct *)
coil_struct_new_valist(const gchar *first_property_name,
                       va_list      properties,
                       GError     **error)
{
  GObject           *object;
  CoilStruct        *self, *container;
  CoilStructPrivate *priv;
  CoilPath          *path;
  GValue            *value;

  object = g_object_new_valist(COIL_TYPE_STRUCT,
                               first_property_name,
                               properties);

  self = COIL_STRUCT(object);
  priv = self->priv;
  container = COIL_EXPANDABLE(self)->container;

  if (priv->path && !COIL_PATH_IS_ROOT(priv->path))
  {
    if (!container)
      g_error("path specified with no container");

    priv->root = coil_struct_get_root(container);
    priv->table = struct_table_ref(container->priv->table);

    if (!coil_path_has_container(priv->path, container->priv->path, FALSE))
    {
      g_set_error(error, COIL_ERROR, COIL_ERROR_PATH,
                  "Invalid path '%s' for struct with container '%s'",
                  priv->path->path,
                  container->priv->path->path);

      goto error;
    }

    /* container may be an ancestor, we want the direct container */
    container = coil_struct_create_containers_fast(self,
                                                   priv->path->path,
                                                   COIL_PATH_CONTAINER_LEN(priv->path),
                                                   priv->is_prototype,
                                                   FALSE, error);

    if (container == NULL)
      goto error;

    g_object_set(object, "container", container, NULL);

    path = struct_resolve_path(container, priv->path, &priv->hash, error);
    if (path == NULL)
      goto error;

    /* XXX: add self to the tree now */
    coil_value_init(value, COIL_TYPE_STRUCT, set_object, self);
    if (!struct_insert_internal(container, path, value, priv->hash, TRUE, error))
      goto error;
  }
  else if (!container)
     become_root_struct(self);
  else
    g_error("container specified with no path or root path");

  g_signal_emit(object,
                struct_signals[CREATE],
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

  CoilStruct *const self = COIL_STRUCT(object);

  coil_struct_empty(self, NULL);

  G_OBJECT_CLASS(coil_struct_parent_class)->dispose(object);
}

/*
 * coil_struct_finalize: Finalize function for CoilStruct
 */
static void
coil_struct_finalize(GObject *object)
{
  g_return_if_fail(COIL_IS_STRUCT(object));

  CoilStruct        *const self = COIL_STRUCT(object);
  CoilStructPrivate *const priv = self->priv;

  coil_path_unref(priv->path);
  struct_table_unref(priv->table);

  G_OBJECT_CLASS(coil_struct_parent_class)->finalize(object);
}

/*
 * coil_struct_set_property: Setter for CoilStruct properties
 */
static void
coil_struct_set_property(GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  CoilStruct        *const self = COIL_STRUCT(object);
  CoilStructPrivate *const priv = self->priv;

  switch (property_id)
  {
    case PROP_HASH:
      priv->hash = g_value_get_uint(value);
      break;

    case PROP_IS_PROTOTYPE:
      priv->is_prototype = g_value_get_boolean(value);
      break;

    case PROP_PATH:
      if (priv->path)
        coil_path_unref(priv->path);
      priv->path = (CoilPath *)g_value_dup_boxed(value);
      break;

    case PROP_ROOT:
      priv->root = g_value_get_object(value);
      break;

    case PROP_IS_ACCUMULATING:
      priv->is_accumulating = g_value_get_boolean(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

/*
 * coil_struct_get_property: Getter for CoilStruct properties.
 */
static void
coil_struct_get_property(GObject      *object,
                         guint         property_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
  CoilStruct        *const self = COIL_STRUCT(object);
  CoilStructPrivate *const priv = self->priv;

  switch (property_id)
  {
    case PROP_HASH:
      g_value_set_uint(value, priv->hash);
      break;

    case PROP_PATH:
      g_value_set_boxed(value, priv->path);
      break;

    case PROP_IS_PROTOTYPE:
      g_value_set_boolean(value, priv->is_prototype);
      break;

    case PROP_ROOT:
      g_value_set_object(value, priv->root);
      break;

    case PROP_IS_ACCUMULATING:
      g_value_set_boolean(value, priv->is_accumulating);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

/*
 * coil_struct_class_init: Class initializer for CoilStruct
 *
 * @klass: A CoilStructClass
 */
static void
coil_struct_class_init (CoilStructClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private(klass, sizeof(CoilStructPrivate));

  /* GObject class methods */
  gobject_class->set_property = coil_struct_set_property;
  gobject_class->get_property = coil_struct_get_property;
  gobject_class->dispose = coil_struct_dispose;
  gobject_class->finalize = coil_struct_finalize;

   /* override expandable methods */
  CoilExpandableClass *expandable_class = COIL_EXPANDABLE_CLASS(klass);
  expandable_class->copy = struct_copy_valist;
  expandable_class->is_expanded = struct_needs_expand;
  expandable_class->expand = struct_expand;
  expandable_class->equals = coil_struct_equals;
  expandable_class->build_string = _struct_build_string;

  /* setup param specifications */

  g_object_class_install_property(gobject_class, PROP_HASH,
      g_param_spec_uint("hash",
                        "Hash",
                        "A pre-computed hash for this container's path.",
                        0, G_MAXUINT, 0,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT));

  g_object_class_install_property(gobject_class, PROP_IS_PROTOTYPE,
      g_param_spec_boolean("is-prototype",
                           "Is Prototype",
                           "If struct is referenced but not defined.",
                           FALSE,
                           G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_PATH,
      g_param_spec_boxed("path",
                         "Path",
                         "The path of this container.",
                         COIL_TYPE_PATH,
                         G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_ROOT,
      g_param_spec_object("root",
                          "Root",
                          "The root node of this container.",
                          COIL_TYPE_STRUCT,
                          G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_IS_ACCUMULATING,
      g_param_spec_boolean("accumulate",
                           "Accumulate",
                           "If container is in an accumulating state.",
                           FALSE, G_PARAM_READWRITE));

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
                  coil_cclosure_marshal_POINTER__VOID,
                  G_TYPE_POINTER, 0, NULL);

  struct_signals[ADD_DEPENDENCY] =
    g_signal_new("add-dependency",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_DETAILED,
                 0, NULL, NULL,
                 coil_cclosure_marshal_POINTER__OBJECT,
                 G_TYPE_POINTER, 1, G_TYPE_OBJECT);
}

