/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coilmodule.h"
#include "coilstruct.h"

PyObject *
cCoil_struct_new(CoilStruct *node)
{
  PyCoilStruct *self;

  if (node == NULL)
  {
    Py_INCREF(Py_None);
    return Py_None;
  }

  self = PyObject_New(PyCoilStruct, &PyCoilStruct_Type);
  if (self == NULL)
    return NULL;

  self->node = node;
  self->iterator = NULL;

  return (PyObject *)self;
}

gboolean
update_struct_from_pyitems(CoilStruct *node,
                           PyObject   *obj)
{
  g_return_val_if_fail(COIL_IS_STRUCT(node), FALSE);
  g_return_val_if_fail(obj, FALSE);

  Py_ssize_t  i, size;
  PyObject   *items, *item, *k, *v, *s;
  CoilPath   *parent_path, *path = NULL, *abspath;
  GValue     *value = NULL;
  GError     *error = NULL;

  if (PyMapping_Check(obj))
    items = PyMapping_Items(obj);
  else if (PySequence_Check(obj))
    items = PySequence_List(obj);
  else
  {
    PyErr_SetString(PyExc_ValueError, "argument must contain iterable items.");
    return FALSE;
  }

  if (items == NULL)
    return FALSE;

  size = PyList_GET_SIZE(items);
  for (i = 0; i < size; i++)
  {
    item = PyList_GET_ITEM(items, i);
    k = PyTuple_GET_ITEM(item, 0);
    v = PyTuple_GET_ITEM(item, 1);

    s = PyObject_Str(k);
    if (s == NULL)
      goto error;

    path = coil_path_from_pystring(s, &error);
    Py_DECREF(s);
    if (path == NULL)
      goto error;

    parent_path = coil_struct_get_path(node);
    abspath = coil_path_resolve(path, parent_path, &error);
    if (abspath == NULL)
     goto error;

    coil_path_unref(path);
    path = abspath;

    if (PyDict_Check(v))
    {
      CoilStruct *s = coil_struct_new(&error,
                                      "container", node,
                                      "path", path,
                                      NULL);

      if (s == NULL)
        goto error;

      if (!update_struct_from_pyitems(s, v))
      {
        g_object_unref(s);
        goto error;
      }

      new_value(value, COIL_TYPE_STRUCT, take_object, s);
    }
    else
    {
      value = coil_value_from_pyobject(v);
      if (value == NULL)
        goto error;
    }

    /* value may exist in another struct */
    /* copy value before inserting, deep copy expandable items */
    /* XXX: may change this to COIL_TYPE_OBJECT in the future */
/*    if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
    {
      CoilStruct *object, *copy;
      object = COIL_STRUCT(g_value_get_object(value));
      copy = coil_struct_copy(object, &error,
                              "container", node,
                              "path", path,
                              NULL);
      if (copy == NULL)
        goto error;

      g_value_reset(value);
      g_value_take_object(value, copy);
    }
    else if (G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE))
    {
      CoilExpandable *object, *copy;

      object = COIL_EXPANDABLE(g_value_get_object(value));
      copy = coil_expandable_copy(object, &error,
                                  "container", node,
                                  NULL);
      if (copy == NULL)
        goto error;

      g_value_reset(value);
      g_value_take_object(value, copy);
    }
    */

    if (!coil_struct_insert_path(node, path, value, FALSE, &error))
      goto error;
  }

  Py_DECREF(items);
  return TRUE;

error:
  if (path)
    coil_path_unref(path);

  if (value)
    free_value(value);

  if (error)
    cCoil_error(&error);

  Py_DECREF(items);
  return FALSE;
}

