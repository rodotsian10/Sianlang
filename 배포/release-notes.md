## SianLang 0.4.1 — 컬렉션 인덱스 대입과 파일 I/O

간단한 문법으로 콘솔 프로그램과 파일 처리를 실행할 수 있는 Windows x64 배포판입니다.

### 다운로드

- **SianLang-0.4.1-windows-x64.zip**: 실행기, VS Code 확장, 예제, 파일 I/O 및 컬렉션 예제, 문법 문서, MIT 라이선스.
- **sianlang-vscode-0.4.1.vsix**: Windows x64 실행기가 포함된 VS Code 확장. 별도 Python/GCC/Node.js 설치 불필요.
- **SHA256SUMS.txt**: 파일 검증용 SHA-256.

### VS Code에서 실행

확장 화면의 `…` → **Install from VSIX...**로 설치한 뒤 `.sian` 파일을 저장하고 **F6**을 누르세요.
`Preferences: File Icon Theme → SianLang File Icons`를 선택하면 제작자의 SVG 파일 아이콘이 적용됩니다.

### 0.4.1 신규 기능
- 컬렉션 인덱스 대입 (`list[i] = x`, `dict[k] = v`, 음수 인덱스)
- 리스트/딕셔너리 메서드 (`append`, `pop`, `keys`, `values`, `items`)
- UTF-8 문자열 인덱싱 및 `for` 순회
- 수학 (`abs`, `min`, `max`, `round`) 및 난수 (`random.int`, `random.float`, `random.choice`) 내장함수
- 파일 I/O (`open`, `file.read`, `file.write`, `file.close`) 및 `try/catch` 에러 처리 연동

[문법 안내서](https://github.com/rodotsian10/Sianlang/blob/main/설명서/시안랭-문법규칙서.md) · [설치 안내](https://github.com/rodotsian10/Sianlang/blob/main/배포/시작하기.md)
