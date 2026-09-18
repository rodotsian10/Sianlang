## SianLang 0.5.1 — 장면 게임과 rodot 스프라이트

Windows x64에서 장면 기반 게임 창, 키 입력, 그리기 명령, PNG+JSON `.rodot` 스프라이트, Frodot 파일 JSON 자동 저장, 충돌 검사를 지원합니다. `rodot.save = true|false`로 다음 장면까지 임시 스프라이트 데이터 유지 여부를 설정합니다(기본 false). VS Code 확장은 `.rodot`의 이미지와 `data` JSON을 보여주고 저장할 수 있습니다.

ZIP의 `sianlanggameex1`에는 `sprite.png`, `Conversion.sian`, `game.sian`, 안내문이 들어 있습니다. 새 아케이드 게임은 첫 실행에 `player.rodot`을 자동 생성합니다. 방향키로 별을 모으고 위험물을 피하며, `Conversion.sian`은 PNG 변환을 따로 체험할 때만 실행하면 됩니다.

배포 파일: `SianLang-0.5.1-windows-x64.zip`, `sianlang-vscode-0.5.1.vsix`, `SHA256SUMS.txt`.

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
