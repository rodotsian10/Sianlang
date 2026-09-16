# AI를 위한 SianLang (시안랭) 종합 이해 및 사양서

이 문서는 AI LLM / 코딩 에이전트가 **SianLang(시안랭)** 소스 코드를 읽고, 작성하고, 디버깅하며, 언어 엔진을 확장할 때 참고하도록 작성된 완전한 시스템 및 문법 참조 가이드이다.

---

## 1. 언어 개요 및 설계 철학

- **언어명**: SianLang (시안랭)
- **목표**: 2D 게임 제작에 특화된 직관적이고 쉬운 프로그래밍 언어.
- **구현 기반**: C 언어 (GCC 기반 컴파일, C11). 외부 컴파일러나 런타임 라이브러리 의존성 없는 독립 포터블 실행기(`Sianlang.exe`).
- **구문 스타일**: Python 스타일의 오프사이드 룰(Offside-rule, 공백 4칸 들여쓰기 구문)과 직관적인 키워드 구조.

---

## 2. 완벽 문법 명세 (Syntax & Semantics)

### 2.1 변수 선언 및 타입 시스템
- **동적 타입 변수**: `var x = 10`
- **명시적 타입 변수**: `int a = 1`, `float b = 3.14`, `str s = "hello"`, `bool flag = true`
- **자료형 종류**:
  - `NoneType`: `None` (파이썬의 None과 동일)
  - `int`: 64비트 부호 있는 정수
  - `float`: 64비트 double 실수
  - `bool`: `true` / `false`
  - `str`: UTF-8 유니코드 문자열
  - `list`: 가변 동적 배열 (`[1, 2, 3]`)
  - `tuple`: 불변 튜플 (`(1, 2)`)
  - `dict`: 가변 키-값 쌍 (`{"a": 1}`)
  - `function`: 사용자 정의 함수 / 클로저
  - `file`: 표준 파일 핸들 객체

### 2.2 제어문 (Control Flow)
- **조건문**:
  ```sian
  if score >= 90
      log "A"
  else if score >= 60
      log "B"
  else
      log "C"
  ```
- **반복문**:
  - `while condition`: 조건 반복
  - `loop condition`: `while`의 별칭
  - `repeat N`: N회 정수 지정 반복
  - `for var in collection`: 리스트/튜플/문자열/딕셔너리 순회
  - `break` / `continue`: 루프 제어
  - 루프 `else` 블록: break 없이 정상 종료 시 실행.

### 2.3 함수 정의 및 클로저
- 키워드: `def` (별칭: `func`, `f`, `function`)
- 기본값 매개변수: `def greet(name, msg="Hello")`
- 가변 인수: `def sum(#args)` (`#` 접두사)
- 키워드 인수: `def config(*kwargs)` (`*` 접두사)
- 클로저: 중첩 함수에서 바깥 스코프 변수 캡처 지원.

```sian
def make_counter(start)
    int count = start
    def inc()
        count = count + 1
        return count
    return inc
```

### 2.4 예외 처리 (Exception Handling)
- `try / catch [err_var]` 구문 지원 (콜론 없음, 들여쓰기 기반).

```sian
try
    var f = open("nonexist.txt", "r")
catch err
    log.f("파일 오픈 실패: {err}")
```

### 2.5 컬렉션 연산 및 메서드
- **인덱스 대입**: `items[0] = 100`, `dict["key"] = "val"` (List, Dict만 가변 가능)
- **리스트 메서드**: `list.append(val)`, `list.pop()`
- **딕셔너리 메서드**: `dict.keys()`, `dict.values()`, `dict.items()`
- **문자열 유니코드 인덱싱**: UTF-8 멀티바이트 글자 단위 인덱스(`str[0]`, `str[-1]`) 및 `for char in str` 지원.

### 2.6 수학 & 난수 유틸리티
- **수학 내장함수**: `abs(x)`, `min(a, b, ...)`, `max(a, b, ...)`, `round(x, [ndigits])`
- **난수 내장 모듈**:
  - `random.int(min, max)`: 범위를 포함하는 정수 난수
  - `random.float()`: `[0.0, 1.0)` 실수 난수
  - `random.choice(list)`: 리스트 무작위 선택

