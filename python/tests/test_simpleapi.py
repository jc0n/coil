#!/usr/bin/env python

import ctypes
import functools

from unittest import TestCase
from common import ccoil


class TestSimpleAPI(TestCase):

    def setUp(self):
        libname = 'libcoil-%s.so.0' % '.'.join(map(str, ccoil.__version_info__[:2]))
        ctypes.cdll.LoadLibrary(libname)
        lib = ctypes.CDLL(libname)

        self.node = ccoil.parse('''
        a: 1
        b: 1.123
        c: "Hello"
        d: True
        e: None
        f: [0 1 2]
        ''')
        obj = ctypes.c_void_p(self.node.c_node())
        self.get = functools.partial(lib.coil_get, obj)

    def test_get_int(self):
        intval = ctypes.c_int()
        result = self.get(ctypes.c_char_p("a"), ccoil.COIL_TYPE_INT, ctypes.pointer(intval))
        self.assertTrue(result)
        self.assertEquals(intval.value, 1)

    def test_get_uint(self):
        intval = ctypes.c_uint()
        result = self.get(ctypes.c_char_p("a"), ccoil.COIL_TYPE_UINT, ctypes.pointer(intval))
        self.assertTrue(result)
        self.assertEquals(intval.value, 1)

    def test_get_float(self):
        floatval = ctypes.c_float(0)
        result = self.get(ctypes.c_char_p("b"), ccoil.COIL_TYPE_FLOAT, ctypes.pointer(floatval))
        self.assertTrue(result)
        self.assertEquals(floatval.value, 1.123)

    def test_get_string(self):
        strval = ctypes.c_char_p()
        result = self.get(ctypes.c_char_p("c"), ccoil.COIL_TYPE_STRING, ctypes.pointer(strval))
        self.assertTrue(result)
        self.assertEquals(strval.value, "Hello")

    def test_get_boolean(self):
        boolval = ctypes.c_int()
        result = self.get(ctypes.c_char_p("d"), ccoil.COIL_TYPE_BOOLEAN, ctypes.pointer(boolval))
        self.assertTrue(result)
        self.assertEquals(boolval.value, True)

    def test_get_list(self):
        # TODO
        pass
