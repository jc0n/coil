#!/usr/bin/env python

import operator

from unittest import TestCase
from common import cCoil

Struct = cCoil.Struct

class TestStruct(TestCase):

  def setUp(self):
    self.data = (('first', {
                    'string': 'something',
                    'float': 2.5,
                    'int': 1,
                    'dict': {
                      'x': 1,
                      'y': 2,
                      'z': 'another_thing'}
                    }),
                  ('second', 'something else'),
                  ('last', ['list', 'of', 'strings']))
    self.struct = Struct(self.data)
    self.empty = Struct()

  def testRoot(self):
    s = self.empty
    self.assertTrue(isinstance(s, Struct))
    self.assertEqual(s.path(), '@root')
    self.assertEqual(len(s), 0)

  def testContains(self):
    s = self.empty
    s['a.b.c'], s['x.y.z'] = (123, 'test')
    self.assertEquals(len(s), 2)
    for k in ('a', 'a.b', 'a.b.c', 'x', 'x.y', 'x.y.z'):
      self.assertTrue(k in s, '%s in %s' % (k, s.path()))
      self.assertTrue(s.has_key(k))

    for k in ('b', 'b.c', 'y', 'y.z', 'z'):
      self.assertFalse(k in s, '%s in %s' % (k , s.path()))
      self.assertFalse(s.has_key(k))

    self.assertEquals(s['a.b.c'], 123)
    self.assertEquals(s['x.y.z'], 'test')

  def testKeyOrder(self):
    self.assertEquals(self.struct.keys(), ['first', 'second', 'last'])

  def testGetItem(self):
    self.assertEquals(self.struct['second'], 'something else')

  def testGetSimple(self):
    self.assertEquals(self.struct.get('second'), 'something else')

  def testGetDefault(self):
    self.assertEquals(self.struct.get('bogus', 'awesome'), 'awesome')
    self.assertEquals(self.struct.get('bogus.sub', 'awesome'), 'awesome')

  def testGetPath(self):
    self.assertEquals(self.struct.path(), '@root')
    self.assertEquals(self.struct.get('first.int'), 1)

  def testGetRelativePath(self):
    self.assertEquals(self.struct.path(''), '@root')
    self.assertEquals(self.struct.path('first'), '@root.first')
    self.assertEquals(self.struct.path('last'), '@root.last')

    first = self.struct.get('first')
    self.assertEquals(first.path('string'), '@root.first.string')
    self.assertEquals(first.path('dict.x'), '@root.first.dict.x')
    self.assertEquals(first.path('dict.y'), '@root.first.dict.y')

  def testGetParent(self):
    child = self.struct['first']
    self.assertEquals(child.get('..second'), 'something else')

  def testGetAbsolute(self):
    child = self.struct['first']
    self.assertEquals(child.get('@root.second'), 'something else')

#  def testGetRoot(self):
#    child = self.struct['first']
#    root = child.get('@root')
#    self.assertTrue(root is self.struct, "root is self.struct")
#    self.assertEquals(root, self.struct)
#    self.assertTrue(root is child['@root'])

  def testGetContainer(self):
    root = Struct({'a.b.c': 123})
    a, b = root['a'], root['a.b']
    self.assertEquals(b.container(), a)
    self.assertEquals(a.container(), root)
    self.assertEquals(root.container(), None)