/*
"""
:param base: A *dict*, *Struct*, or a sequence of (key, value)
    tuples to initialize with. Any child *dict* or *Struct*
    will be recursively copied as a new child *Struct*.
:param container: the parent *Struct* if there is one.
:param name: The name of this *Struct* in *container*.
:param location: The where this *Struct* is defined.
    This is normally only used by the :class:`Parser
    <coil.parser.Parser>`.
"""
*/
static int
struct_init(PyCoilStruct *self,
            PyObject     *args,
            PyObject     *kwargs)
{
  static char  *kwlist[] = {"base", "container", "name", "location", NULL};
  PyCoilStruct *container = NULL;
  PyObject     *name = NULL;
  PyObject     *base = NULL;
  PyObject     *location = NULL;
  CoilStruct   *_container = NULL;
  gchar        *_name = NULL;
  GError       *error = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OO!SO:cCoil.Struct.__init__",
                                   kwlist, &base,
                                   &PyCoilStruct_Type, &container,
                                   &name, &location))
    goto error;

  if (name)
  {
    _name = PyString_AsString(name);
    if (_name == NULL)
      goto error;

    if (container)
      _container = container->node;
  }
  else if (container)
  {
      PyErr_SetString(PyExc_ValueError,
          "A name argument must be specified with a container argument.");

      goto error;
  }

  /* TODO(jcon): handle location */

  self->node = coil_struct_new(&error,
                               "container", _container,
                               "path", _name,
                               NULL);

  if (self->node == NULL)
    goto error;

  if (base == NULL)
    return 0;

  if (PyCoilStruct_Check(base))
  {
    CoilStruct *src;
    src = ((PyCoilStruct *)base)->node;
    if (!coil_struct_merge(src, self->node, FALSE, &error))
      goto error;

    return 0;
  }

  if (!PyMapping_Check(base) && !PySequence_Check(base))
  {
    PyErr_SetString(PyExc_ValueError,
                    "base argument must be a dict-like object, " \
                    "a sequence of items, or a coil struct");
    goto error;
  }

  if (!update_struct_from_pyitems(self->node, base))
    goto error;

  return 0;

error:
  if (error)
    cCoil_error(&error);

  return -1;
}

static void
struct_dealloc(PyCoilStruct *self)
{
  if (self->node != NULL)
  {
    g_object_unref(self->node);
    self->node = NULL;
  }

  if (self->iterator != NULL)
  {
    g_free(self->iterator);
    self->iterator = NULL;
  }

  PyObject_Del(self);
}

static PyObject *
struct_copy(PyCoilStruct *self,
            PyObject     *args,
            PyObject     *kwargs)
{
  static char  *kwlist[] = {"container", "name", NULL};
  PyCoilStruct *container = NULL;
  PyObject     *name = NULL;
  CoilStruct   *copy, *_container = NULL;
  const gchar  *_name = NULL;
  GError       *error = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O!S:cCoil.Struct.copy",
                                   kwlist, &PyCoilStruct_Type, &container,
                                   &name))
    return NULL;

  if (name != NULL
    && (_name = PyString_AsString(name)) == NULL)
    return NULL;

  if (container != NULL)
    _container = container->node;

  /* TODO(jcon): implement name */
  copy = coil_struct_copy(self->node, &error,
                          "container", _container,
                          "name", _name,
                          NULL);
  if (copy == NULL)
  {
    cCoil_error(&error);
    return NULL;
  }

  return cCoil_struct_new(copy);
}

static PyObject *
struct_get_container(PyCoilStruct *self,
                     PyObject     *ignored)
{
  CoilStruct *container;

  container = coil_struct_get_container(self->node);
  if (container == NULL)
  {
    Py_INCREF(Py_None);
    return Py_None;
  }

  g_object_ref(container);
  return cCoil_struct_new(container);
}

static PyObject *
update_pydict_from_struct(CoilStruct *node,
                          PyObject   *d,
                          gboolean    absolute)
{
  g_return_val_if_fail(COIL_IS_STRUCT(node), NULL);
  g_return_val_if_fail(d, NULL);

  CoilStructIter  it;
  const CoilPath *path;
  const GValue   *value;
  PyObject       *k = NULL, *v = NULL;
  ptrdiff_t       key_offset, keylen_offset;
  GError         *error = NULL;

  if (!coil_struct_expand(node, &error))
    goto error;

  if (absolute)
  {
    key_offset = G_STRUCT_OFFSET(CoilPath, path);
    keylen_offset = G_STRUCT_OFFSET(CoilPath, path_len);
  }
  else
  {
    key_offset = G_STRUCT_OFFSET(CoilPath, key);
    keylen_offset = G_STRUCT_OFFSET(CoilPath, key_len);
  }

  coil_struct_iter_init(&it, node);
  while (coil_struct_iter_next(&it, &path, &value))
  {
    const gchar *str = G_STRUCT_MEMBER(gchar *, path, key_offset);
    guint8       len = G_STRUCT_MEMBER(guint8, path, keylen_offset);

    k = PyString_FromStringAndSize(str, len);
    if (k == NULL)
      goto error;

    if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT))
    {
      node = COIL_STRUCT(g_value_get_object(value));

      v = PyDict_New();
      if (v == NULL)
      {
        Py_DECREF(k);
        goto error;
      }

      v = update_pydict_from_struct(node, v, absolute);
      if (v == NULL)
      {
        Py_DECREF(k);
        goto error;
      }
    }
    else
      v = coil_value_as_pyobject(value);

    PyDict_SetItem(d, k, v);

    Py_DECREF(k);
    Py_DECREF(v);
  }

  return d;

