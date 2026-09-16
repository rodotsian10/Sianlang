# 수학 및 난수 (Math & Random)

기본 수학 내장함수와 난수 생성 모듈(`random.*`)을 제공합니다.

```sian
log abs(-42)
log min(10, 20, 5)
log max(10, 20, 5)
log round(3.14159, 2)

int dice = random.int(1, 6)
float rate = random.float()
var item = random.choice(["칼", "방패", "포션"])
```

- `abs(x)`: 절대값 반환.
- `min(...)`, `max(...)`: 최솟값/최댓값 반환.
- `round(x, [ndigits])`: 반올림 수행.
- `random.int(min, max)`: min ~ max 범위 정수 난수 생성.
- `random.float()`: 0.0 이상 1.0 미만 실수 난수 생성.
- `random.choice(list)`: 리스트 요소 무작위 선택.
