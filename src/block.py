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

import abc
import copy
import sys
import os.path

class Preprocessor:
    class _SubprocessWrapper:
        def __init__(self, cmdline):
            import subprocess, io

            self.__proc = subprocess.Popen(cmdline,
                                           stdout = subprocess.PIPE)
            self.__stdout = io.TextIOWrapper(self.__proc.stdout)

        def __exit__(self, exc_type, exc_value, traceback):
            self.__stdout.close()
            ret = self.__proc.wait()
            if ret != 0:
                raise Exception("subprocess failed with %d" % ret)

        def __enter__(self):
            return self.__stdout

    def __init__(self, name):
        self.__name = name

class Preprocessor_m4(Preprocessor):
    def __init__(self):
        Preprocessor.__init__(self, "m4")

    def call(self, file):
        return Preprocessor._SubprocessWrapper(['m4', '-E', '-Q', '-P',
                                                '-I', os.path.dirname(file),
                                                file])

class Preprocessor_plain(Preprocessor):
    def __init__(self):
        Preprocessor.__init__(self, "plain")

    def call(self, file):
        return open(file)

_preprocessors = {
    'm4' :    Preprocessor_m4(),
    'plain' : Preprocessor_plain()
}

class Parser(metaclass=abc.ABCMeta):
    def __init__(self, obj):
        self.o = obj

    @abc.abstractmethod
    def _parse(self, l, enabled):
        pass

    @abc.abstractmethod
    def _validate(self, l):
        pass

    @staticmethod
    def _validate_ranges(l, ranges):
        r = ranges.get(l[0])
        if r == None:
            return False

        cnt = len(l) - 1

        if isinstance(r, int):
            min = r
            max = r
        else:
            min = r[0]
            max = r[1]

        if ((min != -1 and cnt < min) or
            (max != -1 and cnt > max)):
                raise Exception("Bad number of arguments in '%s'" % (l,))

        return True

class MultiParser(metaclass=abc.ABCMeta):
    def __init__(self):
        self._parsers = []

    def add_parser(self, parser):
        assert(isinstance(parser, Parser))
        self._parsers.append(parser)

    @abc.abstractmethod
    def parse(self, l, enabled):
        pass

class Block(MultiParser, metaclass=abc.ABCMeta):
    def __init__(self, parent, is_valid):
        assert(parent == None or isinstance(parent, Block))

        MultiParser.__init__(self)

        self.__i = 0
        self.__parent = parent
        self.__is_valid = is_valid
        self.__is_finalized = False
        self.children = []

    def is_valid(self):
        return self.__is_valid

    def __read_file(self, input, defines, input_name = None):
        from line import Line

        l     = None
        block = self
        lineno = 0

        if input_name is None:
            input_name = input.name

        for txt in input.readlines():
            lineno = lineno + 1
            l = Line.parse(txt, l)
            if l.is_complete():
                info  = l.expand(defines)

                try:
                    block = block.parse(info[2], info[0])
                except:
                    print("%s:%u failed to parse '%s'" %
                          (input_name, lineno, l), file = sys.stderr)
                    raise

                if not block:
                    raise Exception("failed to parse '%s'" % l)

                l = None

        while block and block != self:
            #print(block)
            block = block.parse(['@end'], True)


    def iterate_files(self, files, defines):
        for f in files:
            preproc=None
            with open(f) as input:
                l = input.readline()
                if l.startswith('##!'):
                    preproc = l[3:].strip().split()[0]

            if preproc:
                input_name = "[%s]%s" % (preproc, f)
            else:
                input_name = None
                preproc = 'plain'

            with _preprocessors[preproc].call(f) as input:
                self.__read_file(input, defines, input_name)

    def parse(self, l, enabled):
        if not l:
            return self

        tag = l[0]
        assert(tag != '@end' or enabled)

        if not tag.startswith('@'):
            raise Exception("Invalid line '%s'" % (l))

        if tag == '@end':
            res = self.__parent
        else:
            res = None
            for p in self._parsers:
                if not p._validate(l):
                    continue

                res = p._parse(l, enabled)
                assert(res != None)

            if not res:
                p   = self.parse(['@end'], True)

                if p:
                    res = p.parse(l, enabled)

        #print(res)
        assert(res == None or isinstance(res, Block))

        return res

    def add_child(self, child):
        self.children.append(child)

    def filter(self, fn):
        return filter(lambda r: fn(r) and r.is_valid(), self.children)

    def is_finalized(self):
        return self.__is_finalized

    def finalize(self):
        if not self.__is_finalized:
            self._finalize()
            self.__is_finalized = True

    @abc.abstractmethod
    def _finalize(self):
        pass

class Top(Block):
    def __init__(self):
        Block.__init__(self, None, True)

    def _finalize(self):
        pass

class Mergeable(metaclass=abc.ABCMeta):
    class __Parser(Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@use' : 1,
            }
            return self._validate_ranges(l, ARG_RANGES)

        def _parse(self, l, enabled):
            tag = l[0]
            res = self.o

            if not enabled:
                pass
            elif tag == '@use':
                self.o._append_use(l[1])
            else:
                assert(False)

            return res

    def __init__(self):
        self.add_parser(Mergeable.__Parser(self))

        self.__use = []

        self.__is_merged = False
        self.__is_merging = False

    def _append_use(self, use):
        self.__use.append(use)

    def is_merged(self):
        return self.__is_merged

    def _merge_pre(self):
        pass

    @abc.abstractmethod
    def _merge(self, base):
        pass

    def _merge_post(self):
        pass

    def merge(self, container):
        # check for circular deps
        assert(not self.__is_merging)

        # return when we are already merged
        if self.__is_merged:
            return

        # set marker to avoid circular deps
        self.__is_merging = True

        # step 1: merge all base objects
        for u in self.__use:
            r = container[u]
            assert(isinstance(r, Mergeable))
            r.merge(container)

        # step 2: (optionally) prepare merge
        self._merge_pre()

        # step 3: merge with bases
        for u in self.__use:
            r = container[u]
            self._merge(container[u])

        # step 4: (optionally) execute post tasks
        self._merge_post()

        # (re)set flags and markers
        self.__is_merging = False
        self.__is_merged  = True

    @staticmethod
    def create_container(objects, key_fn):
        res = {}
        for o in objects:
            id = key_fn(o)
            if id in res:
                raise Exception("Duplicate object '%s'" % id)

            res[id] = o

        return res

    @staticmethod
    def update_attr(attr, default):
        #print(attr, default)
        if attr != None or default == None:
            return attr
        else:
            return copy.deepcopy(default)

class Removable(metaclass=abc.ABCMeta):
    class __Parser(Parser):
        def __init__(self, obj):
            super().__init__(obj)

        def _validate(self, l):
            ARG_RANGES = {
                '@remove' : 0,
            }

            return self._validate_ranges(l, ARG_RANGES)


        def _parse(self, l, enabled):
            tag = l[0]
            res = self.o

            if not enabled:
                pass
            elif tag == '@remove':
                self.o._set_removed(True)
            else:
                assert(False)

            return res

    def __init__(self, is_removed = False):
        self.add_parser(Removable.__Parser(self))
        self.__is_removed = is_removed

    def _set_removed(self, rm):
        self.__is_removed = rm

    def is_removed(self):
        return self.__is_removed
