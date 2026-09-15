"""0.3 feature integration and console-print parity tests."""
import contextlib
import io
import random
from pathlib import Path
import sys
import unittest
import test_runtime as base

class FeatureTests(unittest.TestCase):
    run_source = base.RuntimeTests.run_source
    check = base.RuntimeTests.check

    def test_print(self):
        cases = [
            ('log()\nlog 1, "two", None, true, 2.0\n', '\n1 two None True 2.0\n', None),
            ('log(1, 2, sep="|", end="!")\nlog("x", flush=true)\n', '1|2!x\n', None),
            ('log(1, 2, sep=None, end=None, file=None)\n', '1 2\n', None),
            ('var printer = log\nprinter("hi", 3, sep="/")\nlog printer()\n', 'hi/3\n\nNone\n', None),
            ('log(1, sep=3)\n', '', 'sep must be'),
            ('log(1, end=false)\n', '', 'end must be'),
            ('log(1, file="x")\n', '', 'console output only'),
            ('log(1, unknown=2)\n', '', 'unexpected named argument'),
        ]
        for source, output, error in cases:
            with self.subTest(source=source): self.check(source, output, error)
        for value in [0.0, -0.0, 1.0, 1.5, 1e-5, 1e-4, 1e15, 1e16, 1e20, 1e100,
                      0.10000000000000002, 1.2345678901234567, 2.2250738585072014e-308]:
            with self.subTest(value=value): self.check(f'log {value!r}\n', str(value) + '\n')
        for sep, end in [(None, None), ('', ''), (' | ', ' END\n'), ('한글', '\t')]:
            output = io.StringIO()
            with contextlib.redirect_stdout(output): print('text', 12, 1.0, True, None, sep=sep, end=end, flush=True)
            def literal(v):
                if v is None: return 'None'
                return '"' + v.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\t', '\\t') + '"'
            self.check(f'log("text", 12, 1.0, true, None, sep={literal(sep)}, end={literal(end)}, flush=true)\n', output.getvalue())
        rng = random.Random(316)
        values = [rng.uniform(-9, 9) * (10.0 ** rng.randint(-200, 200)) for _ in range(500)]
        self.check('\n'.join(f'log {v!r}' for v in values), ''.join(str(v) + '\n' for v in values))

    def test_formatting(self):
        cases = [
            ('int score = 12\nlog.f("점수: {score}, 다음: {score + 1}")\n', '점수: 12, 다음: 13\n', None),
            ('float x = 1.25\nlog.f "{x:.2f} {x:08.2f} {{ok}}", end="!"\n', '1.25 00001.25 {ok}!', None),
            ('str name = "Sian"\nlog.f "{name:>6}|{name:*^8}|{name:.2s}|{name!r}"\n', "  Sian|**Sian**|Si|'Sian'\n", None),
            ('int n = 255\nlog.f "{n:x} {n:b} {n:+d}"\n', 'ff 11111111 +255\n', None),
            ('int n = 12\nlog.f "{n:+} {n:05}"\n', '+12 00012\n', None),
            ('int n = 12\nstr template = "number {n}"\nlog.f(template)\nvar printer = log.f\nprinter(template)\n', 'number 12\nnumber 12\n', None),
            ('log.f "{None} {true}"\n', 'None True\n', None),
            ('def add(a, b=2)\n    return a + b\nlog.f "{add(3)}"\n', '5\n', None),
            ('log.f "{1 != 2}"\n', 'True\n', None),
            ('log.f "{3.5!s:>5}"\n', '  3.5\n', None),
            ('log.f "bad {"\n', '', "closing '}'"),
            ('log.f "bad }"\n', '', "unmatched '}'"),
            ('log.f "{}"\n', '', 'expected an expression'),
            ('log.f "{missing}"\n', '', "variable 'missing'"),
            ('log.f "{1:q}"\n', '', 'unsupported format'),
            ('try\n    log.f "{1 / 0}"\ncatch err\n    log "caught"\n', 'caught\n', None),
        ]
        for source, output, error in cases:
            with self.subTest(source=source): self.check(source, output, error)
        self.check('str broken = "{"\nrepeat 200\n    try\n        log.f(broken)\n    catch\n        var ignored = None\nlog.f "{1}"\n', '1\n')

    def test_defaults_keywords_variadic(self):
        cases = [
            ('def add(a, b=2)\n    return a + b\nlog add(3), add(b=5, a=4)\n', '5 9\n', None),
            ('def show(a, b=2, #c)\n    log a, b, c, len(c)\n    log(#c, sep="|")\nshow(1, 3, "x", true, None)\nshow(a=1)\n', "1 3 ('x', True, None) 3\nx|True|None\n1 2 () 0\n\n", None),
            ('def get(#args)\n    return args[-1]\nlog get(1, 2, 3)\n', '3\n', None),
            ('def add(a, b)\n    return a + b\ndef forward(#args)\n    return add(#args)\nlog forward(4, 5)\n', '9\n', None),
            ('int counter = 0\ndef next()\n    counter = counter + 1\n    return counter\ndef read(value=next())\n    return value\nlog read(), read(), counter\n', '1 1 1\n', None),
            ('def optional(value=None)\n    return value\nlog optional(), optional(7)\n', 'None 7\n', None),
            ('def take(a)\n    return a\nlog take(1, a=2)\n', '', 'multiple values'),
            ('def take(a)\n    return a\nlog take(z=1)\n', '', 'unexpected named argument'),
            ('def take(a)\n    return a\nlog take()\n', '', 'missing required'),
            ('def bad(a=1, b)\n    return b\n', '', 'required parameter follows'),
            ('def bad(#a, b)\n    return b\n', '', 'must be last'),
            ('def bad(#a=1)\n    return a\n', '', 'cannot have a default'),
            ('log(1, sep="x", sep="y")\n', '', 'duplicate named'),
            ('log(sep="x", 1)\n', '', 'positional argument follows'),
            ('log(#1)\n', '', '#expansion requires'),
            ('def take(#a)\n    return a[0]\nlog take()\n', '', 'index out of range'),
        ]
        for source, output, error in cases:
            with self.subTest(source=source): self.check(source, output, error)

    def test_closures(self):
        cases = [
            ('def make(start)\n    int count = start\n    def step(amount=1)\n        count = count + amount\n        return count\n    return step\nvar first = make(10)\nvar second = make(100)\nlog first(), first(amount=3), second(), first()\n', '11 14 101 15\n', None),
            ('def apply(callback, x)\n    return callback(x)\ndef double(x)\n    return x * 2\nlog apply(double, 4)\n', '8\n', None),
            ('def outer(n)\n    def fact(x)\n        if x <= 1\n            return n\n        return x * fact(x - 1)\n    return fact\nlog outer(1)(5)\n', '120\n', None),
            ('int x = 1\ndef outer()\n    int x = 2\n    def inner()\n        return x\n    return inner\ndef caller(callback)\n    int x = 3\n    return callback()\nlog caller(outer()), x\n', '2 1\n', None),
            ('def make()\n    def helper(x)\n        return x + 1\n    def use(x, callback=helper)\n        return callback(x)\n    return use\nvar use = make()\nlog use(4)\n', '5\n', None),
            ('def make()\n    int x = 1\n    def reader()\n        return x\n    x = 9\n    return reader\nlog make()()\n', '9\n', None),
            ('repeat 1\n    def bad()\n        break\n', '', 'enclosing loop'),
            ('var value = None\nvalue = 7\nlog value\nvalue = log\nvalue("hello")\n', '7\nhello\n', None),
        ]
        for source, output, error in cases:
            with self.subTest(source=source): self.check(source, output, error)
        self.check('def make(n)\n    str s = "hello"\n    def read()\n        return n\n    return read\nvar saved = make(42)\nrepeat 20000\n    var temporary = make(1)\nlog saved()\n', '42\n')
        # Keep closure-valued earlier arguments alive during collection in a later argument.
        self.check('def make(x)\n    def read()\n        return x\n    return read\ndef churn()\n    repeat 1000\n        var temp = make(0)\n    return 1\ndef apply(callback, ignored)\n    return callback()\nlog apply(make(99), churn())\n', '99\n')

    def test_none_try_loop_else(self):
        cases = [
            ('var value = None\nlog value == None, value != None, !value, bool(value), str(value)\ndef empty()\n    return\nlog empty()\n', 'True False True False None\nNone\n', None),
            ('try\n    log 1 / 0\ncatch error\n    log "caught", error\nlog "after"\n', 'caught cannot divide or take remainder by zero\nafter\n', None),
            ('try\n    log 1\ncatch\n    log 2\n', '1\n', None),
            ('def bad()\n    return 1 / 0\ntry\n    bad()\ncatch\n    log "recovered"\n', 'recovered\n', None),
            ('try\n    try\n        log 1 / 0\n    catch\n        log int("bad")\ncatch outer\n    log "outer"\n', 'outer\n', None),
            ('def result()\n    try\n        return 4\n    catch\n        return 5\nlog result()\n', '4\n', None),
            ('int i = 0\nwhile i < 2\n    i = i + 1\nelse\n    log "done"\nrepeat 0\n    log "never"\nelse\n    log "zero"\n', 'done\nzero\n', None),
            ('repeat 2\n    break\nelse\n    log "bad"\nloop false\n    log "bad"\nelse\n    log "good"\n', 'good\n', None),
            ('repeat 2\n    continue\nelse\n    log "done"\n', 'done\n', None),
            ('repeat 2\n    try\n        break\n    catch\n        log "bad"\nelse\n    log "bad"\nlog "ok"\n', 'ok\n', None),
            ('try\n    repeat 2\n        log 1 / 0\n    else\n        log "bad"\ncatch\n    log "caught"\n', 'caught\n', None),
            ('try\n    log (1\ncatch\n    log "not caught"\n', '', 'closing parenthesis'),
            ('try\n    log 1\n', '', 'requires a catch'),
            ('catch\n    log 1\n', '', 'reserved word'),
            ('try\n    log missing\ncatch\n    log other\n', '', "variable 'other'"),
            ('int error = 3\ntry\n    log 1 / 0\ncatch error\n    log error\n', '', 'conflicts with a typed variable'),
        ]
        for source, output, error in cases:
            with self.subTest(source=source): self.check(source, output, error)

if __name__ == '__main__':
    result = unittest.main(argv=['test_v03.py', *base.REST], verbosity=2, exit=False).result
    print(f'{base.EXECUTIONS} feature executions checked')
    sys.exit(0 if result.wasSuccessful() else 1)
