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

class CodeFactory(generator.CodeFactory):
    def __init__(self, allow_block = False, allow_multi = False):
        generator.CodeFactory.__init__(self)
        self.__allow_block = allow_block
        self.__allow_multi = allow_multi

        self.__symbols = {}

    def _add_int(self, v, comment, width, is_signed, fmt = None):
        return None

    def add_string(self, v, comment):
        return None

    def add_comment(self, comment):
        return None

    def add_array(self, a, comment, add_fn, sz_width = 8):
        return None

    def add_symbol(self, symbol):
        name  = symbol.get_name()
        value = symbol.get_value()

        if not self.__allow_multi:
            old = self.__symbols.get(name)
            if old == None:
                self.__symbols[name] = value
            elif old != value:
                raise Exception("different values ('%s' vs. '%s') for symbol '%s'"
                                % (old, value, name))
            else:
                return None

        return C._Symbol(symbol)

    def create_block(self, parent, comment):
        if self.__allow_block or parent == None:
            return C._Block(parent, comment)
        else:
            return None

if __name__ == '__main__':
    cfac = CodeFactory()
    generator._test(cfac)
