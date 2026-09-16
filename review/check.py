"""Run isolated review probes; no changes to interpreter sources."""
import ctypes
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
CASES = {
    'baseline': 'log 2 * 3\ndef add(a, b)\n    return a + b\nlog add(2, 3)\n',
    'int_add': 'int n = 1 + 2\nlog n\n',
    'int_sub': 'int n = 3 - 2\nlog n\n',
    'division_inconsistent': 'log 5 / 2\nlog (4 + 1) / 2\n',
    'float_modulo': 'log 5.0 % 2\n',
    'zero_modulo': 'log 1 % 0\n',
    'string_compare': 'log "a" != "b"\nlog "a" == 0\nlog "a" <= "b"\n',
    'string_arithmetic': 'log "score=" + 10\nlog "abc" * 3\nlog -"abc"\n',
    'string_comment': 'log "a||b"\nlog "after"\n',
    'string_block_comment': 'log "a^^b^^c"\n',
    'unclosed_comment': 'log "before"\n^^\nlog "lost"\n',
    'escape': 'log "a\\nB"\nlog "a\\"b"\n',
    'trailing_tokens': 'log 1 nonsense\nlog 1 2\nlog 1)\nlog 1.2.3\n',
    'chained_compare': 'log 3 < 2 < 1\nlog 1 == 1 == false\n',
    'empty_function': 'def greet()\n    return 7\nlog greet()\n',
    'call_statement': 'def greet(x)\n    log x\ngreet(7)\n',
    'parameter_clobber': 'int x = 100\ndef identity(x)\n    return x\nlog identity(7)\nlog x\n',
    'local_clobber': 'int x = 100\ndef test(y)\n    int x = 7\n    return x\nlog test(0)\nlog x\n',
    'dynamic_scope': 'def read(dummy)\n    return secret\ndef caller(secret)\n    return read(0)\nlog caller(42)\n',
    'iferror_in_function': 'def fail(x)\n    iferror("stop")\nlog fail(0)\nlog "after"\n',
    'continue_after_error': 'log missing\nlog "after"\n',
    'assign_error_value': 'int n = 8\nn = 1 / 0\nlog n\n',
    'double_condition': 'int n = 0\ndef cond(x)\n    n = n + 1\n    return n < 2\nwhile cond(0)\n    log "body"\nlog n\n',
    'empty_block': 'if true\nlog "outside"\n',
    'inconsistent_indent': 'if true\n    log "a"\n  log "b"\n',
    'top_indent': '    log "a"\n',
    'wrong_else_indent': 'if true\n    if false\n        log "inner"\nelse\n    log "outer-else"\n',
    'else_junk': 'if false\n    log 1\nelse garbage\n    log 2\n',
    'unreachable_syntax': 'if false\n    totally invalid syntax\nlog "ok"\n',
    'type_reassignment': 'int x = 1\nx = "hello"\nlog x\n',
    'invalid_names': 'int 1abc = 5\nint true = 3\nlog true\n',
    'repeat_type': 'repeat "3"\n    log "a"\nrepeat 2.9\n    log "b"\n',
    'malformed_function': 'def test(x\n    return x\nlog test(4)\n',
    'conditional_definition': 'if false\n    def hidden(x)\n        return x\nlog hidden(7)\n',
    'overflow_integer': 'log 2147483648\nlog 2147483647 * 2\n',
    'long_number': 'log ' + '1' * 300 + '\n',
    'long_identifier': 'log ' + 'a' * 300 + '\n',
    'long_parameter': 'def test(' + 'x' * 100 + ')\n    return 1\nlog test(0)\n',
    'too_many_parameters': 'def test(' + ','.join('x' + str(i) for i in range(17)) + ')\n    return 1\nlog test(0)\n',
    'line_limit': '\n' * 2048 + 'log "lost"\n',
    'long_line': 'log 1' + ' ' * 1100 + '\nlog 2\n',
    'bom': '\ufefflog "hello"\n',
    'variable_limit': ''.join(f'int v{i} = {i}\n' for i in range(257)),
    'function_limit': ''.join(f'def fn{i}(x)\n    return x\n' for i in range(129)) + 'log fn128(1)\n',
    'input_eof': 'int x = input()\nlog x\n',
    'input_origin': 'str s = input()\nint n = s\nlog n\n',
    'input_reassignment': 'int n = 1\nn = input()\nlog n + 1\n',
    'long_string': 'log "' + 'a' * 256 + '"\n',
    'outer_else_if': 'if true\n    if false\n        log "inner"\nelse if missing\n    log "outer"\nlog "done"\n',
    'long_line_number': 'log 1' + ' ' * 1100 + '\nlog missing\n',
    'error_loop': 'while true\n    log missing\n',
    'float_zero_modulo': 'log 1.0 % 0.0\n',
    'concat_truncation': 'str a = "' + 'x' * 200 + '"\nstr b = a + a + a + a + a + a\nlog b\n',
}

out = ROOT / 'review' / 'cases'
out.mkdir(exist_ok=True)
results = []
for name, source in CASES.items():
    case = out / (name + '.sian')
    case.write_text(source, encoding='utf-8')
    try:
        p = subprocess.run([str(ROOT / 'review' / 'Sianlang-review.exe'), str(case)],
                           input='12\n' if name.startswith('input_') and name != 'input_eof' else '',
                           capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=3)
        result = dict(case=name, exit=p.returncode, stdout=p.stdout, stderr=p.stderr)
    except subprocess.TimeoutExpired:
        result = dict(case=name, timeout=True)
    results.append(result)
    print(json.dumps(result, ensure_ascii=False))
(ROOT / 'review' / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
