# rodot: PNG 이미지와 JSON을 담는 스프라이트

`.rodot`은 PNG 이미지와 JSON 데이터를 함께 담습니다. VS Code의 Rodot 편집기에서 이미지와 `data` JSON을 볼 수 있습니다.

```sian
rodot.create("sprite.png", "player.rodot")
var player = rodot.load("player.rodot")
```

- `rodot.create(PNG, 출력 .rodot)`: PNG의 원본 크기를 읽어 변환합니다. 기존 출력 파일은 덮어쓰지 않습니다.
- `rodot.load(경로)`: 스프라이트를 불러옵니다. 장면의 최상위 변수로 보관하면 자동으로 그려집니다.
- `rodot.save = true`: 장면 전환 시 현재 스프라이트 데이터를 메모리에 보관하고, 다음 장면에서 같은 변수 이름과 파일 경로로 `rodot.load`하면 그대로 이어받습니다.
- `rodot.save = false`: 장면 전환 시 보관하지 않습니다. 다음 장면의 `rodot.load`는 파일의 JSON을 다시 읽습니다. 기본값입니다.

```sian
rodot.save = true
game.start("main")

scene main
    var player = rodot.load("player.rodot")
    player.x = 100
    scene.change("play")

scene play
    var player = rodot.load("player.rodot")
    log player.x
```

`rodot.save`는 게임 세션에서 유지되는 설정입니다. `true`여도 파일에는 기록하지 않으며, 게임을 종료하면 메모리 상태는 사라집니다. `rodot.save(player)` 함수는 0.5.1부터 사용하지 않습니다. 파일을 바꾸려면 [Frodot](frodot.md)을 사용하세요.

`player.x`, `player.y`, `player.visible`, `player.scale`, `player.rotation`, `player.opacity`, `player.layer`, `player.speed`는 `player.data` 속성의 간편 표기입니다. 직접 대입은 현재 실행 중인 스프라이트만 바꿉니다. `player.width`, `player.height`와 이미지·이름·원본 크기는 읽기 전용입니다. [collision](collision.md)은 스프라이트의 충돌 사각형을 검사합니다.
