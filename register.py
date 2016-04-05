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
import sys
import glob
import copy
import functools

import block
import generator

class Top(block.Top):
    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@register' : 1,
            }

            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = None

            if tag == '@register':
                res = Register(self.o, l[1], self.o.get_unit(), enabled)
                self.o.add_child(res)
            else:
                assert(False)

            return res

    def __init__(self, unit):
        block.Top.__init__(self)
        self.add_parser(Top.__Parser(self))

        self.__unit = unit

    def get_registers(self):
        return self.filter(lambda f: isinstance(f, Register))

    def get_unit(self):
        return self.__unit

class Enum(block.Block, block.Removable):
    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@name'    : 1,
                '@seealso' : [1,-1],
            }

            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = self.o

            if not enabled:
                pass                # ignore disabled directives
            elif tag == '@name':
                self.o._assign_name(l[1])
            elif tag == '@seealso':
                pass            # TODO: ignore for now...
            else:
                assert(False)

            return res

    def __init__(self, top, value, is_valid, is_removed = False):
        assert(isinstance(top, Field))

        from unit import Unit
        block.Block.__init__(self, top, is_valid)
        block.Removable.__init__(self, is_removed)

        self.add_parser(Enum.__Parser(self))

        self.__value = value
        self.__name = None
        self.__field = top

    def __str__(self):
        return 'enum(%u){%s, %s}' % (self.__value, self.__name,
                                     self.is_removed())

    def _assign_name(self, name):
        self.__name = name

    def get_value(self):
        return self.__value

    def get_name(self):
        name = self.__name
        if not name:
            name = '%u' % self.__value

        return name

    def clone(self, field = None):
        if field == None:
            field = self.__field

        assert(isinstance(field, Field))

        res = Enum(field, self.__value, self.is_valid(),
                   self.is_removed())
        res.__name = self.__name

        return res

    def move(self, field):
        assert(isinstance(field, Field))
        self.__field = field

    def update(self, other):
        assert(self.__value == other.__value)

        if self.is_removed():
            # when we are removed; skip further processing
            return

        if not self.__name:
            self.__name = other.__name

