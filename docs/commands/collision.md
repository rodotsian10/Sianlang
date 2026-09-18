# collision: 스프라이트 충돌 검사

```sian
if collision(player, enemy)
    draw.text("Hit!", 20, 20)
```

불러온 `.rodot` 스프라이트 둘의 충돌 사각형이 겹치면 `true`를 반환합니다. 각 스프라이트의 `data.collision.enabled`가 `false`이면 충돌하지 않습니다.

충돌 사각형의 위치와 크기는 `data.collision`에 들어 있습니다. 현재 계산은 축에 평행한 사각형 기준이며 이미지의 회전은 반영하지 않습니다.
