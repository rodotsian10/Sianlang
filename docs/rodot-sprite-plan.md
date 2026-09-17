# SianLang `.rodot` 스프라이트 계획

## 1. 목표

SianLang을 콘솔 출력 중심의 언어에서 2D 게임 제작이 가능한 언어로 확장한다.

핵심 아이디어는 게임 코드와 스프라이트 데이터를 분리하는 것이다.

- `.sian`: 장면, 게임 흐름, 입력, 게임 규칙을 작성하는 코드 파일
- `.rodot`: 이미지와 스프라이트 JSON 데이터를 함께 담는 스프라이트 파일

`.rodot`은 장면이나 게임 코드를 포함하지 않는다. 이미지처럼 불러오고 사용할 수 있지만, 이미지 외에 위치·크기·표시 여부·충돌 영역 등의 데이터를 함께 가진다.

## 2. 파일의 역할

### `.sian`

게임을 실행하는 일반 SianLang 코드다.

담당하는 내용:

- 장면 생성 및 장면 전환
- 스프라이트 불러오기
- 스프라이트의 위치와 상태 변경
- 키보드·마우스 입력
- 게임 루프
- 충돌 처리
- 점수·체력·아이템 같은 게임 규칙
- Fjson 세이브 데이터 처리

예상 사용 형태:

```sian
scene start
    var player = sprite.load("player.rodot")
    player.x = 100
    player.y = 200
    player.visible = true

    scene.change("game")
```

### `.rodot`

스프라이트 하나를 표현하는 데이터 파일이다.

담당하는 내용:

- PNG 또는 다른 이미지 데이터
- 스프라이트 이름
- 기본 위치
- 기본 크기
- 표시 여부
- 회전
- 투명도
- 레이어 순서
- 충돌 영역
- 사용자가 추가한 사용자 정의 데이터

`.rodot` 안에 게임 코드를 넣지는 않는다. 스프라이트의 행동은 `.sian`에서 작성한다.

## 3. `.rodot`의 기본 데이터

예상 JSON 데이터:

```json
{
  "name": "player",
  "image": "player.png",
  "width": 64,
  "height": 64,
  "visible": true,
  "x": 0,
  "y": 0,
  "scale": 1.0,
  "rotation": 0,
  "opacity": 1.0,
  "layer": 0,
  "collision": {
    "enabled": true,
    "x": 4,
    "y": 2,
    "width": 56,
    "height": 60
  },
  "data": {
    "kind": "player",
    "hp": 100,
    "speed": 5
  }
}
```

기본 속성과 게임용 사용자 데이터는 구분한다.

- 기본 속성: 엔진이 화면에 그릴 때 사용하는 값
- `data`: 게임이 자유롭게 사용하는 값

예를 들어 `hp`와 `speed`는 이미지 자체의 속성이 아니라 게임 데이터이므로 `data` 안에 둔다.

## 4. Fjson과 `.rodot`

기존 Fjson 기능을 `.rodot`의 JSON 데이터에도 적용한다.

기존 Fjson은 JSON 세이브 파일을 자동으로 불러오고 저장한다.

```sian
Fjson "save.json"
    replace j.player.hp = 100
```

`.rodot`은 이미지 데이터와 JSON 데이터가 함께 있으므로, 전용 문법이 필요하다.

예상 문법:

```sian
var player = sprite.load("player.rodot")

replace player.data.hp = 80
replace player.visible = false

sprite.save(player, "player.rodot")
```

또는 Fjson의 구조를 확장하는 방식도 가능하다.

```sian
Fjson "player.rodot"
    replace j.data.hp = 80
    replace j.visible = false
```

다만 `.rodot`은 일반 JSON 파일이 아니므로, 내부 JSON만 수정한 뒤 이미지 바이트를 보존해서 다시 저장해야 한다. 따라서 초기 구현에서는 `sprite.load()`와 `sprite.save()`를 별도 기능으로 두고, 안정화 후 Fjson 블록과 통합한다.

## 5. PNG를 `.rodot`으로 변환하는 기능

일반 사용자는 PNG 파일을 직접 바이너리 형식으로 만들기 어렵다. 따라서 SianLang에 이미지 변환 명령을 추가한다.

예상 문법:

```sian
rodot.create("player.png", "player.rodot")
```

기본 JSON 데이터까지 지정하는 형태:

```sian
rodot.create("player.png", "player.rodot", {
    "name": "player",
    "width": 64,
    "height": 64,
    "visible": true,
    "layer": 1
})
```

또는 먼저 만들고 속성을 수정한다.

```sian
var player = rodot.create("player.png", "player.rodot")
player.name = "player"
player.x = 100
player.y = 200
player.data.hp = 100
rodot.save(player)
```

추천 초기 API:

- `rodot.create(image, output)` — PNG에서 기본 `.rodot` 생성
- `rodot.create(image, output, data)` — JSON 데이터와 함께 생성
- `rodot.load(path)` — `.rodot` 읽기
- `rodot.save(sprite, path)` — 이미지와 JSON 저장
- `rodot.image(sprite)` — 이미지 데이터 또는 이미지 정보 접근
- `rodot.data(sprite)` — 사용자 JSON 데이터 접근

