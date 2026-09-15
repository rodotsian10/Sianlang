"""Black-box regression tests. Run: python tests/test_runtime.py [--exe PATH]."""
import argparse
import ctypes
import os
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
if os.name == 'nt':
    ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
options = argparse.ArgumentParser()
options.add_argument('--exe', type=Path, default=ROOT / 'Sianlang.exe')
OPTIONS, REST = options.parse_known_args()
EXE = OPTIONS.exe.resolve()
EXECUTIONS = 0

# Read the original review inputs as data without executing its observation script.
ORIGINAL = {p.stem: p.read_text(encoding='utf-8') for p in (ROOT / 'review' / 'cases').glob('*.sian')}

# Each success requires exact stdout, no stderr and exit 0.
# Each error requires exit 1, its first diagnostic, and no subsequent output.
REVIEW_EXPECTED = {
    'baseline': ('6\n5\n', None),
    'int_add': ('3\n', None), 'int_sub': ('1\n', None),
    'division_inconsistent': ('2.5\n2.5\n', None),
    'float_modulo': ('1.0\n', None), 'zero_modulo': ('', 'by zero'),
    'string_compare': ('True\nFalse\nTrue\n', None),
    'string_arithmetic': ('', 'arithmetic requires numbers'),
    'string_comment': ('a||b\nafter\n', None),
    'string_block_comment': ('a^^b^^c\n', None),
    'unclosed_comment': ('', 'closing ^^'),
    'escape': ('a\nB\na"b\n', None),
    'trailing_tokens': ('', 'unexpected text'),
    'chained_compare': ('', 'chained comparisons'),
    'empty_function': ('7\n', None), 'call_statement': ('7\n', None),
    'parameter_clobber': ('7\n100\n', None),
    'local_clobber': ('7\n100\n', None),
    'dynamic_scope': ('', "variable 'secret' not found"),
    'iferror_in_function': ('', "variable 'iferror' not found"),
    'continue_after_error': ('', "variable 'missing' not found"),
    'assign_error_value': ('', 'by zero'),
    'double_condition': ('body\n2\n', None),
    'empty_block': ('', 'indented, nonempty block'),
    'inconsistent_indent': ('', 'indentation'), 'top_indent': ('', 'indentation'),
    'wrong_else_indent': ('', None), 'outer_else_if': ('done\n', None),
    'else_junk': ('', 'unexpected text'),
    'unreachable_syntax': ('', 'expected a declaration'),
    'type_reassignment': ('', 'expects int, got str'),
    'invalid_names': ('', 'expected a variable'),
    'repeat_type': ('', 'nonnegative int'),
    'malformed_function': ('', 'closing parenthesis'),
    'conditional_definition': ('', "variable 'hidden' not found"),
    'overflow_integer': ('2147483648\n4294967294\n', None),
    'long_number': ('', 'supported range'), 'long_identifier': ('', 'name exceeds'),
    'long_parameter': ('1\n', None), 'too_many_parameters': ('', 'missing required argument'),
    'line_limit': ('lost\n', None), 'long_line': ('1\n2\n', None),
    'bom': ('hello\n', None), 'variable_limit': ('', None), 'function_limit': ('1\n', None),
    'input_eof': ('', 'end of input'), 'input_origin': ('', 'expects int, got str'),
    'input_reassignment': ('13\n', None), 'long_string': ('a' * 256 + '\n', None),
    'long_line_number': ('1\n', 'line 2:'), 'error_loop': ('', "variable 'missing' not found"),
    'float_zero_modulo': ('', 'by zero'), 'concat_truncation': ('x' * 1200 + '\n', None),
}

