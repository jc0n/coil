#!/usr/bin/env python

import os

from unittest import TestCase
from common import ccoil


class TestListProxy(TestCase):

    def setUp(self):
        self.node = ccoil.Struct('''
x: [1 2 2 3 'Hello']
y: [None True 1.0]
z: [[1 2] [3 4]]
''')
        self.rx = [1, 2, 2, 3, 'Hello']
        self.ry = [None, True, 1.0]
        self.rz = [[1, 2], [3, 4]]
        self.px = self.node['x']
        self.py = self.node['y']
        self.pz = self.node['z']
        self.proxies = (self.px, self.py, self.pz)
        self.lists = (self.rx, self.ry, self.rz)

    def testIsProperInstance(self):
        for proxy in self.proxies:
            self.assertTrue(isinstance(proxy, ccoil._ListProxy))

    def testListCompare(self):
        for proxy, real in zip(self.proxies, self.lists):
            self.assertEquals(proxy, real)

    def testList(self):
        for proxy, real in zip(self.proxies, self.lists):
            self.assertEquals(list(proxy), real)

    def testLength(self):
        for proxy, real in zip(self.proxies, self.lists):
            self.assertEquals(len(proxy), len(real))

    def testInsert(self):
        tests = ((0, 0),
                 (10, 'World'),
                 (2, 2))
        for proxy, real in zip(self.proxies, self.lists):
            for args in tests:
                proxy.insert(*args)
                real.insert(*args)
                self.assertEquals(proxy, real)

    def testAppend(self):
        tests = (4, 5, 'True', False)
        for proxy, real in zip(self.proxies, self.lists):
            for arg in tests:
                proxy.append(arg)
                real.append(arg)
                self.assertEqual(proxy, real)

    def testCopy(self):
        c = self.px.copy()
        c.append(56)
        self.rx.append(56)
        self.assertEquals(c, self.rx)
        self.assertNotEquals(c, self.px)

    def testClear(self):
        for proxy in self.proxies:
            proxy.clear()
            self.assertEquals(proxy, [])
            self.assertEquals(len(proxy), 0)

    def testCount(self):
        px = self.px
        self.assertEquals(px.count(-42), 0)
        self.assertEquals(px.count(1), 1)
        self.assertEquals(px.count(2), 2)

    def testExtend(self):
        pass

    def testIndex(self):
        pass

    def testPop(self):
        for i in self.rx:
            self.assertEquals(self.px.pop(0), self.rx.pop(0))
        for i in self.ry:
            self.assertEquals(self.py.pop(), self.ry.pop())
        n = self.pz.pop(1)
        self.assertEquals(list(n), [3, 4])
        self.assertEquals(list(self.pz), [[1, 2]])

    def testRemove(self):
        self.assertRaises(ValueError, self.px.remove, -42)
        tests = ((2, 1, 2, 3),
                 (None, True),
                 ([3, 4], [1, 2]))
        for test, proxy, real in zip(tests, self.proxies, self.lists):
            for arg in test:
                proxy.remove(arg)
                real.remove(arg)
                self.assertEquals(proxy, real)


    def testSort(self):
        pass

    def testContains(self):
        for proxy, real in zip(self.proxies, self.lists):
            for x in real:
                self.assertTrue(x in proxy)
            self.assertFalse(-42 in proxy)
            self.assertFalse('other value' in proxy)

    def testGetItem(self):
        pass

    def testConcat(self):
        pass

    def testRepeat(self):
        pass
