# 검증 방법

프로젝트 루트에서:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

GCC로 경고를 오류로 취급해 빌드하고, Python 실행기 테스트와 Node.js 확장 테스트를 통과한 뒤 공식 실행 파일 및 VSIX를 갱신합니다.

개별 실행:

```powershell
python tests/test_runtime.py
python tests/test_runtime.py --exe build/Sianlang.exe
python tests/test_v03.py --exe build/Sianlang.exe
python tests/test_v04.py --exe build/Sianlang.exe
node tests/test_extension.js
```

## 실행기

- 원래 53개 재현 입력은 `review/cases`에 있으며 이전 실행 결과를 덮어쓰지 않습니다.
- 현재 기대 출력·오류·종료 코드는 `test_runtime.py`에서 검증합니다.
- 기존 예제, 공식 문서의 모든 sian 코드 블록, 함수 범위·재귀·반복·오류 흐름을 검증합니다.
- 숫자 경계와 800개 산술 결과를 Python 정수 계산과 비교합니다.
- 긴 입력, UTF-8 문자열과 BOM, 한글/이모지/셸 특수문자 파일명, 길이·호출·문법 깊이 한도를 검증합니다.
- 함수/문자열 20,000회 반복 및 오류 반환 경로를 검증합니다. 실행기는 종료 시 살아 있는 런타임 문자열 수가 0인지 검사합니다.
- 0.3 테스트는 콘솔 log의 Python 비교(실수 500개 포함), log.f, 기본값/이름 지정 인수/#묶음, 클로저, None, try/catch, 반복문의 else를 검증합니다. 순환 환경/함수 객체도 종료 시 0개인지 검사합니다.
- 0.4 테스트는 list/tuple/dict 리터럴, 인덱싱, len, for, range 세 가지 형식, range 오류, dict key 순회, time.now를 검증합니다.
- 임시 입력은 `tests` 아래에 만들고 정리합니다. 각 프로세스는 5초 제한을 둡니다.

## 확장

VS Code API를 대체한 테스트 환경에서 파일 저장, 실행 인수, 파일 누락, 저장 실패, 실행 실패 등 8개 시나리오를 검증합니다. VS Code Extension Host에서 F6와 입력을 직접 사용하는 UI 통합 검증은 별도입니다.

## AddressSanitizer

MSVC의 AddressSanitizer가 설치된 Developer Command Prompt에서 다음 명령으로 메모리 접근 오류를 검사할 수 있습니다.

```bat
cl /nologo /std:c11 /fsanitize=address /Zi main.c /Fo:build\sian-asan.obj /Fe:build\Sianlang-asan.exe /Fd:build\sian-asan.pdb /link /INCREMENTAL:NO
python tests\test_runtime.py --exe build\Sianlang-asan.exe
python tests\test_v03.py --exe build\Sianlang-asan.exe
```

`build` 폴더는 기본 빌드에서 생성됩니다. ASan은 모든 종류의 버그나 누수 부재를 보증하지 않습니다.
# 배포판 검증 (0.3.1)

`build.ps1`은 실행기 241회, 확장 모의 시나리오 14개, ZIP/VSIX/체크섬/독립 실행 검증을 수행합니다.
`python tests/test_release.py`는 한글·공백·셸 기호가 있는 임시 경로에 ZIP을 풀고 개발 도구 없는 PATH로 실행합니다.

실제 VS Code 확장 환경 테스트는 별도 프로필에 설치한 후 실행합니다. 기존 사용자 확장과 설정은 변경하지 않습니다.

```powershell
code --user-data-dir build/vscode-release-profile --extensions-dir build/vscode-release-extensions --install-extension dist/0.3.1/sianlang-vscode-0.3.1.vsix --force
powershell -ExecutionPolicy Bypass -File tests/test_extension_host.ps1
```

확장 활성화, `.sian` 언어 인식, 실행 명령을 통한 프로세스 정상 종료, 포함된 실행기 경로, SVG 매핑을 확인합니다.
결과는 `build/extension-host-result.json`에 저장됩니다. GUI에서 아이콘 모양을 시각 비교하는 테스트는 아닙니다.
