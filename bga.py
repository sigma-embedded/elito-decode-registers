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

import block
import pin

class Top(block.Top):
    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@bga' : 1,
            }

            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = self.o

            if not enabled:
                pass
            elif tag == '@bga':
                res = BGA(self.o, l[1])
                self.o.add_child(res)
            else:
                assert(False)

            return res

    def __init__(self):
        block.Top.__init__(self)
        self.add_parser(Top.__Parser(self))

    def __repr__(self):
        return "%s" % self.children

    def read_pins(self, directory, defines):
        for b in self.children:
            assert(isinstance(b, BGA))
            b.read_pins(directory, defines)

    def merge(self):
        for b in self.children:
            assert(isinstance(b, BGA))
            b.merge()

    def find(self, id):
        # for now; we support only a single BGA
        assert(len(self.children) == 1)
        return self.children[0].find(id)

    def generate_code(self, top):
        for b in self.children:
            assert(isinstance(b, BGA))

            b.generate_code_pins(top);
            b.generate_code(top)

class BGA(block.Top):
    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@pins' : 1,
                '@rows' : 1,
                '@cols' : 1,
                "@skiprows" : [1, -1],
                "@skipcols" : [1, -1],
            }

            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = self.o

            if not enabled:
                pass
            elif tag == '@pins':
                self.o._add_pins(l[1])
            elif tag == '@rows':
                self.o._set_rows(l[1])
            elif tag == '@cols':
                self.o._set_cols(l[1])
            elif tag == "@skiprows":
                self.o._add_skiprows(l[1:])
            elif tag == "@skipcols":
                self.o._add_skipcols(l[1:])
            else:
                assert(False)

            return res

    def __init__(self, top, id):
        block.Top.__init__(self)
        self.add_parser(BGA.__Parser(self))
        self.__id       = id
        self.__pinglobs = []
        self.__pintop   = None
        self.__pins     = None
        self.__skiprows = set()
        self.__skipcols = set()
        self.__cols = None
        self.__rows = None

    def __repr__(self):
        return "%s: pins=%s" % (self.__id, self.__pins)

    def find(self, id):
        res = list(filter(lambda x: x.get_id() == id, self.__pins))

        if not res:
            return None

        assert(len(res) == 1)
        return res[0]

    def _add_pins(self, p):
        if p.endswith('/'):
            p += '*.pin'
        self.__pinglobs.append(p)

    def _add_skipcols(self, cols):
        self.__skipcols.update(cols)

    def _add_skiprows(self, rows):
        self.__skiprows.update(rows)

    def _set_rows(self, rows):
        self.__rows = rows

    def _set_cols(self, cols):
        self.__cols = cols

    def read_pins(self, directory, defines):
        import os, glob

        pin_files = []
        for d in self.__pinglobs:
            p = os.path.join(directory, d)
            assert(os.path.isdir(os.path.dirname(p)))
            pin_files.extend(glob.glob(p))

        pintop = pin.Top(self)
        pintop.iterate_files(pin_files, defines)

        self.__pintop = pintop

    def __int_to_row(self, v):
        if v < 0 or v > 999:
            raise Exception("row #%d out of range" % v)

        for p0 in " ABCDEFGHIJKLMNOPQRSTUVWXYZ"[:]:
            for p1 in " ABCDEFGHIJKLMNOPQRSTUVWXYZ"[:]:
                for p2 in "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[:]:
                    x = ("%s%s%s" % (p0, p1, p2)).strip()
                    if x in self.__skiprows:
                        continue

                    if v == 0:
                        return x

                    v -= 1

        raise Exception("failed to convert row")

    def translate_ball(self, ball):
        row = ""
        col = ""
        i   = 0

        while ball[i].isalpha():
            row += ball[i]
            i   += 1

        col = int(ball[i:])

        if col <= 0 or col in self.__skipcols:
            raise Exception("bad col in %s" %  ball)

        col = col - 1

        tmp = 0
        while self.__int_to_row(tmp) != row:
            tmp += 1

        return (col, tmp)

    def get_id(self):
        return self.__id

    def merge(self):
        self.__pintop.merge()
        self.__pins = list(self.__pintop.filter(lambda x: not x.is_template()))

    def generate_code(self, top):
        import generator

        block = top.create_struct(top, 'bga', "bga_%s" % self.get_code_id())

        block.add_attribute_simple('id', self.__id)
        block.add_attribute_simple('num_cols', int(self.__cols))
        block.add_attribute_simple('num_rows', int(self.__rows))
        block.add_attribute_simple('pins',
                                   generator.Symbol('pins_%s' % self.get_code_id(),
                                                    None, 'pins'))

    def generate_code_pins(self, block):
        for p in self.__pins:
            p.generate_code_pre(block)

        pin_code = block.create_array(block, 'struct bga_pins',
                                      'pins_%s' % self.get_code_id(),
                                      '%u * %u' % (int(self.__cols),
                                                   int(self.__rows)))

        for p in self.__pins:
            tmp = pin_code.add_element_block(pin_code,
                                             p.get_index(self.get_pin_idx),
                                             p.get_id())

            p.generate_code(tmp)

    def get_code_id(self):
        return self.get_id().lower()

    def get_pin_idx(self, col, row):
        return '%u*%u + %u' % (row, int(self.__cols), col)
