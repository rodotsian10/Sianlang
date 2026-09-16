# 게임 기능 계획

현재 SianLang은 콘솔 중심입니다. 0.4 단계에서 기본 컬렉션과 반복 기능을 추가했고, 2D 게임 제작 기능은 그 위에 단계적으로 추가합니다.

## 구현됨

- list, tuple, dict 리터럴
- 컬렉션 인덱싱과 `len()`
- `for item in ...` 반복
- `range(stop)`, `range(start, stop)`, `range(start, stop, step)`
- `time.now()` 현재 Unix 시간

## 다음 단계

- list/dict 인덱스 대입과 `append`, `pop`, `keys`, `values`
- 문자열 순회
- 집합(set)과 명시적 컬렉션 타입
- 파일 전용 입출력 명령
- 모듈과 `import`

## 게임 단계

- 창 생성과 게임 루프
- `key` 기반 키보드 입력
- 이미지·스프라이트·사운드
- 충돌 검사와 프레임 시간 처리

설계가 확정되지 않은 기능은 구현되었다고 가정하고 사용하지 마세요.
