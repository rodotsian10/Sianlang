# 컬렉션 및 인덱스 대입 (Collections & Index Assignment)

리스트(`list`), 튜플(`tuple`), 딕셔너리(`dict`) 자료형의 인덱스 접근, 인덱스 대입 및 메서드를 제공합니다.

```sian
var items = [10, 20, 30]
items[0] = 99

items.append(40)
var last = items.pop()

var player = {"name": "Sian", "hp": 100}
player["hp"] = 150

log player.keys()   || ["name", "hp"]
log player.values() || ["Sian", 150]
```

- `items[idx] = val`: 가변 컬렉션(List, Dict)의 인덱스/키 대입 지원.
- `list.append(val)`: 리스트 끝에 원소 추가.
- `list.pop()`: 리스트 마지막 원소 제거 및 반환.
- `dict.keys()`, `dict.values()`, `dict.items()`: 딕셔너리 키, 값, 키-값 쌍 반환.
