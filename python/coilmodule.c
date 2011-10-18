/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coilmodule.h"
#include "coilstruct.h"

PyDoc_STRVAR(ccoil_module_documentation,
             "C implementation and optimization of the Python coil module.");


PyObject *ccoilError;
PyObject *StructError;
PyObject *LinkError;
PyObject *IncludeError;
PyObject *KeyMissingError;
PyObject *KeyValueError;
PyObject *KeyTypeError;
PyObject *ParseError;

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef ccoil_module = {
    PyModuleDef_HEAD_INIT,
    "ccoil",
    NULL, 0,
    ccoil_functions,
    NULL,
    ccoil_traverse,
    ccoil_clear,
    NULL
};
#endif


#if 0
PyObject *
pylist_from_value_list(GList * list)
{
    g_return_val_if_fail(list, NULL);

    Py_ssize_t i, size;
    PyObject *pylist, *pyitem;

    if (list == NULL)
        return PyList_New(0);

    size = g_list_length(list);
    pylist = PyList_New(size);
    if (pylist == NULL)
        return NULL;

    for (i = 0; i < size; i++) {
        g_assert(G_IS_VALUE(list->data));
        pyitem = coil_value_as_pyobject((GValue *) list->data);
        if (pyitem == NULL) {
            Py_DECREF(pylist);
            return NULL;
        }

        PyList_SET_ITEM(pylist, i, pyitem);
        list = g_list_next(list);
    }

    return pylist;
}
#endif

static CoilList *
coil_list_from_pysequence(PyObject * obj)
{
    GValue *value;
    GValueArray *arr;
    PyObject *fast, *item;
    Py_ssize_t i, n;

    fast = PySequence_Fast(obj, "Expecting sequence type.");
    if (fast == NULL)
        return NULL;

    n = PySequence_Fast_GET_SIZE(fast);
    arr = g_value_array_new(n);
    for (i = 0; i < n; i++) {
        item = PySequence_Fast_GET_ITEM(fast, i);
        value = coil_value_from_pyobject(item);
        if (value == NULL) {
            g_value_array_free(arr);
            return NULL;
        }
        g_value_array_insert(arr, i, value);
    }

    Py_DECREF(fast);
    return arr;
}

CoilPath *
coil_path_from_pyobject(PyObject *obj, GError **error)
{
    gchar *str = NULL;
    PyObject *xstr = NULL;
    CoilPath *path;
    Py_ssize_t len;

    if (PyUnicode_Check(obj)) {
        xstr = PyUnicode_AsEncodedString(obj, "ascii", NULL);
        if (xstr == NULL)
            return NULL;
        obj = xstr;
    }
    if (PyString_Check(obj)) {
        if (PyString_AsStringAndSize(obj, &str, &len) < 0)
            return NULL;
    }
    else {
        PyErr_Format(PyExc_TypeError,
            "Unsupported type %s for coil path.",
            Py_TYPE_NAME(obj));
        return NULL;
    }
    path = coil_path_new_len(str, (guint)len, error);
    Py_XDECREF(xstr);
    return path;
}

GValue *
coil_value_from_pyobject(PyObject *o)
{
    GValue *value = NULL;

    if (o == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "NULL python object.");
        return NULL;
    }
    else if (o == Py_None) {
        coil_value_init(value, COIL_TYPE_NONE, set_object, coil_none_object);
    }
    else if (o == Py_True) {
        coil_value_init(value, G_TYPE_BOOLEAN, set_boolean, TRUE);
    }
    else if (o == Py_False) {
        coil_value_init(value, G_TYPE_BOOLEAN, set_boolean, FALSE);
    }
#if PYTHON_MAJOR_VERSION <= 2
    else if (PyInt_CheckExact(o)) {
        coil_value_init(value, G_TYPE_INT, set_int, (gint) PyInt_AsLong(o));
    }
