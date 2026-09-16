"""SianLang 0.4 collection, iteration, and time feature tests."""
import re
import sys
import unittest
from pathlib import Path

import test_runtime as base


class CollectionTests(unittest.TestCase):
    run_source = base.RuntimeTests.run_source
    check = base.RuntimeTests.check

    def test_literals_indexing_and_length(self):
        self.check(
            'var items = [1, 2, 3]\n'
            'var pair = ("a", "b")\n'
            'var user = {"name": "Sian", "age": 20}\n'
            'log items, pair, user\n'
            'log items[1], pair[-1], user["name"], len(items), len(pair), len(user)\n',
            "[1, 2, 3] ('a', 'b') {'name': 'Sian', 'age': 20}\n"
            "2 b Sian 3 2 2\n",
        )

    def test_for_and_range(self):
        self.check(
            'for item in [1, 2, 3]\n'
            '    log item\n'
            'for item in range(2, 7, 2)\n'
            '    log item\n'
            'for key in {"a": 1, "b": 2}\n'
            '    log key\n',
            '1\n2\n3\n2\n4\n6\na\nb\n',
        )
        self.check('log range(0, 5, 0)\n', '', 'range step cannot be zero')
        self.check('for item in 1\n    log item\n', '', 'for requires a tuple, list, dict, str, or range')

    def test_time_now(self):
        result = self.run_source('log time.now()\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, '')
        self.assertRegex(result.stdout, r'^\d+\n$')

    def test_index_assignment_and_methods(self):
        self.check(
            'var items = [10, 20, 30]\n'
            'items[0] = 99\n'
            'items[-1] = 77\n'
            'items.append(100)\n'
            'log items, items.pop()\n',
            "[99, 20, 77] 100\n",
        )
        self.check(
            'var d = {"a": 1}\n'
            'd["a"] = 10\n'
            'd["b"] = 20\n'
            'log d.keys(), d.values()\n',
            "['a', 'b'] [10, 20]\n",
        )

    def test_string_indexing_and_iteration(self):
        self.check(
            'str s = "시안랭"\n'
            'log s[0], s[-1]\n'
            'for ch in "시안"\n'
            '    log ch\n',
            "시 랭\n시\n안\n",
        )

    def test_math_builtins(self):
        self.check(
            'log abs(-42), min(10, 20, 5), max(10, 20, 5), round(3.14159, 2)\n',
            "42 5 20 3.14\n",
        )

    def test_syntax_errors(self):
        self.check('var broken = {"key" 1}\n', '', "needs ':'")
        self.check('for item range(3)\n    log item\n', '', "requires 'in'")


if __name__ == '__main__':
    result = unittest.main(argv=['test_v04.py', *base.REST], verbosity=2, exit=False).result
    print(f'{base.EXECUTIONS} collection executions checked')
    sys.exit(0 if result.wasSuccessful() else 1)
