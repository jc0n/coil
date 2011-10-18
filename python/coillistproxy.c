/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coilmodule.h"

struct _ListProxyObject {
    PyObject_HEAD

    CoilStruct *node;
    GValueArray *arr;
};

PyDoc_STRVAR(listproxy_doc,
    "TODO(jcon): Document this");


PyObject *
ccoil_listproxy_new(CoilStruct *node, GValueArray *arr)
{
    g_return_val_if_fail(node != NULL, NULL);
    g_return_val_if_fail(arr != NULL, NULL);

    ListProxyObject *self;

    self = PyObject_New(ListProxyObject, &ListProxy_Type);
    if (self == NULL)
        return NULL;

    self->node = g_object_ref(G_OBJECT(node));
    self->arr = arr;

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
    self->arr = NULL;
    g_object_unref(self->node);
}

static const char _listproxy_notinitialized[] = "coil list is not initialized";

#define CHECK_INITIALIZED(self)                                              \
    if (self->arr == NULL || !G_IS_OBJECT(self->node)) {                     \
        PyErr_SetString(PyExc_RuntimeError, _listproxy_notinitialized);      \
        return NULL;                                                         \
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
    GValue *value;

    CHECK_INITIALIZED(self);

    if (!PyArg_ParseTuple(args, "nO:insert", &i, &v))
        return NULL;

    CHECK_VALUE(v);

    /* convert to coil type */
    value = coil_value_from_pyobject(v);
    if (value == NULL)
        return NULL;

    n = self->arr->n_values;
    if (i < 0)
        i = MAX(n - i, 0);
    else if (i > n)
        i = n;

    g_value_array_insert(self->arr, i, value);

    Py_RETURN_NONE;
}

static PyObject *
listproxy_append(ListProxyObject *self, PyObject *v)
{
    GValue *value;

    CHECK_INITIALIZED(self);
    CHECK_VALUE(v);

    value = coil_value_from_pyobject(v);
    if (value == NULL)
        return NULL;

    g_value_array_append(self->arr, value);
    Py_RETURN_NONE;
}

static PyObject *
listproxy_copy(ListProxyObject *self, PyObject *unused)
{
    PyObject *res;
    guint i, n;

    CHECK_INITIALIZED(self);

    n = self->arr->n_values;
    res = PyList_New(n);
    if (res == NULL)
        return NULL;

    for (i = 0; i < n; i++) {
        PyObject *v;
        GValue *arrv;

        arrv = g_value_array_get_nth(self->arr, i);
        v = coil_value_as_pyobject(self->node, arrv);
        if (v == NULL) {
            Py_DECREF(res);
            return NULL;
        }
        PyList_SET_ITEM(res, i, v);
    }
    return res;
}

static PyObject *
listproxy_clear(ListProxyObject *self, PyObject *unused)
{
    GValue *v;
    GValueArray *arr;
    guint i, n;

    CHECK_INITIALIZED(self);

    /* XXX: Glib lacks g_value_array_clear  or the like
     * Using g_value_array_remove in a loop appears to be O(n*n)
     */
    arr = self->arr;
    n = arr->n_values;
    for (i = 0; i < n; i++) {
        v = g_value_array_get_nth(arr, i);
        g_value_unset(v);
    }
    memset(arr->values, 0, arr->n_prealloced * sizeof(arr->values[0]));
    arr->n_values = 0;
    Py_RETURN_NONE;
}