#endif
    else if (PyLong_CheckExact(o)) {
        coil_value_init(value, G_TYPE_LONG, set_long,
                        (glong) PyLong_AsLong(o));
    }
    else if (PyFloat_Check(o)) {
        coil_value_init(value, G_TYPE_FLOAT, set_float,
                        (gfloat) PyFloat_AsDouble(o));
    }
    else if (PyCoilStruct_Check(o)) {
        coil_value_init(value, COIL_TYPE_STRUCT, set_object,
                        ccoil_struct_get_real(o));
    }
    else if (PyString_Check(o)) {
        /* check for an expression */
        Py_ssize_t len;
        char *str;
        const char *s, *e = NULL;

        if (PyString_AsStringAndSize(o, &str, &len) < 0)
            return NULL;

        /* this is a hack, it is possible that the expression
         * characters are escaped but it will still work fine */
        s = memmem(str, len, "${", 2);
        if (s != NULL && len - 3 > str - s)
            e = memchr(s + 3, '}', (str + len) - s);
        if (s == NULL || e == NULL) {
            coil_value_init(value, G_TYPE_STRING, set_string,
                    (gchar *) PyString_AsString(o));
        }
        else {
            coil_value_init(value, COIL_TYPE_EXPR, take_object,
                    coil_expr_new_string(str, len, NULL));
        }
    }
    else if (PySequence_Check(o)) {
        coil_value_init(value, COIL_TYPE_LIST, take_boxed,
                        coil_list_from_pysequence(o));
    }
    else if (PyDict_Check(o)) {
        CoilStruct *node = coil_struct_new(NULL, NULL);
        if (node == NULL)
            return NULL;

        if (!struct_update_from_pyitems(node, o)) {
            g_object_unref(node);
            return NULL;
        }
        coil_value_init(value, COIL_TYPE_STRUCT, take_object, node);
    }
    else {
        PyErr_Format(PyExc_TypeError,
                     "Unsupported python type '%s' for coil value",
                     Py_TYPE_NAME(o));
    }
    /* TODO(jcon): unicode support */
    return value;
}

PyObject *
coil_value_as_pyobject(CoilStruct *node, GValue *value)
{
    GType type;

    if (value == NULL)
        Py_RETURN_NONE;

    type = G_VALUE_TYPE(value);

    switch (G_TYPE_FUNDAMENTAL(type)) {
        case G_TYPE_CHAR: {
            gint8 val = g_value_get_char(value);
            return PyString_FromStringAndSize((char *)&val, 1);
        }
        case G_TYPE_UCHAR: {
            guint8 val = g_value_get_uchar(value);
            return PyString_FromStringAndSize((char *)&val, 1);
        }
        case G_TYPE_BOOLEAN:
            return PyBool_FromLong(g_value_get_boolean(value));
        case G_TYPE_INT:
            return PyLong_FromLong(g_value_get_int(value));
        case G_TYPE_UINT:
            return PyLong_FromUnsignedLong((gulong) g_value_get_uint(value));
        case G_TYPE_LONG:
            return PyLong_FromLong(g_value_get_long(value));
        case G_TYPE_ULONG:
            return PyLong_FromUnsignedLong(g_value_get_ulong(value));
        case G_TYPE_INT64: {
            gint64 val = g_value_get_int64(value);
            if (G_MINLONG <= val && val <= G_MAXLONG)
                return PyLong_FromLong((glong) val);
            else
                return PyLong_FromLongLong(val);
        }
        case G_TYPE_UINT64: {
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
        case G_TYPE_STRING: {
            const gchar *str = g_value_get_string(value);
            if (str)
                return PyString_FromString(str);
            Py_RETURN_NONE;
        }
        case G_TYPE_OBJECT: {
            if (type == COIL_TYPE_STRUCT)
                return ccoil_struct_new(g_value_dup_object(value));

            if (type == COIL_TYPE_NONE)
                Py_RETURN_NONE;

            if (type == COIL_TYPE_EXPR || type == COIL_TYPE_LINK) {
                char *str;
                PyObject *res;
                GError *error = NULL;
                CoilStringFormat fmt;
                const GValue *real_value;

                fmt = default_string_format;
                fmt.options |= DONT_QUOTE_STRINGS;

                if (!coil_expand_value(value, &real_value, TRUE, &error))
                    return NULL;
                if (real_value == NULL)
                    return NULL;
                str = coil_value_to_string(real_value, &fmt, &error);
                if (str == NULL || error != NULL) {
                    ccoil_error(&error);
                    return NULL;
                }
                res = PyString_FromString(str);
                g_free(str);
                return res;
            }
            break;
        }
        case G_TYPE_BOXED:
            if (type == G_TYPE_GSTRING) {
                GString *buf = g_value_get_boxed(value);
                return PyString_FromStringAndSize(buf->str, buf->len);
            }
            if (type == COIL_TYPE_LIST) {
                assert(node != NULL);
                assert(value != NULL);
                GValueArray *arr = (GValueArray *)g_value_get_boxed(value);
                return ccoil_listproxy_new(node, arr);
            }
            break;
    }

    PyErr_Format(PyExc_TypeError,
                 "Unable to handle coil value type '%s'", g_type_name(type));

    return NULL;
}

void
ccoil_error(GError ** error)
{
    g_return_if_fail(error && *error);
    g_return_if_fail((*error)->domain == COIL_ERROR);

    PyObject *e = NULL;
    PyObject *msg = NULL;

    switch ((*error)->code) {
        case COIL_ERROR_INTERNAL:
            e = ccoilError;
            break;

        case COIL_ERROR_INCLUDE:
            e = IncludeError;
            break;

        case COIL_ERROR_KEY:
        case COIL_ERROR_PATH:
            e = KeyValueError;
            break;

        case COIL_ERROR_KEY_MISSING:
            e = KeyMissingError;
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
    g_nullify_pointer((gpointer *) error);

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
parse_pysequence(PyObject *seqobj)
{
    CoilStruct *root;
    PyObject *sepobj, *bufobj;
    Py_ssize_t n;
    const char *buffer;
    GError *error = NULL;

    n = PySequence_Size(seqobj);
    if (n < 0)
        return NULL;
    if (n == 0)
        return coil_struct_new(NULL, NULL);

    sepobj = PyString_FromStringAndSize(NULL, 0);
    if (sepobj == NULL)
        return NULL;

    bufobj = PyObject_CallMethod(sepobj, "join", "O", seqobj);
    Py_DECREF(sepobj);
    if (bufobj == NULL)
        return NULL;

    buffer = PyString_AS_STRING(bufobj);
    n = Py_SIZE(bufobj);

    root = coil_parse_string_len(buffer, n, &error);
    if (root == NULL) {
        ccoil_error(&error);
        Py_DECREF(bufobj);
        return NULL;
    }
    return root;
}

static PyObject *
ccoil_parse(PyObject *ignored, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"expand", "defaults", "ignore_missing", NULL};
    PyObject *input = NULL, *expand = NULL;
    PyObject *defaults = NULL, *ignore_missing = NULL;
    CoilStruct *root;
    GError *error = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|OOO:parse", kwlist,
                                     &input, &expand, &defaults, &ignore_missing))
        return NULL;

    if (PyFile_Check(input)) {
        FILE *fp = PyFile_AsFile(input);
        /* TODO(jcon): fix setting filename when locations are refactored */
        //PyObject *filename = PyFile_Name(input);
        //
/*    PyFile_IncUseCount((PyFileObject *)input); python 2.6 */
        root = coil_parse_stream(fp, NULL /*filename */ , &error);
/*    PyFile_DecUseCount((PyFileObject *)input); python 2.6 */

        if (G_UNLIKELY(error))
            goto error;
    }
    else if (PyString_Check(input)) {
        char *buffer;
        Py_ssize_t len;

        if (PyString_AsStringAndSize(input, &buffer, &len) < 0)
            goto error;

        root = coil_parse_string_len(buffer, len, &error);
        if (root == NULL || G_UNLIKELY(error))
            goto error;
    }
    else if (PySequence_Check(input)) {
        root = parse_pysequence(input);
        if (root == NULL)
            goto error;
    }
    else {
        PyErr_Format(PyExc_TypeError,
                     "input argument must be a string, "
                     "list of strings, or a file object, not '%s'",
                     Py_TYPE(input)->tp_name);

        return NULL;
    }

    /* TODO(jcon): implement expand, defaults, and ignore_missing later */

    return ccoil_struct_new(root);

 error:
    if (error)
        ccoil_error(&error);

    return NULL;
}

