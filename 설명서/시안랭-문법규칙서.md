# SianLang 0.5.1 문법 규칙서

이 문서는 SianLang 0.5.1 구현 기준이다. 컬렉션, 파일 I/O, Fjson과 장면·rodot·Frodot 게임 문법을 포함한다.

## 실행

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic main.c -o Sianlang.exe
.\Sianlang.exe examples/hello.sian
.\Sianlang.exe --version
```

자동 빌드와 검증은 `powershell -ExecutionPolicy Bypass -File .\build.ps1`로 실행한다. 공식 이름은 `Sianlang.exe`이며 `SianlangA.exe`는 동일 바이너리의 호환용 사본이다.

## 소스와 이름

- `.sian` 확장자, UTF-8 텍스트. 맨 앞 BOM은 허용한다. 줄바꿈은 LF/CRLF/CR이다.
- 이름은 영문자 또는 `_`로 시작하며 이후 영문자·숫자·`_`를 쓴다. 대소문자를 구분한다.
- 키워드, 자료형 이름, 내장 함수 이름은 변수·함수·매개변수 이름으로 쓸 수 없다.
- **주의**: `add`, `replace`, `delete`는 Fjson 블록 내 특수 조작 명령어로 해석되므로 일반 변수명이나 함수명으로 사용하는 것을 권장하지 않습니다.
- 한 줄에는 한 문장을 쓴다. 표현식의 물리적 줄 나눔, 세미콜론, 중괄호, 끝의 콜론은 지원하지 않는다.
- 모든 문법을 검사한 뒤 실행한다. 실행되지 않는 블록의 문법 오류도 실행 전에 보고한다.

## 주석과 문자열

`||`는 한 줄 주석, `^^ ... ^^`는 여러 줄 주석이다. 문자열 안에서는 주석 기호가 일반 문자다. 주석은 중첩하지 않으며 닫히지 않은 블록 주석은 오류다.

문자열은 큰따옴표로 감싼다. 지원하는 이스케이프는 `\n`, `\r`, `\t`, `\"`, `\\`다. 알 수 없는 이스케이프는 오류다. 작은따옴표 문자열과 여러 줄 문자열은 지원하지 않는다.

```sian
|| 주석
str title = "Sian || ^^ Game"
log title
log "첫 줄\n다음 줄"
```

## 자료형과 변수

| 타입 | 규칙 |
| --- | --- |
| `int` | 부호 있는 64비트 정수: -9223372036854775808부터 9223372036854775807 |
| `float` | 유한한 배정밀도 실수. NaN/무한대와 리터럴·입력의 범위 초과는 오류 |
| `str` | UTF-8 문자열 |
| `bool`, `TF` | 동일한 불리언 타입. 값은 `true`, `false` |

```sian
int score = 0
score = score + 1
float speed = 3
str title = "Game"
bool running = true
log score
```

변수는 먼저 타입을 붙여 선언한다. 재대입도 선언 타입을 유지한다. `int` 값을 `float`에 넣는 승격만 자동 허용한다. 불리언은 숫자 산술에 자동 변환되지 않는다.

같은 환경에서 같은 타입으로 다시 선언하면 값을 다시 초기화한다. 반복문 안의 선언도 이 규칙을 따른다. 같은 이름을 다른 타입으로 다시 선언하는 것은 오류다. `if`, 반복문, try/catch는 별도의 변수 환경을 만들지 않는다.

명시적 변환 함수는 `int(value)`, `float(value)`, `str(value)`, `bool(value)`, `TF(value)`다. 문자열의 숫자 변환은 앞뒤 공백을 허용하지만 나머지 문자가 남으면 실패한다. 실수 문자열은 10진법과 지수 표기만 허용한다. `int(float)`는 소수 부분을 0 방향으로 버리며 범위를 검사한다. 문자열의 bool 변환은 `true`/`false`만 허용한다. 숫자의 bool 변환은 0인지로 정한다.

```sian
int age = int("20")
log "age=" + str(age)
```

## None과 var

`None`은 값이 없음을 나타내는 실제 값이다. 출력, 저장, 비교, 인수 전달이 가능하고 조건에서는 거짓이다. 반환문 없이 끝난 함수와 값 없는 `return`도 None을 반환한다.

타입을 고정하지 않고 함수·None 등 다양한 값을 저장하려면 `var`로 선언한다. 기존 `int`, `float`, `str`, `bool` 선언은 여전히 타입을 유지한다.

