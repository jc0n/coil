/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coilmodule.h"
#include "coilstruct.h"

static GQuark struct_wrapper_key = 0;

struct _PyCoilStruct {
    PyObject_HEAD CoilStruct * node;
    structiter_object *iter;
};

struct _structiter_object {
    PyObject_HEAD PyCoilStruct * si_struct;
    CoilStructIter si_iter;
};

CoilStruct *
ccoil_struct_get_real(PyObject *self)
{
    return ((PyCoilStruct *)self)->node;
}

static PyObject *
structiter_new(PyCoilStruct * iterstruct, PyTypeObject * itertype)
{
    structiter_object *si;

    si = PyObject_GC_New(structiter_object, itertype);

    Py_INCREF(iterstruct);
    si->si_struct = iterstruct;

    coil_struct_iter_init(&si->si_iter, iterstruct->node);

    PyObject_GC_Track((PyObject *) si);
    return (PyObject *) si;
}

static void
structiter_dealloc(structiter_object * si)
{
    PyObject_GC_UnTrack((PyObject *) si);

    Py_XDECREF(si->si_struct);
    PyObject_GC_Del((PyObject *) si);
}

static int
structiter_traverse(structiter_object * si, visitproc visit, void *arg)
{
    Py_VISIT(si->si_struct);
    return 0;
}

static PyObject *
struct_iter(PyCoilStruct * self)
{
    return structiter_new(self, &PyCoilStructIterKey_Type);
}

static PyObject *
struct_iteritems(PyCoilStruct * self)
{
    return structiter_new(self, &PyCoilStructIterItem_Type);
}

static PyObject *
struct_iterkeys(PyCoilStruct * self)
{
    return structiter_new(self, &PyCoilStructIterKey_Type);
}

static PyObject *
struct_iterpaths(PyCoilStruct * self)
{
    return structiter_new(self, &PyCoilStructIterPath_Type);
}

static PyObject *
struct_itervalues(PyCoilStruct * self)
{
    return structiter_new(self, &PyCoilStructIterValue_Type);
}

