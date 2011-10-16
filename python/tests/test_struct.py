#!/usr/bin/env python

import operator
import pickle

from unittest import TestCase
from common import ccoil

Struct = ccoil.Struct


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

  def testPathOrder(self):
    paths = map(lambda k: "@root.%s" % k, zip(*self.data)[0])
    self.assertEquals(self.struct.paths(), paths)

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

  def testIterPaths(self):
    paths = map(lambda k: "@root.%s" % k, zip(*self.data)[0])
    self.assertEquals(list(self.struct.iterpaths()), paths)

  def testIterValues(self):
    values = tuple((v.dict() if isinstance(v, Struct) else v
                    for v in self.struct.itervalues()))
    self.assertEquals(values, zip(*self.data)[1])

  def testKeyMissing(self):
    self.assertRaises(ccoil.errors.KeyMissingError,
                      lambda: self.struct['bogus'])
    self.assertRaises(ccoil.errors.KeyMissingError,
                      self.struct.get, 'bad')

  def testKeyType(self):
    for k in (None, True, False, 1, 1.0, {}, []):
      self.assertRaises(TypeError, lambda: self.struct[k])
      self.assertRaises(TypeError, self.struct.get, k)
      self.assertRaises(TypeError, self.struct.set, k)
      self.assertRaises(ValueError, lambda: Struct([(k, 123)]))

  def testKeyValue(self):
    self.assertRaises(ccoil.errors.KeyValueError,
                      self.struct.set, 'first#', '')

    self.assertRaises(ccoil.errors.KeyValueError,
                      self.struct.set, 'first..second', '')

  def testToDict(self):
    self.assertEquals(self.struct['first'].dict(), dict(self.data[0][1]))

  def testFromDict(self):
    root = Struct({'a.b.c.d': 123, 'x.y.z': 'Hello'})
    self.assertEquals(root['a.b.c.d'], 123)
    self.assertEquals(root['x.y.z'], 'Hello')

  def testSetShort(self):
    s = Struct()
    s['new'] = True
    self.assertEquals(s['new'], True)

  def testSetLong(self):
    s = Struct()
    s['new.sub'] = True
    self.assertEquals(s['new.sub'], True)
    self.assertEquals(s['new']['sub'], True)

  def testSetExpression(self):
    s = Struct()
    s['x'] = '${y}'
    s['y'] = 123
    self.assertEquals(s['x'], str(s['y']))
    self.assertEquals(s['y'], 123)

  def testSetSubStruct(self):
    s = Struct('sub.x: "${y}"')
    self.assertRaises(KeyError, s.get, 'sub.x')
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

  def testCompareStruct(self):
    a = Struct('a:1 b:2 c:3')
    b = Struct('a:1 b:2 c:3')
    c = Struct('x.y.z: 123')
    d = Struct('x: { y: { z: 123 } }')

    self.assertTrue(a == a, 'a == a')
    self.assertTrue(a == b, 'a == b')
    self.assertTrue(b == a, 'b == a')
    self.assertTrue(a != c, 'a != c')
    self.assertTrue(a != d, 'a != d')
    self.assertTrue(c == c, 'c == c')
    self.assertTrue(c == d, 'c == d')

  def testCompareDict(self):
    a = Struct('a.b.c: 123 x.y.z: "Hello"')
    b = {'a.b.c': 123, 'x.y.z': 'Hello' }
    c = {'a': {'b': {'c': 123}}, 'x': {'y': {'z': 'Hello'}}}
# change one value
    d = {'a': {'b': {'c': None}}, 'x': {'y': {'z': 'Hello'}}}
# change one key
    e = {'a': {'b': {'d': 123}}, 'x': {'y': {'z': 'Hello'}}}


    self.assertTrue(a == b, 'a == b')
    self.assertTrue(a == c, 'b == c')

    self.assertTrue(a != d, 'a != d')
    self.assertTrue(a != e, 'a != e')

