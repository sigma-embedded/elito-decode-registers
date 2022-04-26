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

    def __init__(self, bga):
        block.Top.__init__(self)
        self.add_parser(Top.__Parser(self))
        self.bga = bga

    def get_units(self):
        return self.filter(lambda f: isinstance(f, Unit))

class Unit(block.Block, block.Mergeable):
    ENDIAN_NATIVE	= 0
    ENDIAN_LITTLE	= 1
    ENDIAN_BIG		= 2

    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@registers' : 1,
                '@disabled' : 0,
                '@name' : 1,
                '@description' : 1,
                '@reg' : 2,
                '@regwidth' : [1, 4],
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
            elif tag == '@description':
                pass
            elif tag == '@regwidth':
                self.o._assign_regwidth(int(l[1]))
                if len(l) > 2:
                    self.o._assign_addrwidth(int(l[2]))
                if len(l) > 3:
                    self.o._assign_endian(l[3], l[4] if len(l) > 4 else None)
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
        self.__regwidth = None
        self.__addrwidth = None
        self.__endian_addr = None
        self.__endian_data = None
        self.bga = top.bga

    def __str__(self):
        if self.__memory is None:
            mem = "%s" % None
        else:
            mem = "0x%08x+0x%08x" % (self.__memory[0], self.__memory[1])

        return "UNIT <%x> %s@" % (id(self), self.__name) + mem + \
            " (%s, %s)" % (self.__regwidth, self.__addrwidth)

    @staticmethod
    def cmp_by_addr(self, b):
        return self.__memory[0] - b.__memory[0]

    def _assign_regglobs(self, regs):
        self.__regglobs.append(regs)

    def _assign_enabled(self, ena):
        self.__is_enabled = ena

    def _assign_name(self, name):
        self.__name = name

    def _assign_regwidth(self, width):
        self.__regwidth = width

    def _assign_memory(self, addr, len):
        self.__memory = [addr, len]

    def _assign_addrwidth(self, width):
        if width > 64:
            raise Exception("address width too large")
        self.__addrwidth = width

    def _assign_endian(self, endian_addr, endian_data):
        self.__endian_addr = {
            'LE' : Unit.ENDIAN_LITTLE,
            'BE' : Unit.ENDIAN_BIG
        }[endian_addr]

        self.__endian_data = {
            'LE' : Unit.ENDIAN_LITTLE,
            'BE' : Unit.ENDIAN_BIG,
            None : self.__endian_addr,
        }[endian_data]

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

        self.__registers = regs

    def _finalize(self):
        for r in self.__registers.values():
            r.finalize()

    def _merge_pre(self):
        assert(self.__registers != None)
        assert(not self.is_finalized())

        for r in self.__registers.values():
            r.merge(self.__registers)

    def _merge(self, base):
        assert(isinstance(base, Unit))
        assert(not self.is_finalized())
        assert(base.is_finalized())

        for r in base.__registers.values():
            id = r.get_id(False)
            if id in self.__registers:
                self.__registers[id].update(r)
            else:
                self.__registers[id] = r.clone(self)

            #print("  ", id, self.__registers[id])

        #print(self, base)
        pass

    def _merge_post(self):
        self.finalize()

    @staticmethod
    def __endian_symbol_part(end):
        if end == Unit.ENDIAN_NATIVE:
            return 'NATIVE';
        elif end == Unit.ENDIAN_LITTLE:
            return 'LITTLE';
        elif end == Unit.ENDIAN_BIG:
            return 'BIG';
        else:
            raise Exception("INTERNAL ERROR: unsupported endian %s" % end)

    @staticmethod
    def __endian_symbol(end_addr, end_data):
        end_addr_part = Unit.__endian_symbol_part(end_addr);
        end_data_part = Unit.__endian_symbol_part(end_data);

        return generator.Symbol('UNIT_ENDIAN_%s_%s' % (end_addr_part, end_data_part),
                                (end_addr << 4) | (end_data << 0), "endianess")

    def generate_code(self, top):
        if not self.__is_enabled:
            return None

        code = top.create_block('%s unit' % self.__id)

        end_sym = Unit.__endian_symbol(self.get_endian_addr(),
                                       self.get_endian_data())

        code.add_symbol(end_sym)

        code.add_u32(self.__memory[0], "memory start address", "0x%08x")
        code.add_u32(self.__memory[0] + self.__memory[1] - 1,
                     "memory end address", "0x%08x")
        code.add_string(self.get_id(),   "Unit id")
        code.add_string(self.get_name(), "Unit name")

        regs = list(filter(lambda x: not x.is_template(),
                           self.__registers.values()))

        regs.sort(key = functools.cmp_to_key(lambda a, b: a.cmp_by_addr(a, b)))

        code.add_u8(self.get_addrwidth(), "addr width", "%u")
        code.add_type(end_sym, "endianess")
        code.add_size_t(len(regs), "number of registers")
        block0 = code.create_block("registers")

        for r in regs:
            r.generate_code(block0)

        return code

    def find_pin(self, id):
        return self.bga.find(id)

    def have_bga(self):
        return self.bga is not None

    def get_address(self):
        return self.__memory[0]

    def get_regwidth(self):
        return self.__regwidth or 32

    def get_addrwidth(self):
        return self.__addrwidth or 0

    def get_endian_data(self):
        return self.__endian_data or Unit.ENDIAN_NATIVE

    def get_endian_addr(self):
        return self.__endian_addr or Unit.ENDIAN_NATIVE
