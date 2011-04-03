/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coilmodule.h"
#include "coilstruct.h"

PyDoc_STRVAR(cCoil_module_documentation,
    "C implementation and optimization of the Python coil module.");

PyObject *cCoilError;
PyObject *StructError;
PyObject *LinkError;
PyObject *IncludeError;
PyObject *KeyMissingError;
PyObject *KeyValueError;
PyObject *ParseError;

PyObject *
pylist_from_value_list(const GList *list)
{
  Py_ssize_t i, size;
  PyObject  *pylist, *pyitem;

  if (list == NULL)
    return PyList_New(0);

  size = g_list_length((GList *)list);
  pylist = PyList_New(size);

  for (i = 0; i < size; i++, list = g_list_next(list))
  {
    g_assert(G_IS_VALUE(list->data));
    pyitem = coil_value_as_pyobject((GValue *)list->data);
    if (pyitem == NULL)
    {
      Py_DECREF(pylist);
      return NULL;
    }

    PyList_SET_ITEM(pylist, i, pyitem);
  }

  return pylist;
}

static GList *
value_list_from_pysequence(PyObject *obj)
{
  GList      *list = NULL;
  Py_ssize_t  size;
  PyObject   *seq, *item;

  seq = PySequence_Fast(obj, "Expecting sequence type.");
  if (seq == NULL)
    return NULL;

  Py_DECREF(obj);
  size = PySequence_Fast_GET_SIZE(seq);

  while (size--)
  {
    item = PySequence_Fast_GET_ITEM(obj, size);
    list = g_list_prepend(list, coil_value_from_pyobject(item));
  }

  return list;
}

CoilPath *
coil_path_from_pystring(PyObject  *o,
                        GError   **error)
{
  gchar      *str;
  Py_ssize_t  len;

  if (PyString_AsStringAndSize(o, &str, &len) < 0)
    return NULL;

  return coil_path_new_len(str, (guint)len, error);
}

GValue *
coil_value_from_pyobject(PyObject *o)
{
  GValue       *value = NULL;
  PyTypeObject *type = (PyTypeObject *)o->ob_type;

  if (o == NULL)
  {
    PyErr_SetString(PyExc_RuntimeError, "NULL python object.");
    return NULL;
  }

  if (o == Py_None)
    new_value(value, COIL_TYPE_NONE, set_object, coil_none_object);
  else if (o == Py_True)
    new_value(value, G_TYPE_BOOLEAN, set_boolean, TRUE);
  else if (o == Py_False)
    new_value(value, G_TYPE_BOOLEAN, set_boolean, FALSE);
  else if (type == &PyInt_Type)
    new_value(value, G_TYPE_INT, set_int, (gint)PyInt_AsLong(o));
  else if(type == &PyLong_Type)
    new_value(value, G_TYPE_LONG, set_long, (glong)PyLong_AsLong(o));
  else if(type == &PyFloat_Type)
    new_value(value, G_TYPE_FLOAT, set_float, (gfloat)PyFloat_AsDouble(o));
  else if (type == &PyCoilStruct_Type)
    new_value(value, COIL_TYPE_STRUCT, set_object, ((PyCoilStruct *)o)->node);
  else if(PyString_Check(o))
    new_value(value, G_TYPE_STRING, set_string, (gchar *)PyString_AsString(o));
  else if (PyList_Check(o) || PyTuple_Check(o))
    new_value(value, COIL_TYPE_LIST, set_boxed, value_list_from_pysequence(o));
  else if (PyDict_Check(o))
  {
    CoilStruct *node = coil_struct_new(NULL, NULL);
    if (!update_struct_from_pyitems(node, o))
    {
      g_object_unref(node);
      return NULL;
    }

    new_value(value, COIL_TYPE_STRUCT, take_object, node);
  }
  else
  {
    PyErr_SetString(PyExc_TypeError, "Unsupported coil value type");
  }

#if 0
      case &PyLong_Type:
        new_value(value, G_TYPE_LONG, set_long, (glong)PyLong_AsLong(obj));
        break;
      case &PyFloat_Type:
        new_value(value, G_TYPE_LONG, set_float, (gfloat)PyFloat_AsLong(obj));
        break;
      case &PyString_Type:
        new_value(value, G_TYPE_STRING, set_string, (gchar *)PyString_AsString(obj));
        break;

      default:
        PyErr_SetString(PyExc_TypeError, "Unsupported coil value type");
        return NULL;
    }

    case &PyInt_Type:
    case &PyLong_Type:
    case &PyFloat_Type:
    case &PyComplex_Type:
    case &PyUnicode_Type:
    case &PyByteArray_Type:
    case &PyString_Type:
    case &PyBuffer_Type:
    case &PyTuple_Type:
    case &PyList_Type:
    case &PyDict_Type:
    case &PyClass_Type:
    case &PyInstance_Type:
    case &PyFunction_Type:
    case &PyMethod_Type:
    case &PyFile_Type:
    case &PyModule_Type:
    case &PySeqIter_Type:
    case &PyCallIter_Type:
    case &PyProperty_Type:
    case &PySlice_Type:
    case &PyCell_Type:
    case &PyGen_Type:
    case &PySet_Type:
    case &PyFrozenSet_Type:
    case &PyCode_Type:
    */