class PrototypeTestCase(TestCase):

  def testExtendsConsistency(self):
    buf = '''
    test: base { myvalue: 'another value' }
    base: {
      x: True y: "some value"
    }
    base.z: "other value"
    '''
    root = Struct(buf)
    self.assertRaises(ccoil.errors.KeyMissingError, lambda: root['test.z'])
    self.assertEquals(root['test'], {
                      'x': True,
                      'y': 'some value',
                      'myvalue': 'another value'})
    self.assertEquals(root['base'], {
                      'x': True,
                      'y': 'some value',
                      'z': 'other value'})

  def testMerge(self):
    buf = '''
    base: {
      b: { x:1 y:2 z:3 }
      c: ..test.d {}
    }
    test: base {
      a: b, {}
      d.a: 1
    }
    '''
    root = Struct(buf)
    base = {'b': {'x':1, 'y':2, 'z':3}, 'c.a': 1}
    test = {'a': base['b'], 'b': base['b'], 'd.a': 1, 'c.a': 1}
    self.assertEquals(root['base'], base)
    self.assertEquals(root['test'], test)

  def testMergeSub(self):
    buf = '''
    test: base {
      a.b: { y: 2 z: 3 }
    }
    base: {
      a.b.x: 1
    }
    '''
    root = Struct(buf)
    test = {'a.b': {'x': 1, 'y': 2, 'z': 3}}
    base = {'a.b.x': 1}
    self.assertEquals(root['test'], test)
    self.assertEquals(root['base'], base)

class ContainerTestCase(TestCase):

  def setUp(self):
    self.buf = '''
    a.b.c: 123
    x.y.z: "hello"
    '''
    self.root = ccoil.parse(self.buf)
    a = self.root['a']
    b = self.root['a.b']
    x = self.root['x']
    y = self.root['x.y']
    self.blocks = (a, b, x, y)

  def testChangeContainer(self):
    a, b, x, y = self.blocks

    # check sanity
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

    # create new container and place 'a' within
    new = self.root['new'] = Struct()
    new['a'] = a

    self.assertEquals(len(new), 1)
    self.assertEquals(new.path(), '@root.new')
    self.assertEquals(a.path(), '@root.new.a')
    self.assertEquals(b.path(), '@root.new.a.b')
    self.assertEquals(a.container(), new)
    self.assertEquals(a.root(), self.root)
    self.assertEquals(b.container(), a)
    self.assertEquals(b.root(), self.root)
    self.assertEquals(new.container(), self.root)
    self.assertEquals(new.root(), self.root)
    self.assertEquals(b['c'], 123)

    # check sanity
    self.assertEquals(x.path(), '@root.x')
    self.assertEquals(y.path(), '@root.x.y')
    self.assertEquals(len(self.root), 2)
    self.assertEquals(y['z'], 'hello')

    # place 'x' in new container
    new['x'] = x
    self.assertEquals(x.path(), '@root.new.x')
    self.assertEquals(y.path(), '@root.new.x.y')
    self.assertEquals(x.container(), new)
    self.assertEquals(y.container(), x)
    self.assertEquals(len(self.root), 1)
    self.assertEquals(len(new), 2)
    self.assertEquals(x.root(), self.root)
    self.assertEquals(y.root(), self.root)
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
    root = ccoil.parse("a: {x:1 y:2 z:3} b: a{}")
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


  def testExpandDefault(self):
    root = Struct()
    root["foo"] = "bbq"
    root["bar"] = "omgwtf${foo}${baz}"
    root.expand({'foo':"123",'baz':"456"})
    self.assertEquals(root.get('bar'), "omgwtfbbq456")

