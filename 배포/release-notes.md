## SianLang 0.4.2 — Fjson 데이터 조작 블록 및 예제

간단한 문법으로 콘솔 프로그램과 파일 처리 및 세이브 데이터를 안전하게 조작할 수 있는 Windows x64 배포판입니다.

### 다운로드

- **SianLang-0.4.2-windows-x64.zip**: 실행기, VS Code 확장, 게임 예제, Fjson 데이터 조작 예제, 문법 문서, MIT 라이선스.
- **sianlang-vscode-0.4.2.vsix**: Windows x64 실행기가 포함된 VS Code 확장. 별도 Python/GCC/Node.js 설치 불필요.
- **SHA256SUMS.txt**: 파일 검증용 SHA-256.

### VS Code에서 실행

확장 화면의 `…` → **Install from VSIX...**로 설치한 뒤 `.sian` 파일을 저장하고 **F6**을 누르세요.
`Preferences: File Icon Theme → SianLang File Icons`를 선택하면 제작자의 SVG 파일 아이콘이 적용됩니다.

### 0.4.2 신규 기능
- Fjson 세이브 데이터 조작 들여쓰기 블록 (`Fjson "save.json"`)
- Fjson 데이터 연산 키워드: `replace` (대입 및 `+=`, `-=`, `*=`, `/=` 복합대입), `add` (List append 및 Dict 신규 Key 생성), `delete` (Key 삭제)
- 원자적 저장 (Atomic Write) 및 `j.` 데이터 스코프 분리
- JSON 게임 세이브 데이터 조작 예제 (`examples/fjson-demo.sian`)

[문법 안내서](https://github.com/rodotsian10/Sianlang/blob/main/설명서/시안랭-문법규칙서.md) · [설치 안내](https://github.com/rodotsian10/Sianlang/blob/main/배포/시작하기.md)
