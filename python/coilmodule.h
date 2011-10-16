/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#ifndef _COILMODULE_H

#include "Python.h"
#include "pygobject.h"

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

#define ListProxyObject_Check(obj) \
    (&ListProxyObject_Type == (PyTypeObject *)((PyObject *)obj)->ob_type)

#define PyCoilStruct_Check(obj) \
    (&PyCoilStruct_Type == (PyTypeObject *)((PyObject *)obj)->ob_type || \
     PyObject_IsInstance(obj, (PyObject *)&PyCoilStruct_Type))

#define PyCoilStructIter_Check(o) \
    (&PyCoilStructIter_Type == (PyTypeObject *)((PyObject *)o)->ob_type)

#ifndef Py_TYPE
# define Py_TYPE(o) ((o)->ob_type)
#endif

#ifndef Py_TYPE_NAME
# define Py_TYPE_NAME(o) (Py_TYPE(o)->tp_name)
#endif

extern PyTypeObject ListProxyObject_Type;
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

void
ccoil_error(GError ** error);

PyObject *pylist_from_value_list(GList * list);

GValue *coil_value_from_pyobject(PyObject * o);

PyObject *coil_value_as_pyobject(CoilStruct *node, GValue *value);

CoilPath *coil_path_from_pyobject(PyObject * o, GError ** error);

gboolean struct_update_from_pyitems(CoilStruct * node, PyObject * o);

PyObject *ccoil_listproxy_new(CoilStruct *node, GValueArray *arr);
PyObject *ccoil_struct_new(CoilStruct * node);

CoilStruct *ccoil_struct_get_real(PyObject *obj);

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
