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

import os
import glob
import fileinput
import functools

import block
import generator

class Top(block.Top):
    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@unit' : 1,
            }

            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = None

            if tag == '@unit':
                res = Unit(self.o, l[1], enabled)
                self.o.add_child(res)
            else:
                assert(False)

            return res

    def __init__(self):
        block.Top.__init__(self)
        self.add_parser(Top.__Parser(self))

    def get_units(self):
        return self.filter(lambda f: isinstance(f, Unit))

class Unit(block.Block, block.Mergeable):
    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@registers' : 1,
                '@disabled' : 0,
                '@name' : 1,
                '@reg' : 2,
            }

            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = self.o

            if not enabled:
                pass                # ignore disabled directives
            elif tag == '@registers':
                tmp = l[1]
                if tmp.endswith('/'):
                    tmp += '*.reg'
                self.o._assign_regglobs(tmp)
            elif tag == '@disabled':
                self.o._assign_enabled(False)
            elif tag == '@name':
                self.o._assign_name(l[1])
            elif tag == '@reg':
                self.o._assign_memory(int(l[1], 0), int(l[2], 0))
            else:
                assert(False)

            return res

    def __init__(self, top, name, is_valid):
        assert(isinstance(top, Top))

        block.Block.__init__(self, top, is_valid)
        block.Mergeable.__init__(self)

        self.add_parser(Unit.__Parser(self))

        self.__id = name

        self.__regglobs = []

        self.__registers = None

        self.__name = None
        self.__directory = None
        self.__is_enabled = True
        self.__memory = None

    @staticmethod
    def cmp_by_addr(self, b):
        return self.__memory[0] - b.__memory[0]

    def _assign_regglobs(self, regs):
        self.__regglobs.append(regs)

    def _assign_enabled(self, ena):
        self.__is_enabled = ena

    def _assign_name(self, name):
        self.__name = name

    def _assign_memory(self, addr, len):
        self.__memory = [addr, len]

    def get_id(self):
        return self.__id

    def get_name(self):
        if not self.__name:
            return self.__id
        else:
            return self.__name

    def is_enabled(self):
        return self.__is_enabled

    def read_registers(self, directory, defines):
        import register

        regs = []
        reg_files = []
        for d in self.__regglobs:
            p = os.path.join(directory, d)
            assert(os.path.isdir(os.path.dirname(p)))
            reg_files.extend(glob.glob(p))

        # TODO: warn about empty reg_files

        #print(reg_files)
        top = register.Top(self)

        top.iterate_files(reg_files, defines)

        regs = block.Mergeable.create_container(top.get_registers(),
                                                lambda x: x.get_id(False))

        for r in regs.values():
            r.merge(regs)

        self.__registers = regs

    def _merge_pre(self):
        assert(self.__registers != None)

        pass

    def _merge(self, base):
        assert(isinstance(base, Unit))
        #print("Unit: merging %s into %s" % (base.get_id(), self.get_id()))

        for r in base.__registers.values():
            id = r.get_id(False)
            if id in self.__registers:
                self.__registers[id].update(r)
            else:
                self.__registers[id] = r.clone(self)

            #print("  ", id, self.__registers[id])

        #print(self, base)
        pass

    def generate_code(self, top):
        if not self.__is_enabled:
            return None

        code = top.create_block('%s unit' % self.__id)

        code.add_u32(self.__memory[0], "memory start address", "0x%08x")
        code.add_u32(self.__memory[0] + self.__memory[1] - 1,
                     "memory end address", "0x%08x")
        code.add_string(self.get_id(),   "Unit id")
        code.add_string(self.get_name(), "Unit name")

        regs = list(filter(lambda x: not x.is_template(),
                           self.__registers.values()))

        regs.sort(key = functools.cmp_to_key(lambda a, b: a.cmp_by_addr(a, b)))

        code.add_size_t(len(regs), "number of registers")
        block0 = code.create_block("registers")

        for r in regs:
            r.generate_code(block0)

        return code
