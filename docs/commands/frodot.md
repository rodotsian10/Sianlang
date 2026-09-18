# Frodot: `.rodot` 파일의 JSON 수정

`Frodot`은 불러온 스프라이트의 `data`를 수정하고, 블록이 정상적으로 끝나면 원본 `.rodot` 파일의 JSON에 자동 저장합니다. 이미지 바이트, `name`, 원본 크기는 그대로 둡니다. `r`은 블록 안에서 대상 스프라이트를 가리킵니다.

```sian
var player = rodot.load("player.rodot")
Frodot player
    replace r.data.x = 100
    replace r.data.speed += 2
    add r.data.user.hp = 100
    delete r.data.user.old_item
```

- `replace`: 값 대입과 `+=` 등의 복합 대입.
- `add`: 새 항목을 추가하거나 리스트에 값을 덧붙임.
- `delete`: 항목 제거.

게임 중 `player.x = 100` 같은 직접 대입은 메모리에서만 바뀝니다. 반면 Frodot 블록은 파일에 저장하므로 게임을 껐다 켜도 값이 남습니다. 블록에서 오류가 나거나 장면 전환·게임 종료가 발생하면 자동 저장하지 않습니다. `rodot.save = true`와는 별개의 기능입니다.

[Fjson](fjson.md)과 비슷하게 파일의 JSON을 바꾸지만, 대상은 `.rodot` 스프라이트입니다.
