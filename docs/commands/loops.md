# 반복문

`while`, `loop`, `repeat`를 지원합니다. 반복문 뒤 `else`는 `break` 없이 정상 종료할 때 실행됩니다.

```sian
int n = 0
while n < 3
    log(n)
    n = n + 1
else
    log("완료")

repeat 2
    log("반복")
```