```sian
var callback = None
log callback == None
callback = log
callback("hello")
```

## 출력과 입력

`log`는 Python print의 콘솔 출력 호출 규칙을 따른다.

```text
log(*objects, sep=" ", end="\n", file=None, flush=false)
```

위 `*objects`는 설명용 표기다. SianLang에서 인수 묶음을 펼칠 때는 `#묶음`을 쓴다.

- 값은 0개 이상 전달할 수 있다. `log()`는 줄바꿈 하나를 출력한다.
- `sep`는 값 사이 구분자, `end`는 마지막 문자열이다. 이름을 지정해서만 전달한다.
- `sep=None`, `end=None`은 기본값을 사용한다. 다른 값은 문자열이어야 한다.
- `flush`가 참이면 출력 버퍼를 즉시 비운다. 값의 참/거짓 판정을 사용한다.
- 현재는 콘솔 전용이다. `file=None`만 허용하며 파일 전용 명령어는 후속 계획이다.
- 반환값은 None이다. bool은 `True`/`False`, 정수값 실수는 `1.0`처럼 Python 방식으로 출력한다. 언어의 bool 리터럴과 입력 문자열은 기존 `true`/`false`다.
- 기존 `log value`, `log value1, value2`도 사용할 수 있다. `log (a) + b`는 기존 괄호 표현식 출력으로 유지한다.
- 사용자 객체·파일 객체는 아직 없으므로 Python 객체의 `__str__`/쓰기 프로토콜까지 호환하는 것은 아니다. 함수 표시는 `<function 이름>`이다.

```sian
log("score", 10, true, None, sep=" | ")
log("loading", end="...", flush=true)
log()
```

### log.f 포매팅

`log.f`는 위치 인수의 문자열 안 `{표현식}`을 호출 위치의 변수 환경에서 평가한다. `sep`/`end` 등 출력 옵션은 log와 동일하다. 일반 `log`는 중괄호를 치환하지 않는다.

```sian
int score = 12
float speed = 1.25
log.f("점수: {score}, 다음 점수: {score + 1}")
log.f "속도: {speed:.2f}, {{중괄호}}"
str template = "score={score}"
log.f(template, end="!\n")
```

- `{{`, `}}`는 문자 `{`, `}`를 출력한다.
- `{값!s}`, `{값!r}`는 문자열 변환과 표시용 표현을 사용한다.
- 지원하는 서식: 정렬 `<`, `>`, `^`, ASCII 채움 문자, 너비, 숫자 부호 `+`/공백, 0 채움, 정밀도, `s`, `d`, `x`, `X`, `b`, `o`, `f`, `F`, `e`, `E`, `g`, `G`, `%`.
- 예: `{score:04d}`, `{speed:08.2f}`, `{score:x}`, `{speed:.1%}`.
- 숫자 정밀도는 `f`, `e`, `g` 등 타입과 함께 쓴다. 중첩된 서식 필드와 Python f-string의 디버그 `=` 등은 지원하지 않는다.
- 직접 작성한 포맷 리터럴의 문법은 실행 전에 검사한다. 문자열 변수에 담긴 포맷 오류는 실행 중 발생하므로 try/catch로 처리할 수 있다.
- `log.f`도 변수에 저장하거나 인수로 전달할 수 있다.

### input

`input()` 또는 `input("질문: ")`의 한 줄 입력 기능은 유지한다. 빈 줄은 빈 문자열이다. 더 읽을 줄이 없으면 EOF 오류다. 실시간 키 입력은 이번에 추가하지 않았다.

선언 또는 대입의 오른쪽 식 전체가 `input(...)`이면 변수의 선언 타입에 맞춰 변환한다. `var`는 문자열 그대로 받는다. 다른 곳에 저장한 문자열은 명시적으로 변환한다.

```sian
try
    int age = input("나이: ")
    age = input("새 나이: ")
    str text = input("숫자 문자열: ")
    int count = int(text)
    log age + count
catch error
    log "입력 형식이 올바르지 않습니다."
```

## 연산

