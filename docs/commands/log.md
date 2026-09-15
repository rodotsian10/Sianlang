# log

콘솔에 값을 출력합니다. Python `print`처럼 여러 인수를 받고 `None`을 반환합니다.

```sian
log("HP", 100, true)
log("A", "B", sep=" | ", end="!")
log()
```

`sep`, `end`, `flush`, `file` 키워드를 지원합니다. `file`은 현재 `None`만 허용합니다.
