# 함수

`def`로 함수를 정의합니다. 기본값, 이름 지정 인수, 마지막 `#` 가변 인수를 지원합니다.

```sian
def greet(name="친구", #messages)
    log(name, #messages, sep=" ")

greet("Sian", "안녕", "하세요")
greet(name="개발자")
```

`def`, `func`, `f`, `function`은 같은 함수 정의 키워드입니다.