error:
  Py_XDECREF(d);

  if (error)
    cCoil_error(&error);

  return NULL;
}

static PyObject *
struct_to_dict(PyCoilStruct *self,
               PyObject     *args,
               PyObject     *kwargs)
{
  static char *kwlist[] = {"absolute", NULL};
  PyObject    *d, *absolute = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                  "|O:cCoil.Struct.dict",
                                  kwlist, &absolute))
    return NULL;

  d = PyDict_New();
  if (d == NULL)
    return NULL;

  update_pydict_from_struct(self->node, d,
                            absolute && PyObject_IsTrue(absolute));

  return d;
}

#if 0
/* Default behavior is to not expand
 * unless absolutely necessary
 * this is more of a compatability hack
 * the user should never have to explicitly expand a struct
 */
static PyObject *
struct_expand(PyCoilStruct *self,
                     PyObject     *args,
                     PyObject     *kwargs)
{
  static char *kwlist[] = {"defaults", "ignore_missing", "recursive", "force", NULL};
  CoilStruct  *const node = self->node;
  PyObject    *py_defaults = NULL, *py_missing = NULL;
  PyObject    *py_recursive = NULL, *py_force = NULL;
  GError      *error = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                  "|OOOO:pycoil.Struct.expand",
                                  kwlist,
                                  &py_defaults, &py_missing,
                                  &py_recursive, &py_force))
    return NULL;

  if (py_force && PyObject_IsTrue(py_force))
  {
    if (PyObject_IsTrue(py_recursive))
      coil_struct_expand_recursive(node, &error);
    else
      coil_struct_expand(node, &error);

    if (G_UNLIKELY(error))
    {
      handle_error(error);
      return NULL;
    }
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
struct_expanditem(PyCoilStruct *self,
                         PyObject     *args,
                         PyObject     *kwargs)
{

  // XXX: FIX BROKEN
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
struct_expandvalue(PyCoilStruct *self,
                          PyObject     *args,
                          PyObject     *kwargs)
{
  // XXX: FIX BROKEN
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
struct_extend(PyCoilStruct *self,
                     PyObject     *args,
                     PyObject     *kwargs)
{
  // XXX: FIX BROKEN
  Py_INCREF(Py_None);
  return Py_None;
}

#endif

static PyObject *
struct_get(PyCoilStruct *self,
           PyObject     *args,
           PyObject     *kwargs)
{
  static char   *kwlist[] = {"path", "default", NULL};
  PyObject      *result, *pypath, *pydefault = NULL;
  CoilPath      *path = NULL;
  const GValue  *value;
  GError        *error = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "S|O:pycoil.Struct.get",
                                   kwlist, &pypath, &pydefault))
    return NULL;

  path = coil_path_from_pystring(pypath, &error);
  if (path == NULL)
    goto error;

  value = coil_struct_lookup_path(self->node, path, TRUE, &error);
  if (G_UNLIKELY(error))
    goto error;

  if (value)
  {
    result = coil_value_as_pyobject(value);
    coil_path_unref(path);
    return result;
  }

  if (pydefault)
  {
    Py_INCREF(pydefault);
    coil_path_unref(path);
    return pydefault;
  }
  else
  {
    gchar buf[COIL_PATH_LEN * 2 + 128];

    g_snprintf(buf, sizeof(buf),
              "<%s> The path '%s' was not found",
              coil_struct_get_path(self->node)->path,
              path->path);

    PyErr_SetString(KeyMissingError, buf);
  }

error:
  if (path)
    coil_path_unref(path);

  if (error)
    cCoil_error(&error);

  return NULL;
}

static PyObject *
struct_has_key(PyCoilStruct *self,
               PyObject     *args)
{
  PyObject     *pypath, *result;
  CoilPath     *path;
  const GValue *value;
  GError       *error = NULL;

  if (!PyArg_ParseTuple(args, "S:cCoil.Struct.has_key", &pypath))
    return NULL;

  path = coil_path_from_pystring(pypath, &error);
  if (path == NULL)
    return NULL;

  value = coil_struct_lookup_path(self->node, path, FALSE, &error);
  result = (value == NULL) ? Py_False : Py_True;
  Py_INCREF(result);

  coil_path_unref(path);
  return result;
}

static PyObject *
struct_items(PyCoilStruct *self,
             PyObject     *args,
             PyObject     *kwargs)
{
  static char    *kwlist[] = {"absolute", NULL};
  PyObject       *items, *absolute = NULL;
  PyObject       *t, *p, *v;
  CoilStructIter  it;
  const CoilPath *path;
  const GValue   *value;
  Py_ssize_t      i = 0, size;
  ptrdiff_t       key_offset, keylen_offset;
  GError         *error = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O:cCoil.Struct.items",
                                   kwlist, &absolute))
    return NULL;

  size = coil_struct_get_size(self->node, &error);
  if (G_UNLIKELY(error))
  {
    cCoil_error(&error);
    return NULL;
  }

  items = PyList_New(size);

  if (absolute && PyObject_IsTrue(absolute))
  {
    key_offset = G_STRUCT_OFFSET(CoilPath, path);
    keylen_offset = G_STRUCT_OFFSET(CoilPath, path_len);
  }
  else
  {
    key_offset = G_STRUCT_OFFSET(CoilPath, key);
    keylen_offset = G_STRUCT_OFFSET(CoilPath, key_len);
  }

  coil_struct_iter_init(&it, self->node);
  while (coil_struct_iter_next(&it, &path, &value))
  {
    const gchar *str = G_STRUCT_MEMBER(gchar *, path, key_offset);
    guint8       len = G_STRUCT_MEMBER(guint8, path, keylen_offset);

    p = PyString_FromStringAndSize(str, (Py_ssize_t)len);
    if (p == NULL)
    {
      Py_DECREF(items);
      return NULL;
    }

    v = coil_value_as_pyobject(value);
    if (v == NULL)
    {
      Py_DECREF(p);
      Py_DECREF(items);
      return NULL;
    }

    t = PyTuple_Pack(2, p, v);
    if (t == NULL)
    {
      Py_DECREF(p);
      Py_DECREF(v);
      Py_DECREF(items);
      return NULL;
    }

    PyList_SET_ITEM(items, i++, t);
  }

  return items;
}

