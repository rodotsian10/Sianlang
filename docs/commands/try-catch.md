# try / catch

실행 중 오류를 잡습니다.

```sian
try
    log(1 / 0)
catch error
    log.f("오류: {error}")
```

`iferror`는 삭제되었습니다. `finally`와 `throw`는 아직 없습니다.