class Field(block.Block, block.Mergeable, block.Removable):
    TYPE_ENUM		= 1
    TYPE_BOOL		= 2
    TYPE_FRAC		= 3
    TYPE_SINT		= 4
    TYPE_UINT		= 5

    TYPE_RESERVED	= 6

    class BitField:
        def __init__(self, bits = None):
            self.__v = None

            if bits:
                self.set(bits)

        def set(self, bits):
            if self.__v == None:
                self.__v = []

            splitted = []
            for b in bits:
                tmp = b.split(',')
                splitted.extend(tmp)

            for b in splitted:
                tmp = b.split('-')
                if len(tmp) == 2:
                    self.__set_range(int(tmp[0]), int(tmp[1]))
                elif len(tmp) == 1:
                    self.__set_range(int(tmp[0]), int(tmp[0]))
                else:
                    raise Exception("Invalid bitrange '%s'" % b)

        def __set_range(self, a, b):
            if a < b:
                self.__v.extend(range(a, b+1, +1))
            else:
                self.__v.extend(range(a, b-1, -1))

        def __len__(self):
            return len(self.__v)

        def __getitem__(self, idx):
            return self.__v[idx]

        def __iter__(self):
            return self.__v.__iter__()

        def __str__(self):
            return "%s" % self.__v

        def min(self):
            if self.__v:
                return min(self.__v)
            else:
                return -1

        def get_mask(self):
            res = 0
            for b in self.__v:
                assert((res & (1 << b)) == 0)
                res |= (1 << b)

            return res

        def __eq__(self, a):
            if a == None:
                return self.__v == a

            raise Exception("Unsupported comparision with %s" % a)

    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@enum' : [1, 2],
                '@description' : 1,
                '@boolean' : [0, 1],
                '@bits' : [1, -1],
                '@frac' : 2,
                '@integer' : 1,
                '@uint' : 1,
                '@sint' : 1,
                '@signed' : 1,
                '@reserved' : 0,
            }

            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = self.o

            if tag == '@enum':
                self.o._set_type(Field.TYPE_ENUM)
                res = Enum(self.o, int(l[1],0), enabled)
                if len(l) < 3:
                    pass
                elif l[2] == '-':
                    res._set_removed(True)
                else:
                    res._assign_name(l[2])
                self.o.add_child(res)
            elif not enabled:
                pass                # ignore disabled directives
            elif tag == '@description':
                self.o._assign_description(l[1])
            elif tag == '@boolean':
                if len(l) == 2:
                    self.o._set_boolean(l[1])
                else:
                    self.o._set_boolean()
            elif tag == '@frac':
                self.o._set_frac(l[1], l[2])
            elif tag in ['@integer', '@uint']:
                self.o._set_uint(l[1])
            elif tag in ['@signed', '@sint']:
                self.o._set_sint(l[1])
            elif tag == '@reserved':
                self.o._set_type(Field.TYPE_RESERVED)
            elif tag == '@bits':
                self.o._set_bits(l[1:])
            else:
                assert(False)

            return res

    def __init__(self, top, name, is_valid, is_removed = False):
        assert(isinstance(top, Register))

        block.Block.__init__(self, top, is_valid)
        block.Mergeable.__init__(self)
        block.Removable.__init__(self, is_removed)

        self.add_parser(Field.__Parser(self))

        self.__id   = name

        self.__register = top
        self.__type = None
        self.__name = None
        self.__bits = self.BitField()
        self.__desc = None
        self.__frac = None

    def _assign_description(self, desc):
        self.__desc = desc

    def get_id(self):
        return self.__id

    def get_register(self):
        return self.__register

    def get_name(self):
        if self.__name != None:
            return self.__name
        else:
            return self.get_id()

    def get_min_bit(self):
        return self.__bits.min()

    def get_bitmask(self):
        return self.__bits.get_mask()

    @staticmethod
    def cmp_by_bits(self, b):
        return self.get_min_bit() - b.get_min_bit()

    def _set_type(self, type):
        assert(self.__type == type or self.__type == None)
        self.__type = type

    def get_type(self):
        return self.__type

    def is_reserved(self):
        return self.get_type() == Field.TYPE_RESERVED

    def _set_boolean(self, bits = None):
        self._set_type(Field.TYPE_BOOL)
        if bits != None:
            self._set_bits([bits,])

    def __get_frac_part(self, x):
        pass

    def _set_frac(self, int_part, frac_part):
        assert(self.__frac == None)

        self._set_type(Field.TYPE_FRAC)
        self.__frac = [self.BitField([int_part,]),
                       self.BitField([frac_part,])]

    def _set_sint(self, part):
        assert(self.__bits == None)

        self._set_type(Field.TYPE_SINT)
        self.__bits = self.BitField([part,])

    def _set_uint(self, part):
        assert(self.__bits == None)

        self._set_type(Field.TYPE_UINT)
        self.__bits = self.BitField([part,])

    def _set_bits(self, bits):
        self.__bits.set(bits)

    def get_enums(self, all = False):
        tmp = self.filter(lambda x: isinstance(x, Enum) and
                          (all or not x.is_removed()))
        return block.Mergeable.create_container(tmp, lambda x: x.get_value())

    def __update_enums(self, other):
        self_enums  = self.get_enums(True)
        other_enums = other.get_enums()

        for (v,e) in other_enums.items():
            tmp = self_enums.get(v)
            if not tmp:
                self.add_child(e.clone(self))
            else:
                tmp.update(e)

    def update(self, other):
        assert(isinstance(other, Field))

        self.__type = self.update_attr(self.__type, other.__type)
        self.__bits = self.update_attr(self.__bits, other.__bits)
        self.__frac = self.update_attr(self.__frac, other.__frac)
        self.__name = self.update_attr(self.__name, other.__name)
        self.__desc = self.update_attr(self.__desc, other.__desc)

        if self.__type == None:
            pass
        elif self.__type == self.TYPE_ENUM:
            self.__update_enums(other)
        elif self.__type == self.TYPE_BOOL:
            pass
        elif self.__type == self.TYPE_FRAC:
            pass
        elif self.__type == self.TYPE_SINT:
            pass
        elif self.__type == self.TYPE_UINT:
            pass
        elif self.__type == self.TYPE_RESERVED:
            pass
        else:
            raise Exception("Unhandled type %d" % (self.__type))

    def __convert_bits(self, v):
        o_v = v
        idx = 0
        tmp = 0
        msk = 0
        for b in self.__bits:
            assert((msk & (1 << b)) == 0)

            tmp |= (v & 1) << b
            msk |= (1 << b)
            idx += 1
            v  >>= 1

        assert(v == 0)

        res = 0
        while msk != 0:
            if msk & 1:
                res <<= 1
                res  |= tmp & 1
            else:
                assert((tmp & 1) == 0)

            msk >>= 1
            tmp >>= 1

        return res

    def __generate_code_enum(self, top):
        assert(self.__type == self.TYPE_ENUM)

        symbol = generator.Symbol("TYPE_ENUM", Field.TYPE_ENUM, "'enum' type")
        top.add_symbol(symbol)
        top.add_type(symbol, None)

        code   = top.create_block('enum')
        code.add_x32(self.__bits.get_mask(), "bitmask (%s)" % self.__bits)

        if len(self.__bits) <= 8:
            m = code.add_u8
        elif len(self.__bits) <= 16:
            m = code.add_u16
        elif len(self.__bits) <= 32:
            m = code.add_u32
        else:
            raise Exception("too much bits in enum")

        enums = list(self.get_enums().values())

        m(len(enums), "number of enums")

        for e in enums:
            v = self.__convert_bits(e.get_value())
            m(v,   "enum value")
            code.add_string(e.get_name(), "name")

        return code

    def __generate_code_bool(self, code):
        assert(self.__type == self.TYPE_BOOL)
        assert(len(self.__bits) == 1)

        symbol = generator.Symbol("TYPE_BOOL", Field.TYPE_BOOL, "'bool' type")

        code.add_symbol(symbol)
        code.add_type(symbol, None)
        code.add_u8(self.__bits[0], "bit#")

        return code

    def __generate_code_frac(self, top):
        assert(self.__type == self.TYPE_FRAC)

        symbol = generator.Symbol("TYPE_FRAC", Field.TYPE_FRAC, "'frac' type")

        top.add_symbol(symbol)
        top.add_type(symbol, None)
        top.add_x32(self.__frac[0].get_mask(),
                    "integer part (%s)" % self.__frac[0])
        top.add_x32(self.__frac[1].get_mask(),
                    "frac part (%s)" % self.__frac[1])

        return top

    def __generate_code_sint(self, top):
        assert(self.__type == self.TYPE_SINT)

        symbol = generator.Symbol("TYPE_SINT", Field.TYPE_SINT, "'signed int' type")

        top.add_symbol(symbol)
        top.add_type(symbol, None)
        top.add_x32(self.__bits.get_mask(), "sint (%s)" % self.__bits)

        return top

    def __generate_code_uint(self, top):
        assert(self.__type == self.TYPE_UINT)

        symbol = generator.Symbol("TYPE_UINT", Field.TYPE_UINT, "'signed int' type")

        top.add_symbol(symbol)
        top.add_type(symbol, None)
        top.add_x32(self.__bits.get_mask(), "uint (%s)" % self.__bits)

        return top

    @staticmethod
    def generate_code_reserved(top, msk):
        symbol = generator.Symbol("TYPE_RESERVED", Field.TYPE_RESERVED,
                                  "'reserved' type")

        code = top.create_block('reserved bits')

        code.add_string('reserved', "id")
        code.add_string('reserved', "name")

        code.add_symbol(symbol)
        code.add_type(symbol, None)
        code.add_u32(msk, "bitmask")

    def generate_code(self, top):
        assert(not self.is_removed())
        assert(self.__type != None)
        assert(self.__type != self.TYPE_RESERVED)

        code = top.create_block('%s field' % self.__id)

        code.add_string(self.get_id(), "id")
        code.add_string(self.get_name(), "name")

        if self.__type == self.TYPE_ENUM:
            self.__generate_code_enum(code)
        elif self.__type == self.TYPE_BOOL:
            self.__generate_code_bool(code)
        elif self.__type == self.TYPE_FRAC:
            self.__generate_code_frac(code)
        elif self.__type == self.TYPE_SINT:
            self.__generate_code_sint(code)
        elif self.__type == self.TYPE_UINT:
            self.__generate_code_uint(code)
        else:
            raise Exception("Unhandled type %d" % (self.__type))

        return code


    def _merge(self, base):
        assert(False)
        # TODO