#  def testExpandItem(self):
#    root = Struct()
#    root["foo"] = "bbq"
#    root["bar"] = "omgwtf${foo}"
#    self.assertEquals(root.get('bar'), "omgwtf${foo}")
#    self.assertEquals(root.expanditem('bar'), "omgwtfbbq")
#
#  def testExpandMixed(self):
#    root = Struct()
#    root["foo"] = "${bar}"
#    self.assertEquals(root.expanditem("foo", {'bar': "a"}), "a")
#    root["bar"] = "b"
#    self.assertEquals(root.expanditem("foo", {'bar': "a"}), "b")
#

#  def testExpandItemError(self):
#    root = Struct()
#    root["bar"] = "omgwtf${foo}"
#    self.assertEquals(root.get('bar'), "omgwtf${foo}")
#    self.assertRaises(KeyError, root.expanditem, 'bar')
#    self.assertEquals(root.get('bar'), "omgwtf${foo}")
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

  def testExpressionExpandError(self):
    root = Struct()
    root["bar"] = "omgwtf${foo}"
    self.assertRaises(ccoil.errors.KeyMissingError, root.get, 'bar')

  def testExpandInList(self):
    root = Struct()
    root["foo"] = "bbq"
    root["bar"] = [ "omgwtf${foo}" ]
    self.assertEquals(root['bar'][0], "omgwtfbbq")

  def testCopy(self):
    a = Struct()
    a["foo"] = [ "omgwtf${bar}" ]
    a["bar"] = "a"
    b = a.copy()
    b["bar"] = "b"
    self.assertEquals(a.get("foo"), [ "omgwtfa" ])
    self.assertEquals(b.get("foo"), [ "omgwtfb" ])

class StringTestCase(TestCase):

  def testTrue(self):
    root = Struct("value: True")
    self.assertEquals(str(root), "value: True")

  def testFalse(self):
    root = Struct("value: False")
    self.assertEquals(str(root), "value: False")

  def testNone(self):
    root = Struct("value: None")
    self.assertEquals(str(root), "value: None")

  def testInteger(self):
    root = Struct("value: 123")
    self.assertEquals(str(root), "value: 123")

  def testContainer(self):
    root = Struct("a.b.c: 123 a.b.d: 'Hello'")
    self.assertEquals(str(root),
'''\
a: {
    b: {
        c: 123
        d: 'Hello'
    }
}''')

  def testString1(self):
    root = Struct("value: 'Hello World!'")
    self.assertEquals(str(root), "value: 'Hello World!'")

  def testString2(self):
    root = Struct("value: '''Hello World!'''")
    self.assertEquals(str(root), "value: 'Hello World!'")

  def testString3(self):
    root = Struct("value: 'Hello\nWorld!'")
    self.assertEquals(str(root), "value: '''Hello\nWorld!'''")

  def testString4(self):
    root = Struct("value: '%s'" % ("A" * 80))
    self.assertEquals(str(root), "value: '''%s'''" % ("A" * 80))

  def testNestedList(self):
    root = Struct({'x': ['a', ['b', 'c']]})
    self.assertEquals(str(root), "x: ['a' ['b' 'c']]")
    s = "x: [1 2 3 'hello' True]"
    self.assertEquals(str(Struct(s)), s)



class PickleTestCase(TestCase):

    def tearDown(self):
        if hasattr(self, 'node'):
            buf = pickle.dumps(self.node)
            self.assertEquals(self.node, pickle.loads(buf))

    def testBasic(self):
        self.node = Struct('''
a: True b: False c: None
x: 1 y: -100 z: 42.0
''')

    def testList(self):
        self.node = Struct('''
x: [1 2 3 True False None 'Hello']
''')

    def testNestedList(self):
        self.node = Struct('''
x: [[1 2] [True False] ['Hello' 'World']]
''')

    def testNestedStruct(self):
        self.node = Struct('''
x.y.z: 123
a.b.c: 'Hello'
''')

    def testExpressions(self):
        self.node = Struct('''
x: 'Hello'
y: 'World'
z: "${x} ${y}"
''')

    def testLinks(self):
        self.node = Struct('''
x: 'Hello'
sub.z: =..x
''')

