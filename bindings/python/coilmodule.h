/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#ifndef _COILMODULE_H

#include "Python.h"

#include "coil.h"

#define PyType_Register(dict, type, name) \
  type.ob_type = &PyType_Type; \
  type.tp_alloc = PyType_GenericAlloc; \
  type.tp_new = PyType_GenericNew; \
  if (PyType_Ready(&type)) \
    return; \
  PyDict_SetItemString(dict, name, (PyObject *)&type);

#define PyCoilStruct_Check(obj) \
  (&PyCoilStruct_Type == (PyTypeObject *)((PyObject *)obj)->ob_type)

extern PyTypeObject PyCoilStruct_Type;

typedef struct _PyCoilStruct
{
  PyObject_HEAD

  CoilStruct *node;
  CoilStructIter *iterator;

} PyCoilStruct;

void
cCoil_error(GError **error);

PyObject *
pylist_from_value_list(const GList *list);

GValue *
coil_value_from_pyobject(PyObject *o);

PyObject *
coil_value_as_pyobject(const GValue *value);

CoilPath *
coil_path_from_pystring(PyObject *o,
                        GError   **error);

gboolean
update_struct_from_pyitems(CoilStruct *node,
                          PyObject   *o);


PyObject *
cCoil_struct_new(CoilStruct *node);

extern PyObject *cCoilError;
extern PyObject *StructError;
extern PyObject *LinkError;
extern PyObject *IncludeError;
extern PyObject *KeyMissingError;
extern PyObject *KeyValueError;
extern PyObject *ParseError;

#endif