#if 0
static PyObject *
struct_iteritems(PyCoilStruct *self,
                        PyObject     *ignored)
{
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
struct_iterkeys(PyCoilStruct *self,
                       PyObject     *ignored)
{
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *
struct_itervalues(PyCoilStruct *self,
                         PyObject     *ignored)
{
  Py_INCREF(Py_None);
  return Py_None;
}

#endif

static PyObject *
struct_keys(PyCoilStruct *self,
            PyObject     *args,
            PyObject     *kwargs)
{
  static char *kwlist[] = {"absolute", NULL};
  PyObject    *result, *absolute = NULL;
  GList       *keys, *list;
  Py_ssize_t   i, n;
  ptrdiff_t    key_offset, keylen_offset;
  GError      *error = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O:cCoil.Struct.keys",
                                   kwlist, &absolute))
    return NULL;

  keys = coil_struct_get_paths(self->node, &error);
  if (keys == NULL)
  {
    cCoil_error(&error);
    g_list_free(keys);
    return NULL;
  }

  if (absolute && PyObject_IsTrue(absolute))
  {
    key_offset = G_STRUCT_OFFSET(CoilPath, path);
    keylen_offset = G_STRUCT_OFFSET(CoilPath, path_len);
  }
  else
  {
    key_offset = G_STRUCT_OFFSET(CoilPath, key);
    keylen_offset = G_STRUCT_OFFSET(CoilPath, key_len);
  }

  n = (Py_ssize_t)g_list_length(keys);
  result = PyList_New(n);

  for (i = 0, list = keys;
       i < n;
       i++, list = g_list_next(list))
  {
    CoilPath  *path = (CoilPath *)list->data;
    gchar     *str = G_STRUCT_MEMBER(gchar *, path, key_offset);
    guint8     len = G_STRUCT_MEMBER(guint8, path, keylen_offset);
    PyObject  *s;

    s = PyString_FromStringAndSize(str, (Py_ssize_t)len);
    if (s == NULL)
    {
      g_list_free(keys);
      return NULL;
    }

    PyList_SET_ITEM(result, i, s);
  }

  g_list_free(keys);
  return result;
}

