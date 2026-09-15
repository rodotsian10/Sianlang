# log.f

문자열의 `{표현식}`을 계산해 출력합니다.

```sian
str name = "Sian"
int score = 95
log.f("{name}: {score}점")
log.f("점수={score:03d}, 이름={name!r}")
```

`{{ }}` 이스케이프, 정렬, 너비, 정밀도, 숫자 형식을 지원합니다.