class Register(block.Block, block.Mergeable):
    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@field' : 1,
                '@template' : 0,
                '@addr' : [1, 2],
                "@pin,pad" : 1,
                "@pin,af" : [0, -1],
                "@pin,affield" : 1,
            }

            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = self.o

            assert(not self.o.is_merged())

            #print(l)
            if tag == '@field':
                assert(len(l) == 2)
                res = Field(self.o, l[1], enabled)
                self.o.add_child(res)
            elif not enabled:
                pass                # ignore disabled directives
            elif tag == '@template':
                assert(len(l) == 1)
                self.o._set_template(True)
            elif tag == '@addr':
                if len(l) == 2:
                    self.o._set_address(int(l[1],0), 32)
                elif len(l) == 3:
                    self.o._set_address(int(l[1],0), int(l[2]))
                else:
                    raise Exception("Invalid address '%s'" % l)
            elif tag.startswith("@pin,"):
                print(l, file=sys.stderr)
                pass
            else:
                res = None

            return res

    def __init__(self, top, name, unit, is_valid):
        from unit import Unit

        assert(isinstance(top, Top))
        assert(isinstance(unit, Unit))

        block.Block.__init__(self, top, is_valid)
        block.Mergeable.__init__(self)

        self.add_parser(Register.__Parser(self))

        self.__id = name
        self.__unit = unit
        self.__top = top

        self.__is_template = False
        self.__name = None
        self.__offs = None
        self.__width = None

        self.__fields = {}

    def __str__(self):
        return "REG %s (%s@%s, %08x/%d)" % (self.__id,
                                            self.__name,
                                            self.__unit.get_id(),
                                            self.__offs,
                                            self.__width)

    def update(self, other):
        assert(isinstance(other, Unit))

        self.__is_template = self.update_attr(self.__is_template,
                                              other.__is_template)

    def clone(self, unit = None):
        from unit import Unit

        if unit == None:
            unit = self.__unit

        assert(isinstance(unit, Unit))

        res = Register(self.__top, self.__id, unit, self.is_valid())

        res.__is_template = self.__is_template
        res.__name        = copy.copy(self.__name)
        res.__offs        = copy.copy(self.__offs)
        res.__width       = copy.copy(self.__width)
        res.__fields      = copy.copy(self.__fields)

        return res

    def _set_template(self, ena):
        self.__is_template = ena

    def is_template(self):
        if self.__offs == None:
            return True
        elif self.__is_template == None:
            return False
        else:
            return self.__is_template

    def expand_address(self):
        return self.__unit.get_address() + self.__offs

    def get_offset(self):
        return self.__offs

    @staticmethod
    def cmp_by_addr(a, b):
        return a.__offs - b.__offs

    def _set_address(self, offs, width):
        assert(self.__offs == None)
        self.__offs = offs
        self.__width = width

    def get_id(self, fq = True):
        if fq:
            return self.__unit.get_id() + '_' + self.__id
        else:
            return self.__id

    def get_name(self):
        if self.__name != None:
            return self.__name
        else:
            return self.get_id(False)

    def _merge(self, reg):
        assert(isinstance(reg, Register))
        #print("Register: merging %s into %s" % (reg.get_id(), self.get_id()))

        for f in reg.get_fields().values():
            id = f.get_id()
            if id in self.__fields:
                self.__fields[id].update(f)
            else:
                self.__fields[id] = f

        self.__is_template = self.update_attr(self.__is_template,
                                              reg.__is_template)
        self.__name  = self.update_attr(self.__name, reg.__name)
        self.__offs  = self.update_attr(self.__offs, reg.__offs)
        self.__width = self.update_attr(self.__width, reg.__width)

    def _merge_pre(self):
        fields = self.filter(lambda f: isinstance(f, Field))
        fields = block.Mergeable.create_container(fields, lambda x: x.get_id())

        for f in fields.values():
            assert(isinstance(f, block.Mergeable))
            f.merge(fields)

        self.__fields = fields

    def get_fields(self):
        return self.__fields

    def generate_code(self, block):
        assert(not self.is_template())

        code = block.create_block('%s register' % self.__id)
        code.add_x32(self.__offs,  "offset")
        code.add_u8(self.__width, "width", "%u")
        code.add_string(self.get_id(), "id")
        code.add_string(self.get_name(), "name")

        all_fields = list(self.__fields.values())

        rfields  = filter(lambda x: x.is_reserved(), all_fields)
        msk = 0
        for f in rfields:
            msk |= f.get_bitmask()

        fields  = list(filter(lambda x: not x.is_reserved(), all_fields))
        fields.sort(key = functools.cmp_to_key(lambda a, b: a.cmp_by_bits(a, b)))

        cnt = len(fields)
        if msk != 0:
            cnt += 1

        code.add_size_t(cnt, "number of fields")
        code0 = code.create_block('%d fields' % cnt)

        for f in fields:
            try:
                f.generate_code(code0)
            except:
                print("%s: failed to generate code for '%s'"
                      % (self.__id, f.get_id(),), file = sys.stderr)
                raise

        if msk != 0:
            Field.generate_code_reserved(code0, msk)

        return code
