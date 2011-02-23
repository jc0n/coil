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

  StructTable         *entry_table;

  GQueue               entries;
  GQueue               dependencies;

  guint                size;
  guint                hash;

#ifdef COIL_DEBUG
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
  /* TODO(jcon): NEW_DEPENDENCY */
  /* */
  LAST_SIGNAL
} StructSignals;

static guint
struct_signals[LAST_SIGNAL] = {0, };


static gboolean
struct_delete_internal(CoilStruct   *self,
                       guint         hash,
                       const gchar  *path,
                       guint8        path_len,
                       gboolean      strict,
                       GError      **error);

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

static const GValue *
struct_lookup_internal(CoilStruct     *self,
                       guint           hash,
                       const gchar    *path,
                       guint8          path_len,
                       gboolean        expand_value,
                       GError        **error);

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
 * (3) Minimize recomputing hash values.
 */

static void
struct_change_container(CoilStruct *self,
                        CoilStruct *new_container,
                        CoilPath   *new_path, /* NULL if do not have */
                        guint      *new_hash, /* NULL if do not have */
                        gboolean    cleanup_old_container)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  g_return_if_fail(new_container == NULL || COIL_IS_STRUCT(new_container));
  g_return_if_fail((new_container && new_path && new_hash)
      || !(new_container || new_path || new_hash));
  g_return_if_fail(self != new_container);

  CoilStructPrivate *const priv = self->priv;
  CoilExpandable    *const super = COIL_EXPANDABLE(self);
  CoilPath          *const path = priv->path;
  CoilStruct        *old_container = super->container;
  StructTable       *old_table;
  const GList       *entry_list;

  if (new_container != old_container)
  {
    /* remove self from previous container */
    /* XXX: better have an extra reference */
    if (cleanup_old_container)
    {
      g_assert(G_OBJECT(self)->ref_count > 1);

      struct_delete_internal(old_container,
                             priv->hash,
                             priv->path->path,
                             priv->path->path_len,
                             FALSE, NULL);
    }

    if (new_container == NULL)
    {
      /* set self to @root */
      priv->root = self;
      priv->hash = 0;
      priv->path = coil_path_take_strings(COIL_ROOT_PATH, COIL_ROOT_PATH_LEN,
                                          NULL, 0,
                                          COIL_PATH_IS_ABSOLUTE |
                                          COIL_PATH_IS_ROOT |
                                          COIL_STATIC_PATH);

      old_table = priv->entry_table;
      priv->entry_table = struct_table_new();
    }
    else
    {
      /* change the container related properties */
      if (new_path == NULL)
      {
        /* no prebuilt path to use, change path container here */
        const CoilPath *container_path = coil_struct_get_path(old_container);
        coil_path_change_container(&priv->path, container_path);
      }
      else
      {
        /* new path already built, use that one */
        coil_path_unref(priv->path);
        priv->path = coil_path_ref(new_path);
      }

      if (new_hash == NULL)
      {
        priv->hash = hash_relative_path(new_container->priv->hash,
                                        path->key,
                                        path->key_len);
      }
      else
        priv->hash = *new_hash;

      priv->root = coil_struct_get_root(new_container);

      /* only if our root changes do we use a different entry table */
      if (coil_struct_has_same_root(self, old_container))
        old_table = struct_table_ref(priv->entry_table);
      else
      {
        old_table = priv->entry_table;
        priv->entry_table = struct_table_ref(old_container->priv->entry_table);
      }
    }

    g_object_set(G_OBJECT(self),
                 "container", new_container,
                 NULL);
  }
  else
    old_table = struct_table_ref(priv->entry_table);

  for (entry_list = g_queue_peek_head_link(&priv->entries);
       entry_list; entry_list = g_list_next(entry_list))
  {
    StructEntry *entry = (StructEntry *)entry_list->data;

    struct_table_remove_entry(old_table, entry);

    coil_path_change_container(&entry->path, path);
    entry->hash = hash_relative_path(priv->hash,
                                     entry->path->key,
                                     entry->path->key_len);

    struct_table_insert_entry(priv->entry_table, entry);

    if (G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT))
    {
      CoilStruct *object = COIL_STRUCT(g_value_get_object(entry->value));
      struct_change_container(object, self, entry->path, &entry->hash, FALSE);
    }
  }

  struct_table_unref(old_table);
}

GQuark
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

  if (path->flags & COIL_PATH_IS_ABSOLUTE)
  {
    *return_hash = hash_absolute_path(path->path, path->path_len);
    return coil_path_ref((CoilPath *)path);
  }

  if (path->flags & COIL_PATH_IS_BACKREF)
  {
    CoilPath *resolved = coil_path_resolve(path, priv->path, error);

    if (resolved)
      *return_hash = hash_absolute_path(resolved->path, resolved->path_len);

    return resolved;
  }

  *return_hash = hash_relative_path(priv->hash, path->path, path->path_len);
  return coil_path_resolve(path, priv->path, error);
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

  g_queue_clear(&priv->dependencies);

/*  CoilExpandable *object;
  while ((object = g_queue_pop_head(&priv->dependencies)))
    g_object_unref(object);*/

  StructEntry *entry;
  while ((entry = g_queue_pop_head(&priv->entries)))
    struct_table_delete_entry(priv->entry_table, entry);

  priv->size       = 0;

#ifdef COIL_DEBUG
  priv->version    = 0;
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
struct_is_expanded(gconstpointer s)
{
  g_return_val_if_fail(COIL_IS_STRUCT(s), FALSE);

  CoilStruct *self = COIL_STRUCT(s);
  return g_queue_is_empty(&self->priv->dependencies);
}

/*
 * XXX: If struct is not expanded this will return false.
 * (May report struct is not empty when it is empty but will not report
 * struct is empty when it isn't)
 *
 * Why? extending empty structs is a stupid and rare case and without it
 * this check is fast.
 *
 * If you need to know for sure, then use the API version coil_struct_is_empty
 */
static gboolean
struct_is_definitely_empty(const CoilStruct *self)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);

  return !self->priv->size && struct_is_expanded(self);
}

