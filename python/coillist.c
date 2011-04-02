/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coilmodule.h"

PyDoc_STRVAR(list_doc,
    "TODO(jcon): Document this");

static void
list_dealloc(PyCoilList *list)
{
}

static int
list_init(PyCoilList *self,
          PyObject   *args,
          PyObject   *kwargs)
{
  return 0;
}

PySequenceMethods list_as_sequence =
{
	(lenfunc)0,			                /* sq_length */
	(binaryfunc)0,            			/* sq_concat */
	(ssizeargfunc)0,          			/* sq_repeat */
	(ssizeargfunc)0,			          /* sq_item */
  0,                              /* sq_slice */
	(ssizeobjargproc)0,		        	/* sq_ass_item */
  0,                              /* sq_ass_slice */
	(objobjproc)0,                	/* sq_contains */
	(binaryfunc)0,			            /* sq_inplace_concat */
	(ssizeargfunc)0,			          /* sq_inplace_repeat */
};

#define LIST_METHOD(name, func, flags) \
  { name, (PyCFunction)list##_func, flags, func##__doc__ },

static PyMethodDef list_methods[] =
{
  { NULL, NULL }
};
#undef LIST_METHOD

PyTypeObject PyCoilList_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,
  "ccoil.List",                                 /* tp_name */
  sizeof(PyCoilList),                           /* tp_basicsize */
  0,                                            /* tp_itemsize */

  /* methods */
  (destructor)list_dealloc,                     /* tp_dealloc */
  (printfunc)0,                                 /* tp_print */
  (getattrfunc)0,                               /* tp_getattr */
  (setattrfunc)0,                               /* tp_setattr */
  (cmpfunc)0,                                   /* tp_compare */
  (reprfunc)0,                                  /* tp_repr */

  /* number methods */
  0,                                            /* tp_as_number */
  &list_as_sequence,                            /* tp_as_sequence */
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

  list_doc,                                     /* tp_doc */
  (traverseproc)0,                              /* tp_traverse */
  (inquiry)0,                                   /* tp_clear */
  (richcmpfunc)0,                               /* tp_richcompare */
  0,                                            /* tp_weaklistoffset */
  (getiterfunc)0,                               /* tp_iter */
  (iternextfunc)0,                              /* tp_iternext */
  list_methods,                                 /* tp_methods */
  0,                                            /* tp_members */
  0,                                            /* tp_getset */
  0,                                            /* tp_base */
  NULL,                                         /* tp_dict */
  (descrgetfunc)0,                              /* tp_descr_get */
  (descrsetfunc)0,                              /* tp_descr_set */
  0,                                            /* tp_dictoffset */
  (initproc)list_init,                          /* tp_init */
  PyType_GenericAlloc,                          /* tp_alloc */
  PyType_GenericNew,                            /* tp_new */
  PyObject_Del,                                 /* tp_free */
};
