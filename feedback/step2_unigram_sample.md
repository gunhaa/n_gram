# Step 2: 유니그램 샘플링

## 목적

step1의 완성형 글자 빈도 분포를 확률로 변환하여, 빈도에 비례한 가중 랜덤 샘플링으로 글자 세트를 생성.

## 구현

- 파일: `step2/step02_unigram_sample.c`
- 입력: `step1/charfreq_result.txt` (기본값, 인자로 변경 가능)
- 출력: `step2/unigram_sample_result.txt` (파일 스트림, 인자로 변경 가능)

## 핵심 구조

- 빈도 합산 후 누적 확률(cumulative probability)로 변환
- `[0,1)` 균등 난수 → 이진 탐색으로 O(log n) 샘플링
- SET_COUNT(10세트) × SET_SIZE(10글자) 출력

## 컴파일 및 실행

```bash
gcc -O2 -o step2/step02_unigram_sample step2/step02_unigram_sample.c
./step2/step02_unigram_sample
# 인자 지정 시:
./step2/step02_unigram_sample step1/charfreq_result.txt step2/unigram_sample_result.txt
```

## 결과 형식

```
set 01: 하없원하이어나마이음
set 02: 전앙다심다리트극밤다
...
set 10: 의늘융쿠양환비지반동
```

## 상수 (변경 가능)

| 상수 | 값 | 의미 |
|------|----|------|
| `SET_COUNT` | 10 | 세트 수 |
| `SET_SIZE` | 10 | 세트당 글자 수 |
| `MAX_CHARS` | 3000 | 최대 글자 종류 수 |
