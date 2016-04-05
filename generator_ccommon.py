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

import generator

STYLE_SYMBOL_VALUE_COLUMN	= 50
STYLE_COMMENT_COLUMN		= 70

class _CodeObject(generator.CodeObject):
    def __init__(self, comment):
        generator.CodeObject.__init__(self, comment)

    def _append_comment(self, l, print_comment):
        if print_comment and self.get_comment():
            l  = self.fill_line(l, STYLE_COMMENT_COLUMN)
            l += self.comment(self.get_comment())


        return l

    @staticmethod
    def quote(s):
        res = '"'
        for c in s:
            if c < '\x20' or c > '\x7f':
                res += "\\x%02x" % ord(c)
            elif c in ['\\', '"']:
                res += '\\%c' % c
            else:
                res += c
        res += '"'

        return res

    @staticmethod
    def comment(s):
        return '/* %s */' % s

    @staticmethod
    def make_str(s):
        return _CodeObject.quote(s)

class _Block(_CodeObject, generator.CodeBlock):
    def __init__(self, parent, comment):
        _CodeObject.__init__(self, comment)
        generator.CodeBlock.__init__(self, parent, comment)

    def _emit_pre(self, lvl, print_comment):
        if lvl < 0:
            return ""

        res  = self.indent_line(lvl)
        res += '{'
        if print_comment and self.get_comment():
            res += ' ' + self.comment('{{{ ' + self.get_comment())

        return res + '\n'

    def _emit_post(self, lvl, print_comment):
        if lvl < 0:
            return ""

        res  = self.indent_line(lvl)
        res += '}'
        if print_comment and self.get_comment():
            res += ' ' + self.comment('}}} ' + self.get_comment())

        return res + '\n'

class _Symbol(_CodeObject):
    def __init__(self, symbol):
        assert(isinstance(symbol, generator.Symbol))

        _CodeObject.__init__(self, symbol.get_comment())

        self.__name    = symbol.get_name()
        self.__value   = symbol.get_repr(_CodeObject.make_str)

        assert(isinstance(self.__value, str))

    def emit(self, lvl = 0, print_comment = True):
        res  = self.indent_line(lvl)
        res += "#define %s" % self.__name
        res  = self.fill_line(res, STYLE_SYMBOL_VALUE_COLUMN)
        res += self.__value

        res  = self._append_comment(res, print_comment)

        return res + '\n'

class _Comment(_CodeObject):
    def __init__(self, comment):
        _CodeObject.__init__(self, comment)

    def emit(self, lvl = 0, print_comment = True):
        res  = self.indent_line(lvl)
        res += self.comment(self.get_comment())

        return res + '\n'

class _Forward(_CodeObject):
    def __init__(self, type, id, comment = None):
        _CodeObject.__init__(self, comment)

        self.__type = type
        self.__id = id

    def emit(self, lvl = 0, print_comment = True):
        res  = self.indent_line(lvl)
        res += self.__type;
        res  = self.fill_line(res, STYLE_SYMBOL_VALUE_COLUMN)
        res += self.__id + ";"

        res  = self._append_comment(res, print_comment)
        return res + '\n'

class _BlockAttributeSimple(_CodeObject):
    def __init__(self, symbol):
        _CodeObject.__init__(self, symbol.get_comment())

        self.__id = symbol.get_name()

        value = symbol.get_value()

        if symbol.get_fmt():
            self.__value = symbol.get_repr(None)
        elif isinstance(value, str):
            self.__value = self.quote(value)
        elif isinstance(value, bool):
            self.__value = ['false', 'true'][value]
        elif isinstance(value, generator.Symbol):
            self.__value = value.get_name()
        else:
            self.__value = symbol.get_repr(None)

    def emit(self, lvl = 0, print_comment = True):
        res  = self.indent_line(lvl)
        res += "%s =" % self.__id
        res  = self.fill_line(res, lvl * 8 + 8)
        res += self.__value + ','
        res  = self.fill_line(res, STYLE_SYMBOL_VALUE_COLUMN)

        res  = self._append_comment(res, print_comment)
        return res + '\n'

