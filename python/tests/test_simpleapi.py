#!/usr/bin/env python

import ctypes
import functools

from ctypes import c_char_p, pointer, c_int, c_void_p, c_uint, c_double
from common import ccoil
from unittest import TestCase


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
        a2: a
        b: 1.123
        c: "Hello"
        d: True
        e: None
        f: [0 1 2]
        x.y.z: { foo: 123 }
        ''')
        obj = c_void_p(self.node.c_node())
        self.get = functools.partial(self.lib.coil_get, obj)

    def test_get_int(self):
        intval = c_int()
        result = self.get(c_char_p("a"), ccoil.COIL_TYPE_INT, pointer(intval))
        self.assertTrue(result)
        self.assertEquals(intval.value, 1)

    def test_get_uint(self):
        intval = c_uint()
        result = self.get(c_char_p("a"), ccoil.COIL_TYPE_UINT, pointer(intval))
        self.assertTrue(result)
        self.assertEquals(intval.value, 1)

    def test_get_double(self):
        doubleval = c_double()
        result = self.get(c_char_p("b"), ccoil.COIL_TYPE_DOUBLE, pointer(doubleval))
        self.assertTrue(result)
        self.assertEquals(doubleval.value, 1.123)

    def test_get_string(self):
        strval = c_char_p()
        result = self.get(c_char_p("c"), ccoil.COIL_TYPE_STRING, pointer(strval))
        self.assertTrue(result)
        self.assertEquals(strval.value, "Hello")

    def test_get_boolean(self):
        boolval = c_int()
        result = self.get(c_char_p("d"), ccoil.COIL_TYPE_BOOLEAN, pointer(boolval))
        self.assertTrue(result)
        self.assertEquals(boolval.value, True)

    def test_get_none(self):
        val = c_void_p()
        ptr = pointer(val)
        result = self.get(c_char_p("e"), ccoil.COIL_TYPE_NONE, ptr)
        self.assertTrue(result)
        self.assertEquals(ptr.contents.value, None)

        val = c_int(-1)
        ptr = pointer(val)
        result = self.get(c_char_p("e"), ccoil.COIL_TYPE_NONE, ptr)
        self.assertTrue(result)
        self.assertEquals(ptr.contents.value, 0)

    def test_get_list(self):
        # TODO
        pass

    def test_get_link_value(self):
        val = c_int()
        result = self.get(c_char_p('a2'), ccoil.COIL_TYPE_INT, pointer(val))
        self.assertTrue(result)
        self.assertEquals(val.value, 1)

    def test_get_struct(self):
        val = c_void_p()
        result = self.get(c_char_p('x.y.z'), ccoil.COIL_TYPE_STRUCT, pointer(val))
        self.assertTrue(result)
        self.assertEquals(val.value, self.node['x.y.z'].c_node())
        # coil_get gives us a reference to the object
        self.lib.g_object_unref(val)


class TestSetter(SimpleAPITestCase):

    def setUp(self):
        super(TestSetter, self).setUp()
        self.node = ccoil.Struct()
        obj = c_void_p(self.node.c_node())
        self.set = functools.partial(self.lib.coil_set, obj)

    def test_set_none(self):
        nullptr = c_void_p()
        result = self.set(c_char_p('foo'), ccoil.COIL_TYPE_STRING, pointer(nullptr))
        self.assertTrue(result)
        self.assertEquals(self.node['foo'], None)

    def test_set_int(self):
        intval = c_int(123)
        result = self.set(c_char_p("foo"), ccoil.COIL_TYPE_INT, pointer(intval))
        self.assertTrue(result)
        self.assertEquals(self.node['foo'], 123)
        self.assertEquals(self.node, ccoil.Struct('foo: 123'))

    def test_set_uint(self):
        intval = c_int(123)
        result = self.set(c_char_p("foo"), ccoil.COIL_TYPE_UINT, pointer(intval))
        self.assertTrue(result)
        self.assertEquals(self.node, ccoil.Struct('foo:123'))

    def test_set_double(self):
        doubleval = c_double(123.00000000000123)
        result = self.set(c_char_p('foo'), ccoil.COIL_TYPE_DOUBLE, pointer(doubleval))
        self.assertTrue(result)
        self.assertEquals(self.node, ccoil.Struct('foo:123.00000000000123'))

    def test_set_bool(self):
        true = c_int(1)
        false = c_int(0)

        key = c_char_p("foo")
        result = self.set(key, ccoil.COIL_TYPE_BOOLEAN, pointer(true))
        self.assertTrue(result)
        self.assertTrue(self.node['foo'])

        result = self.set(key, ccoil.COIL_TYPE_BOOLEAN, pointer(false))
        self.assertTrue(result)
        self.assertFalse(self.node['foo'])

    def test_set_string(self):
        strval = c_char_p("Welcome to coil")
        key = c_char_p("message")
        result = self.set(key, ccoil.COIL_TYPE_STRING, pointer(strval))

        self.assertTrue(result)
        self.assertEquals(self.node['message'], "Welcome to coil")


    def test_set_list(self):
        # TODO
        pass