### 2.7 파일 입출력 (File I/O)
- `open(filename, mode)`: `"r"`, `"w"`, `"a"`, `"rb"`, `"wb"`, `"ab"` 모드
- `file.write(str)` / `file.read()` / `file.close()`

### 2.8 Fjson 세이브 데이터 조작 블록
SianLang 고유의 JSON 세이브 파일 자동 조작 전용 블록.

```sian
Fjson "save.json"
    replace j.player.hp = 100
    replace j.player.hp += 50
    add j.inventory = "전설의 검"
    delete j.temp_buff
```

#### 📌 Fjson 핵심 규약:
1. **`j.` 접두사**: JSON 파일 데이터 스코프를 나타냄 (외부 SianLang 변수와 명확히 분리).
2. **원자적 자동 저장 (Atomic Save)**: 들여쓰기 블록이 정상 완료되면 파일에 자동 직렬화 및 저장.
3. **조작 키워드**:
   - `replace`: 기존 값 변경 및 복합 대입(`+=`, `-=`, `*=`, `/=`).
   - `add`: List 요소 추가 또는 Dict 신규 키 생성.
   - `delete`: 키 또는 데이터 삭제.
4. **키워드 주의사항**: `add`, `replace`, `delete` 키워드는 파서가 특별히 처리하므로 일반 변수명/함수명으로 사용하는 것은 권장하지 않음.

---

## 3. C 인터프리터 내부 구조 (Architecture)

소스 코드는 `main.c` 단일 엔트리 포인트에서 다음 헤더 파일들을 순차 포함(Inclusion)하여 컴파일됨:

- `src/common.h`: 기본 타입, 매크로, 유틸리티
- `src/values.h`: `Value` 구조체 정의 (Tagged Union: Int, Float, Bool, String, List, Tuple, Dict, Function, File, None)
- `src/lexer.h`: 어휘 분석기, 어휘 토큰화 및 들여쓰기(`INDENT`/`DEDENT`) 추적
- `src/parser.h`: AST(구문 분석 트리) 생성기 및 Fjson 키워드 파서
- `src/calls.h`: C 내장 함수 바인딩 (math, random, file I/O 등)
- `src/runtime.h`: 트리가이드 AST 실행기, 스코프 환경(Environment) 관리 및 런타임 평가

---

## 4. 빌드, 테스트 및 배포 파이프라인

### 4.1 빌드 실행
```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```
- C 소스 컴파일 → 회귀 테스트(`test_runtime.py`) → VS Code 확장 패키징(`package_extension.py`) → 배포 ZIP 빌드 및 검증(`package_release.py`, `test_release.py`).

### 4.2 주요 테스트 파일
- `tests/test_runtime.py`: 인터프리터 전체 회귀 테스트 suite (100개 이상의 실행 시나리오).
- `tests/test_release.py`: 릴리스 아티팩트 SHA256, 다운로드 링크 및 무설치 실행기 검증.

---

## 5. AI 에이전트 지침 (Guidelines for AI)

1. **코드 생성 시 주의**:
   - 콜론(`:`)이나 중괄호(`{}`)를 `if`, `while`, `def`, `try` 뒤에 넣지 말고 공백 4칸 들여쓰기를 사용한다.
   - 주석은 `||` 또는 `#`을 지원하나 `||`가 관례이다.
   - Fjson 구문에서는 JSON 경로 참조 시 반드시 `j.` 접두사를 붙인다.
2. **새 기능 개발 시**:
   - C 인터프리터 수정 시 `src/lexer.h`, `src/parser.h`, `src/runtime.h` 순으로 구문을 추가한다.
   - `tests/test_runtime.py`에 테스트 케이스를 추가하고 `build.ps1`을 실행하여 검증한다.
   - 웹사이트/문서 업데이트 시 `forai/site_and_docs_guide.md` 지침을 엄격히 따른다.
