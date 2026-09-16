# input

터미널에서 한 줄을 문자열로 입력받습니다.

```sian
try
	str name = input("이름: ")
	log.f("안녕하세요, {name}!")
catch error
	log "입력에 실패했습니다."
```

게임용 실시간 키 입력은 추후 `key` 명령으로 추가할 예정입니다.