class RuntimeTests(unittest.TestCase):
    def run_source(self, source, data='', filename='case.sian'):
        global EXECUTIONS
        EXECUTIONS += 1
        with tempfile.TemporaryDirectory(prefix='sian-tests-', dir=ROOT / 'tests') as directory:
            assert Path(directory).resolve().parent == (ROOT / 'tests').resolve()
            path = Path(directory) / filename
            path.write_text(source, encoding='utf-8')
            return subprocess.run([str(EXE), str(path)], input=data, text=True,
                                  encoding='utf-8', errors='strict', capture_output=True, timeout=5)

    def check(self, source, output='', error=None, data=''):
        p = self.run_source(source, data)
        self.assertEqual(p.returncode, 1 if error else 0, p.stderr)
        self.assertEqual(p.stdout, output, p.stderr)
        if error:
            self.assertIn(error, p.stderr)
            self.assertEqual(p.stderr.count('[Error]'), 1, p.stderr)
            self.assertNotIn('Internal error', p.stderr)
        else:
            self.assertEqual(p.stderr, '')
        return p

    def test_all_review_cases(self):
        self.assertEqual(set(REVIEW_EXPECTED), set(ORIGINAL))
        for name, (output, error) in REVIEW_EXPECTED.items():
            with self.subTest(case=name):
                self.check(ORIGINAL[name], output, error,
                           '12\n' if name.startswith('input_') and name != 'input_eof' else '')

    def test_existing_examples(self):
        for filename, data, output, error in [
            ('input-test.sian', 'Sian\n', 'Name: Hello, Sian\n', None),
            ('input-types-test.sian', '3\n1.5\ntrue\n', 'Count: Ratio: Ready (true/false): 3\n1.5\nTrue\n', None),
            ('try-catch-test.sian', '', 'before\ncaught\nafter\n', None),
        ]:
            with self.subTest(filename=filename):
                self.check((ROOT / filename).read_text(encoding='utf-8'), output, error, data)
        for data, expected in [('50\n', '정답입니다'), ('60\n', '50보다 큽니다'), ('40\n', '50보다 작습니다')]:
            with self.subTest(data=data):
                p = self.run_source((ROOT / 'code.sian').read_text(encoding='utf-8'), data)
                self.assertEqual(p.returncode, 0, p.stderr)
                self.assertIn(expected, p.stdout)
                self.assertEqual(p.stderr, '')

    def test_functions_and_flow(self):
        cases = [
            ('def fact(n)\n    if n <= 1\n        return 1\n    return n * fact(n - 1)\nlog fact(10)\n', '3628800\n', None),
            ('def greet()\n    log "hi"\ngreet()\n', 'hi\n', None),
            ('def greet()\n    return\nlog greet()\n', 'None\n', None),
            ('def recurse(n)\n    return recurse(n + 1)\nrecurse(0)\n', '', 'nesting exceeds'),
            ('def stop()\n    return 1 / 0\nint n = stop()\nlog "lost"\n', '', 'by zero'),
            ('int n = 0\nrepeat 5\n    n = n + 1\n    if n == 2\n        continue\n    if n == 4\n        break\n    log n\nlog n\n', '1\n3\n4\n', None),
            ('if false\n    log 0\nelse\n    if true\n        log 1\n    log 2\nlog 3\n', '1\n2\n3\n', None),
            ('if true\n    if true\n        log 1\n    log 2\nlog 3\n', '1\n2\n3\n', None),
            ('if false\n    log 0\nelse if false\n    log 1\nelse if true\n    log 2\nelse\n    log 3\n', '2\n', None),
            ('int n = 5\nrepeat 0\n    log n\nlog n\n', '5\n', None),
            ('int n = 0\nwhile n < 3\n    int a = n\n    log a\n    n = n + 1\n', '0\n1\n2\n', None),
            ('int n = 0\ndef bump()\n    n = n + 1\n    return true\nlog false and bump()\nlog true or bump()\nlog n\n', 'False\nTrue\n0\n', None),
            ('log 1 < 2 and 2 < 3\nlog false or true and false\n', 'True\nFalse\n', None),
            ('break\n', '', 'enclosing loop'),
            ('continue\n', '', 'enclosing loop'),
            ('def a(x, x)\n    return x\n', '', 'duplicate parameter'),
            ('def a()\n    return 1\ndef a()\n    return 2\n', '', 'duplicate function'),
            ('int true = 1\n', '', 'reserved word'),
            ('log missing()\n', '', "variable 'missing' not found"),
        ]
        for source, output, error in cases:
            with self.subTest(source=source): self.check(source, output, error)

    def test_numeric_boundaries(self):
        cases = [
            ('log 9223372036854775807\nlog -9223372036854775808\n', '9223372036854775807\n-9223372036854775808\n', None),
            ('log 9223372036854775807 + 1\n', '', 'overflow'),
            ('log -9223372036854775808 - 1\n', '', 'overflow'),
            ('log 9223372036854775807 * 2\n', '', 'overflow'),
            ('log -9223372036854775808 * -1\n', '', 'overflow'),
            ('log -(-9223372036854775808)\n', '', 'overflow'),
            ('log -9223372036854775808 % -1\n', '0\n', None),
            ('log 9223372036854775807 < 9223372036854775808.0\nlog 9007199254740993 == 9007199254740992.0\n', 'True\nFalse\n', None),
            ('log int(9223372036854775808.0)\n', '', 'outside int range'),
            ('log 1e308 * 10\n', '', 'finite range'),
            ('log 1 / 0\n', '', 'by zero'),
            ('log .5 + 1e2\n', '100.5\n', None),
            ('log 1e999\n', '', 'supported range'),
            ('log int(2.9)\nlog int(-2.9)\n', '2\n-2\n', None),
            ('log -1.5 < -1\nlog -2 < -1.5\n', 'True\nTrue\n', None),
        ]
        for source, output, error in cases:
            with self.subTest(source=source): self.check(source, output, error)
        rng = random.Random(20260915)
        lines, expected = [], []
        for _ in range(200):
            a, b = rng.randint(-1000000, 1000000), rng.randint(-1000000, 1000000) or 1
            for op, value in [('+', a + b), ('-', a - b), ('*', a * b), ('%', a % b)]:
                lines.append(f'log ({a}) {op} ({b})')
                expected.append(str(value))
        self.check('\n'.join(lines), '\n'.join(expected) + '\n')

    def test_conversions_and_input(self):
        self.check('log str(10) + " points"\nlog int("12")\nlog float(".5")\nlog bool("false")\n', '10 points\n12\n0.5\nFalse\n')
        self.check('int n = input()\nn = input()\nlog n + 1\n', '14\n', data='12\n13\n')
        self.check('float n = input()\nlog n\n', '1.5\n', data=' 1.5 \n')
        self.check('TF b = input()\nlog b\n', 'True\n', data='true\n')
        self.check('str s = input()\nlog s\nlog input()\n', 'x' * 5000 + '\nafter\n', data='x' * 5000 + '\nafter\n')
        self.check('log input()\n', '\n', data='\n')
        for data in ['nan\n', 'inf\n', '1e999\n', '0x1p2\n', 'hello\n']:
            with self.subTest(data=data): self.check('float n = input()\n', '', 'cannot convert', data)
        for data in ['9223372036854775808\n', '1.5\n', 'true\n']:
            with self.subTest(data=data): self.check('int n = input()\n', '', 'cannot convert', data)
        self.check('input(2)\n', '', 'str prompt')
        self.check('input("x", "y")\n', '', 'zero arguments or one str prompt')
        self.check('float n = 1\nn = 2\nlog n\n', '2.0\n')
        self.check('str s = "12"\nint n = s\n', '', 'expects int')

    def test_syntax_before_execution(self):
        for source, error in [
            ('log "before"\nif false\n    log (1\n', 'closing parenthesis'),
            ('log "before"\nlog 1.2.3\n', 'unexpected text'),
            ('log "before"\nlog 1)\n', 'unexpected text'),
            ('log "before"\nlog "bad\\q"\n', 'unknown string escape'),
            ('log "before"\nelse\n    log 1\n', 'reserved word'),
            ('log ' + '(' * 200 + '1' + ')' * 200, 'nesting'),
            ('log ' + '+'.join(['1'] * 200), 'nesting'),
            ('log 1e\n', 'exponent'),
            ('def a()\n|| no body\n', 'nonempty block'),
            ('log 1\x00\n', 'NUL byte'),
        ]:
            with self.subTest(source=source[:70]): self.check(source, '', error)

    def test_comments_unicode_and_limits(self):
        self.check('^^ comment ^^\nlog "한글 || ^^"\n', '한글 || ^^\n')
        self.check('if true\n    || comment\n    log 1 ^^ inline ^^ + 2\n\n    log 4\n', '3\n4\n')
        self.check('^^ leading comment ^^ log 1\nif true\n    ^^ comment ^^ log 2\n', '1\n2\n')
        self.check('log "한글"\n', '한글\n')
        self.check('log "' + '한' * 1000 + '"\n', '한' * 1000 + '\n')
        parameters = ','.join(f'p{i}' for i in range(256))
        arguments = ','.join(str(i) for i in range(256))
        self.check(f'def many({parameters})\n    return p255\nlog many({arguments})\n', '255\n')
        self.check(f'def many({parameters}, extra)\n    return 1\n', '', 'too many parameters')
        self.check(f'log many({arguments}, 0)\n', '', 'too many function arguments')
        p = self.run_source('log 7\n', filename="한글🎮 & dollar$ quote' space.sian")
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, '7\n', ''))
        p = self.run_source('log missing\n', filename='한글🎮.sian')
        self.assertEqual(p.returncode, 1)
        self.assertIn('한글🎮.sian', p.stderr)

    def test_documented_examples(self):
        for document in [ROOT / '설명서.md', ROOT / '설명서' / '시안랭-문법규칙서.md']:
            for index, source in enumerate(re.findall(r'```sian\n(.*?)```', document.read_text(encoding='utf-8'), re.S)):
                with self.subTest(document=document.name, example=index):
                    p = self.run_source(source, data='12\n13\n14\n')
                    self.assertEqual(p.returncode, 0, p.stderr)
                    self.assertEqual(p.stderr, '')
        self.check('func one()\n    return 1\nf two()\n    return 2\nfunction three()\n    return 3\nlog one() + two() + three()\nloop false\n    log 0\n', '6\n')

    def test_string_lifetimes(self):
        self.check('def echo(s)\n    str local = s + "x"\n    return local\nrepeat 20000\n    str result = echo("hello")\n    bool same = result == "hellox"\nlog result\nlog same\n', 'hellox\nTrue\n')
        self.check('def fail(s)\n    str local = s + "x"\n    return 1 / 0\nfail("stop")\n', '', 'by zero')
        self.check('log !"abc"\nlog "a" != "b"\n', 'False\nTrue\n')
        self.check('str x = "a"\nrepeat 25\n    x = x + x\n', '', 'string exceeds')

    def test_command_line(self):
        global EXECUTIONS
        for args, code, fragment in [([], 1, 'Usage:'), (['--version'], 0, '0.3.1'),
                                      (['missing.sian'], 1, 'cannot open'), (['file.txt'], 1, '.sian')]:
            with self.subTest(args=args):
                EXECUTIONS += 1
                p = subprocess.run([str(EXE), *args], capture_output=True, text=True, timeout=5)
                self.assertEqual(p.returncode, code)
                self.assertIn(fragment, p.stdout + p.stderr)

if __name__ == '__main__':
    result = unittest.main(argv=['test_runtime.py', *REST], verbosity=2, exit=False).result
    print(f'{EXECUTIONS} interpreter executions checked; numeric batch includes 800 arithmetic results')
    sys.exit(0 if result.wasSuccessful() else 1)
