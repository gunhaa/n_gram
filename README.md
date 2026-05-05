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
│       ├── kr_korean_simple.csv        # 전처리 완료 데이터 (낱말만, 508,142줄)
│       └── README.md
├── step1/
│   ├── step01_charfreq.c               # 완성형 글자 빈도 집계
│   ├── step01_charfreq                 # 컴파일된 실행 파일
│   └── charfreq_result.txt             # 결과: 글자\t빈도 (빈도 내림차순)
├── step2/
│   ├── step02_unigram_sample.c         # 유니그램 확률 분포 기반 샘플링
│   ├── step02_unigram_sample           # 컴파일된 실행 파일
│   └── unigram_sample_result.txt       # 결과: 10세트 × 10글자
├── feedback/
│   └── heavy_io_analysis.md            # 고비용 I/O 분석 및 최적화 방안
├── README.md
└── CLAUDE.md
```

## 데이터 전처리

`kr_korean.csv` → `kr_korean_simple.csv` 변환 규칙:
- `-`, `^`, 공백은 복합어 표기 구분자 → **제거 후 글자를 합쳐 유지**
- 품사 정보 제거, 낱말만 보존
- 레코드 수 동일 유지 (508,142줄)

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

### Step 1: 완성형 글자 빈도 분석

```bash
gcc -O2 -o step1/step01_charfreq step1/step01_charfreq.c
./step1/step01_charfreq > step1/charfreq_result.txt
```

결과 형식: `글자\t빈도` (빈도 내림차순, 2,467개 글자)

### Step 2: 유니그램 샘플링

```bash
gcc -O2 -o step2/step02_unigram_sample step2/step02_unigram_sample.c
./step2/step02_unigram_sample
```

결과 형식: `set 01: 하없원하이어나마이음` × 10세트

## 진행 단계

| Step | 내용 | 상태 |
|------|------|------|
| step1 | 완성형 글자(가-힣) 빈도 집계 | 완료 |
| step2 | 유니그램 확률 분포 기반 샘플링 | 완료 |
