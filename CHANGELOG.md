# SianLang 0.5.1 (development)

- `Frodot` saves JSON back into the `.rodot` file when its block finishes successfully.
- `rodot.save = true|false` controls in-memory sprite state across scenes (default false); the old `rodot.save(sprite)` function is removed.
- Carry-over is keyed by variable name and file path, so two loaded sprites remain independent.

# SianLang 0.5.0 (development baseline)

- Scene blocks, explicit scene changes, automatic Windows game window loop, configurable FPS, and real-time key state.
- `.rodot` PNG + JSON files with `rodot.create/load/save`, source image dimensions, and temporary `Frodot` edits.
- Automatic sprite rendering and `draw.rect/circle/line/text` commands; VS Code highlighting and snippets.
- Double-buffered drawing and elapsed-time frame pacing; VS Code `.rodot` image/JSON editor.
- Self-contained `sianlanggameex1` PNG conversion and game example in the portable ZIP.

# SianLang 0.4.2

- Fjson 세이브 데이터 조작 들여쓰기 블록 추가 (`Fjson "path.json"`).
- Fjson 데이터 연산 키워드 구현: `replace` (값 대입 및 `+=`, `-=`, `*=`, `/=` 복합 대입), `add` (List append 및 Dict 신규 Key 생성), `delete` (Key 삭제).
- Fjson 원자적 저장 (Atomic Write) 및 `j.` 데이터 스코프 지원.
- JSON 게임 세이브 데이터 조작 예제 파일 추가 (`examples/fjson-demo.sian`).

# SianLang 0.4.1

- 컬렉션 인덱스 대입 지원 (`list[idx] = val`, `dict[key] = val`, 음수 인덱싱 및 딕셔너리 신규 키 추가).
- 리스트/딕셔너리 메서드 추가 (`list.append`, `list.pop`, `dict.keys`, `dict.values`, `dict.items`).
- UTF-8 문자열 인덱싱 (`str[idx]`) 및 `for` 문 글자 단위 순회 지원.
- 수학 내장함수 (`abs`, `min`, `max`, `round`) 및 난수 내장함수 (`random.int`, `random.float`, `random.choice`) 추가.
- 파일 I/O 내장함수 및 메서드 추가 (`open`, `file.read`, `file.write`, `file.close`) 및 `try/catch` 예외 처리 연동.
- 객체 pointer equality 비교 연산자 (`==`, `!=`) 컬렉션 및 파일 객체 확장.

# SianLang 0.4.0

- list, tuple, dict 리터럴과 인덱싱 추가.
- `for item in ...` 반복과 `range()` 세 가지 형식 추가.
- `time.now()` 현재 Unix timestamp 추가.
- 0.4 컬렉션·반복 회귀 테스트 추가.

# SianLang 0.3.1

- Windows x64 실행기를 포함한 VSIX와 예제·문서를 포함한 설치 없는 ZIP 배포.
- SHA-256 체크섬과 정적 다운로드 페이지 생성. `build.ps1`로 빌드·테스트·패키징 수행.
- VS Code에서 저장된 단일 파일 실행, 명시적인 실행기 경로 설정, 작업 공간 신뢰 검사.
- 제작자의 `Sianlangicon.svg`를 `.sian` 파일 아이콘으로 사용. 기본 언어 아이콘 및 선택형 파일 아이콘 테마 제공.
- 기본 문법 스니펫과 실행 버튼 추가. 기존 0.3.0 언어 문법 유지.

# SianLang 0.3.0

## 추가·변경

