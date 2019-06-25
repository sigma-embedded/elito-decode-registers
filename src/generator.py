#! /usr/bin/python3

# Copyright (C) 2015 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import abc

class CodeObject(metaclass=abc.ABCMeta):
    TABSIZE	=  8

    def __init__(self, comment):
        assert(comment == None or isinstance(comment, str))
        self.__comment = comment

    def get_comment(self):
        return self.__comment

    @abc.abstractmethod
    def emit(self, lvl = 0, print_comment = True):
        pass

    @staticmethod
    def _append(a, b):
        if a == None:
            return b
        elif b == None:
            return a
        else:
            return a + b

    @staticmethod
    def indent_line(level):
        return level * '\t'

    @staticmethod
    def next_tab(col):
        return (int((col + CodeObject.TABSIZE)  / CodeObject.TABSIZE) *
                CodeObject.TABSIZE)

    @staticmethod
    def fill_line(line, column):
        l = 0
        t = CodeObject.TABSIZE

        for c in line:
            if c == '\t':
                l = CodeObject.next_tab(l)
            else:
                l += 1

        while l < column:
            line += '\t'
            l     = CodeObject.next_tab(l)

        line += '\t'

        return line

class Symbol:
    def __init__(self, name, value, comment, fmt = None):
        self.__name = name
        self.__value = value
        self.__comment = comment
        self.__fmt = fmt

    def get_name(self):
        return self.__name

    def get_value(self):
        return self.__value

    def get_comment(self):
        return self.__comment

    def get_fmt(self):
        return self.__fmt

    def get_repr(self, str_fn):
        v = self.__value

        if self.__fmt != None:
            return self.__fmt % v
        elif str_fn != None and isinstance(v, str):
            return str_fn(v)
        else:
            return "%s" % v

class Reference(Symbol):
    def __init__(self, id, comment):
        Symbol.__init__(self, id, None, comment)

class Integer(Symbol):
    def __init__(self, name, value, comment, fmt = '%d'):
        Symbol.__init__(self, name, value, comment, fmt)

class CodeBlock(CodeObject):
    class __Wrap:
        def __init__(self, fn, o):
            self.__fn = fn
            self.__o  = o

        def __call__(self, *args, **kwargs):
            c = self.__fn(*args, **kwargs)
            self.__o.add(c)
            return c

    def __init__(self, parent, comment):
        CodeObject.__init__(self, comment)

        assert(parent == None or isinstance(parent, CodeBlock))

        self.__objects = []
        if parent:
            self.__top  = parent.__top
        else:
            self.__top	= self

    def __getattr__(self, *args):
        if self.__top == None:
            raise AttributeError(args)

        o = self.__top._redirect(*args)
        return CodeBlock.__Wrap(o, self)

    @abc.abstractmethod
    def _emit_pre(self, lvl, print_comment):
        pass

    @abc.abstractmethod
    def _emit_post(self, lvl, print_comment):
        pass

    def add(self, o):
        if o != None:
            assert(isinstance(o, CodeObject))
            self.__objects.append(o)

        return self

    def emit(self, lvl = 0, print_comment = True):
        res = self._emit_pre(lvl, print_comment)
        for o in self.__objects:
            res = self._append(res, o.emit(lvl + 1, print_comment))

        res = self._append(res, self._emit_post(lvl, print_comment))

        return res

    def create_block(self, comment):
        o = self.__top._create_block(self, comment)
        if o == None:
            return self

        self.add(o)
        return o

    def add_uint_var(self, v, order, comment, fmt = "%d"):
        if order <= 8:
            m = self.add_u8
        elif order <= 16:
            m = self.add_u16
        elif order <= 32:
            m = self.add_u32
        else:
            raise Exception("too much bits")

        m(v, comment, fmt)

