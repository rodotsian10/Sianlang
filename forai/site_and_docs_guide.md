# SianLang 웹사이트 & 배포 문서 작성/수정 가이드라인

이 문서는 SianLang 프로젝트의 **웹사이트(GitHub Pages) 편집** 및 **배포 패키지 설명서 제작** 시 AI 및 작업자가 반드시 준수해야 하는 표준 가이드라인이다.

---

## 1. 웹사이트 (`docs/`) 관리 규칙

### 1.1 메인 웹페이지 (`docs/index.html`)
- **버전 표기 태그**: `<span class="tag">WINDOWS x64 · v0.X.X · ...</span>` 최신 버전 반영.
- **다운로드 링크**:
  - ZIP: `https://github.com/rodotsian10/Sianlang/releases/download/v0.X.X/SianLang-0.X.X-windows-x64.zip`
  - VSIX: `https://github.com/rodotsian10/Sianlang/releases/download/v0.X.X/sianlang-vscode-0.X.X.vsix`
  - SHA-256 체크섬 링크 갱신.
- **예시 코드 및 주요 기능 요약**: 해당 버전에 신규 추가된 주요 구문(예: `Fjson`, 컬렉션 연산 등)을 반영.

### 1.2 명령어 사전 페이지 (`docs/commands.html`)
- **개별 영문 MD 1:1 매핑 필수**:
  - 명령어 사전의 각 항목은 **반드시 `docs/commands/` 안의 개별 영문 `.md` 파일**로 연결해야 한다.
  - ❌ 금지: `시안랭-문법규칙서.md#앵커` 형태의 통합 문서 링크 작성 금지.
  - ⭕ 준수: `https://github.com/rodotsian10/Sianlang/blob/main/docs/commands/{command_name}.md` 형태 사용.
- **새 기능 추가 시 절차**:
  1. `docs/commands/` 디렉토리에 영문 이름으로 된 신규 `.md` 파일 작성 (예: `fjson.md`, `math-random.md`, `file-io.md`, `index-assign.md`).
  2. `docs/commands/index.md` 레퍼런스 목차에 링크 추가.
  3. `docs/commands.html` 리스트 항목에 GitHub blob URL 링크 연동.

---

## 2. 설명서 (`설명서/`) 및 배포 문서 관리 규칙

### 2.1 버전별 계획서 분리 (`설명서/0.X.X-계획.md`)
- 새로운 마이너/메이저 버전이 출시될 때마다 **통합 문서에 그냥 추가하지 말고 `설명서/0.X.X-계획.md` 파일을 독립 생성**한다.
- `설명서/index.md` 및 최상위 `설명서.md`에 해당 버전 계획서 링크를 명시한다.

### 2.2 배포용 매뉴얼 (`배포/`)
- `배포/index.html`, `배포/release-notes.md`, `배포/시작하기.md`의 버전 번호 및 릴리스 노트 업데이트.
- `build.ps1` 또는 `tools/package_release.py` 실행을 통해 `dist/0.X.X/` 빌드 아티팩트 자동 검증.

---

## 3. 체크리스트 (릴리스 전 필수 확인)

1. [ ] `docs/index.html` 버전 및 다운로드 링크 업데이트 완료
2. [ ] `docs/commands.html` 내 모든 항목이 `docs/commands/*.md` 영문 단일 파일로 1:1 연결됨
3. [ ] `설명서/0.X.X-계획.md` 독립 작성 및 `설명서/index.md` 목차 반영 완료
4. [ ] `python tests/test_release.py` 테스트 통과 확인
5. [ ] Git 커밋, 태그(`v0.X.X`) 생성 및 GitHub 푸시 확인
