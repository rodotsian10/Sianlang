# Fjson 데이터 조작 블록 (Fjson Save Manipulation)

게임 세이브 데이터 및 JSON 파일 조작에 특화된 전용 블록입니다.

```sian
Fjson "save.json"
    replace j.player.hp = 100
    replace j.player.hp += 50
    add j.inventory = "전설의 검"
    delete j.temp_buff
```

- **`j.` 스코프 접두사**: JSON 파일 데이터 경로를 가리키며, 외부 변수와의 이름 충돌을 방지합니다.
- **원자적 자동 저장 (Atomic Save)**: 들여쓰기 블록이 정상 종료될 때 파일에 자동 저장됩니다.
- **조작 키워드**:
  - `replace`: 값 대입 및 복합 연산(`+=`, `-=`, `*=`, `/=`).
  - `add`: 배열(List) 요소 추가(`append`) 및 객체(Dict) 키 추가.
  - `delete`: 키 또는 데이터 삭제.
- **식별자 주의**: `add`, `replace`, `delete` 키워드는 파서가 특별하게 처리하므로 일반 변수명/함수명으로 사용하는 것을 권장하지 않습니다.