| 연산 | 규칙 |
| --- | --- |
| `+`, `-`, `*` | 정수끼리는 정수. 실수가 섞이면 실수. 정수 범위 초과는 오류 |
| `/` | 항상 실수 나눗셈. `5 / 2`는 2.5 |
| `%` | 나머지. 0이 아닌 결과의 부호는 오른쪽 수와 같음. 실수도 지원 |
| `+` (문자열) | 문자열 두 개만 연결. 숫자는 `str(...)`로 변환 |
| `==`, `!=` | 숫자끼리 값 비교, 문자열끼리 내용 비교, bool끼리 값 비교. 서로 다른 비숫자 타입은 같지 않음 |
| `<`, `<=`, `>`, `>=` | 숫자끼리 또는 문자열끼리 비교. 문자열 순서는 UTF-8 바이트 순서 |
| `!` | 참/거짓 반전 |
| `and`, `or` | 단락 평가. 결과는 bool. 결과가 정해지면 오른쪽 식을 실행하지 않음 |

`/`와 `%`의 오른쪽이 0이면 오류다. 나머지 예: `-5 % 2`는 1, `5 % -2`는 -1, `5.0 % 2`는 1.0이다. 부동소수점은 근삿값이므로 매우 큰 정수의 실수 변환이나 실수 계산에는 정밀도 한계가 있다.

우선순위는 높은 순서로 괄호/호출 → 단항 `+`, `-`, `!` → `*`, `/`, `%` → `+`, `-` → 비교 → `and` → `or`다. 연쇄 비교 `a < b < c`는 지원하지 않는다. `a < b and b < c`로 쓴다.

조건에서는 숫자 0, 빈 문자열, false가 거짓이고 나머지는 참이다. None과 빈 가변인수 묶음도 거짓이며 함수 값은 참이다.

## 컬렉션

현재 컬렉션은 `list`, `tuple`, `dict` 세 가지입니다. 컬렉션은 `var`로 저장합니다.

```sian
var items = [1, 2, 3]
var point = (10, 20)
var user = {"name": "Sian", "age": 20}

log items[0]
log point[-1]
log user["name"]
log len(items), len(point), len(user)
```

- `list`는 대괄호로 만들며 순서가 있는 값 묶음입니다.
- `tuple`은 괄호로 만들며 순서가 있는 불변 값 묶음입니다.
- `dict`는 중괄호 안에 `key: value`를 적으며 key로 값을 조회합니다.
- 빈 컬렉션도 사용할 수 있습니다: `[]`, `()`, `{}`.
- 현재 인덱스 대입과 `append`, `pop`, `keys`, `values`는 다음 단계에서 추가합니다.

## for와 range

`for item in 값`은 list, tuple, dict, `range()`를 순회합니다. dict를 순회하면 key가 나옵니다.

```sian
for number in range(1, 6)
    log number

for item in ["a", "b", "c"]
    log item
```

`range`는 다음 형식을 지원합니다.

```text
range(stop)
range(start, stop)
range(start, stop, step)
```

`stop`은 포함하지 않으며 `step`은 0일 수 없습니다. `for`에는 기존 `break`, `continue`, 반복문 `else`를 사용할 수 있습니다.

## 현재 시간

`time.now()`는 현재 Unix timestamp를 정수로 반환합니다.

```sian
int started = time.now()
log started
```

## 인덱스 대입 및 컬렉션 메서드

### 인덱스 대입 (Index Assignment)
리스트(`list`)와 딕셔너리(`dict`)에 인덱스 및 키를 이용해 값을 변경하거나 추가할 수 있습니다.
- `list[index] = value`: 지정 위치 값 변경 (음수 인덱스 지원)
- `dict[key] = value`: 기존 키 값 변경 또는 신규 키 추가

```sian
var items = [10, 20, 30]
items[0] = 99
items[-1] = 77
log items  || [99, 20, 77]

var user = {"name": "Sian"}
user["score"] = 100
log user
```

### 컬렉션 메서드
- `list.append(value)`: 리스트 끝에 항목 추가
- `list.pop()`: 리스트 마지막 항목 제거 및 반환
- `dict.keys()`: 키 목록 리스트 반환
- `dict.values()`: 값 목록 리스트 반환
- `dict.items()`: (키, 값) 튜플 목록 리스트 반환

```sian
var inv = ["검"]
inv.append("방패")
var top = inv.pop()

var d = {"a": 1, "b": 2}
for pair in d.items()
    log pair
```

## 문자열 인덱싱 및 순회

UTF-8 멀티바이트 글자 단위 인덱싱 및 `for` 순회가 가능합니다.
- `str[index]`: 글자 가져오기 (음수 인덱스 지원)
- `for ch in str`: 글자 단위 반복 순회

