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
import struct

class Endianess:
    def __init__(self, fmt):
        self.fmt = fmt

LITTLE_ENDIAN	= Endianess('<')
BIG_ENDIAN	= Endianess('>')

def _create_int_fmt(width, is_signed, endian):
    assert(isinstance(endian, Endianess))

    fmt = endian.fmt

    if width == 8:
        fmt += ['B', 'b'][is_signed]
    elif width == 16:
        fmt += ['H', 'h'][is_signed]
    elif width == 32:
        fmt += ['I', 'i'][is_signed]
    elif width == 64:
        fmt += ['Q', 'q'][is_signed]
    else:
        raise Exception("Unsupported with %d" % width)

    return fmt

class CodeFactory(generator.CodeFactory):
    class _Int(generator.CodeObject):
        def __init__(self, v, width, is_signed, endian):
            generator.CodeObject.__init__(self, None)

            if isinstance(v, generator.Symbol):
                v = v.get_value()

            self.__fmt = _create_int_fmt(width, is_signed, endian)
            self.__v   = v

        def emit(self, lvl = 0, print_comment = True):
            return struct.pack(self.__fmt, self.__v)

    class _String(generator.CodeObject):
        def __init__(self, v, endian):
            generator.CodeObject.__init__(self, None)

            if isinstance(v, generator.Symbol):
                v = v.get_value()

            self.__fmt = endian.fmt + 'H'
            self.__v   = v

        def emit(self, lvl = 0, print_comment = True):
            return (struct.pack(self.__fmt, len(self.__v)) +
                    self.__v.encode('ascii'))

    class _Comment(generator.CodeObject):
        def __init__(self):
            generator.CodeObject.__init__(self, None)

        def emit(self, lvl = 0, print_comment = True):
            return b''

    def __init__(self, endian):
        assert(isinstance(endian, Endianess))
        self.__endian = endian

    def _add_int(self, v, comment, width, is_signed, fmt = None):
        return CodeFactory._Int(v, width, is_signed, self.__endian)

    def add_string(self, s, comment):
        return CodeFactory._String(s, self.__endian)

    def add_comment(self, comment):
        return CodeFactory._Comment()

    def add_symbol(self, symbol):
        return None

    def create_block(self, parent, comment):
        return None

if __name__ == '__main__':
    generator._test(CodeFactory(LITTLE_ENDIAN))
    generator._test(CodeFactory(BIG_ENDIAN))