static PyObject *
struct_values(PyCoilStruct *self,
              PyObject     *ignored)
{
  PyObject *result;
  GList    *values;
  GError   *error = NULL;

  values = coil_struct_get_values(self->node, &error);
  if (values == NULL)
  {
    cCoil_error(&error);
    g_list_free(values);
    return NULL;
  }

  result = pylist_from_value_list(values);
  g_list_free(values);

  return result;
}

static PyObject *
struct_merge(PyCoilStruct *self,
             PyObject     *args)
{
  CoilStruct *src = NULL, *dst = self->node;
  PyObject   *arg;
  Py_ssize_t  i, n;
  GError     *error = NULL;

  n = PyTuple_GET_SIZE(args);
  for (i = 0; i < n; i++)
  {
    arg = PyTuple_GET_ITEM(args, i);

    if (PyCoilStruct_Check(arg))
    {
      src = ((PyCoilStruct *)arg)->node;
      g_object_ref(src);
    }
    else if (PyDict_Check(arg))
    {
      src = coil_struct_new(NULL, NULL);
      if (!update_struct_from_pyitems(src, arg))
        goto error;
    }

    if (!coil_struct_merge(src, dst, FALSE, &error))
      goto error;

    g_object_unref(src);
  }

  Py_INCREF(Py_None);
  return Py_None;

error:
  if (src)
    g_object_unref(src);

  if (error)
    cCoil_error(&error);

  return NULL;
}


static PyObject *
struct_path(PyCoilStruct *self,
            PyObject     *args,
            PyObject     *kwargs)
{
  static char *kwlist[] = {"path", "ref", NULL};
  PyObject    *pyresult, *pypath = NULL, *pyref = NULL;
  CoilPath    *path = NULL, *target = NULL, *abspath = NULL;
  gchar       *str;
  Py_ssize_t   len;
  GError      *error = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|SS:cCoil.path",
                                   kwlist, &pypath, &pyref))
    return NULL;

  if (pypath)
  {
    if (pyref)
    {
      const CoilPath *node_path;
      node_path = coil_struct_get_path(self->node);

      path = coil_path_from_pystring(pyref, &error);
      if (path == NULL)
        goto error;

      abspath = coil_path_resolve(path, node_path, &error);
      if (abspath == NULL)
        goto error;

      coil_path_unref(path);
      path = abspath;
    }
    else
    {
      path = (CoilPath *)coil_struct_get_path(self->node);
      coil_path_ref(path);
    }

    target = coil_path_from_pystring(pypath, &error);
    if (target == NULL)
      goto error;

    abspath = coil_path_resolve(target, path, &error);
    if (abspath == NULL)
      goto error;

    pyresult = PyString_FromStringAndSize(abspath->path, abspath->path_len);

    coil_path_unref(path);
    coil_path_unref(target);
    coil_path_unref(abspath);
  }
  else
  {
    path = (CoilPath *)coil_struct_get_path(self->node);
    pyresult = PyString_FromStringAndSize(path->path, path->path_len);
  }

  return pyresult;

error:
  if (path)
    coil_path_unref(path);

  if (target)
    coil_path_unref(target);

  if (error)
    cCoil_error(&error);

  return NULL;
}

static PyObject *
struct_set(PyCoilStruct *self,
           PyObject     *args,
           PyObject     *kwargs)
{
  static char *kwlist[] = {"path", "value", "location", NULL};
  PyObject    *pypath;
  PyObject    *pyvalue;
  PyObject    *pylocation = NULL;
  CoilPath    *path = NULL;
  GValue      *value = NULL;
  GError      *error = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "SO|O:cCoil.Struct.set",
                                   kwlist, &pypath, &pyvalue, &pylocation))
    return NULL;


  path = coil_path_from_pystring(pypath, &error);
  if (path == NULL)
    goto error;

  value = coil_value_from_pyobject(pyvalue);
  if (value == NULL)
    goto error;

  // TODO(jcon): handle location

  if (!coil_struct_insert_path(self->node, path, value, TRUE, &error))
    goto error;

  Py_INCREF(Py_None);
  return Py_None;

error:
  if (path)
    coil_path_unref(path);

  if (value)
    free_value(value);

  if (error)
    cCoil_error(&error);

  return NULL;
}

