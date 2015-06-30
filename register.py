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

class Register:
    def __init__(self, name, unit, is_valid):
        self.__is_complete = False
        self.__id = name
        self.__name = name
        self.__is_valid = is_valid
        self.__unit = unit

    @staticmethod
    def parse(l, indent, unit, prev_reg, enabled):
        if not l:
            return prev_reg

        tag = l[0]
    
        if not tag.startswith('@'):
            raise Exception("Invalid line '%s'" % (l))
        
        if tag == '@register':
            if prev_reg:
                prev_reg.finish()
                prev_reg = None

            assert(len(l) == 2)            
            reg = Register(l[1], unit, enabled)
        else:
            assert(prev_reg)
            reg = prev_reg

        block = reg.block

        #print(l)
        if tag == '@register':
            pass                # handled above
        elif reg.__block and reg.__block.parse(l, indent, enabled)
        elif not enabled:
            pass                # ignore disabled directives
            