static PyObject *
listproxy_count(ListProxyObject *self, PyObject *v)
{
    GValue *arrv;
    PyObject *w;
    Py_ssize_t count = 0;
    gsize i, n;

    CHECK_INITIALIZED(self);

    n = self->arr->n_values;
    for (i = 0; i < n; i++) {
        int cmp;
        arrv = g_value_array_get_nth(self->arr, i);
        w = coil_value_as_pyobject(self->node, arrv);
        if (w == NULL)
            return NULL;
        cmp = PyObject_RichCompareBool(w, v, Py_EQ);
        if (cmp > 0)
            count++;
        else if (cmp < 0)
            return NULL;
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
convert_to_python_list(CoilStruct *node, GValueArray *arr)
{
    PyObject *res;
    gsize i, n;

    n = arr->n_values;
    res = PyList_New(n);
    if (res == NULL)
        return NULL;

    for (i = 0; i < n; i++) {
        PyObject *v;
        GValue *arrv;

        arrv = g_value_array_get_nth(arr, i);
        if (G_VALUE_HOLDS(arrv, COIL_TYPE_LIST)) {
            GValueArray *subarr = (GValueArray *)g_value_get_boxed(arrv);
            v = convert_to_python_list(node, subarr);
        }
        else {
            v = coil_value_as_pyobject(node, arrv);
        }
        if (v == NULL) {
            Py_DECREF(res);
            return NULL;
        }
        PyList_SET_ITEM(res, i, v);
    }
    return res;
}

static PyObject *
listproxy_pop(ListProxyObject *self, PyObject *args)
{
    PyObject *res;
    Py_ssize_t i = -1;
    GValue *value;
    gsize n;

    CHECK_INITIALIZED(self);

    if (!PyArg_ParseTuple(args, "|n:pop", &i))
        return NULL;

    n = self->arr->n_values;
    if (i < 0)
        i = n - 1;
    else if (i > n) {
        PyErr_SetString(PyExc_IndexError, "index out of range");
        return NULL;
    }

    value = g_value_array_get_nth(self->arr, i);
    if (G_VALUE_HOLDS(value, COIL_TYPE_LIST)) {
        GValueArray *arr = (GValueArray *)g_value_get_boxed(value);
        res = convert_to_python_list(self->node, arr);
    }
    else {
        res = coil_value_as_pyobject(self->node, value);
    }
    if (res == NULL)
        return NULL;

    g_value_array_remove(self->arr, i);
    return res;
}

static PyObject *
listproxy_remove(ListProxyObject *self, PyObject *w)
{
    /* TODO */
    Py_RETURN_NONE;
}

static PyObject *
list_reverse(ListProxyObject *self, PyObject *args)
{
    /* TODO */
    Py_RETURN_NONE;
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
    self->arr = NULL;
    self->node = NULL;
    return 0;
}

static Py_ssize_t
listproxy_len(ListProxyObject *self)
{
    if (self->arr == NULL)
        return 0;

    return self->arr->n_values;
}

static int
listproxy_contains(ListProxyObject *self, PyObject *item)
{
    PyObject *v;
    GValue *arrv;
    gsize i, n;

    if (self->arr == NULL) {
        PyErr_SetString(PyExc_RuntimeError, _listproxy_notinitialized);
        return -1;
    }

    n = self->arr->n_values;
    for (i = 0; i < n; i++) {
        int cmp;

        arrv = g_value_array_get_nth(self->arr, i);
        v = coil_value_as_pyobject(self->node, arrv);
        if (v == NULL)
            return -1;
        cmp = PyObject_RichCompareBool(v, item, Py_EQ);
        Py_DECREF(v);
        if (cmp < 0) {
            if (PyErr_Occurred() &&
                    PyErr_ExceptionMatches(PyExc_NotImplementedError))
                continue;
            else
                return -1;
        }
        if (cmp)
            return 1;
    }
    return 0;
}

static PyObject *
listproxy_item(ListProxyObject *self, Py_ssize_t i)
{
    GValue *value;

    CHECK_INITIALIZED(self);

    if (i < 0 || i >= self->arr->n_values) {
        PyErr_SetString(PyExc_IndexError, "list index out of range");
        return NULL;
    }

    value = g_value_array_get_nth(self->arr, i);
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
    PyObject *fast, *res, *vx = NULL, *vy;
    guint i, n, m;
    gint cmp;

    self = (ListProxyObject *)x;
    CHECK_INITIALIZED(self);

    if (!ListProxyObject_Check(x) || !PySequence_Check(y)) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }

    fast = PySequence_Fast(y, "expecting sequence");
    if (fast == NULL)
        return NULL;

    n = self->arr->n_values;
    m = Py_SIZE(fast);

    if (n != m && (op == Py_EQ || op == Py_NE)) {
        res = (op == Py_EQ) ? Py_False : Py_True;
        Py_INCREF(res);
        Py_DECREF(fast);
        return res;
    }

    for (i = 0; i < n && i < m; i++) {
        GValue *arrv;
        int k;

        arrv = g_value_array_get_nth(self->arr, i);
        vy = PySequence_Fast_GET_ITEM(fast, i);
        vx = coil_value_as_pyobject(self->node, arrv);
        if (vx == NULL)
            return NULL;

        k = PyObject_RichCompareBool(vx, vy, Py_EQ);
        if (k < 0) {
            Py_DECREF(vx);
            Py_DECREF(fast);
            return NULL;
        }
        if (!k)
            break;
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
        res = (cmp) ? Py_True : Py_False;
        Py_INCREF(res);
        Py_XDECREF(vx);
        Py_DECREF(fast);
        return res;
    }

    if (op == Py_EQ || op == Py_NE) {
        Py_XDECREF(vx);
        Py_DECREF(fast);
        res = (op == Py_EQ) ? Py_True : Py_False;
        Py_INCREF(res);
        return res;
    }
    res = PyObject_RichCompare(vx, vy, op);
    Py_XDECREF(vx);
    Py_DECREF(fast);
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