static PyObject *
struct_to_string(PyCoilStruct *self,
                 PyObject     *args,
                 PyObject     *kwargs)
{
  PyObject        *pystring, *o;
  GString         *buffer;
  CoilStringFormat format = default_string_format;
  GError          *error = NULL;

  if (kwargs)
  {
    o = PyDict_GetItemString(kwargs, "indent_level");
    if (o)
    {
      format.indent_level = (guint8)PyInt_AsLong(o);
      if (PyErr_Occurred())
        return NULL;
    }

    o = PyDict_GetItemString(kwargs, "block_indent");
    if (o)
    {
      format.block_indent = (guint8)PyInt_AsLong(o);
      if (PyErr_Occurred())
        return NULL;
    }

    o = PyDict_GetItemString(kwargs, "brace_indent");
    if (o)
    {
      format.brace_indent = (guint8)PyInt_AsLong(o);
      if (PyErr_Occurred())
        return NULL;
    }

    o = PyDict_GetItemString(kwargs, "multiline_length");
    if (o)
    {
      format.multiline_len = (guint)PyInt_AsLong(o);
      if (PyErr_Occurred())
        return NULL;
    }

    o = PyDict_GetItemString(kwargs, "blank_line_after_brace");
    if (o)
    {
      if (PyObject_IsTrue(o))
        format.options |= BLANK_LINE_AFTER_BRACE;
      else
        format.options &= ~BLANK_LINE_AFTER_BRACE;
    }

    o = PyDict_GetItemString(kwargs, "blank_line_after_struct");
    if (o)
    {
      if (PyObject_IsTrue(o))
        format.options |= BLANK_LINE_AFTER_STRUCT;
      else
        format.options &= ~BLANK_LINE_AFTER_STRUCT;
    }

    o = PyDict_GetItemString(kwargs, "blank_line_after_item");
    if (o)
    {
      if (PyObject_IsTrue(o))
        format.options |= BLANK_LINE_AFTER_ITEM;
      else
        format.options &= ~BLANK_LINE_AFTER_ITEM;
    }

    o = PyDict_GetItemString(kwargs, "brace_on_blank_line");
    if (o)
    {
      if (PyObject_IsTrue(o))
        format.options |= BRACE_ON_BLANK_LINE;
      else
        format.options &= ~BRACE_ON_BLANK_LINE;
    }

    o = PyDict_GetItemString(kwargs, "list_on_blank_line");
    if (o)
    {
      if (PyObject_IsTrue(o))
        format.options |= LIST_ON_BLANK_LINE;
      else
        format.options &= ~LIST_ON_BLANK_LINE;
    }

    o = PyDict_GetItemString(kwargs, "commas_in_list");
    if (o)
    {
      if (PyObject_IsTrue(o))
        format.options |= COMMAS_IN_LIST;
      else
        format.options &= ~COMMAS_IN_LIST;
    }

    o = PyDict_GetItemString(kwargs, "quote_strings");
    if (o)
    {
      if (PyObject_IsTrue(o))
        format.options &= ~DONT_QUOTE_STRINGS;
      else
        format.options |= DONT_QUOTE_STRINGS;
    }

    o = PyDict_GetItemString(kwargs, "compact");
    if (o)
    {
      if (PyObject_IsTrue(o))
        format.options |= COMPACT;
      else
        format.options &= ~COMPACT;
    }

    o = PyDict_GetItemString(kwargs, "legacy");
    if (o)
    {
      if (PyObject_IsTrue(o))
        format.options |= LEGACY;
      else
        format.options &= ~LEGACY;
    }
  }

  buffer = g_string_sized_new(8192);
  coil_struct_build_string(self->node, buffer, &format, &error);
  if (G_UNLIKELY(error))
  {
    g_string_free(buffer, TRUE);
    cCoil_error(&error);
    return NULL;
  }

  pystring = PyString_FromStringAndSize(buffer->str, (Py_ssize_t)buffer->len);
  g_string_free(buffer, TRUE);

  return pystring;
}