처음부터 이미지 편집 기능까지 넣지는 않는다. 변환 기능은 이미지 픽셀을 바꾸는 것이 아니라 PNG를 유지한 채 메타데이터를 추가하는 역할만 한다.

## 6. `.rodot` 파일 형식 후보

### 방식 A: PNG 뒤에 JSON 추가

```text
[PNG 바이트]
[RODOT 마커]
[JSON 길이]
[JSON 바이트]
```

장점:

- 구현이 간단하다.
- 앞부분이 PNG라서 이미지 뷰어에서 열 수 있다.
- 기존 PNG 데이터를 그대로 보존할 수 있다.

단점:

- 일반 PNG 도구가 뒤의 JSON을 보존하지 않을 수 있다.
- 형식 검증과 데이터 손상 검사가 필요하다.

### 방식 B: ZIP 기반 컨테이너

```text
player.rodot
├─ image.png
└─ sprite.json
```

장점:

- 이미지와 JSON을 명확히 분리할 수 있다.
- 확장하기 쉽다.
- 애니메이션 이미지나 여러 리소스를 추가하기 쉽다.

단점:

- `.rodot`이 일반 이미지 뷰어에서 바로 열리지 않는다.
- ZIP 처리 코드가 필요하다.

초기 버전은 구현이 간단한 PNG 뒤 JSON 방식을 사용하고, 애니메이션·여러 이미지·사운드까지 지원할 때 ZIP 기반 형식으로 발전시키는 방안을 검토한다.

## 7. 화면 표시 구조

사용자가 매 프레임마다 화면을 지우고 모든 이미지를 직접 다시 그리게 하지 않는다. 엔진이 장면의 스프라이트 목록을 관리하고 자동으로 그린다.

```text
게임 루프
  1. 입력 이벤트 처리
  2. .sian의 on_update 코드 실행
  3. 스프라이트 위치·상태 갱신
  4. 이전 프레임 화면 정리
  5. visible인 스프라이트를 layer 순서로 그림
  6. 화면 표시
```

따라서 `clear()`는 일반 사용자가 매번 호출하는 명령이 아니라 엔진 내부 동작으로 처리하는 것이 좋다.

`draw.` 명령어는 스프라이트 이외의 직접 그리기가 필요할 때 사용하는 보조 기능으로 둔다.

예상 보조 명령:

```sian
draw.rect(10, 10, 100, 50)
draw.circle(200, 100, 30)
draw.line(0, 0, 400, 300)
draw.text("Score: 10", 10, 10)
```

## 8. 장면과 스프라이트 사용 예시

모든 장면 관리는 `.sian`에서 한다.

```sian
var player = rodot.load("player.rodot")
var enemy = rodot.load("enemy.rodot")

scene.start("game")

while scene.open()
    if key.down("left")
        player.x -= player.data.speed

    if key.down("right")
        player.x += player.data.speed

    if collision(player, enemy)
        player.data.hp -= 10

    scene.draw()
```

장면 전환은 `.sian` 함수로 작성한다.

```sian
def open_title()
    scene.change("title")

def start_game()
    scene.change("game")

def game_over()
    scene.change("game_over")
```

## 9. 단계별 개발 순서

### 1단계: `.rodot` 포맷 정의

- 파일 헤더와 버전 정의
- 이미지 데이터 위치 정의
- JSON 길이와 JSON 데이터 저장 방식 정의
- 손상된 파일 오류 처리

### 2단계: 읽기·쓰기

- `rodot.load()`
- `rodot.save()`
- 이미지 보존 확인
- JSON 읽기·쓰기 확인

### 3단계: PNG 변환

- `rodot.create()` 구현
- 기본 메타데이터 생성
- 사용자 지정 JSON 데이터 저장

### 4단계: 창과 렌더링

- 창 생성
- `.rodot` 이미지 표시
- 위치·크기·visible 적용
- layer 순서 적용

### 5단계: `.sian` 게임 API

- 스프라이트 불러오기
- 속성 접근
- 장면 생성 및 전환
- 게임 루프

### 6단계: 입력과 충돌

- 키보드 입력
- 마우스 입력
- 충돌 영역
- 충돌 이벤트

### 7단계: 확장 기능

- 애니메이션
- 이미지 여러 장
- 텍스트와 도형 그리기
- 사운드
- ZIP 기반 `.rodot` 확장 형식

## 10. 핵심 설계 원칙

1. `.sian`은 게임 로직을 담당한다.
2. `.rodot`은 이미지와 스프라이트 데이터를 담당한다.
3. `.rodot` 안에는 실행 코드를 넣지 않는다.
4. Fjson의 JSON 조작 개념을 `.rodot` 메타데이터에도 재사용한다.
5. 이미지 변환은 `rodot.create()`로 자동화한다.
6. 화면 지우기와 화면 갱신은 엔진이 자동으로 처리한다.
7. 사용자는 스프라이트의 속성과 게임 규칙만 작성하면 된다.
8. 초기 형식은 단순하게 만들고, 애니메이션·사운드·여러 리소스는 이후 확장한다.
