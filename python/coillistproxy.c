/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coilmodule.h"

struct _ListProxyObject {
    PyObject_HEAD

    CoilObject *node;
    CoilObject *list;
};

PyDoc_STRVAR(listproxy_doc,
    "TODO(jcon): Document this");


PyObject *
ccoil_listproxy_new(CoilObject *node, CoilObject *list)
{
    g_return_val_if_fail(node != NULL, NULL);
    g_return_val_if_fail(list != NULL, NULL);

    ListProxyObject *self;

    self = PyObject_New(ListProxyObject, &ListProxy_Type);
    if (self == NULL) {
        return NULL;
    }

    self->node = coil_object_ref(node);
    self->list = coil_object_ref(list);

    return (PyObject *)self;
}

int
listproxy_register_types(PyObject *m, PyObject *d)
{
    PyType_Register(d, ListProxy_Type, "_ListProxy", 0);
    return 0;
}

static void
listproxy_dealloc(ListProxyObject *self)
{
    CLEAR(self->list, coil_object_unref);
    CLEAR(self->node, coil_object_unref);
}

static const char _listproxy_notinitialized[] = "coil list is not initialized";

#define CHECK_INITIALIZED(self, rval)                                        \
    if (self->list == NULL || !G_IS_OBJECT(self->node)) {                    \
        PyErr_SetString(PyExc_RuntimeError, _listproxy_notinitialized);      \
        return (rval);                                                       \
    }                                                                        \

#define CHECK_VALUE(value) \
    if (PyCoilStruct_Check(value)) {                                         \
        PyErr_Format(PyExc_TypeError,                                        \
                     "coil list cannot contain type '%s'",                   \
                     Py_TYPE_NAME(value));                                   \
        return NULL;                                                         \
    }                                                                        \

static PyObject *
listproxy_insert(ListProxyObject *self, PyObject *args)
{
    Py_ssize_t i, n;
    PyObject *v;
    CoilValue *value;

    CHECK_INITIALIZED(self, NULL);

    if (!PyArg_ParseTuple(args, "nO:insert", &i, &v)) {
        return NULL;
    }
    CHECK_VALUE(v);

    /* convert to coil type */
    value = coil_value_from_pyobject(v);
    if (value == NULL) {
        return NULL;
    }
    n = coil_list_length(self->list);
    if (i < 0) {
        i = MAX(n - i, 0);
    }
    else if (i > n) {
        i = n;
    }
    coil_list_insert(self->list, i, value);
    Py_RETURN_NONE;
}

static PyObject *
listproxy_append(ListProxyObject *self, PyObject *v)
{
    CoilValue *value;

    CHECK_INITIALIZED(self, NULL);
    CHECK_VALUE(v);

    value = coil_value_from_pyobject(v);
    if (value == NULL) {
        return NULL;
    }
    coil_list_append(self->list, value);
    Py_RETURN_NONE;
}

static PyObject *
listproxy_copy(ListProxyObject *self, PyObject *unused)
{
    PyObject *res;
    guint i, n;

    CHECK_INITIALIZED(self, NULL);

    n = coil_list_length(self->list);
    res = PyList_New(n);
    if (res == NULL) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        PyObject *pyval;
        CoilValue *value;

        value = coil_list_get_index(self->list, i);
        pyval = coil_value_as_pyobject(self->node, value);
        if (pyval == NULL) {
            Py_DECREF(res);
            return NULL;
        }
        PyList_SET_ITEM(res, i, pyval);
    }
    return res;
}

static PyObject *
listproxy_clear(ListProxyObject *self, PyObject *unused)
{
    guint n;

    CHECK_INITIALIZED(self, NULL);
    n = coil_list_length(self->list);
    coil_list_remove_range(self->list, 0, n);
    Py_RETURN_NONE;
}

