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

class Block:
    def __init__(self, parent):
        self.__i = 0
        self.__parent = parent

    def parse(self, l, indent, enabled):
        res = self._parse(l, indent, enabled)
        if res:
            pass
        elif self.__parent:
            res = self.__parent._parse(l, indent, enabled)
        else:
            res = None

        assert(res == None || isinstance(res, Block))

        return res
            
        
    
