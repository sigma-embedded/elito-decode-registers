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
    class _Int(_CodeObject):
        def __init__(self, v, comment, width, is_signed, fmt):
            _CodeObject.__init__(self, comment)

            if fmt == None:
                fmt = "%d"

            self.__v = v
            self.__width = width
            self.__is_signed = is_signed

            if isinstance(v, generator.Symbol):
                self.__repr = v.get_name()
            else:
                self.__repr = fmt % v

            assert(isinstance(self.__repr, str))

        def emit(self, lvl = 0, print_comment = True):
            res  = self.indent_line(lvl)
            res += ('push_%s%u(' % (['u', 's'][self.__is_signed],
                                    self.__width) +
                    self.__repr +
                    ');')

            res  = self._append_comment(res, print_comment)

            return res + '\n'

    class _String(_CodeObject):
        def __init__(self, s, comment):
            _CodeObject.__init__(self, comment)

            if isinstance(s, generator.Symbol):
                assert(isinstance(s.get_value(), str))

                self.__repr = s.get_name()
                self.__len  = "(sizeof %s) - 1" % s.get_name()
            else:
                self.__repr = self.quote(s)
                self.__len  = "%2u" % len(s)

            assert(isinstance(self.__repr, str))
            assert(isinstance(self.__len, str))

        def emit(self, lvl = 0, print_comment = True):
            res  = self.indent_line(lvl)
            res += 'push_data16(%s, %s);' % (self.__len, self.__repr)
            res  = self._append_comment(res, print_comment)

            return res + '\n'

    def _add_int(self, v, comment, width, is_signed, fmt = None):
        return CodeFactory._Int(v, comment, width, is_signed, fmt)

    def add_string(self, s, comment):
        return CodeFactory._String(s, comment)

    def add_comment(self, comment):
        return C._Comment(comment)

    def add_symbol(self, symbol):
        return None

    def create_block(self, parent, comment):
        return C._Block(parent, comment)


if __name__ == '__main__':
    cfac = CodeFactory()
    generator._test(cfac)
