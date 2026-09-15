## SianLang 0.3.1 — 첫 배포판

간단한 문법으로 콘솔 프로그램을 만들 수 있는 Windows x64 배포판입니다. 언어와 게임 기능은 계속 개발 중이므로 초기 배포(pre-release)로 제공합니다.

### 다운로드

- **SianLang-0.3.1-windows-x64.zip**: 실행기, VS Code 확장, 예제, 문법 문서, MIT 라이선스.
- **sianlang-vscode-0.3.1.vsix**: Windows x64 실행기가 포함된 VS Code 확장. 별도 Python/GCC/Node.js 설치 불필요.
- **SHA256SUMS.txt**: 파일 검증용 SHA-256.

### VS Code에서 실행

확장 화면의 `…` → **Install from VSIX...**로 설치한 뒤 `.sian` 파일을 저장하고 **F6**을 누르세요.
`Preferences: File Icon Theme → SianLang File Icons`를 선택하면 제작자의 SVG 파일 아이콘이 적용됩니다.
기존 테마가 기본 언어 아이콘을 허용하는 경우에는 테마 변경 없이도 표시됩니다.

### 검증

실행기 241회, 확장 모의 시나리오 14개, 개발 도구 없는 PATH에서 압축 해제 후 실행, 실제 VS Code 확장 활성화·프로그램 실행을 확인했습니다.

### 현재 범위

게임 창, 실시간 키 입력, 파일 전용 명령어, 언어 서버 진단 및 디버거는 아직 구현하지 않았습니다.
실행 파일은 코드 서명이 없으며 Windows x64에서 검증했습니다.

[문법 안내서](https://github.com/rodotsian10/Sianlang/blob/main/설명서/시안랭-문법규칙서.md) · [설치 안내](https://github.com/rodotsian10/Sianlang/blob/main/배포/시작하기.md)