#endif

  return value;
}

PyObject *
coil_value_as_pyobject(const GValue *value)
{
    gchar buf[128];
    GType type;

    if (value == NULL)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }

    type = G_VALUE_TYPE(value);

    switch (G_TYPE_FUNDAMENTAL(type))
    {
      case G_TYPE_CHAR:
      {
	      gint8 val = g_value_get_char(value);
	      return PyString_FromStringAndSize((char *)&val, 1);
      }
      case G_TYPE_UCHAR:
      {
	      guint8 val = g_value_get_uchar(value);
	      return PyString_FromStringAndSize((char *)&val, 1);
      }
      case G_TYPE_BOOLEAN:
      {
	      return PyBool_FromLong(g_value_get_boolean(value));
      }
      case G_TYPE_INT:
	      return PyInt_FromLong(g_value_get_int(value));
      case G_TYPE_UINT:
	    {
	     /* in Python, the Int object is backed by a long.  If a
	         long can hold the whole value of an unsigned int, use
	         an Int.  Otherwise, use a Long object to avoid overflow.
	        This matches the ULongArg behavior in codegen/argtypes.h */
        #if (G_MAXUINT <= G_MAXLONG)
          return PyLong_FromLong((glong) g_value_get_uint(value));
        #else
          return PyLong_FromUnsignedLong((gulong) g_value_get_uint(value));
        #endif
	    }
      case G_TYPE_LONG:
	      return PyLong_FromLong(g_value_get_long(value));
      case G_TYPE_ULONG:
	    {
	       gulong val = g_value_get_ulong(value);
	       if (val <= G_MAXLONG)
		      return PyLong_FromLong((glong) val);
	       else
		      return PyLong_FromUnsignedLong(val);
	    }
      case G_TYPE_INT64:
	    {
	      gint64 val = g_value_get_int64(value);
	      if (G_MINLONG <= val && val <= G_MAXLONG)
		      return PyLong_FromLong((glong) val);
	      else
		      return PyLong_FromLongLong(val);
	    }
      case G_TYPE_UINT64:
	    {
	       guint64 val = g_value_get_uint64(value);

	       if (val <= G_MAXLONG)
		       return PyLong_FromLong((glong) val);
	       else
		       return PyLong_FromUnsignedLongLong(val);
	    }
      case G_TYPE_FLOAT:
	      return PyFloat_FromDouble(g_value_get_float(value));
      case G_TYPE_DOUBLE:
	      return PyFloat_FromDouble(g_value_get_double(value));
      case G_TYPE_STRING:
	    {
	      const gchar *str = g_value_get_string(value);
	      if (str)
		      return PyString_FromString(str);
	      Py_INCREF(Py_None);
	      return Py_None;
	    }
      case G_TYPE_OBJECT:
      {
        if (type == COIL_TYPE_STRUCT)
          return cCoil_struct_new(g_value_dup_object(value));
        else if (type == COIL_TYPE_NONE)
        {
          Py_INCREF(Py_None);
          return Py_None;
        }
        break;
      }

      case G_TYPE_BOXED:
        if (type == G_TYPE_GSTRING)
        {
          GString *buf = g_value_get_boxed(value);
          return PyString_FromStringAndSize(buf->str, buf->len);
        }
        else if (type == COIL_TYPE_LIST)
          return pylist_from_value_list(g_value_get_boxed(value));
    }

    g_snprintf(buf, sizeof(buf), "unknown type %s",
	       g_type_name(type));
    PyErr_SetString(PyExc_TypeError, buf);

    return NULL;
}
void
cCoil_error(GError **error)
{
  g_return_if_fail(error && *error);
  g_return_if_fail((*error)->domain == COIL_ERROR);

  PyObject *e = NULL;
  PyObject *msg = NULL;

  switch ((*error)->code)
  {
    case COIL_ERROR_INTERNAL:
      e = cCoilError;
      break;

    case COIL_ERROR_INCLUDE:
      e = IncludeError;
      break;

    case COIL_ERROR_KEY:
    case COIL_ERROR_PATH:
      e = KeyValueError;
      break;

    case COIL_ERROR_LINK:
      e = LinkError;
      break;

    case COIL_ERROR_PARSE:
      e = ParseError;
      break;

    case COIL_ERROR_STRUCT:
      e = StructError;
      break;

    case COIL_ERROR_VALUE:
      e = PyExc_ValueError;
      break;

    default:
      g_error("Unknown coil error code %d", (*error)->code);
  }

  msg = PyString_FromString((*error)->message);
  if (msg == NULL)
    return;

  PyErr_SetObject(e, msg);

  g_error_free(*error);
  g_nullify_pointer((gpointer *)error);

  Py_DECREF(msg);
}