static PyObject *
listproxy_count(ListProxyObject *self, PyObject *v)
{
    Py_ssize_t count = 0;
    gsize i, n;

    CHECK_INITIALIZED(self, NULL);
    n = coil_list_length(self->list);
    for (i = 0; i < n; i++) {
        int cmp;
        CoilValue *value = coil_list_get_index(self->list, i);
        PyObject *pyval = coil_value_as_pyobject(self->node, value);
        if (pyval == NULL) {
            return NULL;
        }
        cmp = PyObject_RichCompareBool(pyval, v, Py_EQ);
        if (cmp > 0) {
            count++;
        }
        else if (cmp < 0) {
            return NULL;
        }
    }
    return PyLong_FromLong(count);
}

static PyObject *
listproxy_extend(ListProxyObject *self, PyObject *args)
{
    /* TODO */
    Py_RETURN_NONE;
}

static PyObject *
listproxy_index(ListProxyObject *self, PyObject *args)
{
    /* TODO */
    Py_RETURN_NONE;
}

static PyObject *
convert_to_python_list(CoilObject *node, CoilObject *list)
{
    PyObject *res;
    gsize i, n;

    n = coil_list_length(list);
    res = PyList_New(n);
    if (res == NULL) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        PyObject *pyval;
        CoilValue *value;

        value = coil_list_get_index(list, i);
        if (G_VALUE_HOLDS(value, COIL_TYPE_LIST)) {
            CoilObject *inner_list = coil_value_get_object(value);
            pyval = convert_to_python_list(node, inner_list);
        }
        else {
            pyval = coil_value_as_pyobject(node, value);
        }
        if (pyval == NULL) {
            Py_DECREF(res);
            return NULL;
        }
        PyList_SET_ITEM(res, i, pyval);
    }
    return res;
}

static PyObject *
listproxy_pop(ListProxyObject *self, PyObject *args)
{
    PyObject *res;
    Py_ssize_t i = -1;
    CoilValue *value;
    gsize n;

    CHECK_INITIALIZED(self, NULL);
    if (!PyArg_ParseTuple(args, "|n:pop", &i)) {
        return NULL;
    }

    n = coil_list_length(self->list);
    if (n == 0) {
        PyErr_SetString(PyExc_IndexError, "pop from empty list");
        return NULL;
    }
    if (i < 0) {
        i += n;
    }
    if (i < 0 || i >= n) {
        PyErr_SetString(PyExc_IndexError, "index out of range");
        return NULL;
    }

    value = coil_list_get_index(self->list, i);
    if (G_VALUE_HOLDS(value, COIL_TYPE_LIST)) { /* XXX: necessary ? */
        CoilObject *inner_list = coil_value_get_object(value);
        res = convert_to_python_list(self->node, inner_list);
    }
    else {
        res = coil_value_as_pyobject(self->node, value);
    }
    if (res == NULL) {
        return NULL;
    }
    coil_list_remove(self->list, i);
    return res;
}

static PyObject *
listproxy_remove(ListProxyObject *self, PyObject *arg)
{
    gsize i, n;

    CHECK_INITIALIZED(self, NULL);
    CHECK_VALUE(arg);

    n = coil_list_length(self->list);
    for (i = 0; i < n; i++) {
        int cmp;
        CoilValue *value;
        PyObject *pyval;

        value = coil_list_get_index(self->list, i);
        pyval = coil_value_as_pyobject(self->node, value);
        if (pyval == NULL) {
            return NULL;
        }
        cmp = PyObject_RichCompareBool(pyval, arg, Py_EQ);
        Py_DECREF(pyval);
        if (cmp < 0) {
            return NULL;
        }
        if (cmp) {
            coil_list_remove(self->list, i);
            Py_RETURN_NONE;
        }
    }
    PyErr_SetString(PyExc_ValueError, "item not in list");
    return NULL;
}

static PyObject *
listproxy_sort(ListProxyObject *self, PyObject *args)
{
    /* TODO */
    Py_RETURN_NONE;
}

static int
listproxy_init(ListProxyObject *self, PyObject *args, PyObject *kwargs)
{
    self->list = NULL;
    self->node = NULL;
    return 0;
}

static Py_ssize_t
listproxy_len(ListProxyObject *self)
{
    if (self->list == NULL) {
        return 0;
    }
    return coil_list_length(self->list);
}

