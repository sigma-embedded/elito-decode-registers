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
from generator import Symbol

class Top(block.Top):
    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@pin' : 1,
            }

            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = self.o

            if not enabled:
                pass
            elif tag == '@pin':
                res = Pin(self.o, l[1])
                self.o.add_child(res)
            else:
                assert(False)

            return res

    def __init__(self, bga):
        block.Top.__init__(self)
        self.add_parser(Top.__Parser(self))
        self.bga = bga

    def merge(self):
        tmp = block.Mergeable.create_container(self.children,
                                               lambda x: x.get_id())
        for p in tmp.values():
            p.merge(tmp)

class Pin(block.Block, block.Mergeable):
    class BallInfo:
        def __init__(self, ball, col, row):
            self.ball = ball
            self.col  = col
            self.row  = row

        def __repr__(self):
            return '%s(%u,%u)' % (self.ball, self.col, self.row)

    class PowerInfo:
        def __init__(self, domain, is_consumer):
            self.domain = domain
            self.is_consumer = is_consumer

        def __repr__(self):
            if self.is_consumer:
                return '*%s' % self.domain
            else:
                return self.domain

        def identifier(self):
            return 'power_domain_' + self.domain.lower()

        def pointer(self):
            import generator
            return generator.Symbol('&%s' % self.identifier(), None, None);

    class AfInfo:
        def __init__(self, s):
            tmp = s.split('|')
            info = []
            for t in tmp:
                info.append(t.split('@'))

            assert(len(info) in [1,2])
            assert(len(info[0]) in [1,2])
            assert(len(info) == 1 or len(info[1]) in [1,2])

            self.__unit = info[0][0]
            if len(info[0]) == 2:
                self.__unit_idx = int(info[0][1]) - 1
            else:
                self.__unit_idx = -1


            self.__fn_flags = []
            if len(info) < 2:
                self.__fn     = None
                self.__fn_idx = -1
            else:
                tmp = info[1][0]
                if len(info[1]) == 2:
                    self.__fn_idx = int(info[1][1])
                    self.__fn_flags.append('has_index')
                else:
                    self.__fn_idx = -1

                if tmp.startswith('~'):
                    self.__fn_flags.append('single')
                    tmp = tmp[1:]

                if tmp.startswith('!'):
                    self.__fn_flags.append('required')
                    tmp = tmp[1:]

                self.__fn = tmp

        def __repr__(self):
            return '%s@%d|%s@%d' % (self.__unit, self.__unit_idx,
                                    self.__fn, self.__fn_idx)

        def get_fn_identifier(self):
            if self.__fn:
                return "MUX_FN_%s_%s" % (self.__unit.upper(),
                                         self.__fn.upper())

        def get_fn(self):
            return self.__fn

        def get_fn_idx(self):
            return self.__fn_idx

        def get_flag_identifiers(self):
            return map(lambda x: 'MUX_FNFLAG_%s' % x.upper(), self.__fn_flags)

        def get_index(self):
            return self.__fn_idx

        def get_unit_identifier(self):
            return "cpu_unit_%s" % self.__unit.lower()

        def has_multiple_units(self):
            return self.__unit_idx != -1

        def get_unit_ptr(self):
            import generator

            t = self.get_unit_identifier()
            if self.has_multiple_units():
                t += '[%u]' % (self.__unit_idx)

            return generator.Symbol('&(%s)' % t, None, None);


    class __Parser(block.Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@template' :  0,
                '@name' :      1,
                '@power' :     1,
                '@reset,af' :  1,
                '@reset,dir' : 1,
                '@reset,val' : 1,
                '@pad':        [1,3]

            }
            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = self.o

            assert(not self.o.is_merged())

            if not enabled:
                pass                # ignore disabled directives
            elif tag == '@template':
                assert(len(l) == 1)
                self.o._set_template(True)
            elif tag == '@pad':
                self.o._set_pad(l[1])
            elif tag == '@name':
                self.o._set_title(l[1])
            elif tag.startswith("@reset,"):
                # TODO: ignore for now
                pass
            elif tag == '@power':
                self.o._set_power(l[1])
            else:
                assert(False)

            return res

    def __init__(self, top, name):
        block.Block.__init__(self, top, True)
        block.Mergeable.__init__(self)

        self.add_parser(Pin.__Parser(self))

        self.__mux_field = None
        self.__mux_fn = {}
        self.__id    = name
        self.__title = None
        self.__pad   = None
        self.__is_template = False
        self.__bga = top.bga
        self.__power = None

    def __repr__(self):
        return "%s: %s, %s" % (self.__id, self.__mux_field, self.__mux_fn)

    def add(self, num, fn, field):
        assert(self.__mux_field == None or self.__mux_field == field)
        self.__mux_field = field

        if num not in self.__mux_fn:
            self.__mux_fn[num] = []

        self.__mux_fn[num].append(Pin.AfInfo(fn))

    def _set_template(self, ena):
        self.__is_template = ena

    def _set_pad(self, pad):
        (col, row) = self.__bga.translate_ball(pad)
        self.__pad = Pin.BallInfo(pad, col, row)

        #print("%s => %s" % (pad, self.__pad))

    def _set_title(self, title):
        self.__title = title

    def _set_power(self, power):
        if power.startswith('*'):
            power = power[1:]
            is_consumer = False
        else:
            is_consumer = True

        self.__power = Pin.PowerInfo(power, is_consumer)

    def is_template(self):
        return self.__is_template

    def _merge(self, other):
        assert(isinstance(other, Pin))

        assert(self.__mux_field == None)
        assert(other.__mux_field == None)

        self.__pad         = self.update_attr(self.__pad, other.__pad)
        self.__title       = self.update_attr(self.__title, other.__title)
        self.__power       = self.update_attr(self.__power, other.__power)
        self.__is_template = self.update_attr(self.__is_template,
                                              other.__is_template)

    def get_id(self):
        return self.__id

    def get_title(self):
        if self.__title:
            return self.__title

        return self.__id

    def get_index(self, fn):
        return fn(self.__pad.col, self.__pad.row)

    def generate_code_pre(self, block):
        if self.__power and not block.add_and_test_existing(self.__power.identifier()):
            block.add_forward('struct power_domain', '%s' % self.__power.identifier())

        if self.__mux_field:
            m = block.create_array(block, 'struct pin_mux',
                                   'pin_mux_%s' % self.__id.lower(), '')

            for (idx,fns) in self.__mux_fn.items():
                for f in fns:
                    fn = m.add_element_block(m, None)
                    fn.add_attribute_simple('af', idx),
                    fn.add_attribute_simple('unit', f.get_unit_ptr())

                    tmp = f.get_fn()
                    if tmp:
                        fn.add_attribute_simple('fn', tmp)

                    fn.add_attribute_simple('fn_idx', f.get_fn_idx())
                    fn.add_attribute_flags('fn_flags', f.get_flag_identifiers())

    def generate_code(self, block):
        block.add_attribute_simple("id",    self.__id)
        block.add_attribute_simple("title", self.get_title())
        block.add_attribute_simple("ball",  self.__pad.ball)
        block.add_attribute_simple("row",   self.__pad.row)
        block.add_attribute_simple("col",   self.__pad.col)

        if self.__power:
            p = block.add_attribute_block(block, "power")
            p.add_attribute_simple("domain", self.__power.pointer());
            p.add_attribute_simple("is_supply", not self.__power.is_consumer);

        if self.__mux_field:
            m = block.add_attribute_block(block, "mux")
            m.add_attribute_simple('functions',
                                   Symbol('&pin_mux_%s' % self.__id.lower(),
                                          None, None))
            m.add_attribute_simple('mux_register',
                                   self.__mux_field.get_register().expand_address(),
                                   fmt = "0x%08x")

            m.add_attribute_simple('mux_bitmask',
                                   self.__mux_field.get_bitmask(),
                                   fmt = "0x%08x")