class _BlockAttributeBlock(_CodeObject, generator.CodeBlock):
    def __init__(self, parent, id, comment = None):
        _CodeObject.__init__(self, comment)
        generator.CodeBlock.__init__(self, parent, comment)

        self.__id = id

    def _emit_pre(self, lvl, print_comment):
        res  = self.indent_line(lvl)

        if self.__id is not None:
            res += "%s =" % (self.__id)
            res  = self.fill_line(res, lvl * 8 + 8)

        res += '{'
        res  = self._append_comment(res, print_comment)
        return res + '\n'

    def _emit_post(self, lvl, print_comment):
        if lvl < 0:
            return ""

        res  = self.indent_line(lvl)
        res += '},'

        return res + '\n'

class _StructDesignator(generator.Symbol):
    def __init__(self, id, value, comment, fmt = None):
        super().__init__('.%s' % id, value, comment, fmt)

    @staticmethod
    def from_symbol(symbol):
        return _StructDesignator(symbol.get_name(),
                                 symbol.get_value(),
                                 symbol.get_comment(),
                                 symbol.get_fmt())


class _ArrayDesignator(generator.Symbol):
    def __init__(self, idx, value, comment):
        if idx is None:
            id = None
        else:
            id = '[%s]' % id

        super().__init__(id, value, comment)

class _StructAttributeSimple(_BlockAttributeSimple):
    def __init__(self, symbol):
        _BlockAttributeSimple.__init__(self, symbol)

class _StructAttributeBlock(_BlockAttributeBlock):
    def __init__(self, parent, id, comment):
        _BlockAttributeBlock.__init__(self, parent, '.%s' % id, comment)

class _ArrayElementSimple(_BlockAttributeSimple):
    def __init__(self, idx, value, comment):
        _BlockAttributeSimple.__init__(self, _ArrayDesignator(id, value, comment))

class _ArrayElementBlock(_BlockAttributeBlock):
    def __init__(self, parent, idx, comment):
        if idx is None:
            id = None
        else:
            id = '[%s]' % idx

        _BlockAttributeBlock.__init__(self, parent, id, comment)

class _Array(_CodeObject, generator.CodeBlock):
    def __init__(self, parent, type, name, dim, comment = None):
        _CodeObject.__init__(self, comment)
        generator.CodeBlock.__init__(self, parent, comment)

        self.__id = name
        self.__type = type
        self.__dim = dim

    def _emit_pre(self, lvl, print_comment):
        if lvl < 0:
            return ""

        res  = self.indent_line(lvl)
        res += "static %s const" % self.__type
        res  = self.fill_line(res, STYLE_SYMBOL_VALUE_COLUMN)
        res += "%s[%s] = {" % (self.__id, self.__dim)

        if print_comment and self.get_comment():
            res += ' ' + self.comment('{{{ ' + self.get_comment())

        return res + '\n'

    def _emit_post(self, lvl, print_comment):
        if lvl < 0:
            return ""

        res  = self.indent_line(lvl)
        res += '};'

        if print_comment and self.get_comment():
            res += ' ' + self.comment('}}} ' + self.get_comment())

        return res + '\n'

class _Struct(_CodeObject, generator.CodeBlock):
    def __init__(self, parent, type, name, comment = None):
        _CodeObject.__init__(self, comment)
        generator.CodeBlock.__init__(self, parent, comment)

        self.__id = name
        self.__type = type

    def _emit_pre(self, lvl, print_comment):
        if lvl < 0:
            return ""

        res  = self.indent_line(lvl)
        res += "static %s const" % self.__type
        res  = self.fill_line(res, STYLE_SYMBOL_VALUE_COLUMN)
        res += "%s = {" % (self.__id)

        if print_comment and self.get_comment():
            res += ' ' + self.comment('{{{ ' + self.get_comment())

        return res + '\n'

    def _emit_post(self, lvl, print_comment):
        if lvl < 0:
            return ""

        res  = self.indent_line(lvl)
        res += '};'

        if print_comment and self.get_comment():
            res += ' ' + self.comment('}}} ' + self.get_comment())

        return res + '\n'
