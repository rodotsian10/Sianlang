# 파일 입출력 (File I/O)

표준 파일 열기, 읽기, 쓰기, 닫기 기능을 제공합니다.

```sian
var file = open("save.txt", "w")
file.write("hello sianlang\n")
file.close()

var file2 = open("save.txt", "r")
str content = file2.read()
file2.close()
log content
```

- `open(filename, mode)`: 파일 핸들 생성 (`"r"`, `"w"`, `"a"`, `"rb"`, `"wb"`, `"ab"` 지원).
- `file.write(text)`: 파일에 텍스트 쓰기.
- `file.read()`: 파일 내용 전체 읽기.
- `file.close()`: 파일 닫기.
- 존재하지 않는 파일 읽기는 `try/catch`로 처리할 수 있습니다.
