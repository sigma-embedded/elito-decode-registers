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

from line import Line

class Unit:
    def __init__(self, name, is_valid):
        self.__is_complete = False
        self.__id = name
        self.__name = name
        self.__is_valid = is_valid

        self.__directory = None
        self.__is_enabled = True
        self.__memory = None
        self.__regdirs = []

    def is_complete(self):
        return self.__is_complete

    def finish(self):
        print("finish: %s" % (self.__name))
        self.__is_complete = True

    def is_valid(self):
        return self.__is_valid

    def read_registers(self, directory, defines):
        regs = []
        reg_files = []
        for d in self.__regdirs:
            p = os.path.join(directory, d)
            assert(os.path.isdir(p))
            reg_files.extend(glob.glob(os.path.join(p, "*.reg")))

        # TODO: warn about empty reg_files
        if not reg_files:
            return

        l = None
        with fileinput.FileInput(files = reg_files) as input:
            for txt in input:
                l = Line.parse(txt, l)
                if l.is_complete():
                    info = l.expand(defines)
                    print(info)
                    l    = None

        pass
        
    @staticmethod
    def parse(l, prev_unit, enabled):
        if not l:
            return prev_unit

        tag = l[0]
    
        if not tag.startswith('@'):
            raise Exception("Invalid line '%s'" % (l))

        if tag == '@unit':
            if prev_unit:
                prev_unit.finish()
                prev_unit = None

            assert(len(l) == 2)            
            unit = Unit(l[1], enabled)
        else:
            assert(prev_unit)
            unit = prev_unit

        #print(l)
        if tag == '@unit':
            pass                # handled above
        elif not enabled:
            pass                # ignore disabled directives
        elif tag == '@directory':
            assert(len(l) == 2)
            unit.__regdirs.append(l[1])
        elif tag == '@disabled':
            assert(len(l) == 1)
            unit.__is_enabled = False
        elif tag == '@name':
            assert(len(l) == 2)
            unit.__name = l[1]
        elif tag == '@reg':
            assert(len(l) == 3)
            unit.__memory = [int(l[1], 0), int(l[2], 0)]
        else:
            print("WARNING: unsupported '%s' line" % (l,))
            
        return unit

        
