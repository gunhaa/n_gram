# 데이터 수집 및 전처리

## 원본 데이터

- 저장소: https://github.com/korean-word-game/db (주의: korea가 아니라 korean)
- 파일: Google Drive에서 별도 다운로드 필요 (git clone에 데이터 없음)
- 다운로드: `python3 -m gdown "1PdzYubqcPKAIsHRtWZEFdQ1m4-fba6Oj" -O kr_korean.zip` 후 unzip
- 인코딩: UTF-8 with BOM

## 전처리 결과

- 원본: `data/korean-dict/kr_korean.csv` — 508,142줄, 형식: `낱말,품사`
- 전처리: `data/korean-dict/kr_korean_simple.csv` — 508,142줄, 형식: 낱말만 (레코드 수 동일)

## 전처리 방식 (최종)

`-`, `^`, 공백은 사전의 복합어 표기 구분자이므로 **제거하여 단어를 합친다**.
레코드는 삭제하지 않고 모두 유지한다.

```python
import re
with open('data/korean-dict/kr_korean.csv', encoding='utf-8-sig') as f, \
     open('data/korean-dict/kr_korean_simple.csv', 'w', encoding='utf-8') as out:
    for line in f:
        word = line.split(',')[0]
        word = re.sub(r'[-^ ]', '', word)
        out.write(word + '\n')
```

## 변환 예시

| 원본 | 변환 후 |
|------|---------|
| `냉수-권,명사` | `냉수권` |
| `가감^소거법,공업` | `가감소거법` |
| `-가,어미` | `가` |
| `흰꽃향장미,명사` | `흰꽃향장미` |

## 주의사항

- BOM 때문에 awk `$2 != "접사"` 매칭 안 됨 → python `utf-8-sig` 또는 grep 사용
- `-`, `^`을 단어 제거 조건으로 오해하지 말 것 — 구분자 제거 후 합치는 것이 올바른 처리