static int
listproxy_contains(ListProxyObject *self, PyObject *item)
{
    gsize i, n;

    CHECK_INITIALIZED(self, -1);

    n = coil_list_length(self->list);
    for (i = 0; i < n; i++) {
        int cmp;
        CoilValue *value;
        PyObject *pyval;

        value = coil_list_get_index(self->list, i);
        pyval = coil_value_as_pyobject(self->node, value);
        if (pyval == NULL) {
            return -1;
        }
        cmp = PyObject_RichCompareBool(pyval, item, Py_EQ);
        Py_DECREF(pyval);
        if (cmp < 0) {
            if (PyErr_Occurred() &&
                    PyErr_ExceptionMatches(PyExc_NotImplementedError)) {
                continue;
            }
            else {
                return -1;
            }
        }
        if (cmp) {
            return 1;
        }
    }
    return 0;
}

static PyObject *
listproxy_item(ListProxyObject *self, Py_ssize_t i)
{
    CoilValue *value;
    guint n;

    CHECK_INITIALIZED(self, NULL);

    n = coil_list_length(self->list);
    if (i < 0 || i >= n) {
        PyErr_SetString(PyExc_IndexError, "list index out of range");
        return NULL;
    }
    value = coil_list_get_index(self->list, i);
    return coil_value_as_pyobject(self->node, value);
}

static PyObject *
listproxy_concat(ListProxyObject *self, PyObject *other)
{
    /* TODO */
    Py_RETURN_NONE;
}

static PyObject *
listproxy_richcompare(PyObject *x, PyObject *y, int op)
{
    ListProxyObject *self;
    PyObject *fast, *res, *vx = NULL, *vy = NULL;
    gsize i, n, m, k;
    gint cmp;

    self = (ListProxyObject *)x;
    if (!ListProxyObject_Check(self) || !PySequence_Check(y)) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }
    CHECK_INITIALIZED(self, NULL);

    fast = PySequence_Fast(y, "expecting sequence");
    if (fast == NULL) {
        return NULL;
    }

    n = coil_list_length(self->list);
    m = Py_SIZE(fast);

    if (n != m && (op == Py_EQ || op == Py_NE)) {
        Py_DECREF(fast);
        res = (op == Py_EQ) ? Py_False : Py_True;
        Py_INCREF(res);
        return res;
    }

    k = MIN(n, m);
    for (i = 0; i < k; i++) {
        CoilValue *value = coil_list_get_index(self->list, i);
        vx = coil_value_as_pyobject(self->node, value);
        if (vx == NULL) {
            return NULL;
        }
        vy = PySequence_Fast_GET_ITEM(fast, i);
        assert(vy != NULL);
        cmp = PyObject_RichCompareBool(vx, vy, Py_EQ);
        if (cmp < 0) {
            Py_DECREF(vx);
            Py_DECREF(fast);
            return NULL;
        }
        if (!cmp) {
            break;
        }
    }
    if (i >= n || i >= m) {
        switch (op) {
            case Py_LT: cmp = n < m; break;
            case Py_LE: cmp = n <= m; break;
            case Py_EQ: cmp = n == m; break;
            case Py_NE: cmp = n != m; break;
            case Py_GT: cmp = n > m; break;
            case Py_GE: cmp = n >= m; break;
            default: {
                assert(0);
                Py_XDECREF(vx);
                Py_DECREF(fast);
                return NULL;
            }
        }
        Py_XDECREF(vx);
        Py_DECREF(fast);
        res = (cmp) ? Py_True : Py_False;
        Py_INCREF(res);
        return res;
    }

    if (op == Py_EQ || op == Py_NE) {
        Py_XDECREF(vx);
        Py_DECREF(fast);
        res = (op == Py_EQ) ? Py_True : Py_False;
        Py_INCREF(res);
        return res;
    }
    Py_XDECREF(vx);
    Py_DECREF(fast);
    res = PyObject_RichCompare(vx, vy, op);
    return res;
}