```sian
str name = "시안랭"
log name[0]  || "시"
log name[-1] || "랭"

for ch in "시안"
    log ch
```

## 수학 및 난수 내장함수

### 수학 내장함수
- `abs(x)`: 절대값 반환
- `min(a, b, ...)` / `min(list)`: 최솟값 반환
- `max(a, b, ...)` / `max(list)`: 최댓값 반환
- `round(x, [ndigits])`: 반올림 수행

### 난수 내장함수 (`random.*`)
- `random.int(min, max)`: 범위를 포함하는 정수 난수
- `random.float()`: `[0.0, 1.0)` 실수 난수
- `random.choice(list)`: 리스트 무작위 요소 추출

```sian
log abs(-42)
log min(10, 20, 5)
log round(3.14159, 2)
int dice = random.int(1, 6)
var item = random.choice(["사과", "바나나"])
```

## 파일 I/O

파일 열기, 읽기, 쓰기, 닫기를 지원합니다. 존재하지 않는 파일 열기는 `try/catch`로 예외 포착이 가능합니다.
- `open(path, mode)`: 파일 열기 (`"r"`, `"w"`, `"a"`, `"rb"`, `"wb"`, `"ab"`)
- `file.write(text)`: 텍스트 쓰기
- `file.read()`: 내용 전체 읽기
- `file.close()`: 파일 닫기

```sian
var file1 = open("test.txt", "w")
file1.write("안녕 시안랭\n")
file1.close()

var file2 = open("test.txt", "r")
str content = file2.read()
file2.close()
log content
```

## 들여쓰기와 조건문

공백 4칸을 권장한다. 탭 1개는 공백 4칸으로 계산한다. 같은 블록은 같은 깊이여야 하고, 깊이가 줄 때는 기존 바깥 블록의 깊이로 돌아와야 한다. 최상위는 들여쓰지 않는다. 빈 줄과 주석만 있는 줄은 깊이를 변경하지 않는다.

`if`, `else if`, `else`, `while`, `loop`, `repeat`, `try`, `catch`, 함수 정의 뒤에는 들여쓴 문장이 하나 이상 필요하다. 빈 블록은 허용하지 않는다. 조건의 괄호는 선택 사항이다.

```sian
int score = 80
if score >= 90
    log "great"
else if score >= 60
    log "pass"
else
    log "retry"
```

## 반복문

`repeat`는 0 이상의 int 횟수를 한 번 계산한다. 음수·실수·문자열 횟수는 오류다. `while`과 별칭 `loop`는 매 반복 시작에 조건을 정확히 한 번 계산한다.

```sian
int count = 0
while count < 3
    log count
    count = count + 1
repeat 2
    log "again"
```

`break`는 가장 가까운 반복문을 종료한다. `continue`는 다음 반복으로 넘어간다. 반복문 밖에서 쓰면 문법 오류다. 중첩 함수는 바깥 반복문의 break/continue 범위를 물려받지 않는다.

반복문 뒤에 같은 깊이의 `else`를 붙일 수 있다. 조건이 거짓이 되거나 지정 횟수를 끝내면 실행하며, 처음부터 0회여도 실행한다. 해당 반복문에서 break, return, 전파되는 오류로 빠져나오면 실행하지 않는다. continue는 정상 완료를 막지 않는다.

```sian
repeat 2
    log "tick"
else
    log "completed"
```

## 함수, 인수, 클로저

대표 정의 키워드는 `def`다. `func`, `f`, `function`도 유지한다. 함수를 정의하는 문장이 실행될 때 함수와 기본값이 만들어진다. 호출하기 전에 정의 문장을 실행해야 한다. 중첩 함수와 조건부 정의도 가능하다.

```sian
def show(a, b=2, #c)
    log(a, b, #c, sep=" | ")
    log("extra count", len(c))
show(1)
show(b=4, a=3)
show(1, 2, "extra", None)
```

