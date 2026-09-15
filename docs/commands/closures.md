# 중첩 함수와 클로저

함수 안의 함수는 바깥 변수를 기억합니다.

```sian
def counter(start=0)
    int value = start
    def next()
        value = value + 1
        return value
    return next

var count = counter(10)
log(count())
```

함수는 변수에 저장하고 인수로 전달하거나 반환할 수 있습니다.