#  def testIterItems(self):
#    itemlist = [('one', 1), ('two', 2), ('three', 3)]
#    self.assertEquals(list(Struct(itemlist).iteritems()), itemlist)

  def testKeyMissing(self):
    self.assertRaises(cCoil.KeyMissingError, lambda: self.struct['bogus'])
    self.assertRaises(cCoil.KeyMissingError, self.struct.get, 'bad')

  def testKeyType(self):
    self.assertRaises(cCoil.KeyTypeError, lambda: self.struct[None])
    self.assertRaises(cCoil.KeyTypeError, self.struct.get, None)

  def testKeyValue(self):
    self.assertRaises(cCoil.KeyValueError,
                      self.struct.set, 'first#', '')

    self.assertRaises(cCoil.KeyValueError,
                      self.struct.set, 'first..second', '')

  def testDict(self):
    self.assertEquals(self.struct['first'].dict(), dict(self.data[0][1]))

  def testSetShort(self):
    s = Struct()
    s['new'] = True
    self.assertEquals(s['new'], True)

  def testSetLong(self):
    s = Struct()
    s['new.sub'] = True
    self.assertEquals(s['new.sub'], True)
    self.assertEquals(s['new']['sub'], True)

  def testSetSubStruct(self):
    s = Struct({'sub': {'x': '${y}'}})
    self.assertRaises(KeyError, s.expand)
    s['sub.y'] = 'zap'
    s.expand()
    self.assertEquals(s['sub.x'], 'zap')
    self.assertEquals(s['sub.y'], 'zap')
    self.assertEquals(s['sub']['x'], 'zap')
    self.assertEquals(s['sub']['y'], 'zap')

  def testCopy(self):
    a = self.struct['first'].copy()
    b = self.struct['first'].copy()
    a['string'] = 'this is a'
    b['string'] = 'this is b'
    self.assertEquals(a['string'], 'this is a')
    self.assertEquals(b['string'], 'this is b')
    self.assertEquals(a['@root.string'], 'this is a')
    self.assertEquals(b['@root.string'], 'this is b')
    self.assertEquals(self.struct['first.string'], 'something')

  def testValidate(self):
    self.assertEquals(Struct.validate_key("foo"), True)
    self.assertEquals(Struct.validate_key("foo.bar"), False)
    self.assertEquals(Struct.validate_key("@root"), False)
    self.assertEquals(Struct.validate_key("#blah"), False)
    self.assertEquals(Struct.validate_path("foo"), True)
    self.assertEquals(Struct.validate_path("foo.bar"), True)
    self.assertEquals(Struct.validate_path("@root"), True)
    self.assertEquals(Struct.validate_path("#blah"), False)

  def testMerge(self):
    s1 = self.struct.copy()
    s2 = Struct()
    s2['first.new'] = "whee"
    s2['other.new'] = "woot"
    s2['new'] = "zomg"
    s1.merge(s2)
    self.assertEquals(s1['first.string'], "something")
    self.assertEquals(s1['first.new'], "whee")
    self.assertEquals(s1['other'], Struct({'new': "woot"}))
    self.assertEquals(s1['new'], "zomg")

  def testCopyList(self):
    a = Struct({'list': [1, 2, [3, 4]]})
    b = a.copy()
    l1, l2 = a['list'], b['list']
    self.assertEquals(l1, [1, 2, [3, 4]])
    self.assertEquals(l1, l2)
    self.assertFalse(l1 is l2, "l1 is l2")

  def testModifyList(self):
    s1 = Struct({'list': [1, 2, [3, 4]]})
    self.assertEquals(s1['list'], [1, 2, [3, 4]])
    s2 = s1.copy()
    s1['list'].append(8)
    s1['list'][2].append(9)
    self.assertEquals(s1['list'], [1, 2, [3, 4, 9], 8])
    self.assertEquals(s2['list'], [1, 2, [3, 4]])

