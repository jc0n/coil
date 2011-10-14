/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coilmodule.h"

struct _ListProxyObject {
    PyObject_HEAD

    CoilStruct *node;
    GValue *value;
};

PyDoc_STRVAR(listproxy_doc,
    "TODO(jcon): Document this");


PyObject *
ccoil_listproxy_new(CoilStruct *node, GValue *value)
{
    g_return_val_if_fail(node != NULL, NULL);
    g_return_val_if_fail(value != NULL, NULL);

    ListProxyObject *proxy;

    if (!G_VALUE_HOLDS(value, COIL_TYPE_LIST)) {
        PyErr_Format(PyExc_TypeError,
                     "list proxy requires a coil list value, not %s value",
                     G_VALUE_TYPE_NAME(value));
        return NULL;
    }

    proxy = PyObject_New(ListProxyObject, &ListProxyObject_Type);
    if (proxy == NULL)
        return NULL;

    proxy->node = g_object_ref(G_OBJECT(node));
    proxy->value = value;

    return (PyObject *)proxy;
}

int
listproxy_register_types(PyObject *m, PyObject *d)
{
    PyType_Register(d, ListProxyObject_Type, "_ListProxy", 0);
}

static void
listproxy_dealloc(ListProxyObject *self)
{
    self->value = NULL;
    g_object_unref(self->node);
}

static void
listproxy_free(ListProxyObject *self)
{
}

#define CHECK_INITIALIZED(list) \
    if (list == NULL) { \
        PyErr_SetString(PyExc_RuntimeError, "coil list is not initialized"); \
        return NULL; \
    }

static PyObject *
list_insert(ListProxyObject *self, PyObject *args)
{
    Py_ssize_t i;
    PyObject *v;
    GValue *value;
    GList *list;

    list = g_value_get_boxed(self->value);

    CHECK_INITIALIZED(list);

    if (!PyArg_ParseTuple(args, "nO:insert", &i, &v))
        return NULL;

    /* check what the value is */
    if (PyCoilStruct_Check(v)) {
        PyErr_Format(PyExc_TypeError,
                     "coil list cannot contain type '%s'",
                     Py_TYPE_NAME(v));
        return NULL;
    }

    /* convert to coil type */
    value = coil_value_from_pyobject(v);
    if (value == NULL)
        return NULL;

    list = g_list_insert(list, value, i);
    g_value_take_boxed(self->value, list);

    Py_RETURN_NONE;
}

static PyObject *
list_append(PyObject *self, PyObject *args)
{

}

static PyObject *
list_count(PyObject *self, PyObject *args)
{

}

static PyObject *
list_extend(PyObject *self, PyObject *args)
{

}

static PyObject *
list_index(PyObject *self, PyObject *args)
{

}

static PyObject *
list_pop(PyObject *self, PyObject *args)
{
}

static PyObject *
list_remove(PyObject *self, PyObject *args)
{

}

static PyObject *
list_reverse(PyObject *self, PyObject *args)
{

}

static PyObject *
list_sort(PyObject *self, PyObject *args)
{

}

static int
listproxy_init(ListProxyObject *self, PyObject *args, PyObject *kwargs)
{
    self->value = NULL;
    self->node = NULL;
    return 0;
}

PySequenceMethods listproxy_as_sequence =
{
	(lenfunc)0,			          /* sq_length */
	(binaryfunc)0,            	  /* sq_concat */
	(ssizeargfunc)0,          	  /* sq_repeat */
	(ssizeargfunc)0,			  /* sq_item */
    0,                              /* sq_slice */
	(ssizeobjargproc)0,		      /* sq_ass_item */
    0,                              /* sq_ass_slice */
	(objobjproc)0,                /* sq_contains */
	(binaryfunc)0,			      /* sq_inplace_concat */
	(ssizeargfunc)0,			  /* sq_inplace_repeat */
};

static PyMethodDef listproxy_methods[] =
{
    {"insert", (PyCFunction)list_insert, METH_VARARGS, NULL},
    {NULL, NULL}
};

PyTypeObject ListProxyObject_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,
  "ccoil._ListProxy",                            /* tp_name */
  sizeof(ListProxyObject),                      /* tp_basicsize */
  0,                                            /* tp_itemsize */

  /* methods */
  (destructor)listproxy_dealloc,                     /* tp_dealloc */
  (printfunc)0,                                 /* tp_print */
  (getattrfunc)0,                               /* tp_getattr */
  (setattrfunc)0,                               /* tp_setattr */
  (cmpfunc)0,                                   /* tp_compare */
  (reprfunc)0,                                  /* tp_repr */

  /* number methods */
  0,                                            /* tp_as_number */
  &listproxy_as_sequence,                            /* tp_as_sequence */
  0,                                            /* tp_as_mapping */

  /* standard operations */
  (hashfunc)0,                                  /* tp_hash */
  (ternaryfunc)0,                               /* tp_call */
  (reprfunc)0,                                  /* tp_str */
  (getattrofunc)0,                              /* tp_getattro */
  (setattrofunc)0,                              /* tp_setattro */

  /* buffer */
  0,                                            /* tp_as_buffer */

  /* flags */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,

  listproxy_doc,                                     /* tp_doc */
  (traverseproc)0,                              /* tp_traverse */
  (inquiry)0,                                   /* tp_clear */
  (richcmpfunc)0,                               /* tp_richcompare */
  0,                                            /* tp_weaklistoffset */
  (getiterfunc)0,                               /* tp_iter */
  (iternextfunc)0,                              /* tp_iternext */
  listproxy_methods,                            /* tp_methods */
  0,                                            /* tp_members */
  0,                                            /* tp_getset */
  0,                                            /* tp_base */
  NULL,                                         /* tp_dict */
  (descrgetfunc)0,                              /* tp_descr_get */
  (descrsetfunc)0,                              /* tp_descr_set */
  0,                                            /* tp_dictoffset */
  (initproc)listproxy_init,                     /* tp_init */
  PyType_GenericAlloc,                          /* tp_alloc */
  PyType_GenericNew,                            /* tp_new */
  (freefunc)listproxy_free,                     /* tp_free */
};
