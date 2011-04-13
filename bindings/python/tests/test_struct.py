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
    keys = zip(*self.data)[0]
    self.assertEquals(tuple(self.struct.keys()),  keys)

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
    first = self.struct['first']
    self.assertEquals(first.get('..second'), 'something else')

  def testGetAbsolute(self):
    first = self.struct['first']
    self.assertEquals(first.get('@root.second'), 'something else')


  def testIter(self):
    keys = zip(*self.data)[0]
    self.assertEquals(tuple(self.struct), keys)

  def testIterItems(self):
    items = tuple(((k, v.dict()) if isinstance(v, Struct) else (k, v)
                    for k, v in self.struct.iteritems()))
    self.assertEquals(items, self.data)

  def testIterKeys(self):
    keys = zip(*self.data)[0]
    self.assertEquals(tuple(self.struct.iterkeys()), keys);

  def testIterValues(self):
    values = tuple((v.dict() if isinstance(v, Struct) else v
                    for v in self.struct.itervalues()))
    self.assertEquals(values, zip(*self.data)[1])

  def testKeyMissing(self):
    self.assertRaises(cCoil.KeyMissingError, lambda: self.struct['bogus'])
    self.assertRaises(cCoil.KeyMissingError, self.struct.get, 'bad')

  def testKeyType(self):
    for k in (None, True, False, 1, 1.0, {}, []):
      # TODO(jcon): use coil exception
      self.assertRaises(TypeError,
                        lambda: self.struct[k])

      self.assertRaises(cCoil.KeyTypeError, self.struct.get, k)
      # TODO(jcon): use coil exceptions
      self.assertRaises(TypeError, self.struct.set, k)
      self.assertRaises(ValueError, lambda: Struct([(k, 123)]))

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

class ContainerTestCase(TestCase):

  def setUp(self):
    self.buf = '''
    a.b.c: 123
    x.y.z: "hello"
    '''
    self.root = cCoil.parse(self.buf)
    a = self.root['a']
    b = self.root['a.b']
    x = self.root['x']
    y = self.root['x.y']
    self.blocks = (a, b, x, y)

  def testChangeContainer(self):
    a, b, x, y = self.blocks

    self.assertEquals(len(self.root), 2)
    self.assertEquals(a.path(), '@root.a')
    self.assertEquals(b.path(), '@root.a.b')
    self.assertEquals(x.path(), '@root.x')
    self.assertEquals(y.path(), '@root.x.y')
    self.assertEquals(a.container(), self.root)
    self.assertEquals(b.container(), a)
    self.assertEquals(x.container(), self.root)
    self.assertEquals(y.container(), x)
    self.assertEquals(b['c'], 123)
    self.assertEquals(y['z'], 'hello')

    self.root['new.a'] = a
    new = self.root['new']

    self.assertEquals(len(new), 1)
    self.assertEquals(new.path(), '@root.new')
    self.assertEquals(a.path(), '@root.new.a')
    self.assertEquals(b.path(), '@root.new.a.b')
    self.assertEquals(x.path(), '@root.new.x')
    self.assertEquals(y.path(), '@root.new.x.y')
    self.assertEquals(a.container(), new)
    self.assertEquals(a.root(), self.root)
    self.assertEquals(b.container(), a)
    self.assertEquals(b.root(), self.root)
    self.assertEquals(new.container(), self.root)
    self.assertEquals(new.root(), self.root)
    self.assertEquals(len(self.root), 0)
    self.assertEquals(b['c'], 123)
    self.assertEquals(y['z'], 'hello')

  def testSetContainer(self):
    r = Struct((('s', True),))
    q = Struct((('r', r),))

    self.assertEquals(q.root(), q)
    self.assertEquals(q.path(), '@root')
    self.assertEquals(q.container(), None)
    self.assertEquals(r.root(), q)
    self.assertEquals(r.container(), q)
    self.assertEquals(r.path(), '@root.r')
    self.assertEquals(q['r.s'], True)

    self.root['q'] = q

    self.assertTrue(self.root['q'] is q)
    self.assertTrue('q' in self.root)
    self.assertEquals(len(self.root), 3)
    self.assertEquals(self.root['q.r.s'], True)

    self.assertEquals(self.root['q.r.s'], True)
    self.assertEquals(q.path(), '@root.q')
    self.assertEquals(r.path(), '@root.q.r')
    self.assertEquals(r.container(), q)
    self.assertEquals(q.container(), self.root)
    self.assertEquals(r.root(), self.root)
    self.assertEquals(q.root(), self.root)

  def testDeleteFromContainer(self):
    a, b, x, y = self.blocks
    del self.root['x']

    self.assertTrue(len(self.root), 1)
    self.assertEquals(x.container(), None)
    self.assertEquals(x.root(), x)
    self.assertEquals(x.path(), '@root')
    self.assertEquals(y.root(), x)
    self.assertEquals(y.container(), x)
    self.assertEquals(y.path(), '@root.y')
    self.assertEquals(y['z'], 'hello')

  def testDeleteContainerRef(self):
    copy = self.root.copy()
    a = self.root['a']
    del a
    self.assertEquals(self.root, copy)

  def testGetRoot(self):
    for s in self.blocks:
      self.assertEquals(s.root(), self.root)
      self.assertTrue(s.root() is self.root,
                      's.root() is self.root')
      self.assertEquals(s['@root'], self.root)
      self.assertTrue(s['@root'] is self.root,
                      "s['@root'] is self.root")
      self.assertEquals(s.get('@root'), self.root)
      self.assertTrue(s.get('@root') is self.root,
                      's.get("@root") is self.root')

  def testGetContainer(self):
    a, b, x, y = self.blocks
    self.assertEquals(a.container(), self.root)
    self.assertEquals(b.container(), a)
    self.assertEquals(x.container(), self.root)
    self.assertEquals(y.container(), x)
    self.assertEquals(self.root.container(), None)

