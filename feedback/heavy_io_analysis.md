# 고비용 I/O 분석 및 최적화 방안

## 1. 사전 로딩 (step03_bigram_sample.c: `load_dict`)

**문제**
- `kr_korean_simple.csv` 508,142줄을 매 실행마다 읽음
- 각 단어를 `malloc` → `qsort` (O(n log n)) → 이진 탐색
- 실행 시 가장 오래 걸리는 구간

**최적화 방안**
- `kr_korean_simple.csv`는 이미 생성 시 정렬되어 있지 않으므로 매번 정렬 필요
- → 사전을 미리 정렬된 바이너리 파일로 저장하면 `qsort` 생략 가능
- → 또는 해시셋으로 교체하면 정렬 불필요 + O(1) 조회

---

## 2. 바이그램 통계 로딩 (step03_bigram_sample.c: `load_bigram`)

**문제**
- `bigram_stats.txt` 172,685줄을 2번 순차 읽음 (1차 집계 패스, 2차 채우기 패스)
- 1차 패스에서 `from_keys` 선형 탐색 O(n × unique_from) — unique_from ≈ 2,500

**최적화 방안**
- `bigram_stats.txt`를 바이너리 포맷(고정 크기 레코드)으로 저장하면
  1패스 로딩 + 파싱 오버헤드 제거 가능
- 1차 패스의 선형 탐색 → 해시맵으로 교체하면 O(1)

---

## 3. 바이그램 빈도 집계 (step03_bigram_freq.c)

**문제**
- 508,142줄 × 평균 2.78자 = 약 125만 바이그램 처리
- 해시 테이블 2^21 슬롯 × 12바이트 = 약 25MB 메모리 상주

**현재 상태: 허용 가능**
- 결과가 `bigram_stats.txt`로 캐싱되므로 재실행 불필요
- 입력 데이터가 변경될 때만 재실행하면 됨

---

## 4. 대용량 파일 직접 읽기 (Claude 컨텍스트 토큰)

**문제**
- `kr_korean.csv` (9.7MB), `kr_korean_simple.csv` (5.5MB)를
  Read 도구로 열면 수만 토큰 즉시 소진

**해결: CLAUDE.md에 직접 읽기 금지 명시 완료**
- 확인 필요 시: `head -N`, `grep`, `wc -l` 만 사용