- 위치 인수와 `이름=값` 인수를 지원한다. 같은 인수의 중복 전달·알 수 없는 이름·필수 인수 누락은 오류다.
- 이름 지정 인수 뒤에 일반 위치 인수는 올 수 없다. `#묶음` 펼치기는 허용하며 위치 인수로 먼저 바인딩한다.
- 기본값은 함수 정의 시 정의 위치의 환경에서 한 번 평가한다. 매번 호출할 때 재평가하지 않는다.
- 기본값이 있는 일반 매개변수 뒤에는 기본값 없는 일반 매개변수를 둘 수 없다.
- `#c`는 마지막 매개변수여야 하며 남은 위치 인수를 불변 묶음으로 받는다. 기본값을 붙이거나 `c=...`로 직접 전달할 수 없다. 딕셔너리 형태의 가변 이름 인수는 아직 없다.
- 묶음은 `c[0]`, `c[-1]`, `len(c)`로 읽고 `other(#c)`로 펼칠 수 있다. 범위 밖 인덱스는 오류다. 변경 가능한 일반 목록은 후속 계획이다.
- 매개변수는 호출별 지역 변수이며 타입을 고정하지 않는다. 내부의 명시적 타입 선언은 타입을 유지한다.

```sian
def make_counter(start=0)
    int count = start
    def next(amount=1)
        count = count + amount
        return count
    return next

var counter = make_counter(10)
log counter(), counter(amount=3)
```

변수 검색은 현재 함수 → 정의를 둘러싼 함수 환경들 → 전역 순서다. 호출자의 지역 변수는 검색하지 않는다. 바깥 함수가 반환해도 내부 함수는 필요한 환경을 유지한다. 이것이 클로저다. 같은 환경의 변수 변경은 그 환경을 공유하는 클로저에서 보인다.

지역 선언은 바깥 이름을 가린다. 재대입은 스코프 체인에서 발견한 가장 가까운 변수를 변경한다. Python의 `nonlocal`/`global` 키워드는 사용하지 않는다. 서로 다른 `make_counter()` 호출은 독립적인 환경을 갖는다.

함수는 `var`에 저장하고, 인수로 전달하고, 반환하고, 반환 직후 `make_counter()()`처럼 호출할 수 있다. 함수끼리의 동등 비교는 같은 함수 값인지 확인한다.

```sian
def apply(callback, value)
    return callback(value)
def double(value)
    return value * 2
log apply(double, 4)
```

`return value`는 값을 반환하며, 값 없는 return과 함수 끝 도달은 None을 반환한다. 호환성을 위해 최상위 return은 프로그램을 정상 종료한다. 같은 최상위 함수 이름의 중복 정의는 문법 오류다. 함수 정의 이름은 기존 명시적 타입 변수와 충돌할 수 없다.

## try / catch와 오류

`iferror` 명령은 삭제했다. 예외 처리는 들여쓰기만 사용하는 `try` / `catch`로 작성한다. 콜론이나 중괄호는 쓰지 않는다.

```sian
try
    int value = int("bad")
    log "not reached"
catch error
    log.f("입력 오류: {error}")
log "continued"
```

- try에는 catch 블록이 하나 필요하다. `catch`만 쓰거나 `catch error`로 메시지 문자열을 받을 수 있다.
- try의 실행 오류만 잡는다. 프로그램의 문법 오류는 실행 전에 발생하므로 잡을 수 없다.
- 잡힌 오류는 stderr에 출력하지 않고 catch를 실행한 뒤 다음 문장으로 진행한다. 함수 내부 오류도 호출자를 통해 전파된다.
- catch에서 새 오류가 나면 같은 catch가 다시 잡지 않으며 바깥 try/catch로 전파한다.
- 오류가 없으면 catch를 건너뛴다. return/break/continue는 예외가 아니므로 catch를 실행하지 않는다.
- 이미 성공한 출력·대입은 되돌리지 않는다. 실패한 식의 결과는 저장하지 않는다.
- catch 이름은 현재 환경에 문자열로 바인딩되어 이후에도 사용할 수 있다. 기존 var/str 변수는 재사용할 수 있고 다른 명시적 타입 변수와 충돌하면 오류다.
- 예외 타입별 분기, finally, 직접 예외를 던지는 전용 명령은 이번 범위에 포함하지 않는다.

## Fjson 데이터 조작 블록

`Fjson`은 게임 세이브 데이터 및 구조화된 JSON 파일 조작에 특화된 전용 블록입니다.

```sian
Fjson "save.json"
    replace j.player.hp = 100
    replace j.player.hp += 50
    add j.inventory = "전설의 검"
    delete j.temp_buff
```