static PyObject *
struct_validate_key(PyCoilStruct *self,
                    PyObject     *args)
{
  gchar      *key;
  Py_ssize_t  key_len;
  PyObject   *pykey, *pyresult;

  if (!PyArg_ParseTuple(args, "S:cCoil.Struct.validate_key", &pykey))
   return NULL;

  if (PyString_AsStringAndSize(pykey, &key, &key_len) < 0)
    return NULL;

  if (coil_validate_key_len(key, key_len))
    pyresult = Py_True;
  else
    pyresult = Py_False;

  Py_INCREF(pyresult);
  return pyresult;
}

static PyObject *
struct_validate_path(PyCoilStruct *self,
                     PyObject     *args)
{
  gchar      *path;
  Py_ssize_t  path_len;
  PyObject   *pypath, *pyresult;

  if (!PyArg_ParseTuple(args, "S:cCoil.Struct.validate_path", &pypath))
    return NULL;

  if (PyString_AsStringAndSize(pypath, &path, &path_len) < 0)
    return NULL;

  if (coil_validate_path_len(path, path_len))
    pyresult = Py_True;
  else
    pyresult = Py_False;

  Py_INCREF(pyresult);
  return pyresult;
}


static int
struct_compare(PyCoilStruct *a,
               PyCoilStruct *b)
{
  gboolean result;
  GError  *error = NULL;

  result = coil_struct_equals(a->node, b->node, &error);
  if (G_UNLIKELY(error))
  {
    cCoil_error(&error);
    return -1;
  }

  return (result) ? 0 : -1;
}

static PyObject *
struct_str(PyCoilStruct *self)
{
  gchar    *string;
  PyObject *pystring;
  GError   *error = NULL;
  GString  *buffer;

  buffer = g_string_sized_new(8192);
  coil_struct_build_string(self->node, buffer, &default_string_format, &error);
  if (G_UNLIKELY(error))
  {
    g_string_free(buffer, TRUE);
    cCoil_error(&error);
    return NULL;
  }

  pystring = PyString_FromStringAndSize(buffer->str, buffer->len);
  if (pystring == NULL)
  {
    g_string_free(buffer, TRUE);
    return NULL;
  }

  g_string_free(buffer, TRUE);

  return pystring;
}

static PyObject *
struct_mp_get(PyCoilStruct *self,
              PyObject     *pypath)
{
  CoilPath     *path = NULL;
  const GValue *value = NULL;
  GError       *error = NULL;

  path = coil_path_from_pystring(pypath, &error);
  if (path == NULL)
    goto error;

  value = coil_struct_lookup_path(self->node, path, TRUE, &error);
  if (G_UNLIKELY(error))
    goto error;

  if (value == NULL)
  {
    gchar buf[2 * COIL_PATH_LEN + 128];

    g_snprintf(buf, sizeof(buf),
               "<%s> The path '%s' was not found.",
               coil_struct_get_path(self->node)->path,
               path->path);

    PyErr_SetString(KeyMissingError, buf);
    goto error;
  }

  coil_path_unref(path);
  return coil_value_as_pyobject(value);

error:
  if (path)
    coil_path_unref(path);

  if (error)
    cCoil_error(&error);

  return NULL;
}

static int
struct_mp_set(PyCoilStruct *self,
              PyObject     *pypath,
              PyObject     *pyvalue)
{
  CoilPath   *path;
  GValue     *value;
  GError     *error = NULL;

  path = coil_path_from_pystring(pypath, &error);
  if (path == NULL)
    goto error;

  if (pyvalue == NULL)
  {
    /* delete the value */
    if (!coil_struct_delete_path(self->node, path, TRUE, &error))
      goto error;
  }
  else
  {
    value = coil_value_from_pyobject(pyvalue);
    if (value == NULL)
      goto error;

    if (!coil_struct_insert_path(self->node, path, value, TRUE, &error))
      goto error;
  }

  coil_path_unref(path);
  return 0;

error:
  if (path)
    coil_path_unref(path);

  if (error)
    cCoil_error(&error);

  return -1;
}

static Py_ssize_t
struct_mp_size(PyCoilStruct *self)
{
  GError     *error = NULL;
  Py_ssize_t  size;

  size = (Py_ssize_t)coil_struct_get_size(self->node, &error);
  if (G_UNLIKELY(error))
  {
    cCoil_error(&error);
    return -1;
  }

  return size;
}

static PyObject *
struct_iter_init(PyObject *obj)
{
  PyCoilStruct *self = (PyCoilStruct *)obj;

  if (self->iterator == NULL)
    self->iterator = g_new(CoilStructIter, 1);

  coil_struct_iter_init(self->iterator, self->node);

  Py_INCREF(obj);
  return obj;
}

