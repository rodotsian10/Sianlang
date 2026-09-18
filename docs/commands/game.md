# game: 게임 창과 프레임

Windows x64에서 게임 창을 열고 선택한 장면을 실행합니다.

```sian
game.fps = 60
game.start("play", 640, 480, "My Game")
```

- `game.fps = 정수`: 초당 목표 프레임 수입니다. 1~240을 지원하며 기본값은 60입니다.
- `game.start("장면")`: 지정한 장면에서 시작합니다. 창 크기 기본값은 800×600입니다.
- `game.start("장면", 너비, 높이, "제목")`: 창 크기와 제목을 지정합니다. 크기는 각각 64~4096의 정수여야 합니다.
- `game.delta_time()`: 직전 프레임부터 경과한 시간을 초 단위로 반환합니다.
- `game.close()`: 게임을 종료합니다.

```sian
if key.down("right")
    player.x += player.speed * game.delta_time()
```

`speed`를 초당 이동 거리로 쓸 때 경과 시간을 곱합니다. 장면 정의와 전환은 [scene](scene.md)을 참고하세요.
