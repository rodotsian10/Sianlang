<p align="center"><img src="Sianlangicon.svg" width="96" alt="SianLang logo"></p>

# SianLang

**간단한 문법으로 시작하는 프로그래밍 언어.**

Python처럼 쉽게 배우고, 앞으로 2D 게임을 만들 수 있는 언어를 목표로 개발하고 있습니다.
현재 **0.3.1**은 콘솔 프로그램용 기본 문법을 제공하는 첫 배포판입니다.

[다운로드 웹사이트](https://rodotsian10.github.io/Sianlang/) · [릴리스 다운로드](https://github.com/rodotsian10/Sianlang/releases) · [문법 안내서](설명서/시안랭-문법규칙서.md)

[명령어 사전](https://rodotsian10.github.io/Sianlang/commands.html)에서 각 명령어를 문서별로 확인할 수 있습니다.

## 바로 사용하기

**Windows 10/11 x64**에서 사용할 수 있습니다. 사용자에게 Python, GCC, Node.js 설치가 필요하지 않습니다.

1. [릴리스](https://github.com/rodotsian10/Sianlang/releases)에서 ZIP 또는 VSIX를 다운로드합니다.
2. VS Code 확장 화면의 `…` → **Install from VSIX...**로 VSIX를 설치합니다.
3. `.sian` 파일을 저장하고 **F6** 또는 편집기 오른쪽 위 실행 버튼을 누릅니다.

```sian
str name = "SianLang"
log.f("Hello, {name}!")

def greet(who="친구")
    log.f("반가워요, {who}!")

greet()
greet(who="개발자")
```

ZIP에는 실행기, VS Code 확장, 실행 예제, 문법 문서가 포함됩니다.
콘솔에서는 `Sianlang.exe 프로그램.sian`으로 실행합니다. [자세한 설치 안내](배포/시작하기.md)

## VS Code 지원

- 문법 강조, 괄호 자동 닫기, 들여쓰기, 기본 문법 스니펫
- 실행기가 포함된 확장으로 F6 실행 및 터미널 입력
- 제작자의 SVG를 사용하는 `.sian` 파일 아이콘

아이콘을 적용하려면 **Preferences: File Icon Theme → SianLang File Icons**를 선택하세요.
기존 테마가 기본 언어 아이콘을 지원하면 테마 변경 없이도 표시됩니다.
언어 서버 기반 자동 완성·실시간 오류 표시·디버거는 아직 구현하지 않았습니다.
현재 VSIX 직접 설치를 제공하며 Marketplace 검색 설치는 별도 게시 후 지원할 예정입니다.

## 현재 언어 기능

`int`, `float`, `str`, `bool`, `var`, `None`, 조건문과 반복문, 반복문 `else`, 함수 기본값,
이름 지정 인수, `#가변인수`, 중첩 함수와 클로저, 함수 전달, `try`/`catch`, `log`와 `log.f`를 지원합니다.
범위는 들여쓰기로 구분합니다.

게임 창·실시간 키 입력·파일 명령·일반 컬렉션과 객체 기능은 [게임 기능 계획](게임기능-계획.md)에 정리했습니다.
현재 모든 Python 문법을 호환하는 언어는 아닙니다.

## 소스에서 빌드하기

개발자에게는 GCC, Python 3, Node.js가 필요합니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

경고를 오류로 검사하고 실행기·확장·배포 테스트를 실행한 뒤 `dist/0.3.1/`에 배포 파일을 만듭니다.
실행기만 빌드하려면 `gcc main.c -o Sianlang.exe`를 사용할 수 있습니다.

[테스트 안내](tests/README.md) · [변경 기록](CHANGELOG.md) · [배포 절차](배포/배포하기.md)

## 라이선스

[MIT](LICENSE) — Copyright © 2026 rodotsian10 and SianLang contributors.