#class ExpansionTestCase(TestCase):
#
#  def testExpand(self):
#    root = cCoil.parse("a: {x:1 y:2 z:3} b:a{}")
#    self.assertEquals(root['a.x'], 1)
#    self.assertEquals(root['a.y'], 2)
#    self.assertEquals(root['a.z'], 3)
#    self.assertEquals(root['b.x'], 1)
#    self.assertEquals(root['b.y'], 2)
#    self.assertEquals(root['b.z'], 3)
#
#
#  def testExpressionExpand(self):
#    root = Struct()
#    root["foo"] = "bbq"
#    root["bar"] = "omgwtf${foo}"
#    self.assertEquals(root.get('bar'), "omgwtfbbq")
#
#  def testExpandItem(self):
#    root = Struct()
#    root["foo"] = "bbq"
#    root["bar"] = "omgwtf${foo}"
#    self.assertEquals(root.get('bar'), "omgwtf${foo}")
#    self.assertEquals(root.expanditem('bar'), "omgwtfbbq")
#
#  def testExpandDefault(self):
#    root = Struct()
#    root["foo"] = "bbq"
#    root["bar"] = "omgwtf${foo}${baz}"
#    root.expand({'foo':"123",'baz':"456"})
#    self.assertEquals(root.get('bar'), "omgwtfbbq456")
#
#  def testExpandItemDefault(self):
#    root = Struct()
#    root["foo"] = "bbq"
#    root["bar"] = "omgwtf${foo}${baz}"
#    self.assertEquals(root.get('bar'), "omgwtf${foo}${baz}")
#    self.assertEquals(root.expanditem('bar',
#    defaults={'foo':"123",'baz':"456"}), "omgwtfbbq456")
#
#  def testExpandIgnore(self):
#    root = Struct()
#    root["foo"] = "bbq"
#    root["bar"] = "omgwtf${foo}${baz}"
#    root.expand(ignore_missing=True)
#    self.assertEquals(root.get('bar'), "omgwtfbbq${baz}")
#    root.expand(ignore_missing=('baz',))
#    self.assertEquals(root.get('bar'), "omgwtfbbq${baz}")
#
#  def testUnexpanded(self):
#    root = Struct()
#    root["foo"] = "bbq"
#    root["bar"] = "omgwtf${foo}${baz}"
#    root.expand(ignore_missing=True)
#    self.assertEquals(root.unexpanded(), set(["baz"]))
#    self.assertEquals(root.unexpanded(True), set(["@root.baz"]))
#
#  def testExpandItemIgnore(self):
#    root = Struct()
#    root["foo"] = "bbq"
#    root["bar"] = "omgwtf${foo}${baz}"
#    self.assertEquals(root.get('bar'), "omgwtf${foo}${baz}")
#    self.assertEquals(root.expanditem('bar', ignore_missing=('baz',)),
#                "omgwtfbbq${baz}")
#
#  def testExpandError(self):
#    root = Struct()
#    root["bar"] = "omgwtf${foo}"
#    self.assertRaises(KeyError, root.expand)
#    self.assertEquals(root.get('bar'), "omgwtf${foo}")
#
#  def testExpandItemError(self):
#    root = Struct()
#    root["bar"] = "omgwtf${foo}"
#    self.assertEquals(root.get('bar'), "omgwtf${foo}")
#    self.assertRaises(KeyError, root.expanditem, 'bar')
#    self.assertEquals(root.get('bar'), "omgwtf${foo}")
#
#  def testExpandInList(self):
#    root = Struct()
#    root["foo"] = "bbq"
#    root["bar"] = [ "omgwtf${foo}" ]
#    self.assertEquals(root['bar'][0], "omgwtf${foo}")
#    root.expand()
#    self.assertEquals(root['bar'][0], "omgwtfbbq")
#
#  def testExpandMixed(self):
#    root = Struct()
#    root["foo"] = "${bar}"
#    self.assertEquals(root.expanditem("foo", {'bar': "a"}), "a")
#    root["bar"] = "b"
#    self.assertEquals(root.expanditem("foo", {'bar': "a"}), "b")
#
#  def testCopy(self):
#    a = Struct()
#    a["foo"] = [ "omgwtf${bar}" ]
#    a["bar"] = "a"
#    b = a.copy()
#    b["bar"] = "b"
#    self.assertEquals(a.expanditem("foo"), [ "omgwtfa" ])
#    self.assertEquals(b.expanditem("foo"), [ "omgwtfb" ])
#    a.expand()
#    b.expand()
#    self.assertEquals(a.get("foo"), [ "omgwtfa" ])
#    self.assertEquals(b.get("foo"), [ "omgwtfb" ])
#
#class StringTestCase(TestCase):
#    def testNestedList(self):
#        root = struct.Struct({'x': ['a', ['b', 'c']]})
#        self.assertEquals(str(root), 'x: ["a" ["b" "c"]]')
