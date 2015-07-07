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

class Line:
    class __IndentLevel:
        def __init__(self):
            self.i = 0

        def inc(self, c):
            if c == ' ':
                self.i += 1
            elif c == '\t':
                self.i = (self.i + 8) / 8 * 8
            else:
                raise Exception("Bad char for indentation")

    def __init__(self):
        self.__is_complete = False
        self.__exp_quote = None
        self.__level = None
        self.tokens = []
        pass

    def __str__(self):
        return ' '.join(self.tokens)

    def get_expected_quote(self):
        return self.__exp_quote

    def is_complete(self):
        return self.__is_complete

    def __add_token(self, tk, exp_quote = None):
        if self.__exp_quote:
            self.tokens[-1] += tk
        else:
            self.tokens.append(tk)

        self.__exp_quote = exp_quote

    def __set_complete(self, is_complete, level):
        assert(not self.__is_complete)
        if self.__level == None:
            self.__level = level
        self.__is_complete = is_complete

    def expand(self, defines):
        assert(self.is_complete())

        if not self.tokens:
            return (False, -1, None)

        res = self.tokens[:]
        f   = res[0]
        idx = f.find('[')
        valid = idx < 0

        if idx < 0:
            valid = True
        else:
            res[0] = f[:idx]
            f = f[idx+1:]

            idx = f.index(']')
            if f[idx:] != ']':
                raise Exception("Invalid condition in '%s'" % (self.tokens,))

            f = f[:idx]
            if f[0] == '!':
                valid = f not in defines
            else:
                valid = f in defines

        return (valid, self.__level, res)

    @staticmethod
    def parse(l, prev_line):
        res = prev_line
        if not res:
            res = Line()

        end_quote = None

        l = l.rstrip('\n\r')

        #print("== >%s<" % l)
        l = list(l)
        if not l:
            state = -1
        else:
            end_quote = res.get_expected_quote()
            if end_quote:
                tmp   = ''
                state = 22
            else:
                tmp = None
                state = 0

            c = l.pop(0)

        indent_lvl = Line.__IndentLevel()
        while state != -1:
            if state == 0:
                # start of line
                if not l:
                    state = 99
                elif c in [' ', '\t']:
                    indent_lvl.inc(c)
                    c = l.pop(0)
                elif c in ['#']:
                    c = l.pop(0)
                    state = 1
                else:
                    state = 2
            elif state == 1:
                # comment
                if l[-1] == '\\':
                    state = 98
                else:
                    state = 99
            elif state == 2:
                # normal input; check EOL condition
                if l:
                    state = 4
                else:
                    state = 99
            elif state == 4:
                # post state 2; initialize
                assert(l)
                tmp = ''
                end_quote = None
                state = 21
            elif state == 3:
                # normal input; continue token
                if l:
                    c = l.pop(0)
                    state = 20
                else:
                    res.__add_token(tmp)
                    tmp = None
                    state = 99
            elif state == 20:
                # normal input; process
                if c in [' ', '\t']:
                    # whitespace input
                    res.__add_token(tmp)
                    state = 2
                elif c in ["'", '"']:
                    # quoted input
                    end_quote = c
                    state = 22
                    c    = l.pop(0)
                elif c == '#':
                    state = 1
                elif c != '\\':
                    # normal (non-backslashified) input
                    tmp  += c
                    state = 3
                elif l:
                    # backslashified input within line
                    tmp  += l.pop(0)
                    state = 3
                else:
                    # backslash at end of line
                    state = 98
            elif state == 21:
                # whitespace in normal input
                if c not in [' ', '\t']:
                    state = 20
                elif l:
                    c = l.pop(0)
                else:
                    state = 99
            elif state == 22:
                # quoted input
                assert(end_quote)
                if c == end_quote:
                    res.__add_token(tmp)
                    state = 2
                elif c != '\\':
                    tmp += c
                    c = l.pop(0)
                elif l:
                    tmp += l.pop(0)
                    c = l.pop(0)
                else:
                    res.__add_token(tmp, end_quote)
                    state = 98
            elif state == 98:
                # backslashified line
                res.__set_complete(False, indent_lvl.i)
                state = -1
            elif state == 99:
                res.__set_complete(True, indent_lvl.i)
                state = -1
            else:
                raise Exception("Unsupported state %d" % state)

            #print(state, tmp, c)

        if res and res.is_complete() and not res.tokens:
            res = Line()

        return res
