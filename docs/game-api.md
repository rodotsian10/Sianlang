# SianLang 0.5.1 게임 문법

Windows에서 실행되는 장면 기반 게임 기능입니다. 장면 이름은 영문 식별자를 사용합니다. `.sian` 파일이 장면과 게임 로직을 담당하고, `.rodot` 파일은 PNG 이미지와 JSON 속성을 함께 담습니다.

## 전체 예제

```sian
game.fps = 60
game.start("title", 800, 600, "내 게임")

scene title
    draw.text("Enter를 누르세요", 30, 30, "#ffffff")
    if key.down("enter")
        scene.change("play")

scene play
    var player = rodot.load("player.rodot")
    if key.down("right")
        player.x += player.speed * game.delta_time()
    if key.down("left")
        player.x -= player.speed * game.delta_time()
    if key.down("space")
        scene.change("game_over")

scene game_over
    draw.text("Game over", 30, 30)
    if key.down("enter")
        game.close()
```

`game.start("title")`은 창을 열고 시작 장면을 실행합니다. 기본 화면은 800×600입니다. 창 크기와 제목을 바꾸려면 `game.start("title", 800, 600, "제목")`을 사용합니다. 창을 닫거나 `game.close()`가 실행될 때까지 현재 장면의 코드가 매 프레임 실행됩니다. 다음 장면으로 자동 이동하지 않으며 `scene.change("이름")`이 반드시 필요합니다. 같은 장면에 재진입하면 해당 장면의 변수들이 새로 만들어집니다.

`game.fps = 60`은 초당 최대 실행 프레임을 1~240 범위에서 정합니다. `game.delta_time()`은 이전 프레임 이후의 경과 시간을 초 단위로 반환합니다. `key.down("left")`는 해당 키가 눌려 있는 동안 참입니다. `left`, `right`, `up`, `down`, `enter`, `space`, 영문 한 글자를 지원합니다.

## `.rodot` 파일

`rodot.create("image.png", "sprite.rodot")`은 원본 PNG 바이트와 JSON 기본값을 담은 새 파일을 만듭니다. 변환용 `.sian` 파일에서 한 번 실행하세요. 기존 `.rodot` 파일은 덮어쓰지 않습니다. 이름은 출력 파일 이름에서 가져오며, 원본 `width`와 `height`는 PNG 헤더에서 자동으로 읽습니다. 생성된 파일 안의 기본 형태는 다음과 같습니다.

```json
{
  "name": "sprite",
  "meta": {"width": 64, "height": 64},
  "data": {
    "visible": true, "x": 0, "y": 0,
    "scale": 1.0, "rotation": 0, "opacity": 1.0,
    "layer": 0, "speed": 5,
    "collision": {"enabled": true, "x": 0, "y": 0, "width": 64, "height": 64},
    "user": {}
  }
}
```

`name`과 `meta.width/height`는 읽기 전용입니다. `data`는 게임 중 변경할 수 있습니다. `rodot.load("sprite.rodot")`이 반환한 스프라이트는 현재 장면에서 자동으로 그려집니다. `visible`, `x`, `y`, `scale`, `rotation`, `opacity`, `layer`가 현재 렌더링에 반영됩니다. `speed`, `collision`, `user`는 게임 코드에서 사용할 수 있는 데이터입니다. `collision(a, b)`는 두 스프라이트의 충돌 사각형이 겹치는지 검사합니다. 충돌 검사는 축에 평행한 사각형 기준이며 회전은 반영하지 않습니다.

```sian
var player = rodot.load("player.rodot")
player.x = 100
player.y += 5
player.visible = false
log player.width, player.height, player.speed
```

## `Frodot`: `.rodot` 파일의 JSON 자동 저장

`Frodot`은 메모리에 올라온 스프라이트의 `data`를 바꾸고, 블록이 정상적으로 끝나면 `.rodot` 파일의 JSON에 자동 저장합니다. 블록 안에서는 `r.data`를 사용하고, `replace`, `add`, `delete`는 기존 Fjson과 같은 의미입니다.

```sian
Frodot player
    replace r.data.x += 10
    replace r.data.visible = true
    add r.data.user.hp = 100
    delete r.data.user.old_item
```

게임 중 직접 대입은 파일을 바꾸지 않습니다. `rodot.save = true`는 다음 장면으로 데이터를 메모리에서 이어받게 할 뿐 파일 저장은 하지 않습니다. 기본값 `false`이면 다음 장면의 재로드 때 파일값으로 돌아갑니다. 자세한 규칙은 [0.5.1 문서](../설명서/0.5.1-계획.md)를 참고하세요.

## 자동 그리기와 `draw.`

엔진은 장면 코드를 실행한 뒤 화면을 비우고, 보이는 스프라이트를 `layer` 순서로 그립니다. 직접 갱신 명령을 호출할 필요가 없습니다. 도형과 글자만 추가로 그릴 때 다음을 사용합니다.

```sian
draw.rect(10, 10, 80, 40, "#ff0000")
draw.circle(200, 100, 25, "#00ff00")
draw.line(0, 0, 300, 200, "#ffffff")
draw.text("점수: 10", 10, 10, "#ffffff")
```

색상 인수는 생략하면 흰색입니다. 이 명령들은 호출된 프레임에만 표시됩니다. 장면 코드가 매 프레임 실행되므로 화면에는 계속 보입니다.
