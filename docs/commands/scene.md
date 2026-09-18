# scene: 장면 정의와 전환

장면은 `.sian` 파일의 최상위에 선언합니다. 이름은 영문 식별자이며 중복할 수 없습니다.

```sian
game.start("title")

scene title
    draw.text("Press Enter", 30, 30)
    if key.down("enter")
        scene.change("play")

scene play
    draw.text("Playing", 30, 30)
```

현재 장면의 본문은 게임이 열려 있는 동안 매 프레임 반복됩니다. 다음 장면으로 자동 이동하지 않으므로 `scene.change("이름")`을 호출해야 합니다. 장면을 다시 방문하면 그 장면의 변수는 새로 만들어집니다. 단, `rodot.save = true`이면 같은 변수 이름·파일 경로로 `rodot.load`한 스프라이트는 이전 장면의 값을 이어받습니다.

장면의 최상위 `var` 선언은 처음 실행될 때 값을 만들고 이후 프레임에는 같은 값을 유지합니다. 일반 대입문은 매 프레임 실행됩니다. 시작 위치처럼 한 번만 정할 값은 조건으로 묶어 주세요.

```sian
scene play
    var player = rodot.load("player.rodot")
    var initialized = false
    if initialized == false
        player.x = 100
        initialized = true
    if key.down("right")
        player.x += 5
```

위 코드에서 `player.x = 100`을 조건 밖에 두면 매 프레임 위치가 되돌아갑니다.
