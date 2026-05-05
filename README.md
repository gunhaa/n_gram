# Korean N-gram Analyzer

한국어 사전 데이터를 기반으로 글자 빈도 분석 및 N-gram 샘플링을 수행하는 C 프로젝트.

## 데이터 출처

| 항목 | 내용 |
|------|------|
| 원본 | 국립국어원 표준국어대사전 (stdict.korean.go.kr) |
| 소스 | [korean-word-game/db](https://github.com/korean-word-game/db) |
| 수집 | 2018-11-13 |
| 크기 | 508,142줄 / 9.8MB |
| 라이선스 | 저작권 없음 |

## 프로젝트 구조

```
n_gram/
├── data/
│   └── korean-dict/
│       ├── kr_korean.csv               # 원본 사전 데이터 (낱말,품사)
│       ├── kr_korean_simple.csv        # 전처리 완료 (낱말만, 508,142줄)
│       └── README.md
├── step1/
│   ├── step01_charfreq.c               # 완성형 글자(가-힣) 빈도 집계
│   └── charfreq_result.txt             # 결과: 2,467개 글자, 빈도 내림차순
├── step2/
│   ├── step02_unigram_sample.c         # 유니그램 확률 분포 기반 샘플링
│   └── unigram_sample_result.txt       # 결과: 10세트 × 10글자
├── step3/
│   ├── step03_bigram_freq.c            # 바이그램 빈도 집계 (^/$ 경계 포함)
│   ├── bigram_stats.txt                # 통계: 172,685개 바이그램
│   ├── step03_bigram_sample.c          # 바이그램 샘플링 + 사전 검증 [O]/[X]
│   └── bigram_result.txt              # 결과: 1,000개 샘플
├── step4/
│   ├── step04_trigram_freq.c           # 트라이그램 빈도 집계 (^^/^/$ 경계 포함)
│   ├── trigram_stats.txt               # 통계: 659,449개 트라이그램
│   ├── step04_trigram_sample.c         # 트라이그램 샘플링 + 사전 검증 [O]/[X]
│   └── trigram_result.txt             # 결과: 1,000개 샘플
├── report/
│   ├── generate_report.py              # 바이그램 vs 트라이그램 비교 분석 스크립트
│   └── comparison_report.md           # 비교 리포트
├── feedback/
│   └── high_token_patterns.md          # 고토큰 소비 원인 및 방지책
├── README.md
└── CLAUDE.md
```

## 데이터 전처리

`kr_korean.csv` → `kr_korean_simple.csv` 변환 규칙:
- `-`, `^`, 공백은 복합어 표기 구분자 → **제거 후 글자를 합쳐 유지**
- 품사 정보 제거, 낱말만 보존 / 레코드 수 동일 유지 (508,142줄)

```python
import re
with open('data/korean-dict/kr_korean.csv', encoding='utf-8-sig') as f, \
     open('data/korean-dict/kr_korean_simple.csv', 'w', encoding='utf-8') as out:
    for line in f:
        word = line.split(',')[0]
        word = re.sub(r'[-^ ]', '', word)
        out.write(word + '\n')
```

## 실행 방법

```bash
# Step 1: 글자 빈도
gcc -O2 -o step1/step01_charfreq step1/step01_charfreq.c
./step1/step01_charfreq > step1/charfreq_result.txt

# Step 2: 유니그램 샘플링
gcc -O2 -o step2/step02_unigram_sample step2/step02_unigram_sample.c
./step2/step02_unigram_sample

# Step 3: 바이그램
gcc -O2 -o step3/step03_bigram_freq step3/step03_bigram_freq.c
./step3/step03_bigram_freq
gcc -O2 -o step3/step03_bigram_sample step3/step03_bigram_sample.c
./step3/step03_bigram_sample

# Step 4: 트라이그램
gcc -O2 -o step4/step04_trigram_freq step4/step04_trigram_freq.c
./step4/step04_trigram_freq
gcc -O2 -o step4/step04_trigram_sample step4/step04_trigram_sample.c
./step4/step04_trigram_sample

# 비교 리포트
python3 report/generate_report.py
```

## 진행 단계 및 주요 결과

| Step | 내용 | 주요 수치 |
|------|------|---------|
| step1 | 완성형 글자 빈도 집계 | 2,467개 고유 글자 |
| step2 | 유니그램 샘플링 | 빈도 비례 가중 샘플링 |
| step3 | 바이그램 빈도 + 샘플링 | 172,685개 바이그램, 사전 일치율 41.3% |
| step4 | 트라이그램 빈도 + 샘플링 | 659,449개 트라이그램, 사전 일치율 62.7% |
| report | 바이그램 vs 트라이그램 비교 | 트라이그램 +21.4%p 높음 |