class CodeFactory(metaclass=abc.ABCMeta):
    class __Array(CodeObject):
        def __init__(self, a, comment):
            CodeObject.__init__(self, comment)
            assert(None not in a)
            self.__a = a

        def emit(self, lvl = 0, print_comment = True):
            res = None
            for e in self.__a:
                res = self._append(res, e.emit(lvl, print_comment))

            return res

    @abc.abstractmethod
    def add_symbol(self, symbol):
        pass

    @abc.abstractmethod
    def add_string(self, v, comment):
        pass

    @abc.abstractmethod
    def _add_int(self, v, comment, width, is_signed, fmt = None):
        pass

    @abc.abstractmethod
    def add_comment(self, comment):
        pass

    @abc.abstractmethod
    def create_block(self, parent, comment):
        pass

    def add_array(self, a, comment, add_fn, sz_width = 8):
        res = []

        l = len(a)
        assert(l < (1 << sz_width))

        res.append(self.add_comment(comment))
        res.append(self._add_int(l, "number of elements", sz_width, False, "%2d"))

        i = 0
        for e in a:
            o = add_fn(e, "#%d" % i)
            res.append(o)
            i += 1

        return CodeFactory.__Array(res, "array")

    def add_uarray(self, a, comment, width, sz_width = 8, fmt = None):
        return self.add_array(a, comment,
                              lambda x, c: self._add_int(x, c, width, False, fmt),
                              sz_width)

    def add_size_t(self, v, comment):
        return self._add_int(v, comment, 16, False, "%u")

    def add_type(self, v, comment):
        return self._add_int(v, comment,  8, False, "%u")

    def add_u32(self, v, comment, fmt = "%d"):
        return self._add_int(v, comment, 32, False, fmt)

    def add_x32(self, v, comment, fmt = "0x%08x"):
        return self._add_int(v, comment, 32, False, fmt)

    def add_u16(self, v, comment, fmt = "%d"):
        return self._add_int(v, comment, 16, False, fmt)

    def add_x16(self, v, comment, fmt = "0x%04x"):
        return self._add_int(v, comment, 32, False, fmt)

    def add_u8(self, v, comment, fmt = "%d"):
        return self._add_int(v, comment, 8, False, fmt)

    def add_x8(self, v, comment, fmt = "0x%02x"):
        return self._add_int(v, comment, 32, False, fmt)

    def add_uint(self, v, num_bits, comment):
        return self._add_int(v, comment, num_bits, False)

    def add_sint(self, v, num_bits, comment):
        return self._add_int(v, comment, num_bits, True)

class CodeGenerator(CodeBlock):
    def __init__(self, factory):
        CodeBlock.__init__(self, None, None)
        self.__factory = factory
        self.__existing = set()

    def _emit_pre(self, lvl, print_comment):
        pass

    def _emit_post(self, lvl, print_comment):
        pass

    def _redirect(self, *args, **kwargs):
        return self.__factory.__getattribute__(*args, **kwargs)

    def emit(self, lvl = -1, print_comment = True):
        return CodeBlock.emit(self, lvl, print_comment)

    def _create_block(self, parent, comment):
        assert(isinstance(parent, CodeBlock))
        assert(comment == None or isinstance(comment, str))

        return self.__factory.create_block(parent, comment)

    def add_and_test_existing(self, symbol):
        if symbol in self.__existing:
            return True

        self.__existing.add(symbol)
        return False

def _test(factory):
    top = CodeGenerator(factory)

    code = top.create_block("code")

    s0 = Symbol("TEST0", 0x1234, "test", '0x%4x')
    s1 = Symbol("TEST1", "0xabcd", "test")

    code.add_symbol(s0)
    code.add_symbol(s1)

    code.add_u32(23, "test")
    code.add_u32(s0, "test")

    code.add_string(s1, "symbol")

    code.add_string("", "empty")
    code.add_string("0", "comment")
    code.add_string("01", "comment")
    code.add_string("012", "comment")
    code.add_string("0123", "comment")
    code.add_string("01234", "comment")
    code.add_string("012345", "comment")
    code.add_string("0123456", "comment")
    code.add_string("01234567", "comment")
    code.add_string("012345678", "comment")
    code.add_string("0123456789", "comment")
    code.add_string("0123456789a", "comment")
    code.add_string("0123456789ab", "comment")
    code.add_string("0123456789abc", "comment")
    code.add_string("0123456789abcd", "comment")
    code.add_string("0123456789abcde", "comment")

    block = code.create_block("new block")
    block.add_u32(42, "block")

    block0 = block.create_block("anther block")
    block0.add_u32(42, "block")

    block0.add_uarray([1,2,3,4], comment = "an array", width = 8)

    print(code.emit())