static PyObject *
struct_iter_next(PyCoilStruct *self)
{
  const CoilPath *path;

  if (self->iterator == NULL)
  {
    PyErr_SetString(PyExc_RuntimeError, "cCoil.Struct iterator must be \
        initialized before iteration");

    return NULL;
  }

  if (coil_struct_iter_next(self->iterator, &path, NULL))
    return PyString_FromString(path->key);

  return NULL;
}

static PyMethodDef struct_methods[] =
{
/*  { "copy",          (PyCFunction)struct_copy, METH_NOARGS, NULL},
  { "container",     (PyCFunction)struct_get_container, METH_NOARGS, NULL}, */
  { "dict",          (PyCFunction)struct_to_dict, METH_VARARGS | METH_KEYWORDS, NULL}, /*
  { "expand",        (PyCFunction)struct_expand, METH_VARARGS | METH_KEYWORDS, NULL},
  { "expanditem",    (PyCFunction)struct_expanditem, METH_VARARGS | METH_KEYWORDS, NULL},
  { "expandvalue",   (PyCFunction)struct_expandvalue, METH_VARARGS | METH_KEYWORDS, NULL},
  { "extend",        (PyCFunction)struct_extend, METH_VARARGS | METH_KEYWORDS, NULL}, */
  { "get",           (PyCFunction)struct_get, METH_VARARGS | METH_KEYWORDS, NULL},
  { "has_key",       (PyCFunction)struct_has_key, METH_VARARGS, NULL},
  { "items",         (PyCFunction)struct_items, METH_VARARGS | METH_KEYWORDS, NULL},
/*  { "iteritems",     (PyCFunction)struct_iteritems, METH_NOARGS, NULL},
  { "iterkeys",      (PyCFunction)struct_iterkeys, METH_NOARGS, NULL},
  { "itervalues",    (PyCFunction)struct_itervalues, METH_NOARGS, NULL},
  { "is_ancestor",   (PyCFunction)NULL, 0, NULL},
  { "is_descendent", (PyCFunction)NULL, 0, NULL}, */
  { "keys",          (PyCFunction)struct_keys, METH_VARARGS | METH_KEYWORDS, NULL},
  { "merge",         (PyCFunction)struct_merge, METH_VARARGS, NULL},
  { "path",          (PyCFunction)struct_path, METH_VARARGS | METH_KEYWORDS, NULL},
  { "set",           (PyCFunction)struct_set, METH_VARARGS | METH_KEYWORDS, NULL},
  { "string",        (PyCFunction)struct_to_string, METH_VARARGS | METH_KEYWORDS, NULL},
//  { "unexpanded",    (PyCFunction)struct_unexpanded, METH_VARARGS | METH_KEYWORDS, NULL},

  { "validate_key",  (PyCFunction)struct_validate_key, METH_VARARGS | METH_CLASS, NULL},
  { "validate_path", (PyCFunction)struct_validate_path, METH_VARARGS | METH_CLASS, NULL},
  { "values",        (PyCFunction)struct_values, METH_NOARGS, NULL },
  { NULL, NULL, 0, NULL },
};

PyMappingMethods PyCoilStruct_Mapping =
{
 (lenfunc)struct_mp_size,
 (binaryfunc)struct_mp_get,
 (objobjargproc)struct_mp_set,
};

PyTypeObject PyCoilStruct_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,
  "cCoil.Struct",
  sizeof(PyCoilStruct),
  0,
  /* methods */
  (destructor)struct_dealloc,
  (printfunc)0,
  (getattrfunc)0,
  (setattrfunc)0,
  (cmpfunc)struct_compare,
  (reprfunc)0,
  0, /* number methods */
  0, /* sequence methods */
  &PyCoilStruct_Mapping, /* mapping methods */
  (hashfunc)0,
  (ternaryfunc)0,
  (reprfunc)struct_str,
  (getattrofunc)0,
  (setattrofunc)0,
  0,
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  NULL, /* Doc String */
  (traverseproc)0,
  (inquiry)0,
  (richcmpfunc)0,
  0,
  (getiterfunc)struct_iter_init,
  (iternextfunc)struct_iter_next,
  struct_methods,
  0,
  0,
  NULL,
  NULL,
  (descrgetfunc)0,
  (descrsetfunc)0,
  0,
  (initproc)struct_init,
};
