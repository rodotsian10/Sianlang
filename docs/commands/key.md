# key.down: 키가 눌렸는지 확인

```sian
if key.down("left")
    player.x -= 5
if key.down("q")
    game.close()
```

키를 누르고 있는 동안 `true`, 아니면 `false`를 반환합니다. 게임 창에 포커스가 있어야 합니다.

지원하는 이름은 `left`, `right`, `up`, `down`, `enter`, `space`, 영문 한 글자입니다. 현재 Windows 게임 창에서 사용합니다.
