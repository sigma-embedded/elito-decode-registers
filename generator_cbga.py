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
import generator_ccommon as C

class _CodeObject(C._CodeObject):
    def __init__(self, comment):
        C._CodeObject.__init__(self, comment)

class CodeFactory(generator.CodeFactory):
    def _add_int(self, v, comment, width, is_signed, fmt = None):
        pass

    def add_symbol(self, symbol):
        pass

    def add_string(self, v, comment):
        pass

    def add_comment(self, comment):
        pass

    def create_block(self, parent, comment):
        return C._Block(parent, comment)

    def create_struct(self, parent, type, id):
        return C._Struct(parent, 'struct ' + type, id)

    def create_array(self, parent, type, id, dim):
        return C._Array(parent, type, id, dim)

    def add_array_forward(self, type, id, dim):
        return C._Forward('static %s const' % type,'%s[%s]' % (id, dim))

    def add_forward(self, type, id):
        return C._Forward('static %s const' % type,'%s' % (id))

    def add_comment(self, comment):
        return C._Comment(comment)

    def add_attribute_flags(self, name, flags):
        if flags:
            tmp = C._StructDesignator(name, '|'.join(flags), None, fmt='(%s)')
        else:
            tmp = C._StructDesignator(name, 0, None)

        return C._StructAttributeSimple(tmp)

    def add_attribute_symbol(self, symbol):
        tmp = C._StructDesignator.from_symbol(symbol)
        return C._StructAttributeSimple(tmp)

    def add_attribute_simple(self, name, val, comment = None, fmt=None):
        tmp = C._StructDesignator(name, val, comment, fmt)
        return C._StructAttributeSimple(tmp)

    def add_attribute_block(self, parent, id, comment = None):
        return C._StructAttributeBlock(parent, id, comment)

    def add_element_block(self, parent, idx, comment = None):
        return C._ArrayElementBlock(parent, idx, comment)