static PyObject *
ccoil_parse_file(PyObject * ignored, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = { "expand", "defaults", "ignore_missing", NULL };
    CoilStruct *root = NULL;
    GError *error = NULL;
    PyObject *expand = NULL;
    PyObject *defaults = NULL;
    PyObject *ignore_missing = NULL;
    gchar *filepath = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|OOO:parse_file", kwlist,
                                     &filepath, &expand, &defaults,
                                     &ignore_missing))
        return NULL;

    root = coil_parse_file(filepath, &error);
    if (root == NULL) {
        ccoil_error(&error);
        return NULL;
    }

    /* TODO(jcon): expand, defaults, ignore_missing */

    return ccoil_struct_new(root);
}

static PyMethodDef ccoil_functions[] = {
    {"_struct_reconstructor", (PyCFunction)struct_reconstructor, METH_VARARGS,
    PyDoc_STR("Internal. Used for pickling support.")},
    {"_listproxy_reconstructor", (PyCFunction)listproxy_reconstructor, METH_VARARGS,
    PyDoc_STR("Internal. Used for pickling support.")},

    {"parse", (PyCFunction)ccoil_parse, METH_VARARGS | METH_KEYWORDS, NULL},
    {"parse_file", (PyCFunction)ccoil_parse_file,
     METH_VARARGS | METH_KEYWORDS, NULL},
    {NULL, NULL, 0, NULL},
};

