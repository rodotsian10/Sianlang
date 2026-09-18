# NEON STAR RUN

시안랭으로 만든 작은 아케이드 게임입니다. 45초 안에 노란 별 8개를 모으세요. 빨간 위험물에 세 번 닿으면 패배합니다.

- 제목 화면에서 **Enter**: 시작
- 게임 중 **방향키**: 이동
- 승리/패배 화면에서 **Enter**: 다시 시작
- 모든 화면에서 **Q**: 종료

## 실행

VS Code에서 `game.sian`을 열고 F6 또는 실행 버튼을 누르세요. 0.5.1 확장이 설치되어 있어야 합니다. 콘솔에서는 압축을 푼 폴더를 기준으로 다음처럼 실행합니다.

```powershell
Set-Location .\sianlanggameex1
..\Sianlang.exe game.sian
```

처음 실행할 때 `player.rodot`이 없으면 `game.sian`이 `sprite.png`에서 자동 생성합니다. 별도로 PNG→rodot 변환 과정을 체험하려면 **게임을 실행하기 전에** `Conversion.sian`을 한 번 실행하세요. 기존 `player.rodot`은 덮어쓰지 않습니다.

게임의 제목·플레이·승리·패배 화면은 `scene`으로 분리되어 있습니다. 플레이어는 `.rodot` 스프라이트이고, 별·위험물·화면 글자는 `draw.` 명령으로 그립니다. 점수와 목숨은 게임 중 메모리에서만 바뀝니다.