static PyObject *
structiter_iternextitem(structiter_object * si)
{
    g_return_val_if_fail(si, NULL);

    const CoilPath *path;
    GValue *value;

    if (si->si_struct == NULL)
        return NULL;

    assert(PyCoilStruct_Check(si->si_struct));

    if (coil_struct_iter_next(&si->si_iter, &path, (const GValue **)&value)) {
        PyObject *k = NULL, *v = NULL, *item;

        k = PyString_FromStringAndSize(path->key, path->key_len);
        if (k == NULL)
            return NULL;

        v = coil_value_as_pyobject(si->si_struct->node, value);
        if (v == NULL) {
            Py_DECREF(k);
            return NULL;
        }

        item = PyTuple_New(2);
        PyTuple_SET_ITEM(item, 0, k);
        PyTuple_SET_ITEM(item, 1, v);
        return item;
    }

    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

static PyObject *
structiter_iternextkey(structiter_object * si)
{
    g_return_val_if_fail(si, NULL);

    const CoilPath *path;

    if (si->si_struct == NULL)
        return NULL;

    assert(PyCoilStruct_Check(si->si_struct));

    if (coil_struct_iter_next(&si->si_iter, &path, NULL)) {
        PyObject *k;

        k = PyString_FromStringAndSize(path->key, path->key_len);
        return k;
    }

    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

static PyObject *
structiter_iternextpath(structiter_object * si)
{
    g_return_val_if_fail(si, NULL);

    const CoilPath *path;

    if (si->si_struct == NULL)
        return NULL;

    assert(PyCoilStruct_Check(si->si_struct));

    if (coil_struct_iter_next(&si->si_iter, &path, NULL)) {
        PyObject *p;

        p = PyString_FromStringAndSize(path->path, path->path_len);
        return p;
    }

    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

static PyObject *
structiter_iternextvalue(structiter_object * si)
{
    g_return_val_if_fail(si, NULL);

    GValue *value;

    if (si->si_struct == NULL)
        return NULL;

    assert(PyCoilStruct_Check(si->si_struct));

    if (coil_struct_iter_next(&si->si_iter, NULL, (const GValue **)&value)) {
        PyObject *v;

        v = coil_value_as_pyobject(si->si_struct->node, value);
        return v;
    }

    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

PyTypeObject PyCoilStructIterItem_Type = {
    PyObject_HEAD_INIT(NULL)
        0,
    "ccoil.StructItemIterator",
    sizeof(structiter_object),
    0,
    /* methods */
    (destructor) structiter_dealloc,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    PyObject_GenericGetAttr,
    0,
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    0,
    (traverseproc) structiter_traverse,
    0,
    0,
    0,
    PyObject_SelfIter,
    (iternextfunc) structiter_iternextitem,
    0,
    0,
};

PyTypeObject PyCoilStructIterKey_Type = {
    PyObject_HEAD_INIT(NULL)
        0,
    "ccoil.StructKeyIterator",
    sizeof(structiter_object),
    0,
    /* methods */
    (destructor) structiter_dealloc,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    PyObject_GenericGetAttr,
    0,
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    0,
    (traverseproc) structiter_traverse,
    0,
    0,
    0,
    PyObject_SelfIter,
    (iternextfunc) structiter_iternextkey,
    0,
    0,
};

PyTypeObject PyCoilStructIterPath_Type = {
    PyObject_HEAD_INIT(NULL)
        0,
    "ccoil.StructPathIterator",
    sizeof(structiter_object),
    0,
    /* methods */
    (destructor) structiter_dealloc,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    PyObject_GenericGetAttr,
    0,
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    0,
    (traverseproc) structiter_traverse,
    0,
    0,
    0,
    PyObject_SelfIter,
    (iternextfunc) structiter_iternextpath,
    0,
    0,
};

PyTypeObject PyCoilStructIterValue_Type = {
    PyObject_HEAD_INIT(NULL)
        0,
    "ccoil.StructValueIterator",
    sizeof(structiter_object),
    0,
    /* methods */
    (destructor) structiter_dealloc,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    PyObject_GenericGetAttr,
    0,
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    0,
    (traverseproc) structiter_traverse,
    0,
    0,
    0,
    PyObject_SelfIter,
    (iternextfunc) structiter_iternextvalue,
    0,
    0,
};

static void
struct_free(PyCoilStruct * self)
{
    PyObject_GC_Del(self);
}

int
struct_register_types(PyObject * m, PyObject * d)
{
    PyObject *gobject_module, *gobject_dict;
    PyTypeObject *gobject_type;

    struct_wrapper_key = g_quark_from_static_string("_PyCoilStruct");

    gobject_module = pygobject_init(-1, -1, -1);
    if (gobject_module == NULL)
        return 0;

    gobject_dict = PyModule_GetDict(gobject_module);
    if (gobject_dict == NULL)
        return 0;

    gobject_type =
        (PyTypeObject *) PyDict_GetItemString(gobject_dict, "GObject");
    if (gobject_type == NULL)
        return 0;

    Py_INCREF(gobject_type);
    Py_DECREF(gobject_module);

    PyCoilStruct_Type.tp_free = (freefunc) struct_free;
    PyCoilStruct_Type.tp_base = gobject_type;

    PyType_Register(d, PyCoilStruct_Type, "Struct", 0);

    return 1;
}

/* steals reference to node */
PyObject *
ccoil_struct_new(CoilStruct * node)
{
    PyCoilStruct *self;

    if (node == NULL)
        Py_RETURN_NONE;

    self =
        (PyCoilStruct *) g_object_get_qdata(G_OBJECT(node),
                                            struct_wrapper_key);
    if (self == NULL) {
        self = PyObject_GC_New(PyCoilStruct, &PyCoilStruct_Type);
        if (self == NULL)
            return NULL;

        self->node = node;
        self->iter = NULL;

        g_object_set_qdata_full(G_OBJECT(node), struct_wrapper_key, self,
                                NULL);
        PyObject_GC_Track((PyObject *) self);
    }
    else {
        Py_INCREF(self);
        g_assert(self->node);
        g_object_unref(node);
    }

    return (PyObject *) self;
}

static int
struct_clear(PyCoilStruct * self)
{
    if (self->node) {
        g_object_set_qdata_full(G_OBJECT(self->node),
                                struct_wrapper_key, NULL, NULL);

        g_object_unref(self->node);
        self->node = NULL;
    }

    Py_CLEAR(self->iter);

    return 0;
}

static int
struct_traverse(PyCoilStruct * self, visitproc visit, void *arg)
{
    Py_VISIT(self->iter);
    return 0;
}

static void
struct_dealloc(PyCoilStruct * self)
{
    PyObject_GC_UnTrack((PyObject *) self);

    struct_clear(self);

    PyObject_GC_Del((PyObject *) self);
}

static long
struct_hash(PyCoilStruct * self)
{
    return (long)self->node;
}

gboolean
struct_update_from_pyitems(CoilStruct * node, PyObject * items)
{
    g_return_val_if_fail(COIL_IS_STRUCT(node), FALSE);
    g_return_val_if_fail(items, FALSE);

    Py_ssize_t i;
    PyObject *it, *item = NULL, *fast = NULL;
    const CoilPath *node_path;
    CoilPath *path = NULL;
    GValue *value = NULL;
    GError *error = NULL;

    node_path = coil_struct_get_path(node);
    g_assert(node_path);

    if (PyMapping_Check(items))
        items = PyMapping_Items(items);
    else
        Py_INCREF(items);

    it = PyObject_GetIter(items);
    if (it == NULL)
        return FALSE;

    for (i = 0;; i++) {
        PyObject *k, *v;
        Py_ssize_t n;

        fast = NULL;
        item = PyIter_Next(it);
        if (item == NULL) {
            if (PyErr_Occurred())
                goto error;

            break;
        }

        fast = PySequence_Fast(item, "");
        if (fast == NULL) {
            if (PyErr_ExceptionMatches(PyExc_TypeError))
                PyErr_Format(PyExc_TypeError,
                             "cannot convert struct update sequence element "
                             "#%zd to a sequence", i);
            goto error;
        }

        n = PySequence_Fast_GET_SIZE(fast);
        if (n != 2) {
            PyErr_Format(PyExc_ValueError,
                         "struct update sequence element %zd "
                         "expecting key-value pair length 2, has length %zd",
                         i, n);
            goto error;
        }

        k = PySequence_Fast_GET_ITEM(fast, 0);
        if (!PyString_Check(k)) {
            PyErr_Format(PyExc_ValueError,
                         "struct update expecting string in index 0 of element #%zd, found %s",
                         i, Py_TYPE_NAME(k));
            goto error;
        }

        path = coil_path_from_pyobject(k, &error);
        if (path == NULL)
            goto error;

        if (!coil_path_resolve_into(&path, node_path, &error))
            goto error;

        value = (GValue *) coil_struct_lookup_path(node, path, FALSE, &error);
        if (value)
            goto next;

        v = PySequence_Fast_GET_ITEM(fast, 1);

        if (PyDict_Check(v)) {
            CoilStruct *s;

            s = coil_struct_new(&error, "container", node, "path", path, NULL);
            if (s == NULL)
                goto error;

            Py_INCREF(v);

            if (!struct_update_from_pyitems(s, v)) {
                g_object_unref(s);
                Py_DECREF(v);
                goto error;
            }

            Py_DECREF(v);
            g_object_unref(s);
        }
        else {
            value = coil_value_from_pyobject(v);
            if (value == NULL)
                goto error;

            if (!coil_struct_insert_path(node, path, value, FALSE, &error)) {
                path = NULL;
                value = NULL;
                goto error;
            }
        }

 next:
        Py_DECREF(fast);
        Py_DECREF(item);
    }

    Py_DECREF(it);
    Py_DECREF(items);
    return TRUE;

 error:
    Py_XDECREF(item);
    Py_XDECREF(fast);
    Py_DECREF(items);

    if (path)
        coil_path_unref(path);

    if (value)
        coil_value_free(value);

    if (error)
        ccoil_error(&error);

    return FALSE;
}

static PyObject *
update_pydict_from_struct(CoilStruct * node, PyObject * d, gboolean absolute)
{
    g_return_val_if_fail(COIL_IS_STRUCT(node), NULL);
    g_return_val_if_fail(d, NULL);

    CoilStructIter it;
    const CoilPath *path;
    GValue *value;
    PyObject *k = NULL, *v = NULL;
    ptrdiff_t key_offset, klen_offset;
    GError *error = NULL;

    if (!coil_struct_expand(node, &error))
        goto error;

    if (absolute) {
        key_offset = G_STRUCT_OFFSET(CoilPath, path);
        klen_offset = G_STRUCT_OFFSET(CoilPath, path_len);
    }
    else {
        key_offset = G_STRUCT_OFFSET(CoilPath, key);
        klen_offset = G_STRUCT_OFFSET(CoilPath, key_len);
    }

    coil_struct_iter_init(&it, node);
    while (coil_struct_iter_next(&it, &path, (const GValue **)&value)) {
        const gchar *str = G_STRUCT_MEMBER(gchar *, path, key_offset);
        guint8 len = G_STRUCT_MEMBER(guint8, path, klen_offset);

        v = NULL;
        k = PyString_FromStringAndSize(str, len);
        if (k == NULL)
            goto error;

        if (G_VALUE_HOLDS(value, COIL_TYPE_STRUCT)) {
            node = COIL_STRUCT(g_value_get_object(value));

            v = PyDict_New();
            if (v == NULL)
                goto error;

            v = update_pydict_from_struct(node, v, absolute);
            if (v == NULL)
                goto error;
        }
        else {
            v = coil_value_as_pyobject(node, value);
            if (v == NULL)
                goto error;
        }

        if (PyDict_SetItem(d, k, v) < 0)
            goto error;

        Py_DECREF(k);
        Py_DECREF(v);
    }

    return d;

 error:
    Py_XDECREF(d);
    Py_XDECREF(k);
    Py_XDECREF(v);

    if (error)
        ccoil_error(&error);

    return NULL;
}

static CoilStruct *
struct_from_pyitems(PyObject * items)
{
    CoilStruct *node;

    node = coil_struct_new(NULL, NULL);
    if (!struct_update_from_pyitems(node, items))
        return NULL;

    return node;
}

static int
struct_sq_contains(PyCoilStruct * self, PyObject * arg)
{
    CoilPath *path;
    const GValue *value;
    GError *error = NULL;

    if (!PyString_Check(arg)) {
        PyErr_Format(PyExc_ValueError,
                     "keys must be strings, not %s", Py_TYPE_NAME(arg));

        return -1;
    }

    path = coil_path_from_pyobject(arg, &error);
    if (path == NULL) {
        ccoil_error(&error);
        return -1;
    }

    value = coil_struct_lookup_path(self->node, path, FALSE, &error);
    coil_path_unref(path);

    if (error)
        ccoil_error(&error);

    return (value == NULL) ? 0 : 1;
}

static PyObject *
struct_contains(PyCoilStruct * self, PyObject * arg)
{
    if (struct_sq_contains(self, arg))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *
struct_copy(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = { "container", "name", NULL };
    PyCoilStruct *pycontainer = NULL;
    PyObject *pyname = NULL;
    CoilStruct *copy, *container = NULL;
    CoilPath *path = NULL;
    GError *error = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O!S:ccoil.Struct.copy",
                                     kwlist, &PyCoilStruct_Type, &pycontainer,
                                     &pyname))
        return NULL;

    if (((pycontainer == NULL) ^ (pyname == NULL))) {
        PyErr_SetString(PyExc_ValueError,
                        "name and container must be specified together");

        return NULL;
    }

    if (pyname) {
        container = pycontainer->node;
        path = coil_path_from_pyobject(pyname, &error);
        if (path == NULL)
            goto error;

        copy = coil_struct_copy(self->node, &error,
                                "container", container, "path", path, NULL);
        coil_path_unref(path);
    }
    else
        copy = coil_struct_copy(self->node, &error, NULL, NULL);

    if (copy == NULL)
        goto error;

    return ccoil_struct_new(copy);

 error:
    if (error)
        ccoil_error(&error);

    return NULL;
}

static PyObject *
struct_get_container(PyCoilStruct * self, PyObject * ignored)
{
    CoilStruct *container;

    assert(self->node);

    container = coil_struct_get_container(self->node);
    if (container == NULL)
        Py_RETURN_NONE;

    g_object_ref(container);
    return ccoil_struct_new(container);
}

static PyObject *
struct_root(PyCoilStruct * self, PyObject * ignored)
{
    CoilStruct *root;

    assert(self->node);

    root = coil_struct_get_root(self->node);
    if (root == NULL)
        Py_RETURN_NONE;

    g_object_ref(root);
    return ccoil_struct_new(root);
}

static PyObject *
struct_is_ancestor(PyCoilStruct * self, PyObject * other)
{
    if (!PyCoilStruct_Check(other)) {
        PyErr_Format(PyExc_ValueError,
                     "expecting type %s, found type %s",
                     Py_TYPE_NAME(self), Py_TYPE_NAME(other));

        return NULL;
    }

    if (coil_struct_is_ancestor(self->node, ((PyCoilStruct *) other)->node))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *
struct_is_descendent(PyCoilStruct * self, PyObject * other)
{
    if (!PyCoilStruct_Check(other)) {
        PyErr_Format(PyExc_ValueError,
                     "expecting type %s, found type %s",
                     Py_TYPE_NAME(other), Py_TYPE_NAME(self));

        return NULL;
    }

    if (coil_struct_is_descendent(self->node, ((PyCoilStruct *) other)->node))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *
struct_is_root(PyCoilStruct * self, PyObject * unused)
{
    if (coil_struct_is_root(self->node))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *
struct_empty(PyCoilStruct * self, PyObject * unused)
{
    GError *error = NULL;

    coil_struct_empty(self->node, &error);
    if (G_UNLIKELY(error)) {
        ccoil_error(&error);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
struct_to_dict(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = { "absolute", NULL };
    PyObject *d, *absolute = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "|O:ccoil.Struct.dict",
                                     kwlist, &absolute))
        return NULL;

    d = PyDict_New();
    if (d == NULL)
        return NULL;

    update_pydict_from_struct(self->node, d,
                              absolute && PyObject_IsTrue(absolute));

    return d;
}

static PyObject *
struct_expand(PyCoilStruct *self, PyObject *args, PyObject *kwargs)
{
    PyObject *defaults = NULL;
    PyObject *force = Py_True;
    PyObject *ignore = Py_False;
    PyObject *recursive = Py_True;
    GError *error = NULL;
    static char *kwlist[] = {"defaults", "ignore_missing",
        "recursive", "force", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OOOO:ccoil.Struct.expand",
                kwlist, &defaults, &ignore, &recursive, &force))
        return NULL;

    if (defaults != NULL) {
        if (!PyMapping_Check(defaults) &&
            !PySequence_Check(defaults) &&
            !PyCoilStruct_Check(defaults)) {
            PyErr_SetString(PyExc_ValueError,
                            "defaults argument must be a dict-like object, "
                            "a sequence of items, or a coil struct.");
            return NULL;
        }
        if (!struct_update_from_pyitems(self->node, defaults))
            return NULL;
    }
    if (PyObject_IsTrue(force)) {
        if (!coil_struct_expand_items(self->node,
                    PyObject_IsTrue(recursive), &error)) {
            ccoil_error(&error);
            return NULL;
        }
    }
    Py_RETURN_NONE;
}

#if 0
static PyObject *
struct_expanditem(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{

    // XXX: FIX BROKEN
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
struct_expandvalue(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    // XXX: FIX BROKEN
    Py_INCREF(Py_None);
    return Py_None;
}
#endif

static PyObject *
struct_extend(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = { "parent", NULL };
    PyCoilStruct *parent;
    GError *error = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:ccoil.Struct.extend",
                                     kwlist, &PyCoilStruct_Type, &parent))
        return NULL;

    assert(self->node);
    assert(parent->node);

    if (!coil_struct_extend(self->node, parent->node, &error)) {
        ccoil_error(&error);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
struct_extend_path(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = { "path", "context", NULL };
    PyObject *py_path;
    PyCoilStruct *py_context = NULL;
    CoilStruct *context = NULL;
    CoilPath *path;
    GError *error = NULL;

    if (!PyArg_ParseTupleAndKeywords
        (args, kwargs, "S|O!:ccoil.Struct.extend_path", kwlist, &py_path,
         &PyCoilStruct_Type, &context))
        return NULL;

    path = coil_path_from_pyobject(py_path, &error);
    if (path == NULL)
        return NULL;

    if (py_context)
        context = py_context->node;

    if (!coil_struct_extend_path(self->node, path, context, &error)) {
        ccoil_error(&error);
        return NULL;
    }

    coil_path_unref(path);

    Py_RETURN_NONE;
}

static PyObject *
struct_get(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = { "path", "default", NULL };
    PyObject *py_key, *py_default = NULL;
    CoilPath *path = NULL;
    GValue *value;
    GError *error = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O:ccoil.Struct.get",
                                     kwlist, &py_key, &py_default))
        return NULL;

    path = coil_path_from_pyobject(py_key, &error);
    if (path == NULL)
        goto error;

    value = (GValue *)coil_struct_lookup_path(self->node, path, TRUE, &error);
    if (G_UNLIKELY(error))
        goto error;

    if (value) {
        coil_path_unref(path);
        return coil_value_as_pyobject(self->node, value);
    }

    if (py_default) {
        coil_path_unref(path);
        Py_INCREF(py_default);
        return py_default;
    }

    PyErr_Format(KeyMissingError, "<%s> The path '%s' was not found.",
                 coil_struct_get_path(self->node)->path, path->path);

 error:
    if (path)
        coil_path_unref(path);

    if (error)
        ccoil_error(&error);

    return NULL;
}

static PyObject *
struct_richcompare(PyObject * self, PyObject * other, int op)
{
    PyObject *r;
    CoilStruct *a = NULL, *b = NULL;
    gboolean result;
    GError *error = NULL;

    if (!(op == Py_EQ || op == Py_NE)) {
        r = Py_NotImplemented;
        goto end;
    }

    if (PyDict_Check(self))
        a = struct_from_pyitems(self);
    else if (!PyCoilStruct_Check(self)) {
        r = Py_NotImplemented;
        goto end;
    }
    else {
        a = ((PyCoilStruct *) self)->node;
        g_object_ref(a);
    }

    if (PyDict_Check(other))
        b = struct_from_pyitems(other);
    else if (!PyCoilStruct_Check(other)) {
        r = Py_NotImplemented;
        goto end;
    }
    else {
        b = ((PyCoilStruct *) other)->node;
        g_object_ref(b);
    }

    if (a == b) {
        r = (op == Py_EQ) ? Py_True : Py_False;
        goto end;
    }

    result = coil_struct_equals(a, b, &error);
    if (error) {
        ccoil_error(&error);
        r = NULL;
        goto end;
    }

//  r = !(result ^ (op == Py_EQ)) ? Py_True : Py_False;
    r = (result == (op == Py_EQ)) ? Py_True : Py_False;

 end:
    if (a)
        g_object_unref(a);

    if (b)
        g_object_unref(b);

    Py_XINCREF(r);
    return r;
}

static PyObject *
struct_items(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = { "absolute", NULL };
    PyObject *it, *items, *absolute = NULL;

    assert(self->node);

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O:ccoil.Struct.items",
                                     kwlist, &absolute))
        return NULL;

    if (absolute && PyObject_IsTrue(absolute))
        it = struct_iterpaths(self);
    else
        it = struct_iterkeys(self);

    if (it == NULL)
        return NULL;

    items = PySequence_List(it);

    Py_DECREF(it);
    return items;
}

static PyObject *
struct_keys(PyCoilStruct * self, PyObject * unused)
{
    PyObject *it, *keys;

    it = struct_iterkeys(self);
    if (it == NULL)
        return NULL;

    keys = PySequence_List(it);

    Py_DECREF(it);
    return keys;
}

static PyObject *
struct_paths(PyCoilStruct * self, PyObject * unused)
{
    PyObject *it, *paths;

    it = struct_iterpaths(self);
    if (it == NULL)
        return NULL;

    paths = PySequence_List(it);

    Py_DECREF(it);
    return paths;
}

static PyObject *
struct_values(PyCoilStruct * self, PyObject * ignored)
{
    PyObject *it, *values;

    it = struct_itervalues(self);
    if (it == NULL)
        return NULL;

    values = PySequence_List(it);

    Py_DECREF(it);
    return values;
}

static PyObject *
struct_merge(PyCoilStruct * self, PyObject * args)
{
    CoilStruct *src = NULL, *dst = self->node;
    PyObject *arg;
    Py_ssize_t i, n;
    GError *error = NULL;

    n = PyTuple_GET_SIZE(args);
    for (i = 0; i < n; i++) {
        arg = PyTuple_GET_ITEM(args, i);

        if (PyCoilStruct_Check(arg)) {
            src = ((PyCoilStruct *) arg)->node;
            g_object_ref(src);
        }
        else {
            src = struct_from_pyitems(arg);
            if (src == NULL)
                goto error;
        }

        if (!coil_struct_merge(src, dst, &error))
            goto error;

        g_object_unref(src);
    }

    Py_RETURN_NONE;

 error:
    if (src)
        g_object_unref(src);

    if (error)
        ccoil_error(&error);

    return NULL;
}

static PyObject *
struct_path(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = { "path", "ref", NULL };
    PyObject *pyresult, *pypath = NULL, *pyref = NULL;
    CoilPath *path = NULL, *target = NULL, *abspath = NULL;
    GError *error = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|SS:ccoil.path",
                                     kwlist, &pypath, &pyref))
        return NULL;

    if (pypath && PyString_GET_SIZE(pypath) > 0) {
        if (pyref) {
            const CoilPath *node_path;
            node_path = coil_struct_get_path(self->node);

            path = coil_path_from_pyobject(pyref, &error);
            if (path == NULL)
                goto error;

            abspath = coil_path_resolve(path, node_path, &error);
            if (abspath == NULL)
                goto error;

            coil_path_unref(path);
            path = abspath;
        }
        else {
            path = (CoilPath *) coil_struct_get_path(self->node);
            coil_path_ref(path);
        }

        target = coil_path_from_pyobject(pypath, &error);
        if (target == NULL)
            goto error;

        abspath = coil_path_resolve(target, path, &error);
        if (abspath == NULL)
            goto error;

        pyresult =
            PyString_FromStringAndSize(abspath->path, abspath->path_len);

        coil_path_unref(path);
        coil_path_unref(target);
        coil_path_unref(abspath);
    }
    else {
        path = (CoilPath *) coil_struct_get_path(self->node);
        pyresult = PyString_FromStringAndSize(path->path, path->path_len);
    }

    return pyresult;

 error:
    if (path)
        coil_path_unref(path);

    if (target)
        coil_path_unref(target);

    if (error)
        ccoil_error(&error);

    return NULL;
}

static PyObject *
struct_set(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = { "path", "value", "location", NULL };
    PyObject *pypath;
    PyObject *pyvalue;
    PyObject *pylocation = NULL;
    CoilPath *path = NULL;
    GValue *value = NULL;
    GError *error = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "SO|O:ccoil.Struct.set",
                                     kwlist, &pypath, &pyvalue, &pylocation))
        return NULL;

    path = coil_path_from_pyobject(pypath, &error);
    if (path == NULL)
        goto error;

    value = coil_value_from_pyobject(pyvalue);
    if (value == NULL)
        goto error;

    // TODO(jcon): handle location

    if (!coil_struct_insert_path(self->node, path, value, TRUE, &error)) {
        path = NULL;
        value = NULL;
        goto error;
    }

    Py_RETURN_NONE;

 error:
    if (path)
        coil_path_unref(path);

    if (value)
        coil_value_free(value);

    if (error)
        ccoil_error(&error);

    return NULL;
}