COIL_API(gboolean)
coil_struct_is_empty(CoilStruct *self,
                     GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);

  if (self->priv->size)
    return FALSE;

  if (!struct_is_expanded(self)
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
coil_struct_has_same_root(const CoilStruct *a,
                          const CoilStruct *b)
{
  g_return_val_if_fail(COIL_IS_STRUCT(a), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(b), FALSE);

  return a == b || coil_struct_get_root(a) == coil_struct_get_root(b);
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
make_prototype_final(CoilStruct *self, gpointer unused)
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
struct_insert_internal(CoilStruct    *self,
                       CoilPath      *path, /* steals */
                       GValue        *value, /* steals */
                       gboolean       replace, /* TRUE to replace old value */
                       GError       **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(path->flags & COIL_PATH_IS_ABSOLUTE, FALSE);
  g_return_val_if_fail(!(path->flags & COIL_PATH_IS_ROOT), FALSE);
  g_return_val_if_fail(g_str_has_prefix(path->path, self->priv->path->path) &&
      ((path->path_len - path->key_len) - 1) == self->priv->path->path_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *const priv = self->priv;
  StructEntry       *old_entry;
  GError            *internal_error = NULL;
  guint              hash;

  /*g_debug("%s: self=%p path=%s", __FUNCTION__, self, path->path);*/

  if (!priv->is_accumulating)
  {
    g_signal_emit(self, struct_signals[MODIFY],
                  0, (gpointer *)&internal_error);

    if (G_UNLIKELY(internal_error))
    {
      g_propagate_error(error, internal_error);
      goto error;
    }
  }

//  if (!priv->size)
//    goto new_entry;

  /*g_debug("%s: priv->hash=%d", __FUNCTION__, priv->hash);*/
  hash = hash_relative_path(priv->hash, path->key, path->key_len);

  if (priv->size
    && (old_entry = struct_table_lookup(priv->entry_table, hash,
                                        path->path, path->path_len)))
  {
    /* entry exists for path (value may be null) */
    GValue *old_value = old_entry->value;

    if (old_value && G_VALUE_HOLDS(old_value, COIL_TYPE_STRUCT))
    {
      CoilStruct *src, *dst;
      dst = COIL_STRUCT(g_value_get_object(old_value));

      if (coil_struct_is_prototype(dst))
      {
      /* Overwriting a prototype.
       * Merge the items from struct we're trying to set now
       * and destroy it (leaving the prototype in place but casting to
       * non-prototype).
       *
       * TODO: Consider destroying the prototype instead. We may
       * be able to pass the defined struct in with the closure to the
       * signals registered to prototype. More thought on this is needed...
       */
        if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
        {
          src = COIL_STRUCT(g_value_get_object(value));

          g_object_set(dst, "accumulate", TRUE, NULL);

          coil_struct_merge(src, dst, TRUE, &internal_error);

          g_object_set(dst, "accumulate", FALSE, NULL);

          if (G_UNLIKELY(internal_error))
          {
            g_propagate_error(error, internal_error);
            goto error;
          }

          coil_path_unref(path);
          free_value(value);

          g_object_set(self,
                       "is-prototype", FALSE,
                       NULL);

#ifdef COIL_DEBUG
          priv->version++;
#endif
          return TRUE;
        }
        else
        { /* old value is prototype - new value is not struct */
          coil_struct_error(error, self,
                      "Attempting to overwrite struct prototype '%s' "
                      "with non struct value. This implies struct is used but "
                      "never defined.", path->path);
          goto error;
        }
      }
    }
    else if (!replace)
    {
       coil_struct_error(error, self,
                         "Attempting to insert value at path '%s' "
                         "which already exists.",
                         path->path);
       goto error;
    }

    /* entry exists, overwrite value */
    free_value(old_value);
    coil_path_unref(old_entry->path);

    old_entry->value = value;
    old_entry->path = path;
  }
  else
  {
    /* entry does NOT exist for key/path */
    StructEntry *entry;
    entry = struct_table_insert(priv->entry_table, hash, path, value);
    g_queue_push_tail(&priv->entries, entry);
    priv->size++;
  }

  /* if value is struct,
   * the path will be different so we need up update that and
   * also update children */

  if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
  {
    CoilStruct *object = COIL_STRUCT(g_value_get_object(value));
    CoilStruct *container = coil_struct_get_container(object);

    if (container != self)
      struct_change_container(object, self, path, &hash, TRUE);
  }
  else if (G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE))
  {
    CoilExpandable *object = COIL_EXPANDABLE(g_value_get_object(value));

    if (object->container != self)
    {
      g_object_set(G_OBJECT(object),
                   "container", self,
                   NULL);
    }
  }

#ifdef COIL_DEBUG
  priv->version++;
#endif

  /*
   * if we were a prototype cast self and our ancestry to finals
   * sine we just set a value.
   */
  if (coil_struct_is_prototype(self))
  {
    CoilStructFunc fn = (CoilStructFunc)make_prototype_final;
    coil_struct_foreach_ancestor(self, TRUE, fn, NULL);
  }

  return TRUE;

error:
  coil_path_unref(path);
  free_value(value);
  return FALSE;
}

/* Create container for path inside self */
static CoilStruct *
struct_create_container(CoilStruct  *self,
                        CoilPath    *path,
                        guint        hash,
                        gboolean     prototype,
                        GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(hash > 0, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStructPrivate *const priv = self->priv;
  StructEntry       *entry;
  CoilStruct        *container;
  GValue            *value;

  if (!coil_struct_is_prototype(self))
  {
/*    GError *internal_error = NULL;
    g_signal_emit(self, struct_signals[MODIFY],
                0, (gpointer *)&internal_error);

    if (G_UNLIKELY(internal_error))
    {
      g_propagate_error(error, internal_error);
      return NULL;
    }
 */
  }

  container = coil_struct_new("container", self,
                              "path", path,
                              "hash", hash,
                              "is-prototype", prototype,
                              NULL);

  new_value(value, COIL_TYPE_STRUCT,
            take_object, container);

  entry = struct_table_insert(priv->entry_table, hash, path, value);

  g_queue_push_tail(&priv->entries, entry);

#ifdef COIL_DEBUG
  priv->version++;
#endif
  priv->size++;

  return container;
}

COIL_API(CoilStruct *)
coil_struct_create_containers(CoilStruct     *self,
                              const gchar    *path, /* of container */
                              guint8          path_len,
                              gboolean        prototype,
                              gboolean        has_previous_lookup, /* whether path has been looked up */
                              GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(path && *path == '@'
      && path_len >= COIL_ROOT_PATH_LEN, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (path_len == COIL_ROOT_PATH_LEN)
  {
    g_assert(!memcmp(path, COIL_ROOT_PATH, COIL_ROOT_PATH_LEN));
    return coil_struct_get_root(self);
  }

  CoilStructPrivate *const priv = self->priv;
  StructEntry       *entry;
  CoilStruct        *container = priv->root;
  const gchar       *next_delim, *end, *delim = path + COIL_ROOT_PATH_LEN + 1;
  guint              saved_hashes[COIL_PATH_MAX_PARTS] = {0, };
  guint8             saved_lens[COIL_PATH_MAX_PARTS] = {COIL_ROOT_PATH_LEN, 0,};
  guint8             i = 1, missing_keys = 0;

  for (end = &path[path_len];
       (next_delim = memchr(delim, COIL_PATH_DELIM, end - delim));
       i++, delim = next_delim + 1)
  {
    saved_lens[i] = next_delim - path;
    saved_hashes[i] = hash_relative_path(saved_hashes[i - 1],
                                         delim, next_delim - delim);
  }

  saved_lens[i] = path_len;
  saved_hashes[i] = hash_relative_path(saved_hashes[i - 1], delim, end - delim);

  if (has_previous_lookup)
  {
    i--;
    missing_keys++;
  }

  for (; i > 0; missing_keys++, i--)
  {
    entry = struct_table_lookup(priv->entry_table, saved_hashes[i],
                                path, saved_lens[i]);

    if (entry)
    {
      if (G_UNLIKELY(!entry->value
            || !G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT)))
      {
        coil_struct_error(error, self,
          "Attempting to assign values in non-struct object %.*s.",
          saved_lens[i], path);

        return NULL;
      }

      container = COIL_STRUCT(g_value_get_object(entry->value));
      break;
    }
  }

  if (!prototype)
  {
    CoilStructFunc fn = (CoilStructFunc)make_prototype_final;
    coil_struct_foreach_ancestor(container, TRUE, fn, NULL);
  }

  for (i++; G_LIKELY(container != NULL) && missing_keys > 0;
      missing_keys--, i++)
  {
    /* container i exists and i > 1 */
    /* or i == 1 */
    CoilPath *container_path;
    gchar    *container_path_str, *container_key_str;
    guint8    container_key_len;

    container_path_str = g_strndup(path, saved_lens[i]);
    container_key_str = &container_path_str[saved_lens[i - 1]] + 1;
    container_key_len = (saved_lens[i] - saved_lens[i - 1]) - 1;

    container_path = coil_path_take_strings(container_path_str,
                                            saved_lens[i],
                                            container_key_str,
                                            container_key_len,
                                            COIL_PATH_IS_ABSOLUTE |
                                            COIL_STATIC_KEY);

    container = struct_create_container(container, container_path,
                                        saved_hashes[i], prototype, error);
  }

  return container;
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
  CoilStructPrivate *const priv = self->priv;
  CoilPath          *resolved_path = NULL;
/*
  if (G_UNLIKELY(!coil_validate_path(path)))
  {
    coil_struct_error(error, self,
        "Inserting invalid path '%s'.",
        path->path);

    goto error;
  }
  else
 */
  if (!(resolved_path = coil_path_resolve(path, priv->path, error)))
    goto error;

  if (G_UNLIKELY(path->flags & COIL_PATH_IS_ROOT))
  {
    coil_struct_error(error, self,
                      "Cannot assign a value directly to @root.");
    goto error;
  }

  if (memchr(path->path, COIL_PATH_DELIM, path->path_len))
  {
    container = coil_struct_create_containers(self,
                                              resolved_path->path,
                                              COIL_PATH_CONTAINER_LEN(resolved_path),
                                              TRUE,
                                              FALSE,
                                              error);

    if (G_UNLIKELY(container == NULL))
      goto error;
  }
  else
    container = self;

  coil_path_unref(path);

  return struct_insert_internal(container,
                                resolved_path,
                                value,
                                replace, /* overwrite value */
                                error);

error:
  if (resolved_path)
    coil_path_unref(resolved_path);
  coil_path_unref(path);
  free_value(value);
  return FALSE;
}

COIL_API(gboolean)
coil_struct_insert(CoilStruct  *self,
                   gchar       *path_str, /* steals */
                   guint8       path_len,
                   GValue      *value, /* steals */
                   gboolean     replace,
                   GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path_str && *path_str, FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(value, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilPath *path = coil_path_take_strings(path_str, path_len, NULL, 0, 0);
  return coil_struct_insert_path(self, path, value, replace, error);
}

COIL_API(gboolean)
coil_struct_insert_key(CoilStruct  *self,
                       const gchar *key,
                       guint8       key_len,
                       GValue      *value, /* steals */
                       gboolean     replace,
                       GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(key && *key, FALSE);
  g_return_val_if_fail(key_len, FALSE);
  g_return_val_if_fail(value, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  const CoilPath *const parent_path = self->priv->path;
  CoilPath       *path;
  guint8          path_len = parent_path->path_len + key_len + 1;

  path = coil_path_build_new_len(path_len,
                                 parent_path->path, key,
                                 NULL);

  return struct_insert_internal(self, path, value, replace, error);
}

static gboolean
struct_remove_entry(CoilStruct            *self,
                    const StructEntry     *entry,
                    GError               **error)
{
  CoilStructPrivate *const priv = self->priv;
/*
  GError            *internal_error = NULL;
  g_signal_emit(self, struct_signals[MODIFY],
                0, (gpointer *)&internal_error);

  if (G_UNLIKELY(internal_error))
  {
    g_propagate_error(error, internal_error);
    return FALSE;
  }
*/
  g_queue_remove(&priv->entries, entry);

#ifdef COIL_DEBUG
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

  struct_table_delete_entry(priv->entry_table, entry);

  return TRUE;
}

static gboolean
struct_delete_internal(CoilStruct   *self,
                       guint         hash,
                       const gchar  *path,
                       guint8        path_len,
                       gboolean      strict,
                       GError      **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(!coil_struct_is_prototype(self), FALSE);
  g_return_val_if_fail(path && *path && *path == '@', FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail(!struct_is_definitely_empty(self), FALSE);

  CoilStructPrivate *const priv = self->priv;
  StructEntry       *entry;

  entry = struct_table_lookup(priv->entry_table, hash, path, path_len);

  if (entry == NULL)
  {
    if (strict)
    {
      coil_struct_error(error, self,
          "Attempting to delete non-existent path '%s'.", path);
      return FALSE;
    }

    return TRUE;
  }

  if (G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT))
  {
    GObject *object = g_value_get_object(entry->value);

    if (object->ref_count > 1)
      struct_change_container(COIL_STRUCT(object), NULL, NULL, NULL, FALSE);
  }

  return struct_delete_entry(self, entry, error);
}

COIL_API(gboolean)
coil_struct_delete_path(CoilStruct     *self,
                        const CoilPath *path,
                        gboolean        strict,
                        GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  guint              hash;
  gboolean           result = FALSE;
  CoilPath          *resolved_path;

  resolved_path = struct_resolve_path(self, path, &hash, error);

  if (!resolved_path)
    goto error;

  if (G_UNLIKELY(path->flags & COIL_PATH_IS_ROOT))
  {
    coil_struct_error(error, self,
        "Cannot delete '@root' path.");
    goto error;
  }

  result = struct_delete_internal(self, hash,
                                  path->path, path->path_len,
                                  strict, error);

error:
  coil_path_unref(resolved_path);
  return result;
}

COIL_API(gboolean)
coil_struct_delete(CoilStruct  *self,
                   const gchar *path_str,
                   guint8       path_len,
                   gboolean     strict,
                   GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path_str && *path_str, FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilPath *path = coil_path_take_strings((gchar *)path_str, path_len,
                                         NULL, 0, COIL_STATIC_PATH);

  return coil_struct_delete_path(self, path, strict, error);
}

COIL_API(gboolean)
coil_struct_delete_key(CoilStruct  *self,
                       const gchar *key,
                       guint8       key_len,
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

  hash = hash_relative_path(priv->hash, key, key_len);

  COIL_PATH_QUICK_BUFFER(path, path_len,
                         p->path, p->path_len,
                         key, key_len);

  return struct_delete_internal(self, hash, path, path_len, strict, error);
}

static gboolean
struct_mark_deleted_internal(CoilStruct  *self,
                             CoilPath    *path, /* steal */
                             gboolean     force,
                             GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(path->flags & COIL_PATH_IS_ABSOLUTE, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *const priv = self->priv;
  StructEntry       *entry;
  guint              hash = hash_relative_path(priv->hash, path->key, path->key_len);

  entry = struct_table_lookup(priv->entry_table, hash,
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

  struct_table_insert(priv->entry_table, hash, path, NULL);

#ifdef COIL_DEBUG
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

  CoilStructPrivate *const priv = self->priv;
  CoilStruct        *container;
  CoilPath          *resolved;

  resolved = coil_path_resolve(path, priv->path, error);

  if (G_UNLIKELY(resolved == NULL))
    goto error;

  if (G_UNLIKELY(resolved->flags & COIL_PATH_IS_ROOT))
  {
    coil_struct_error(error, self,
        "Cannot mark @root as deleted.");
    goto error;
  }

  container = coil_struct_create_containers(self,
                                            resolved->path,
                                            COIL_PATH_CONTAINER_LEN(resolved),
                                            TRUE, FALSE,
                                            error);

  if (G_UNLIKELY(container == NULL))
    goto error;

  coil_path_unref(path);
  return struct_mark_deleted_internal(container, resolved, force, error);

error:
  coil_path_unref(path);
  return FALSE;
}

COIL_API(gboolean)
coil_struct_mark_deleted(CoilStruct  *self,
                         gchar       *path_str, /* steal */
                         guint8       path_len,
                         gboolean     force,
                         GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path_str && *path_str, FALSE);
  g_return_val_if_fail(path_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilPath *path = coil_path_take_strings(path_str, path_len, NULL, 0, 0);

  return coil_struct_mark_deleted_path(self, path, force, error);
}

COIL_API(gboolean)
coil_struct_mark_deleted_key(CoilStruct  *self,
                             const gchar *key,
                             guint8       key_len,
                             gboolean     force,
                             GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(key && *key, FALSE);
  g_return_val_if_fail(key_len, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructPrivate *const priv = self->priv;
  const CoilPath    *const p = priv->path;
  CoilPath          *path;
  guint8             path_len;

  g_assert(p->path_len + key_len + 1 <= COIL_PATH_LEN);

  path_len = p->path_len + key_len + 1;
  path = coil_path_build_new_len(path_len, p->path, key, NULL);

  return struct_mark_deleted_internal(self, path, force, error);
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

  if (G_UNLIKELY(!coil_struct_has_same_root(self, parent)))
  {
    G_BREAKPOINT();
    coil_struct_error(error, self,
      "cannot extend structs in disjoint roots.");

    return FALSE;
  }

  return TRUE;
}

static gboolean
struct_check_dependency(gconstpointer object)
{
  GObject *gobject = G_OBJECT(object);

  return COIL_IS_STRUCT(gobject)
      || COIL_IS_LINK(gobject)
      || COIL_IS_INCLUDE(gobject);
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

  if (!struct_check_dependency(object))
    g_error("Adding invalid dependency type '%s'.",
        G_OBJECT_TYPE_NAME(object));

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

    if (!coil_struct_has_same_root(self, container))
      g_error("Adding dependency in '%s' from a different @root.",
          self->priv->path->path);
  }

  g_queue_push_tail(&priv->dependencies, object);

#ifdef COIL_DEBUG
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

  CoilStruct *self, *parent;
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
                        CoilPath    *path, /* steal */
                        CoilStruct  *context,
                        GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(path, FALSE);
  g_return_val_if_fail(context == NULL || COIL_IS_STRUCT(context), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilExpandable    *dependency;
  CoilPath          *resolved = NULL;
  const GValue      *value;
  guint              hash;
  GError            *internal_error = NULL;

  if (context == NULL)
    context = self;

  resolved = struct_resolve_path(context, path, &hash, error);

  if (G_UNLIKELY(resolved == NULL))
    goto error;

  value = struct_lookup_internal(context, hash,
                                 resolved->path,
                                 resolved->path_len,
                                 FALSE, &internal_error);

  if (G_UNLIKELY(internal_error))
    goto error;

  if (value)
  {
    GObject *object = g_value_get_object(value);

    if (G_UNLIKELY(!struct_check_dependency(object)))
    {
      coil_struct_error(error, self,
        "@extends target '%s' must be a struct, found type '%s'.",
        path->path, G_OBJECT_TYPE_NAME(object));

      goto error;
    }

    dependency = COIL_EXPANDABLE(object);
  }
  else
  {
    CoilStruct *container;

    container = coil_struct_create_containers(context,
                                              resolved->path,
                                              resolved->path_len,
                                              TRUE, TRUE, &internal_error);

    if (G_UNLIKELY(container == NULL))
      goto error;

    dependency = COIL_EXPANDABLE(container);
  }

  coil_path_unref(path);
  coil_path_unref(resolved);

  return coil_struct_add_dependency(self, dependency, error);

error:
  if (resolved)
    coil_path_unref(resolved);

  coil_path_unref(path);
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
      goto error;

    path_list = g_list_delete_link(path_list, path_list);
  }

  return TRUE;

error:
  coil_path_list_free(path_list);
  return FALSE;
}

COIL_API(void)
coil_struct_iter_init(CoilStructIter   *iter,
                      const CoilStruct *self)
{
  g_return_if_fail(COIL_IS_STRUCT(self));
  g_return_if_fail(!coil_struct_is_prototype(self));
  g_return_if_fail(iter);

  CoilStructPrivate *const priv = self->priv;
  iter->node = self;
#ifdef COIL_DEBUG
  iter->version = priv->version;
#endif
  iter->position = g_queue_peek_head_link(&priv->entries);
}

COIL_API(gboolean)
coil_struct_iter_next(CoilStructIter *iter,
                      StructEntry   **entry)
{
  g_return_val_if_fail(iter, FALSE);
  g_return_val_if_fail(entry, FALSE);

#ifdef COIL_DEBUG
  const CoilStruct *self = iter->node;
  g_return_val_if_fail(iter->version == self->priv->version, FALSE);
#endif

  if (!iter->position)
    return FALSE;

  *entry = (StructEntry *)iter->position->data;
  iter->position = g_list_next(iter->position);

  return TRUE;
}
/*
COIL_API(gboolean)
coil_struct_iter_next_value(CoilStructIter *iter,
                            const GValue  **value,
                            gboolean        expand_value,
                            GError        **error)
{
  g_return_val_if_fail(iter, FALSE);
  g_return_val_if_fail(value, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

#ifdef COIL_DEBUG
  const CoilStruct *self = iter->node;
  g_return_val_if_fail(iter->version == self->priv->version, FALSE);
#endif

  if (!iter->position)
    return FALSE;

  *value = (GValue *)((StructEntry *)iter->position->data)->value;

  if (expand_value)
     return coil_expand_value(value, FALSE, error);

  return TRUE;
}
*/

COIL_API(gboolean)
coil_struct_merge(CoilStruct  *src,
                  CoilStruct  *dst,
                  gboolean     overwrite,
                  GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(src), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(dst), FALSE);
  g_return_val_if_fail(src != dst, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructIter   it;
  StructEntry     *entry, *existing_entry;
  gboolean         different_roots = FALSE;
  guint            hash;

  if (struct_is_definitely_empty(src))
    return TRUE;

  if (!coil_struct_expand(src, error))
    return FALSE;

  /* XXX: if roots are different then we need a
   * full recursive expand on source
   * also intelligently copy only
   * the real value's from expanded values in the loop below
   */
  if (!coil_struct_has_same_root(src, dst))
  {
    different_roots = TRUE;
    if (!coil_struct_expand_recursive(src, error))
      return FALSE;
  }

  coil_struct_iter_init(&it, src);

  while (coil_struct_iter_next(&it, &entry))
  {
    GValue   *value_copy;
    CoilPath *path;

    // check if value exists in dst or key is marked deleted
    hash = hash_relative_path(dst->priv->hash,
                              entry->path->key,
                              entry->path->key_len);

    path = coil_path_concat(dst->priv->path, entry->path);

    existing_entry = struct_table_lookup(dst->priv->entry_table,
                                         hash,
                                         path->path,
                                         path->path_len);

    if (existing_entry && !overwrite)
    {
      /* merge values if old and new entries are structs */
      if (existing_entry->value
        && G_VALUE_HOLDS(existing_entry->value, COIL_TYPE_STRUCT)
        && G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT))
      {
        CoilStruct *_src, *_dst;

        _src  = COIL_STRUCT(g_value_get_object(entry->value));
        _dst = COIL_STRUCT(g_value_get_object(existing_entry->value));

        g_object_set(_dst, "accumulate", TRUE, NULL);

        if (!coil_struct_merge(_src, _dst, overwrite, error))
        {
          g_object_set(_dst, "accumulate", FALSE, NULL);
          coil_path_unref(path);
          return FALSE;
        }

        g_object_set(_dst, "accumulate", FALSE, NULL);

        if (coil_struct_is_prototype(_dst))
        {
          CoilStructFunc fn = (CoilStructFunc)make_prototype_final;
          coil_struct_foreach_ancestor(_dst, TRUE, fn, NULL);
        }
      }

      /* entry exists, dont overwrite
       * includes keys marked deleted */
      continue;
    }

    /* entry does not exist OR we're overwriting it */

    if (G_VALUE_HOLDS(entry->value, COIL_TYPE_STRUCT))
    {
      CoilStruct *node = COIL_STRUCT(g_value_get_object(entry->value));
      CoilStruct *node_copy = coil_struct_deep_copy(node, dst, error);

      if (!node_copy)
      {
        coil_path_unref(path);
        return FALSE;
      }

      new_value(value_copy, COIL_TYPE_STRUCT, take_object, node_copy);
    }
    else if (different_roots
        && G_VALUE_HOLDS(entry->value, COIL_TYPE_EXPANDABLE))
    {
      /* TODO(jcon): implement expandable->copy protocol for this reason
       * should not have to expand to copy */
      const GValue *real_value;

      if (!coil_expand_value(entry->value, &real_value, FALSE, error))
      {
        coil_path_unref(path);
        return FALSE;
      }

      if (!real_value)
      {
        coil_path_unref(path);
        g_error("Expecting return value from expansion of '%s' type '%s'",
            entry->path->path, G_VALUE_TYPE_NAME(entry->value));
      }

      value_copy = copy_value(real_value);
    }
    else
      value_copy = copy_value(entry->value);

    if (!struct_insert_internal(dst, path, value_copy, TRUE, error))
      return FALSE;
  }

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

  CoilStruct        *parent, *self = COIL_STRUCT(object);
  CoilStructPrivate *priv = self->priv;
  CoilExpandable    *dep;

  g_return_val_if_fail(!priv->is_prototype, FALSE);

  if (struct_is_expanded(self))
    return TRUE;

  /* Since we waited to expand we're not really changing anything
   * (theoretically). */
  g_object_set(self,
               "accumulate", TRUE,
               NULL);

  while ((dep = g_queue_pop_head(&priv->dependencies)))
  {
    const GValue *return_value = NULL;

    g_assert(struct_check_dependency(dep));

    if (!coil_expand(dep, &return_value, TRUE, error))
      return FALSE;

    /* structs are checked before they are added */
    if (!COIL_IS_STRUCT(dep))
    {
      if (return_value == NULL)
        continue;

      g_assert(G_IS_VALUE(return_value));
      dep = COIL_EXPANDABLE(g_value_get_object(return_value));

      if (!COIL_IS_STRUCT(dep))
      {
        coil_struct_error(error, self,
            "Invalid type '%s' in @extends, expecting '%s' type.",
            G_OBJECT_TYPE_NAME(dep), g_type_name(COIL_TYPE_STRUCT));

        return FALSE;
      }

      if (!check_parent_sanity(self, COIL_STRUCT(dep), error))
        return FALSE;
    }

    parent = COIL_STRUCT(dep);

    if (coil_struct_is_prototype(parent))
    {
      coil_struct_error(error, self,
        "dependency struct '%s' is still a prototype"
        "(extended but never defined).",
          coil_struct_get_path(parent)->path);

      return FALSE;
    }

    g_assert(struct_is_expanded(parent));

    if (!coil_struct_merge(parent, self, FALSE, error))
      return FALSE;

    /* if the parent changes after now we dont care */
    g_signal_handlers_disconnect_matched(parent,
                                         G_SIGNAL_MATCH_ID |
                                         G_SIGNAL_MATCH_FUNC |
                                         G_SIGNAL_MATCH_DATA,
                                         struct_signals[MODIFY], 0, NULL,
                                         G_CALLBACK(struct_expand_notify),
                                         self);

  }

#ifdef COIL_DEBUG
  priv->version++;
#endif

  /* resume reporting modifications -- particularly to dependents */
  g_object_set(self, "accumulate", FALSE, NULL);

  return TRUE;
}

COIL_API(gboolean)
coil_struct_expand_recursive(CoilStruct  *self,
                             GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilStructIter     it;
  const StructEntry *entry;

  if (!coil_struct_expand(self, error))
    return FALSE;

  if (!self->priv->size)
    return TRUE;

  coil_struct_iter_init(&it, self);
  while (coil_struct_iter_next(&it, (StructEntry **)&entry))
  {
    const GValue *value = entry->value;
    if (G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE))
    {
      CoilExpandable *object = COIL_EXPANDABLE(g_value_get_object(value));
      if ((COIL_IS_STRUCT(object)
          && !coil_struct_expand_recursive(COIL_STRUCT(object), error))
          || !coil_expand(object, NULL, TRUE, error))
        return FALSE;
    }
  }

  return TRUE;
}

static const GValue *
maybe_expand_value(const GValue  *value,
                   GError       **error)
{
  g_return_val_if_fail(value, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE)
    && !coil_expand_value(value, &value, TRUE, error))
    return NULL;

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

  entry = struct_table_lookup(priv->entry_table,
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

static const GValue *
struct_lookup_internal(CoilStruct     *self,
                       guint           hash,
                       const gchar    *path,
                       guint8          path_len,
                       gboolean        expand_value,
                       GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(path && *path == '@', NULL);
  g_return_val_if_fail(path_len, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStructPrivate *const priv = self->priv;
  StructEntry       *entry;
  const GValue      *result;

  entry = struct_table_lookup(priv->entry_table, hash, path, path_len);

  if (!entry)
  {
    /* try to expand the container and search again */
    CoilStruct *container;

    container = struct_lookup_container_internal(self,
                                                 path, path_len,
                                                 error);

    if (container == NULL
        || struct_is_expanded(container)
        || G_UNLIKELY(!coil_struct_expand(container, error)))
      return NULL;

    entry = struct_table_lookup(priv->entry_table, hash, path, path_len);
    result = (entry) ? entry->value : NULL;
  }
  else
    result = entry->value;

  return (expand_value) ? maybe_expand_value(result, error) : result;
}

COIL_API(const GValue *)
coil_struct_lookup_path(CoilStruct     *self,
                        const CoilPath *path,
                        gboolean        expand_value,
                        GError        **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(path, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilPath     *resolved_path;
  const GValue *result = NULL;
  guint         hash;

  if ((resolved_path = struct_resolve_path(self, path, &hash, error)))
  {
    result = struct_lookup_internal(self, hash,
                                    resolved_path->path,
                                    resolved_path->path_len,
                                    expand_value, error);

    coil_path_unref(resolved_path);
  }

  return result;
}

COIL_API(const GValue *)
coil_struct_lookup_key(CoilStruct  *self,
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
                                expand_value, error);
}

COIL_API(const GValue *)
coil_struct_lookup(CoilStruct  *self,
                   const gchar *path_str,
                   guint8       path_len,
                   gboolean     expand_value,
                   GError     **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(!coil_struct_is_prototype(self), NULL);
  g_return_val_if_fail(path_str && *path_str, NULL);
  g_return_val_if_fail(path_len, NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilPath     *path;
  const GValue *result;

  path = coil_path_take_strings((gchar *)path_str, path_len,
                                NULL, 0,
                                COIL_STATIC_PATH);

  result = coil_struct_lookup_path(self, path, expand_value, error);
  coil_path_unref(path);

  return result;
}

COIL_API(GList *)
coil_struct_get_paths(CoilStruct *self,
                      GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);

  if (!coil_struct_expand(self, error))
    return NULL;

  CoilStructPrivate *const priv = self->priv;
  GQueue             result_list = G_QUEUE_INIT;
  const GList       *entry_list;

  for (entry_list = g_queue_peek_head_link(&priv->entries);
       entry_list; entry_list = g_list_next(entry_list))
  {
    const StructEntry *entry = (StructEntry *)entry_list->data;
    const CoilPath    *path = entry->path;
    g_queue_push_tail(&result_list, (gpointer)path);
  }

  return g_queue_peek_head_link(&result_list);
}


COIL_API(GList *)
coil_struct_get_values(CoilStruct *self,
                       GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (!coil_struct_expand(self, error))
    return NULL;

  CoilStructPrivate *const priv = self->priv;
  GQueue             result_list = G_QUEUE_INIT;
  const GList       *entry_list;

  for (entry_list = g_queue_peek_head_link(&priv->entries);
       entry_list; entry_list = g_list_next(entry_list))
  {
    const StructEntry *entry = (StructEntry *)entry_list->data;
    const GValue      *value = entry->value;
    g_queue_push_tail(&result_list, (gpointer)value);
  }

  return g_queue_peek_head_link(&result_list);
}

/*
 * coil_struct_get_size: get the size of a struct
 *
 * @self: A CoilStruct instance.
 *
 * Return Value: the number of elements in a struct.
 *
 */
COIL_API(guint)
coil_struct_get_size(CoilStruct *self,
                     GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), 0);
  g_return_val_if_fail(error == NULL || *error == NULL, 0);

  if (!struct_is_expanded(self)
      && !coil_struct_expand(self, error))
    return 0;

  return self->priv->size;
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

  CoilStructIter  it;
  StructEntry    *entry;
  const GValue   *value;
  const CoilPath *path;
  GError         *internal_error = NULL;

  if (!coil_struct_expand(self, error))
    return;

  coil_struct_iter_init(&it, self);

  if (format->options & FLATTEN_PATHS)
  {
    while (coil_struct_iter_next(&it, &entry))
    {
      value = entry->value;
      path = entry->path;

      if (!G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
      {
        g_string_append_printf(buffer, "%s: ", path->path);
        coil_value_build_string(value, buffer, format, &internal_error);
        g_string_append_c(buffer, '\n');
      }
      else
      {
#if 0
        if (G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE)
            && !coil_expand_value(&value, FALSE, &internal_error))
          goto error;
#endif

        coil_value_build_string(value, buffer, format, &internal_error);
      }

      if (G_UNLIKELY(internal_error))
        goto error;
    }
  }
  else
  {
    gboolean hug_brace = (format->options & BRACE_ON_PATH_LINE);
    gboolean bl_after_brace = (format->options & BLANK_LINE_AFTER_BRACE);
    gboolean bl_after_struct = (format->options & BLANK_LINE_AFTER_STRUCT);
    gchar    indent[128];
    guint    indent_len = format->indent_level;

    memset(indent, ' ', MIN(indent_len, sizeof(indent)));
    indent[indent_len] = '\0';

    format->indent_level += format->block_indent;

    while (coil_struct_iter_next(&it, &entry))
    {
      value = entry->value;
      path = entry->path;

      if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
      {
        g_string_append_printf(buffer, "%s%s: %s{%s",
                               indent, path->key,
                               (hug_brace) ? "" : "\n",
                               (bl_after_brace) ? "\n" : "");

        coil_value_build_string(value, buffer, format, &internal_error);

        g_string_append_printf(buffer, "\n%s}%s",
                               indent,
                               (bl_after_struct) ? "\n" : "");
      }
      else
      {
#if 0
        if (G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE)
            && !coil_expand_value(&return_value, FALSE, &internal_error))
          goto error;
#endif

        g_string_append_printf(buffer, "%s%s: ", indent, path->key);

        coil_value_build_string(value, buffer, format, &internal_error);
      }

      if (G_UNLIKELY(internal_error))
      {
        format->indent_level -= format->block_indent;
        goto error;
      }

      g_string_append_c(buffer, '\n');
    }

    g_string_truncate(buffer, buffer->len - 1);

    if (buffer->str[buffer->len - 1] == '\n')
      g_string_truncate(buffer, buffer->len - 1);

    format->indent_level -= format->block_indent;
  }

  return;

error:
  g_propagate_error(error, internal_error);
  return;
 }

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

/*
 * coil_struct_build_string: Convert a CoilStruct into a string.
 *
 * @self: A CoilStruct instance.
 *
 * @prefix: The string to prefix every line
 *
 * @prefix_len: The length of @prefix.
 *
 * Return Value: The string representation of a CoiLStruct
 *
 */
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

  /* short circuit
  if (struct_is_definitely_empty(self))
    return g_strndup(COIL_STATIC_STRLEN("{}"));
    */

  buffer = g_string_sized_new(512);
  struct_build_string_internal(self, buffer, format, error);

  return g_string_free(buffer, FALSE);
}

/*
 * coil_struct_deep_copy: Deep copy for CoilStruct objects.
 *
 * @self: The CoilStruct to copy
 *
 * @container: The container for the copy
 *
 * Return Value: A CoilStruct copy of @self
 *
 */
COIL_API(CoilStruct *)
coil_struct_deep_copy(CoilStruct       *self,
                      const CoilStruct *new_container,
                      GError          **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(self), NULL);
  g_return_val_if_fail(COIL_IS_STRUCT(new_container), NULL);
  g_return_val_if_fail(!coil_struct_is_prototype(self), NULL);
  g_return_val_if_fail(!coil_struct_is_prototype(new_container), NULL);
  g_return_val_if_fail(self != new_container, NULL);

  CoilStructPrivate *const priv = self->priv;
  CoilStruct        *copy;

  if (new_container)
  {
    CoilPath       *path = coil_path_ref(priv->path);
    const CoilPath *ctnr_path = coil_struct_get_path(new_container);

    coil_path_change_container(&path, ctnr_path);

    copy = coil_struct_new("container", new_container,
                           "path", path,
                           NULL);

    coil_path_unref(path);
  }
  else
    copy = coil_struct_new(NULL, NULL);

  if (!coil_struct_expand_recursive(self, error))
    goto error;

  if (!struct_is_definitely_empty(self))
  {
    CoilStructIter it;
    StructEntry   *entry;
    GValue        *value_copy;

    /* iterate keys in order */
    coil_struct_iter_init(&it, self);
    while (coil_struct_iter_next(&it, &entry))
    {
      const GValue   *value = entry->value;
      const CoilPath *path = entry->path;

      if (!value)
      {
        if (!coil_struct_mark_deleted_key(copy,
                                          path->key,
                                          path->key_len,
                                          FALSE,
                                          error))
          goto error;

        continue;
      }

      if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
      {
        CoilStruct *node, *node_copy;

        node  = COIL_STRUCT(g_value_get_object(entry->value));
        node_copy = coil_struct_deep_copy(node, copy, error);

        if (G_UNLIKELY(!node_copy))
          goto error;

        new_value(value_copy, COIL_TYPE_STRUCT,
                  take_object, node_copy);
      }
      else
        value_copy = copy_value(entry->value);

      if (!coil_struct_insert_key(copy,
                                  path->key,
                                  path->key_len,
                                  value_copy,
                                  FALSE,
                                  error))
        goto error;
    }
  }

  return copy;

error:
  g_object_unref(copy);
  return NULL;
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
    || !coil_struct_expand(self, error)
    || !coil_struct_expand(other, error)
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
    if (strcmp(p1->key, p2->key))
      goto done;

    v1 = e1->value;
    v2 = e2->value;

    if ((v1 != v2
      && (v1 == NULL || v2 == NULL))
      || (G_VALUE_HOLDS(v1, COIL_TYPE_EXPANDABLE)
        && !coil_expand_value(v1, &v1, TRUE, error))
      || (G_VALUE_HOLDS(v2, COIL_TYPE_EXPANDABLE)
        && !coil_expand_value(v2, &v2, TRUE, error))
      || coil_value_compare(v1, v2, error))
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
coil_struct_new(const gchar *first_property_name, ...)
{
  va_list            args;
  GObject           *object;
  CoilStruct        *self;
  CoilStructPrivate *priv;
  CoilExpandable    *super;
  CoilStruct        *container;
  CoilPath          *path;

  va_start(args, first_property_name);
  object = g_object_new_valist(COIL_TYPE_STRUCT,
                             first_property_name, args);
  va_end(args);

  self = COIL_STRUCT(object);
  priv = self->priv;
  super = COIL_EXPANDABLE(self);
  container = super->container;
  path = priv->path;

  if (container && path
      && path->flags & COIL_PATH_IS_ABSOLUTE)
  {
    priv->root = coil_struct_get_root(container);
    priv->entry_table = struct_table_ref(container->priv->entry_table);

    if (!priv->hash)
      priv->hash = hash_relative_path(container->priv->hash,
                            path->key,
                            path->key_len);
  }
  else if (!(container || path)
    || (path && (path->flags & COIL_PATH_IS_ROOT)))
  {
    /* struct is root */
    priv->root = self;

    if (!path)
    {
      priv->path = coil_path_take_strings(COIL_ROOT_PATH,
                                          COIL_ROOT_PATH_LEN,
                                          NULL, 0,
                                          COIL_STATIC_PATH |
                                          COIL_PATH_IS_ABSOLUTE |
                                          COIL_PATH_IS_ROOT);
    }

    priv->hash = 0;
    priv->entry_table = struct_table_new();
  }
  else if (!container)
    g_error("Must specify container argument for non-root struct.");
  else if (!path)
    g_error("Must specify path argument for non-root struct.");
  else if (!(path->flags & COIL_PATH_IS_ABSOLUTE))
    g_error("Path argument must contain an absolute path.");

  g_signal_emit(object, struct_signals[CREATE],
                priv->is_prototype ? coil_struct_prototype_quark() : 0);

  return self;
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
  struct_table_unref(priv->entry_table);

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

static CoilExpandable *
struct_deep_copy(gconstpointer     obj,
                 const CoilStruct *container,
                 GError           **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(obj), NULL);
  g_return_val_if_fail(COIL_IS_STRUCT(container), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilStruct *copy = coil_struct_deep_copy(COIL_STRUCT(obj),
                                           container, error);

  return COIL_EXPANDABLE(copy);
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
  expandable_class->copy = struct_deep_copy;
  expandable_class->is_expanded = struct_is_expanded;
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
}

