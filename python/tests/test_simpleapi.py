#!/usr/bin/env python

import ctypes
import functools

from unittest import TestCase
from common import ccoil


class SimpleAPITestCase(TestCase):

    def setUp(self):
        libname = 'libcoil-%s.so.0' % '.'.join(map(str, ccoil.__version_info__[:2]))
        ctypes.cdll.LoadLibrary(libname)
        self.lib = ctypes.CDLL(libname)


class TestGetter(SimpleAPITestCase):

    def setUp(self):
        super(TestGetter, self).setUp()
        self.node = ccoil.parse('''
        a: 1
        b: 1.123
        c: "Hello"
        d: True
        e: None
        f: [0 1 2]
        ''')
        obj = ctypes.c_void_p(self.node.c_node())
        self.get = functools.partial(self.lib.coil_get, obj)

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

    def test_get_double(self):
        doubleval = ctypes.c_double()
        result = self.get(ctypes.c_char_p("b"), ccoil.COIL_TYPE_DOUBLE, ctypes.pointer(doubleval))
        self.assertTrue(result)
        self.assertEquals(doubleval.value, 1.123)

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


class TestSetter(SimpleAPITestCase):

    def setUp(self):
        super(TestSetter, self).setUp()
        self.node = ccoil.Struct()
        obj = ctypes.c_void_p(self.node.c_node())
        self.set = functools.partial(self.lib.coil_set, obj)

    def test_set_int(self):
        intval = ctypes.c_int(123)
        result = self.set(ctypes.c_char_p("foo"), ccoil.COIL_TYPE_INT, ctypes.pointer(intval))
        self.assertTrue(result)
        self.assertEquals(self.node, ccoil.Struct('foo: 123'))

    def test_set_uint(self):
        intval = ctypes.c_int(123)
        result = self.set(ctypes.c_char_p("foo"), ccoil.COIL_TYPE_UINT, ctypes.pointer(intval))
        self.assertTrue(result)
        self.assertEquals(self.node, ccoil.Struct('foo:123'))

    def test_set_double(self):
        doubleval = ctypes.c_double(123.00000000000123)
        result = self.set(ctypes.c_char_p('foo'), ccoil.COIL_TYPE_DOUBLE, ctypes.pointer(doubleval))
        self.assertTrue(result)
        self.assertEquals(self.node, ccoil.Struct('foo:123.00000000000123'))

    def test_set_bool(self):
        true = ctypes.c_int(1)
        false = ctypes.c_int(0)

        key = ctypes.c_char_p("foo")
        result = self.set(key, ccoil.COIL_TYPE_BOOLEAN, ctypes.pointer(true))
        self.assertTrue(result)
        self.assertEquals(self.node, ccoil.Struct('foo:True'))

        result = self.set(key, ccoil.COIL_TYPE_BOOLEAN, ctypes.pointer(false))
        self.assertTrue(result)
        self.assertEquals(self.node, ccoil.Struct('foo:False'))

    def test_set_string(self):
        strval = ctypes.c_char_p("Welcome to coil")
        key = ctypes.c_char_p("message")
        result = self.set(key, ccoil.COIL_TYPE_STRING, ctypes.pointer(strval))

        self.assertTrue(result)
        self.assertEquals(self.node['message'], "Welcome to coil")