static PyObject *
struct_string(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    PyObject *pystring, *o;
    GString *buffer;
    CoilStringFormat format = default_string_format;
    GError *error = NULL;

    if (kwargs) {
        o = PyDict_GetItemString(kwargs, "indent_level");
        if (o) {
            format.indent_level = (guint8) PyInt_AsLong(o);
            if (PyErr_Occurred())
                return NULL;
        }

        o = PyDict_GetItemString(kwargs, "block_indent");
        if (o) {
            format.block_indent = (guint8) PyInt_AsLong(o);
            if (PyErr_Occurred())
                return NULL;
        }

        o = PyDict_GetItemString(kwargs, "brace_indent");
        if (o) {
            format.brace_indent = (guint8) PyInt_AsLong(o);
            if (PyErr_Occurred())
                return NULL;
        }

        o = PyDict_GetItemString(kwargs, "multiline_length");
        if (o) {
            format.multiline_len = (guint) PyInt_AsLong(o);
            if (PyErr_Occurred())
                return NULL;
        }

        o = PyDict_GetItemString(kwargs, "blank_line_after_brace");
        if (o) {
            if (PyObject_IsTrue(o))
                format.options |= BLANK_LINE_AFTER_BRACE;
            else
                format.options &= ~BLANK_LINE_AFTER_BRACE;
        }

        o = PyDict_GetItemString(kwargs, "blank_line_after_struct");
        if (o) {
            if (PyObject_IsTrue(o))
                format.options |= BLANK_LINE_AFTER_STRUCT;
            else
                format.options &= ~BLANK_LINE_AFTER_STRUCT;
        }

        o = PyDict_GetItemString(kwargs, "blank_line_after_item");
        if (o) {
            if (PyObject_IsTrue(o))
                format.options |= BLANK_LINE_AFTER_ITEM;
            else
                format.options &= ~BLANK_LINE_AFTER_ITEM;
        }

        o = PyDict_GetItemString(kwargs, "brace_on_blank_line");
        if (o) {
            if (PyObject_IsTrue(o))
                format.options |= BRACE_ON_BLANK_LINE;
            else
                format.options &= ~BRACE_ON_BLANK_LINE;
        }

        o = PyDict_GetItemString(kwargs, "list_on_blank_line");
        if (o) {
            if (PyObject_IsTrue(o))
                format.options |= LIST_ON_BLANK_LINE;
            else
                format.options &= ~LIST_ON_BLANK_LINE;
        }

        o = PyDict_GetItemString(kwargs, "commas_in_list");
        if (o) {
            if (PyObject_IsTrue(o))
                format.options |= COMMAS_IN_LIST;
            else
                format.options &= ~COMMAS_IN_LIST;
        }

        o = PyDict_GetItemString(kwargs, "quote_strings");
        if (o) {
            if (PyObject_IsTrue(o))
                format.options &= ~DONT_QUOTE_STRINGS;
            else
                format.options |= DONT_QUOTE_STRINGS;
        }

        o = PyDict_GetItemString(kwargs, "compact");
        if (o) {
            if (PyObject_IsTrue(o))
                format.options |= COMPACT;
            else
                format.options &= ~COMPACT;
        }

        o = PyDict_GetItemString(kwargs, "legacy");
        if (o) {
            if (PyObject_IsTrue(o))
                format.options |= LEGACY;
            else
                format.options &= ~LEGACY;
        }
    }

    buffer = g_string_sized_new(8192);
    coil_struct_build_string(self->node, buffer, &format, &error);
    if (G_UNLIKELY(error)) {
        g_string_free(buffer, TRUE);
        ccoil_error(&error);
        return NULL;
    }

    pystring =
        PyString_FromStringAndSize(buffer->str, (Py_ssize_t) buffer->len);
    g_string_free(buffer, TRUE);

    return pystring;
}