static PyObject *
listproxy_repeat(ListProxyObject *self, Py_ssize_t n)
{
    /* TODO */
    Py_RETURN_NONE;
}

#if 0
static int
listproxy_ass_slice(ListProxyObject *self,
               Py_ssize_t low, Py_ssize_t high,
               PyObject *v)
{
    /* TODO */
    return 0;
}
#endif

static PyObject *
listproxy_inplace_repeat(ListProxyObject *self, Py_ssize_t n)
{
    /* TODO */
    Py_RETURN_NONE;
}

static PyObject *
listproxy_inplace_concat(ListProxyObject *self, PyObject *other)
{
    /* TODO */
    Py_RETURN_NONE;
}

static int
listproxy_ass_item(ListProxyObject *self, Py_ssize_t i, PyObject *v)
{
    /* TODO */
    return 0;
}

PyObject *
listproxy_reconstructor(ListProxyObject *self, PyObject *args)
{
    PyObject *list;

    if (!PyArg_ParseTuple(args, "O:ccoil._listproxy_reconstructor", &list))
        return NULL;

    if (!PyList_CheckExact(list)) {
        PyErr_Format(PyExc_TypeError, "expecting list got type '%s'.",
                Py_TYPE_NAME(list));
        return NULL;
    }

    return list;
}

static PyObject *
listproxy_reduce(ListProxyObject *self, PyObject *unused)
{
    PyObject *res, *list;
    static PyObject *reconstructor = NULL;

    list = PySequence_List((PyObject *)self);
    if (list == NULL)
        return NULL;

    if (reconstructor == NULL) {
        PyObject *module = PyImport_ImportModule("ccoil");
        if (module == NULL)
            return NULL;
        reconstructor = PyObject_GetAttrString(module, "_listproxy_reconstructor");
        Py_DECREF(module);
        if (reconstructor == NULL)
            return NULL;
    }

    res = Py_BuildValue("O(O)", reconstructor, list);
    Py_DECREF(list);
    return res;
}

PySequenceMethods listproxy_as_sequence =
{
	(lenfunc)listproxy_len,                /* sq_length */
	(binaryfunc)listproxy_concat,   	      /* sq_concat */
	(ssizeargfunc)listproxy_repeat,        /* sq_repeat */
	(ssizeargfunc)listproxy_item,          /* sq_item */
    0,                                /* sq_slice */
	(ssizeobjargproc)listproxy_ass_item,	  /* sq_ass_item */
    0,                                /* sq_ass_slice */
	(objobjproc)listproxy_contains,        /* sq_contains */
	(binaryfunc)listproxy_inplace_concat,  /* sq_inplace_concat */
	(ssizeargfunc)listproxy_inplace_repeat,/* sq_inplace_repeat */
};

static PyMethodDef listproxy_methods[] =
{
    {"__reduce__", (PyCFunction)listproxy_reduce, METH_NOARGS, NULL},
    {"append", (PyCFunction)listproxy_append, METH_O, NULL},
    {"count", (PyCFunction)listproxy_count, METH_O, NULL},
    {"copy", (PyCFunction)listproxy_copy, METH_NOARGS, NULL},
    {"clear", (PyCFunction)listproxy_clear, METH_NOARGS, NULL},
    {"extend", (PyCFunction)listproxy_extend, METH_O, NULL},
    {"index", (PyCFunction)listproxy_index, METH_VARARGS, NULL},
    {"insert", (PyCFunction)listproxy_insert, METH_VARARGS, NULL},
    {"pop", (PyCFunction)listproxy_pop, METH_VARARGS, NULL},
    {"remove", (PyCFunction)listproxy_remove, METH_O, NULL},
    {"sort", (PyCFunction)listproxy_sort, METH_VARARGS | METH_KEYWORDS, NULL},
    {NULL, NULL}
};

PyTypeObject ListProxy_Type =
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
  (hashfunc)PyObject_HashNotImplemented,        /* tp_hash */
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
  (richcmpfunc)listproxy_richcompare,                /* tp_richcompare */
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
};