/*
"""The standard coil parser.

    :param input_: An iterator over lines of input.
        Typically a C{file} object or list of strings.
    :param path: Path to input file, used for errors and @file imports.
    :param encoding: Read strings using the given encoding. All
        string values will be `unicode` objects rather than `str`.
    :param expand: Enables/disables expansion of the parsed tree.
    :param defaults: See :meth:`struct.Struct.expanditem`
    :param ignore_missing: See :meth:`struct.Struct.expanditem`
    """
*/
static CoilStruct *
parse_pysequence(PyObject *seq)
{
  CoilStruct *result;
  PyObject   *s = NULL, *o = NULL;
  Py_ssize_t  n, i;
  GString    *buffer;
  GError     *error = NULL;

  buffer = g_string_sized_new(8192);

  n = PySequence_Size(seq);
  if (n < 0)
    return NULL;

  for (i = 0; i < n; i++)
  {
    char      *str;
    Py_ssize_t len;

    o = PySequence_ITEM(seq, i);
    if (o == NULL)
      goto error;

    s = PyObject_Str(o);
    if (s == NULL)
      goto error;

    if (PyString_AsStringAndSize(s, &str, &len) < 0)
      goto error;

    g_string_append_len(buffer, str, len);

    Py_DECREF(o);
    Py_DECREF(s);
  }

  result = coil_parse_string_len(buffer->str, buffer->len, &error);
  if (result == NULL)
    goto error;

  g_string_free(buffer, TRUE);
  return result;

error:
  if (error)
    cCoil_error(&error);

  Py_XDECREF(o);
  Py_XDECREF(s);

  g_string_free(buffer, TRUE);

  return NULL;
}