- **스코프 구분 (`j.`)**: JSON 내부 데이터 경로(예: `j.player.hp`)와 SianLang 외부 변수의 스코프를 명확히 분리합니다.
- **원자적 저장 (Atomic Write)**: 들여쓰기 블록이 정상 종료될 때 파일에 자동으로 안전하게 원자적 저장(Atomic Save)됩니다.
- **조작 키워드 역할 분담**:
  - `replace`: 기존 데이터의 값 대입 및 복합 연산(`+=`, `-=`, `*=`, `/=`)을 수행합니다.
  - `add`: List(배열) 요소 추가(`append`) 및 객체(Dict)의 신규 Key를 생성합니다.
  - `delete`: Key 또는 데이터를 삭제합니다.
- **이름 권장사항**: `add`, `replace`, `delete` 키워드는 파서가 Fjson 구문에서 특별하게 해석하므로 일반 함수나 변수 이름으로 사용하는 것은 권장하지 않습니다.

처리하지 않은 오류는 파일·줄·열(바이트 기준)·원인과 호출 위치를 출력하고 종료 코드 1로 끝난다. 성공과 처리된 오류 뒤 정상 종료는 0이다.

## 0.5.1 게임 장면과 스프라이트

게임 창은 Windows에서 지원한다. 장면 정의는 파일의 최상위에 둔다. 같은 장면 이름을 두 번 선언하거나 장면을 블록 안에서 정의하면 문법 오류다. 장면 이름은 영문 식별자다.

```text
game.fps = 60
game.start("play", 640, 480, "My Game")

scene play
    var player = rodot.load("player.rodot")
    var initialized = false
    if initialized == false
        player.x = 100
        initialized = true
    if key.down("right")
        player.x += player.speed * game.delta_time()
    if key.down("q")
        game.close()
```

`game.start`는 지정한 장면으로 창을 연다. 장면 본문은 매 프레임 반복되며 `scene.change("name")`을 호출해야 다른 장면으로 전환된다. 장면의 최상위 `var` 선언은 장면 진입 후 첫 실행에서만 값을 만들고 다음 프레임에는 유지한다. 일반 대입은 매 프레임 다시 실행하므로 시작 위치는 `initialized` 같은 조건으로 보호한다. `game.fps`는 1~240의 정수, 기본값은 60이다. `game.delta_time()`은 실제 경과 초를 반환한다.

`key.down("key")`는 게임 창에 포커스가 있고 키가 눌려 있으면 참이다. 방향키, Enter, Space, 영문 한 글자를 지원한다. `draw.rect`, `draw.circle`, `draw.line`, `draw.text`는 현재 프레임에 도형·글자를 그린다. `.rodot` 스프라이트는 장면의 변수에 보관하면 자동으로 그려진다.

`.rodot`은 PNG 바이트와 JSON(`name`, `meta`, `data`)을 함께 담는다. `rodot.create("sprite.png", "player.rodot")`으로 만들고 `rodot.load("player.rodot")`으로 불러온다. `player.x`는 `player.data.x`의 간단한 표기다. 직접 대입은 실행 중 임시 변경이다. `Frodot player` 블록에서 `r.data`에 `replace`, `add`, `delete`를 사용하면 블록 정상 종료 시 원본 `.rodot` JSON에 자동 저장한다. `rodot.save = true`는 다음 장면까지 임시 데이터를 유지하며 기본값 `false`는 재로드 때 파일값을 사용한다. `name`과 `meta`는 읽기 전용이다. `collision(player, enemy)`는 두 스프라이트의 축 평행 충돌 사각형을 검사하고 이미지 회전은 반영하지 않는다.

명령별 인수와 예제는 [게임 명령어 사전](../docs/commands/index.md), [0.5.1 저장 규칙](0.5.1-계획.md)을 참고한다.

한도는 조용한 잘림 대신 오류로 보고한다.

| 항목 | 한도 |
| --- | --- |
| 소스 파일 | 16 MiB |
| 개별 문자열/입력 | 16 MiB |
| 이름 | ASCII 255바이트 |
| 함수 인수/매개변수 | 256개 |
| 문법 블록/표현식 깊이 | 128 |
| 동적 호출/식 평가 깊이 | 128 (복합 식에서는 호출 수가 이보다 적어도 도달 가능) |
| 실행 블록 깊이 | 256 |

줄·변수·함수 개수는 예전의 2048/256/128 고정 배열 한도를 없앴다. 이용 가능한 메모리와 소스 크기 제한을 따른다. 유효한 무한 반복은 자동 중단하지 않으며 터미널에서 Ctrl+C로 중단할 수 있다.