- `log`를 여러 값을 출력하는 함수로 확장. `sep`, `end`, `flush`, `file=None`, 빈 호출과 None 반환 지원. 콘솔 출력에 한정하며 파일 출력은 후속 전용 명령으로 계획.
- `log.f`에 `{표현식}`, `{{`/`}}`, `!s`/`!r`, 정렬·너비·정밀도·숫자 표시 추가. 문자열 변수와 함수로 전달한 log.f도 호출 위치에서 치환.
- bool 출력은 True/False, 정수값 실수는 1.0처럼 출력. 리터럴 true/false는 유지.
- while/loop/repeat의 else. 정상 완료 시 실행하고 break/return/전파된 오류에서는 생략.
- 함수 기본값을 정의 실행 시 한 번 평가. 이름 지정 인수, 마지막 `#rest`, 묶음 인덱싱/len/#펼치기 지원.
- 중첩·조건부 함수와 클로저, 함수 저장·전달·반환·연속 호출 지원. 함수는 정의 문장 실행 후부터 사용.
- `var`와 `None`. 기존 명시적 타입 변수의 재대입 규칙 유지.
- `iferror` 삭제. 들여쓰기 기반 `try`/`catch`와 선택적 오류 메시지 바인딩 추가.
- 클로저와 환경의 순환 참조 회수. 평가 중인 임시 함수 값도 보존.
- 기존 예제 및 신규 기능 회귀 검증, Python 콘솔 출력 비교, AddressSanitizer 검증.

당시 게임 기능과 파일/키 입력 계획은 [보관된 계획서](history/게임기능-계획.md), 현재 문법은 [규칙서](설명서/시안랭-문법규칙서.md), 실행 예제는 [features-demo.sian](features-demo.sian)을 참고하세요.

# SianLang 0.2.0 (이전 변경 기록)

## 수정

- 실행 전 문법 검사와 문법 트리 도입. 주석/문자열/괄호/들여쓰기/표현식 끝 검증.
- 정수 덧셈·뺄셈, 문자열 비교, 나머지 계산, 0 나눗셈과 오버플로 처리.
- 64비트 정수. `/`는 항상 실수. `%` 결과의 부호는 제수와 동일.
- 선언 타입을 재대입에서도 유지. 명시적 변환 함수 추가.
- 함수 매개변수와 지역 선언이 전역을 덮어쓰던 오류 수정. 호출자의 지역 변수를 읽지 않음.
- 빈 매개변수 함수, 독립 호출, `and`/`or`, `break`/`continue` 지원.
- 모든 오류의 즉시 전파. 함수 안 `iferror`도 전체 프로그램 중단.
- 반복 조건을 첫 진입에도 정확히 한 번 평가.
- 문자열 참조 수와 환경 수명 관리. 정상/오류 반환 모두 메모리 정리.
- 입력의 선언/재대입 변환 통일. 숨겨진 입력 출처 제거. EOF와 긴 입력 처리.
- 고정 줄/변수/함수 배열 제거. 길이·숫자·호출·문법 깊이 초과를 명시적 오류로 처리.
- UTF-8 BOM, 긴 줄, 긴 문자열, 이스케이프 지원.
- VS Code 실행을 ProcessExecution으로 변경. 저장 실패/실행 실패 처리.
- 확장 버전 0.2.0, 공식 실행 파일 이름 Sianlang.exe 통일, 파일 아이콘 테마 등록.
- 이전 설명서의 한글 키워드/중괄호/미지원 설명을 현재 구현으로 교체.

## 호환성에 영향을 주는 규칙

- `int n = 5 / 2`는 오류. 소수 결과는 float로 받고 버림이 필요하면 `int(5 / 2)` 사용.
- `"score=" + 10`은 오류. `"score=" + str(10)` 사용.
- int 변수에 str을 재대입할 수 없음.
- 함수는 최상위에서만 정의. 중첩/조건부/중복 정의는 오류.
- 함수 범위는 지역 → 전역. 동일 이름의 매개변수/지역 선언은 전역을 가림.
- `str s = input()` 뒤 `int n = s`는 오류. `int n = int(s)` 사용.
- 잘못된 문법은 실행되지 않는 블록 안에서도 실행 전에 오류.

원본 검토와 당시 실행 결과는 `history/review/검토보고서.md`, `results.json`에 보존했습니다. 현재 기대 결과는 `tests/test_runtime.py`에 있습니다. 원본 C 파일은 `history/review/main-before-fixes.c`에 보관했습니다.
