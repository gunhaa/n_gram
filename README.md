# Korean N-gram Analyzer

한국어 단어 빈도 및 N-gram 분석 프로젝트.

## 데이터 출처

| 항목 | 내용 |
|------|------|
| 원본 | 국립국어원 표준국어대사전 (stdict.korean.go.kr) |
| 소스 | [korean-word-game/db](https://github.com/korean-word-game/db) |
| 수집 | 2018-11-13 |
| 크기 | 508,412줄 / 9.8MB |
| 라이선스 | 저작권 없음 |

## 프로젝트 구조

```
n_gram/
├── data/
│   └── korean-dict/
│       ├── kr_korean.csv           # 원본 사전 데이터 (낱말,품사)
│       ├── kr_korean_simple.csv    # 전처리 완료 데이터 (순수 한글 단어만)
│       └── README.md
├── step1/
│   ├── step01_charfreq.c           # 완성형 글자 빈도 분석
│   ├── step01_charfreq             # 컴파일된 실행 파일
│   └── charfreq_result.txt         # 분석 결과 (글자\t빈도, 내림차순)
├── feedback/                       # 작업 기록
├── README.md
└── CLAUDE.md
```

## 데이터 전처리

`kr_korean.csv` → `kr_korean_simple.csv` 변환 규칙:
- `-` 포함 단어 제거 (`냉수-권`, `-가` 등)
- `^` 포함 단어 제거 (`가감^소거법` 등)
- 공백 포함 단어 제거 (외래어 표기 등)
- 순수 한글 음절(가-힣)로만 구성된 단어만 유지
- 품사 정보 제거, 단어만 보존

```bash
grep -P '^[가-힣]+,' data/korean-dict/kr_korean.csv | cut -d',' -f1 > data/korean-dict/kr_korean_simple.csv
```

## 실행 방법

### Step 1: 완성형 글자 빈도 분석

```bash
gcc -O2 -o step1/step01_charfreq step1/step01_charfreq.c
./step1/step01_charfreq > step1/charfreq_result.txt
```

결과 형식: `글자\t빈도` (빈도 내림차순)
