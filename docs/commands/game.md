# 게임 문법

SianLang의 게임 문법은 장면(scene), 스프라이트(rodot), 키 입력, 자동 화면 그리기를 제공합니다.
현재 게임 창과 이미지 렌더링은 Windows에서 지원됩니다.

## 장면

```sian
scene title
    draw.text("Press Enter", 30, 30)

scene play
    draw.text("Playing", 30, 30)
```

장면 이름은 영문 식별자를 사용합니다. 장면은 파일의 최상위에서만 선언할 수 있고, 같은 이름을 두 번 선언할 수 없습니다.

게임이 실행되면 선택된 장면의 본문이 프레임마다 실행됩니다. 따라서 키 입력, 이동, 충돌 검사, `draw.` 명령은 장면 안에 작성합니다.

현재 구현에서는 장면 본문의 초기화 코드도 매 프레임 실행됩니다. 예를 들어 다음 코드는 플레이어 위치를 매 프레임 되돌립니다.

```sian
scene play
    player.x = 100
    player.x += 5
```

장면 초기화와 프레임 실행을 분리하는 기능은 개발 중입니다. 게임 코드를 작성할 때는 이 점을 주의해야 합니다.

## 게임 시작과 종료

```sian
game.fps = 60
game.start("play")
```

`game.fps`는 목표 FPS입니다. 1부터 240까지의 정수만 사용할 수 있습니다.

```sian
game.start("play", 800, 600, "My Game")
```

인수 순서는 다음과 같습니다.

```text
game.start(시작 장면, 창 너비, 창 높이, 창 제목)
```

너비와 높이를 생략하면 기본값은 800×600입니다. 제목을 생략하면 기본 제목은 `SianLang Game`입니다.

```sian
game.close()
```

게임 창과 게임 루프를 종료합니다.

```sian
var elapsed = game.delta_time()
```

이전 프레임부터 현재 프레임까지의 경과 시간을 초 단위의 실수로 반환합니다.

## 장면 전환

```sian
if key.down("space")
    scene.change("game_over")
```

`scene.change()`는 지정한 장면으로 이동합니다. 장면 이름이 존재하지 않으면 오류가 발생합니다.

장면은 자동으로 다음 장면으로 넘어가지 않습니다. 현재 장면이 끝났다고 해서 다음 장면이 실행되지 않으며, 반드시 `scene.change("장면이름")`을 호출해야 합니다.

## 키 입력

```sian
if key.down("right")
    player.x += player.speed
```

지원하는 키 이름:

```text
left, right, up, down, enter, space
```

영문 한 글자도 사용할 수 있습니다.

```sian
if key.down("q")
    game.close()
```

키를 누르고 있는 동안 매 프레임 `true`를 반환합니다.

## 자동 그리기

게임 엔진은 매 프레임 화면을 지우고 보이는 `.rodot` 스프라이트를 자동으로 그립니다. 스프라이트를 다시 그리는 별도 명령은 필요하지 않습니다.

모든 직접 그리기 명령은 `draw.`으로 시작합니다.

```sian
draw.rect(10, 10, 100, 50, "#ff0000")
draw.circle(200, 100, 25, "#00ff00")
draw.line(0, 0, 300, 200, "#ffffff")
draw.text("Hello", 20, 20, "#ffffff")
```

색상은 `#RRGGBB` 형식입니다. 색상을 생략하면 흰색을 사용합니다.

명령의 인수는 다음과 같습니다.

```text
draw.rect(x, y, width, height [, color])
draw.circle(center_x, center_y, radius [, color])
draw.line(x1, y1, x2, y2 [, color])
draw.text(text, x, y [, color])
```

도형과 글자는 해당 프레임에만 그려집니다. 다음 프레임에도 보이게 하려면 장면 코드에서 매 프레임 다시 호출해야 합니다.

## PNG를 `.rodot`으로 변환

```sian
rodot.create("player.png", "player.rodot")
```

PNG 파일을 `.rodot` 파일로 변환합니다. `.rodot`에는 원본 PNG 바이트와 JSON 데이터가 함께 저장됩니다.

기존 `.rodot` 파일은 덮어쓰지 않습니다. 변환 스크립트는 보통 `Conversion.sian`으로 따로 작성합니다.

```sian
|| Conversion.sian
rodot.create("sianlang-vscode/icon.png", "sianlang1stgame/player.rodot")
```

## `.rodot` 불러오기

```sian
var player = rodot.load("player.rodot")
```

불러온 스프라이트는 현재 장면에서 자동으로 렌더링됩니다.

```sian
player.x = 100
player.y = 200
player.scale = 0.5
player.rotation = 15
player.opacity = 0.8
player.visible = true
player.layer = 1
player.speed = 5
```

자동 렌더링에 사용하는 데이터:

```text
visible, x, y, scale, rotation, opacity, layer
```

게임 코드에서 사용할 수 있는 데이터:

```text
speed, collision, user
```

PNG 원본 크기는 `width`, `height`로 읽을 수 있습니다.

```sian
log player.width, player.height
```

`name`, `width`, `height` 같은 원본 정보는 읽기 전용입니다.

## Frodot

`Frodot`은 게임 중 스프라이트의 `data`를 메모리에서 임시로 수정합니다. 수정 내용은 `.rodot` 파일에 자동 저장되지 않습니다.

```sian
Frodot player
    replace r.data.x = 200
    replace r.data.visible = true
    add r.data.user.hp = 100
```

Frodot 블록 안에서는 대상 스프라이트를 `r`로 접근합니다.

```sian
Frodot player
    replace r.data.speed += 2
    delete r.data.user.old_value
```

Frodot에서는 `r.data`만 수정할 수 있습니다. PNG 원본과 메타데이터를 바꿀 수 없습니다.

파일에 저장하려면 명시적으로 호출합니다.

```sian
rodot.save(player)
rodot.save(player, "backup.rodot")
```

`rodot.save()`는 원본 PNG 바이트를 보존하고 JSON 데이터만 저장합니다.

## 충돌 검사

```sian
if collision(player, enemy)
    log "hit"
```

두 개의 불러온 `.rodot` 스프라이트가 충돌 사각형으로 겹치는지 검사합니다.

현재는 축에 평행한 사각형 기준이며 스프라이트의 회전은 충돌 계산에 반영되지 않습니다.

## 전체 예제

```sian
game.fps = 60
game.start("play", 640, 480, "My Game")

scene play
    var player = rodot.load("player.rodot")
    var enemy = rodot.load("enemy.rodot")

    if key.down("right")
        player.x += player.speed
    if key.down("left")
        player.x -= player.speed
    if collision(player, enemy)
        draw.text("Hit!", 20, 20, "#ff0000")
    if key.down("q")
        game.close()
```
