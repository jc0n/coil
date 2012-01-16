/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#ifndef _COILMODULE_H
#define _COILMODULE_H

#include "Python.h"
#include "pygobject.h"

#undef HAVE_STAT

#include "config.h"

#include "coil.h"

#define PyType_Register(dict, type, name, rval) \
  type.ob_type = &PyType_Type; \
  if (!type.tp_alloc) \
    type.tp_alloc = PyType_GenericAlloc; \
  if (!type.tp_new) \
    type.tp_new = PyType_GenericNew; \
  if (PyType_Ready(&type) < 0) \
    return rval; \
  if (dict) \
    PyDict_SetItemString(dict, name, (PyObject *)&type);

#ifndef Py_TYPE
# define Py_TYPE(o) ((PyTypeObject *)((PyObject *)o)->ob_type)
#endif

#ifndef Py_TYPE_NAME
# define Py_TYPE_NAME(o) (Py_TYPE(o)->tp_name)
#endif

#define _PyType_Check(obj, type) \
    ((PyTypeObject *)&type == Py_TYPE(obj))

#define _PyType_CheckExact(obj, type)                                        \
    (_PyType_Check(obj) ||                                                   \
     PyObject_IsInstance((PyObject *)obj, (PyObject *)&type))

#define ListProxyObject_Check(obj) \
    _PyType_Check(obj, ListProxy_Type)

#define ListProxyObject_CheckExact(obj) \
    _PyType_CheckExact(obj, ListProxy_Type)

#define PyCoilStruct_Check(obj) \
    _PyType_Check(obj, PyCoilStruct_Type)

#define PyCoilStruct_CheckExact(obj) \
    _PyType_CheckExact(obj, PyCoilStruct_Type)

#define PyCoilStructIter_Check(obj) \
    _PyType_Check(obj, PyCoilStructIter_Type)

#define PyCoilStructIter_CheckExact(obj) \
    _PyType_CheckExact(obj, PyCoilStructIter_Type)


extern PyTypeObject ListProxy_Type;
extern PyTypeObject PyCoilStruct_Type;
extern PyTypeObject PyCoilStructIterItem_Type;
extern PyTypeObject PyCoilStructIterKey_Type;
extern PyTypeObject PyCoilStructIterPath_Type;
extern PyTypeObject PyCoilStructIterValue_Type;

typedef struct _structiter_object structiter_object;
typedef struct _PyCoilStruct PyCoilStruct;
typedef struct _ListProxyObject ListProxyObject;

int
struct_register_types(PyObject * m, PyObject * d);

int
listproxy_register_types(PyObject *m, PyObject *d);

void ccoil_handle_error(void);

GValue *coil_value_from_pyobject(PyObject *o);
PyObject *coil_value_as_pyobject(CoilObject *node, GValue *value);
CoilPath *coil_path_from_pyobject(PyObject *o);
gboolean struct_update_from_pyitems(CoilObject *node, PyObject *o);

PyObject *ccoil_listproxy_new(CoilObject *node, GValueArray *arr);
PyObject *ccoil_struct_new(CoilObject *node);

CoilObject *ccoil_struct_get_real(PyObject *obj);

PyObject *struct_reconstructor(PyCoilStruct *, PyObject *);
PyObject *listproxy_reconstructor(ListProxyObject *, PyObject *);

extern PyObject *ccoilError;
extern PyObject *StructError;
extern PyObject *LinkError;
extern PyObject *IncludeError;
extern PyObject *KeyMissingError;
extern PyObject *KeyValueError;
extern PyObject *KeyTypeError;
extern PyObject *ParseError;

#if PY_MAJOR_VERSION >= 3
#define PyString PyBytes
#define PyInt PyLong
#endif

#endif
