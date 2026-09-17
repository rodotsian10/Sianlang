# SianLang 1st Game

방향키로 스프라이트를 움직여 목표물에 닿으면 점수가 1이 됩니다. Q 키로 종료합니다.

프로젝트 폴더를 VS Code에서 열고 `game.sian`을 F6 또는 실행 버튼으로 실행할 수 있습니다. 게임 폴더만 열어도 됩니다. 확장은 게임 문법이 들어 있는 0.5.0 개발판이 필요합니다.

명령줄에서 실행할 때는 이 폴더를 현재 작업 폴더로 사용합니다.

```powershell
Set-Location D:\forSianlang\sianlang1stgame
..\build\Sianlang.exe game.sian
```

`player.rodot`은 `Conversion.sian`으로 `../sianlang-vscode/icon.png`를 변환한 결과물입니다. 이미 생성되어 있으므로 게임만 실행하면 됩니다. 다른 PNG를 쓰려면 변환 스크립트의 출력 파일명을 새 이름으로 바꾸세요. `rodot.create`는 기존 파일을 덮어쓰지 않습니다.

장면 본문은 매 프레임 실행됩니다. `var` 선언은 첫 프레임에만 적용되고 값을 유지합니다. 초기 위치 설정은 `initialized` 조건으로 한 번만 실행하도록 했습니다. 이동량은 `game.delta_time()`을 곱해 초당 속도에 맞춥니다.