class ExpansionTestCase(TestCase):

  def testExpand(self):
    root = cCoil.parse("a: {x:1 y:2 z:3} b: a{}")
    self.assertEquals(root['a.x'], 1)
    self.assertEquals(root['a.y'], 2)
    self.assertEquals(root['a.z'], 3)
    self.assertEquals(root['b.x'], 1)
    self.assertEquals(root['b.y'], 2)
    self.assertEquals(root['b.z'], 3)

  def testExpressionExpand(self):
    root = Struct()
    root["foo"] = "bbq"
    root["bar"] = "omgwtf${foo}"
    self.assertEquals(root.get('bar'), "omgwtfbbq")

  def testExpandItem(self):
    root = Struct()
    root["foo"] = "bbq"
    root["bar"] = "omgwtf${foo}"
    self.assertEquals(root.get('bar'), "omgwtf${foo}")
    self.assertEquals(root.expanditem('bar'), "omgwtfbbq")

  def testExpandDefault(self):
    root = Struct()
    root["foo"] = "bbq"
    root["bar"] = "omgwtf${foo}${baz}"
    root.expand({'foo':"123",'baz':"456"})
    self.assertEquals(root.get('bar'), "omgwtfbbq456")

  def testExpandItemDefault(self):
    root = Struct()
    root["foo"] = "bbq"
    root["bar"] = "omgwtf${foo}${baz}"
    self.assertEquals(root.get('bar'), "omgwtf${foo}${baz}")
    self.assertEquals(root.expanditem('bar',
    defaults={'foo':"123",'baz':"456"}), "omgwtfbbq456")

  def testExpandIgnore(self):
    root = Struct()
    root["foo"] = "bbq"
    root["bar"] = "omgwtf${foo}${baz}"
    root.expand(ignore_missing=True)
    self.assertEquals(root.get('bar'), "omgwtfbbq${baz}")
    root.expand(ignore_missing=('baz',))
    self.assertEquals(root.get('bar'), "omgwtfbbq${baz}")
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