static int
init_errors(PyObject *m, PyObject * d)
{
    PyObject *bases = NULL;
    PyObject *errors = NULL;

    errors = PyModule_New("ccoil.errors");
    if (errors == NULL)
        goto error;

    if (PyModule_AddObject(m, "errors", errors) < 0)
        goto error;


    /* CoilError */
    ccoilError = PyErr_NewException("errors.CoilError",
                                    NULL, NULL);
    if (ccoilError == NULL)
        goto error;

    if (PyModule_AddObject(errors, "CoilError", ccoilError) < 0)
        goto error;


    /* StructError */
    StructError = PyErr_NewException("errors.StructError",
                                     ccoilError, NULL);
    if (StructError == NULL)
        goto error;

    if (PyModule_AddObject(errors, "StructError", StructError) < 0)
        goto error;


    /* Link Error */
    LinkError = PyErr_NewException("errors.LinkError",
                                   StructError, NULL);
    if (LinkError == NULL)
        goto error;

    if (PyModule_AddObject(errors, "LinkError", LinkError) < 0)
        goto error;


    /* Include Error */
    IncludeError = PyErr_NewException("errors.IncludeError",
                                      StructError, NULL);
    if (IncludeError == NULL)
        goto error;

    if (PyModule_AddObject(errors, "IncludeError", IncludeError) < 0)
        goto error;



    bases = PyTuple_Pack(2, ccoilError, PyExc_KeyError);
    if (bases == NULL)
        goto error;


    /* Key Missing Error */
    KeyMissingError = PyErr_NewException("errors.KeyMissingError",
                                         bases, NULL);
    if (KeyMissingError == NULL)
        goto error;

    if (PyModule_AddObject(errors, "KeyMissingError", KeyMissingError) < 0)
        goto error;


    /* Key Value Error */
    KeyValueError = PyErr_NewException("errors.KeyValueError",
                                       bases, NULL);
    if (KeyValueError == NULL)
        goto error;

    if (PyModule_AddObject(errors, "KeyValueError", KeyValueError) < 0)
        goto error;


    Py_DECREF(bases);
    bases = PyTuple_Pack(1, PyExc_TypeError);

    /* KeyType Error */
    KeyTypeError = PyErr_NewException("errors.KeyTypeError", bases, NULL);
    if (KeyTypeError == NULL)
        goto error;

    if (PyModule_AddObject(errors, "KeyTypeError", KeyTypeError) < 0)
        goto error;

    Py_DECREF(bases);


    /* ParseError */
    ParseError = PyErr_NewException("errors.ParseError",
                                    ccoilError, NULL);
    if (ParseError == NULL)
        return 0;

    if (PyModule_AddObject(errors, "ParseError", ParseError) < 0)
        return 0;

    return 1;

 error:
    Py_XDECREF(bases);
    Py_XDECREF(errors);
    return 0;
}

static int
init_constants(PyObject * m)
{
    PyObject *version, *version_info;

    version_info = Py_BuildValue("(iii)",
                                 COIL_MAJOR_VERSION,
                                 COIL_MINOR_VERSION,
                                 COIL_RELEASE_VERSION);

    if (PyModule_AddObject(m, "__version_info__", version_info) < 0)
        return 0;

    version = PyString_FromFormat("%d.%d.%d",
                                  COIL_MAJOR_VERSION,
                                  COIL_MINOR_VERSION,
                                  COIL_RELEASE_VERSION);

    if (PyModule_AddObject(m, "__version__", version) < 0)
        return 0;

    if (PyModule_AddStringConstant(m, "__author__", PACKAGE_BUGREPORT) < 0)
        return 0;

    if (PyModule_AddIntMacro(m, COIL_DEBUG) < 0)
        return 0;

    if (PyModule_AddIntMacro(m, COIL_INCLUDE_CACHING) < 0)
        return 0;

    if (PyModule_AddIntMacro(m, COIL_PATH_TRANSLATION) < 0)
        return 0;

    if (PyModule_AddIntMacro(m, COIL_STRICT_CONTEXT) < 0)
        return 0;

    if (PyModule_AddIntMacro(m, COIL_STRICT_FILE_CONTEXT) < 0)
        return 0;

    return 1;
}


#if PY_MAJOR_VERSION >= 3
PyObject *
PyInit_ccoil(void)
#define INITERROR return NULL
#else
void
initccoil(void)
#define INITERROR return
#endif
{
    PyObject *m, *d;

    coil_init();

 #if PY_MAJOR_VERSION >= 3
    m = PyModule_Create(&ccoil_module);
#else
    m = Py_InitModule3("ccoil", ccoil_functions, ccoil_module_documentation);
#endif

    if (m == NULL)
        INITERROR;

    d = PyModule_GetDict(m);

    if (!init_constants(m))
        INITERROR;

    if (!init_errors(m, d))
        INITERROR;

    if (!struct_register_types(m, d))
        INITERROR;

    if (!listproxy_register_types(m, d))
        INITERROR;

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