static PyObject *
struct_validate_key(PyCoilStruct * self, PyObject * args)
{
    gchar *key;
    Py_ssize_t key_len;
    PyObject *pykey;

    if (!PyArg_ParseTuple(args, "S:ccoil.Struct.validate_key", &pykey))
        return NULL;

    if (PyString_AsStringAndSize(pykey, &key, &key_len) < 0)
        return NULL;

    if (coil_validate_key_len(key, key_len))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *
struct_validate_path(PyCoilStruct * self, PyObject * args)
{
    gchar *path;
    Py_ssize_t path_len;
    PyObject *pypath;

    if (!PyArg_ParseTuple(args, "S:ccoil.Struct.validate_path", &pypath))
        return NULL;

    if (PyString_AsStringAndSize(pypath, &path, &path_len) < 0)
        return NULL;

    if (coil_validate_path_len(path, path_len))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *
struct_str(PyCoilStruct * self)
{
    PyObject *pystring;
    GError *error = NULL;
    GString *buffer;

    buffer = g_string_sized_new(8192);
    coil_struct_build_string(self->node, buffer, &default_string_format,
                             &error);
    if (G_UNLIKELY(error)) {
        g_string_free(buffer, TRUE);
        ccoil_error(&error);
        return NULL;
    }

    pystring = PyString_FromStringAndSize(buffer->str, buffer->len);
    if (pystring == NULL) {
        g_string_free(buffer, TRUE);
        return NULL;
    }

    g_string_free(buffer, TRUE);

    return pystring;
}

static PyObject *
struct_mp_get(PyCoilStruct * self, PyObject * pypath)
{
    CoilPath *path = NULL;
    GValue *value = NULL;
    GError *error = NULL;

    path = coil_path_from_pyobject(pypath, &error);
    if (path == NULL)
        goto error;

    value = (GValue *)coil_struct_lookup_path(self->node, path, TRUE, &error);
    if (G_UNLIKELY(error))
        goto error;

    if (value == NULL) {
        PyErr_Format(KeyMissingError,
                     "<%s> The path '%s' was not found.",
                     coil_struct_get_path(self->node)->path, path->path);

        goto error;
    }

    coil_path_unref(path);
    return coil_value_as_pyobject(self->node, value);

 error:
    if (path)
        coil_path_unref(path);

    if (error)
        ccoil_error(&error);

    return NULL;
}

static int
struct_mp_set(PyCoilStruct * self, PyObject * py_key, PyObject * py_value)
{
    CoilPath *path;
    GValue *value;
    GError *error = NULL;

    path = coil_path_from_pyobject(py_key, &error);
    if (path == NULL)
        goto error;

    if (py_value == NULL) {
        /* delete the value */
        if (!coil_struct_delete_path(self->node, path, TRUE, &error))
            goto error;

        coil_path_unref(path);
    }
    else {
        value = coil_value_from_pyobject(py_value);
        if (value == NULL)
            goto error;

        if (!coil_struct_insert_path(self->node, path, value, TRUE, &error)) {
            path = NULL;
            value = NULL;
            goto error;
        }
    }

    return 0;

 error:
    if (path)
        coil_path_unref(path);

    if (error)
        ccoil_error(&error);

    return -1;
}

static Py_ssize_t
struct_mp_size(PyCoilStruct * self)
{
    GError *error = NULL;
    Py_ssize_t size;

    size = (Py_ssize_t) coil_struct_get_size(self->node, &error);
    if (size < 0) {
        ccoil_error(&error);
        return -1;
    }

    return size;
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
struct_init(PyCoilStruct * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = { "base", "container", "name", "location", NULL };
    PyCoilStruct *container = NULL;
    PyObject *name = NULL;
    PyObject *base = NULL;
    PyObject *location = NULL;
    CoilStruct *_container = NULL;
    gchar *_name = NULL;
    GError *error = NULL;

    if (!PyArg_ParseTupleAndKeywords
        (args, kwargs, "|OO!SO:ccoil.Struct.__init__", kwlist, &base,
         &PyCoilStruct_Type, &container, &name, &location))
        goto error;

    if (name) {
        _name = PyString_AsString(name);
        if (_name == NULL)
            goto error;

        if (container)
            _container = container->node;
    }
    else if (container) {
        PyErr_SetString(PyExc_ValueError,
                        "A name argument must be specified with a container argument.");

        goto error;
    }

    /* TODO(jcon): handle location */

    self->node = coil_struct_new(&error,
                                 "container", _container, "path", _name, NULL);

    if (self->node == NULL)
        goto error;

    g_object_set_qdata_full(G_OBJECT(self->node),
                            struct_wrapper_key, self, NULL);

    if (base == NULL)
        return 0;

    if (PyString_Check(base)) {
        CoilStruct *root;
        gchar *buffer;
        Py_ssize_t buflen;

        if (PyString_AsStringAndSize(base, &buffer, &buflen) < 0)
            goto error;

        root = coil_parse_string_len(buffer, buflen, &error);
        if (root == NULL)
            goto error;

        if (!coil_struct_merge(root, self->node, &error)) {
            g_object_unref(root);
            goto error;
        }

        g_object_unref(root);
        return 0;
    }

    if (!(PyCoilStruct_Check(base) ||
          PyMapping_Check(base) || PySequence_Check(base))) {
        PyErr_SetString(PyExc_ValueError,
                        "base argument must be a dict-like object, "
                        "a sequence of items, or a coil struct");
        goto error;
    }

    if (!struct_update_from_pyitems(self->node, base))
        goto error;

    return 0;

 error:
    if (error)
        ccoil_error(&error);

    return -1;
}

static PyMethodDef struct_methods[] = {
    {"clear",(PyCFunction)struct_empty, METH_NOARGS, NULL},
    {"container",(PyCFunction)struct_get_container, METH_NOARGS, NULL},
    {"copy",(PyCFunction)struct_copy, METH_VARARGS | METH_KEYWORDS, NULL},
    {"dict",(PyCFunction)struct_to_dict, METH_VARARGS | METH_KEYWORDS, NULL},
    {"expand",(PyCFunction)struct_expand, METH_VARARGS | METH_KEYWORDS, NULL},
    {"extend",(PyCFunction)struct_extend, METH_VARARGS | METH_KEYWORDS, NULL},
    {"extend_path",(PyCFunction)struct_extend_path, METH_VARARGS | METH_KEYWORDS, NULL},
    {"get",(PyCFunction)struct_get, METH_VARARGS | METH_KEYWORDS, NULL},
    {"has_key",(PyCFunction)struct_contains, METH_O, NULL},
    {"is_ancestor",(PyCFunction)struct_is_ancestor, METH_O, NULL},
    {"is_descendent",(PyCFunction)struct_is_descendent, METH_O, NULL},
    {"is_root",(PyCFunction)struct_is_root, METH_NOARGS, NULL},
    {"items",(PyCFunction)struct_items, METH_VARARGS | METH_KEYWORDS, NULL},
    {"iteritems",(PyCFunction)struct_iteritems, METH_NOARGS, NULL},
    {"iterkeys",(PyCFunction)struct_iterkeys, METH_NOARGS, NULL},
    {"iterpaths",(PyCFunction)struct_iterpaths, METH_NOARGS, NULL},
    {"itervalues",(PyCFunction)struct_itervalues, METH_NOARGS, NULL},
    {"keys",(PyCFunction)struct_keys, METH_NOARGS, NULL},
    {"merge",(PyCFunction)struct_merge, METH_VARARGS, NULL},
    {"path",(PyCFunction)struct_path, METH_VARARGS | METH_KEYWORDS, NULL},
    {"paths",(PyCFunction)struct_paths, METH_NOARGS, NULL},
    {"root",(PyCFunction)struct_root, METH_NOARGS, NULL},
    {"set",(PyCFunction)struct_set, METH_VARARGS | METH_KEYWORDS, NULL},
    {"string",(PyCFunction)struct_string, METH_VARARGS | METH_KEYWORDS, NULL},
    {"validate_key",(PyCFunction)struct_validate_key, METH_VARARGS | METH_CLASS, NULL},
    {"validate_path",(PyCFunction)struct_validate_path, METH_VARARGS | METH_CLASS, NULL},
    {"values",(PyCFunction)struct_values, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL},
};

static PySequenceMethods struct_as_sequence = {
    0,                          /* sq_length */
    0,                          /* sq_concat */
    0,                          /* sq_repeat */
    0,                          /* sq_item */
    0,                          /* sq_slice */
    0,                          /* sq_ass_item */
    0,                          /* sq_ass_slice */
    (objobjproc) struct_sq_contains,    /* sq_contains */
    0,                          /* sq_inplace_concat */
    0,                          /* sq_inplace_repeat */
};

static PyMappingMethods struct_as_mapping = {
    (lenfunc) struct_mp_size,
    (binaryfunc) struct_mp_get,
    (objobjargproc) struct_mp_set,
};

PyDoc_STRVAR(struct_doc, "TODO(jcon): Document this");

PyTypeObject PyCoilStruct_Type = {
    PyObject_HEAD_INIT(NULL)
        0,
    "ccoil.Struct",             /* tp_name */
    sizeof(PyCoilStruct),       /* tp_basicsize */
    0,                          /* tp_itemsize */

    /* methods */
    (destructor) struct_dealloc,        /* tp_dealloc */
    (printfunc) 0,              /* tp_print */
    (getattrfunc) 0,            /* tp_getattr */
    (setattrfunc) 0,            /* tp_setattr */
    (cmpfunc) 0,                /* tp_compare */
    (reprfunc) 0,               /* tp_repr */

    /* number methods */
    0,                          /* tp_as_number */
    &struct_as_sequence,        /* tp_as_sequence */
    &struct_as_mapping,         /* tp_as_mapping */

    /* standard operations */
    (hashfunc) struct_hash,     /* tp_hash */
    (ternaryfunc) 0,            /* tp_call */
    (reprfunc) struct_str,      /* tp_str */
    (getattrofunc) 0,           /* tp_getattro */
    (setattrofunc) 0,           /* tp_setattro */

    /* buffer */
    0,                          /* tp_as_buffer */

    /* flags */
    (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC) &
        (~Py_TPFLAGS_HAVE_GETCHARBUFFER & ~Py_TPFLAGS_HAVE_INPLACEOPS),

    struct_doc,                 /* tp_doc */
    (traverseproc) struct_traverse,     /* tp_traverse */
    (inquiry) struct_clear,     /* tp_clear */
    (richcmpfunc) struct_richcompare,   /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    (getiterfunc) struct_iter,  /* tp_iter */
    (iternextfunc) 0,           /* tp_iternext */
    struct_methods,             /* tp_methods */
    0,                          /* tp_members */
    0,                          /* tp_getset */
    0,                          /* tp_base */
    NULL,                       /* tp_dict */
    (descrgetfunc) 0,           /* tp_descr_get */
    (descrsetfunc) 0,           /* tp_descr_set */
    0,                          /* tp_dictoffset */
    (initproc) struct_init,     /* tp_init */
};
