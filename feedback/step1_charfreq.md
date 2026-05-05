# Step 1: 완성형 글자 빈도 분석

## 목적

한국어 사전 단어에서 각 완성형 글자(가-힣)가 몇 번 등장하는지 집계.

## 구현

- 파일: `step1/step01_charfreq.c`
- 입력: `data/korean-dict/kr_korean_simple.csv` (기본값, 인자로 변경 가능)
- 출력: `글자\t빈도` 형식, 빈도 내림차순

## 핵심 구조

- 한글 범위: U+AC00(가) ~ U+D7A3(힣), 총 11,172자
- `long freq[11172]` 배열로 인덱싱 (`cp - 0xAC00`)
- UTF-8 → 코드포인트 직접 파싱 (3바이트 시퀀스)
- 결과를 `CharFreq` 배열로 수집 후 `qsort`

## 컴파일 및 실행

```bash
gcc -O2 -o step1/step01_charfreq step1/step01_charfreq.c
./step1/step01_charfreq > step1/charfreq_result.txt
```

## 결과 요약

- 결과 파일: `step1/charfreq_result.txt`
- 등장 글자 수: 2,467개
- 상위 10개:

| 순위 | 글자 | 빈도 |
|------|------|------|
| 1 | 다 | 90,519 |
| 2 | 하 | 58,848 |
| 3 | 기 | 28,632 |
| 4 | 이 | 28,011 |
| 5 | 리 | 26,123 |
| 6 | 사 | 22,049 |
| 7 | 지 | 21,569 |
| 8 | 자 | 17,322 |
| 9 | 수 | 17,181 |
| 10 | 대 | 16,876 |