static PyObject *
cCoil_parse(PyObject *ignored,
            PyObject *args,
            PyObject *kwargs)
{
  static char *kwlist[] = {"expand", "defaults", "ignore_missing", NULL};
  PyObject    *input = NULL;
  PyObject    *expand = NULL;
  PyObject    *defaults = NULL;
  PyObject    *ignore_missing = NULL;

  CoilStruct  *root;
  GError      *error = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                  "O|OOO:parse",
                                  kwlist,
                                  &input,
                                  &expand,
                                  &defaults,
                                  &ignore_missing))
    return NULL;

  if (PyFile_Check(input))
  {
    FILE *fp = PyFile_AsFile(input);
    /* TODO(jcon): fix setting filename when locations are refactored*/
    //PyObject *filename = PyFile_Name(input);
    //
/*    PyFile_IncUseCount((PyFileObject *)input); python 2.6 */
    root = coil_parse_stream(fp, NULL/*filename*/, &error);
/*    PyFile_DecUseCount((PyFileObject *)input); python 2.6 */

    if (G_UNLIKELY(error))
      goto error;
  }
  else if (PyString_Check(input))
  {
    char      *buffer;
    Py_ssize_t len;

    if (PyString_AsStringAndSize(input, &buffer, &len) < 0)
      goto error;

    root = coil_parse_string_len(buffer, len, &error);
    if (root == NULL)
      goto error;
  }
  else if (PySequence_Check(input))
  {
    root = parse_pysequence(input);
    if (root == NULL)
      goto error;
  }
  else
  {
    PyErr_Format(PyExc_TypeError,
                 "input argument must be a string, " \
                 "list of strings, or a file object, not '%s'",
                 Py_TYPE(input)->tp_name);

    return NULL;
  }

  /* TODO(jcon): implement expand, defaults, and ignore_missing later */

  return cCoil_struct_new(root);


error:
  if (error)
    cCoil_error(&error);

  return NULL;
}

static PyObject *
cCoil_parse_file(PyObject *ignored,
                 PyObject *args,
                 PyObject *kwargs)
{
  static char *kwlist[] = {"expand", "defaults", "ignore_missing", NULL};
  CoilStruct  *root = NULL;
  GError      *error = NULL;
  PyObject    *expand = NULL;
  PyObject    *defaults = NULL;
  PyObject    *ignore_missing = NULL;
  gchar       *filepath = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|OOO:parse_file",
                                   kwlist, &filepath, &expand,
                                   &defaults, &ignore_missing))
    return NULL;

  root = coil_parse_file(filepath, &error);
  if (root == NULL)
  {
    cCoil_error(&error);
    return NULL;
  }

  /* TODO(jcon): expand, defaults, ignore_missing */

  return cCoil_struct_new(root);
}

static PyMethodDef cCoil_functions[] =
{
  { "parse",      (PyCFunction)cCoil_parse,      METH_VARARGS | METH_KEYWORDS, NULL},
  { "parse_file", (PyCFunction)cCoil_parse_file, METH_VARARGS | METH_KEYWORDS, NULL},
  { NULL, NULL, 0, NULL },
};

static int
init_exceptions(PyObject *module_dict)
{
  PyObject *bases;

  cCoilError = PyErr_NewException("cCoil.CoilError", NULL, NULL);
  if (cCoilError == NULL)
    return 0;

  StructError = PyErr_NewException("cCoil.StructError", cCoilError, NULL);
  if (StructError == NULL)
    return 0;

  LinkError = PyErr_NewException("cCoil.LinkError", cCoilError, NULL);
  if (LinkError == NULL)
    return 0;

  IncludeError = PyErr_NewException("cCoil.IncludeError", cCoilError, NULL);
  if (IncludeError == NULL)
    return 0;

  bases = PyTuple_Pack(2, cCoilError, PyExc_KeyError);
  if (bases == NULL)
    return 0;

  KeyMissingError = PyErr_NewException("cCoil.KeyMissingError", bases, NULL);
  if (KeyMissingError == NULL)
    return 0;

  KeyValueError = PyErr_NewException("cCoil.KeyValueError", bases, NULL);
  if (KeyValueError == NULL)
    return 0;

  Py_DECREF(bases);

  ParseError = PyErr_NewException("cCoil.ParseError", cCoilError, NULL);
  if (ParseError == NULL)
    return 0;

  return 1;
}

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif

PyMODINIT_FUNC
initcCoil(void)
{
  PyObject *m, *d;

  m = Py_InitModule3("cCoil",
                     cCoil_functions,
                     cCoil_module_documentation);

  if (m == NULL)
    return;

  d = PyModule_GetDict(m);

  if (!init_exceptions(d))
    return;

  PyType_Register(d, PyCoilStruct_Type, "Struct");

  coil_init();
}
