#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define HANGUL_START  0xAC00
#define HANGUL_END    0xD7A3

/*
 * 내부 표현용 특수 코드포인트 (한글 범위 밖)
 *   START_CP: 문장 시작 토큰 (stats 파일의 '^')
 *   END_CP  : 문장 종료 토큰 (stats 파일의 '$')
 */
#define START_CP      0x0001u
#define END_CP        0x0002u

#define SEQ_COUNT     1000   /* 생성할 문장 수 */
#define MAX_SEQ_LEN   50     /* 무한 루프 방지용 최대 글자 수 */
#define MAX_DICT_SIZE 520000 /* 사전 최대 항목 수 */

typedef struct {
    uint32_t cp;
    double   prob;  /* 누적 확률(CDF) */
} TransEntry;

typedef struct {
    uint32_t    from_cp;
    TransEntry *trans;
    int         trans_count;
} BigramRow;

/* ── 사전 검증 계층 ─────────────────────────────────────────────────────── */

typedef struct {
    char  **words;  /* 정렬된 단어 포인터 배열 */
    int     size;
} Dict;

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/*
 * kr_korean_simple.csv를 읽어 정렬된 사전 배열을 구성한다.
 * 이진 탐색으로 O(log n) 검증에 사용한다.
 */
static Dict load_dict(const char *path) {
    Dict d = { NULL, 0 };
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "cannot open dict: %s\n", path); return d; }

    d.words = malloc(MAX_DICT_SIZE * sizeof(char *));
    char line[256];
    while (fgets(line, sizeof(line), fp) && d.size < MAX_DICT_SIZE) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) len--;
        if (len == 0) continue;
        d.words[d.size] = malloc(len + 1);
        memcpy(d.words[d.size], line, len);
        d.words[d.size][len] = '\0';
        d.size++;
    }
    fclose(fp);

    qsort(d.words, d.size, sizeof(char *), cmp_str);
    return d;
}

/*
 * word가 사전에 존재하면 1, 아니면 0 반환.
 * 정렬된 배열에 이진 탐색을 수행한다.
 */
static int in_dict(const Dict *d, const char *word) {
    int lo = 0, hi = d->size - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = strcmp(d->words[mid], word);
        if (cmp == 0) return 1;
        if (cmp < 0)  lo = mid + 1;
        else          hi = mid - 1;
    }
    return 0;
}

static double rand_unit(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

/*
 * 누적 확률 배열(CDF)에서 이진 탐색으로 하나의 인덱스를 샘플링한다.
 */
static int sample_cdf(const TransEntry *trans, int n) {
    double r = rand_unit();
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (trans[mid].prob < r) lo = mid + 1;
        else                     hi = mid;
    }
    return lo;
}

/*
 * bigram_stats.txt를 파싱해 BigramRow 배열을 구성한다.
 *
 * 파일 형식:
 *   ^가\t빈도   → from_cp = START_CP
 *   다$\t빈도   → to_cp   = END_CP
 *   가나\t빈도  → 일반 바이그램
 *
 * 구성 순서:
 *   1차 패스: from_cp별 전이 수 및 빈도 합 집계
 *   2차 패스: TransEntry 채우기 후 CDF 변환
 */
static BigramRow *load_bigram(const char *path, int *out_n) {
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "cannot open: %s\n", path); return NULL; }

    /* from_cp → (count, sum) 집계용 임시 구조 */
#define MAX_FROM 15000
    uint32_t from_keys[MAX_FROM];
    int      from_cnt[MAX_FROM];
    long     from_sum[MAX_FROM];
    int      nfrom = 0;

    char line[64];

    /* 1차 패스 */
    while (fgets(line, sizeof(line), fp)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        long freq = atol(tab + 1);

        uint32_t from_cp;
        const unsigned char *s = (unsigned char *)line;

        if (s[0] == '^') {
            from_cp = START_CP;
        } else {
            /* 3바이트 UTF-8 → 코드포인트 */
            from_cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        }

        /* from_keys 선형 탐색(수가 적어 허용) */
        int found = -1;
        for (int i = 0; i < nfrom; i++)
            if (from_keys[i] == from_cp) { found = i; break; }
        if (found == -1) {
            found = nfrom++;
            from_keys[found] = from_cp;
            from_cnt[found]  = 0;
            from_sum[found]  = 0;
        }
        from_cnt[found]++;
        from_sum[found] += freq;
    }

    /* BigramRow 배열 할당 */
    BigramRow *rows = malloc(nfrom * sizeof(BigramRow));
    for (int i = 0; i < nfrom; i++) {
        rows[i].from_cp     = from_keys[i];
        rows[i].trans       = malloc(from_cnt[i] * sizeof(TransEntry));
        rows[i].trans_count = 0;
    }

    /* 2차 패스 */
    rewind(fp);
    while (fgets(line, sizeof(line), fp)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        long freq = atol(tab + 1);

        uint32_t from_cp, to_cp;
        const unsigned char *s = (unsigned char *)line;

        if (s[0] == '^') {
            from_cp = START_CP;
            to_cp   = ((s[1] & 0x0F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        } else {
            from_cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
            /* 4번째 바이트가 '$'(0x24)이면 END_CP, 아니면 한글 */
            if (s[3] == '$') {
                to_cp = END_CP;
            } else {
                to_cp = ((s[3] & 0x0F) << 12) | ((s[4] & 0x3F) << 6) | (s[5] & 0x3F);
            }
        }

        /* 해당 from_cp의 row 탐색 */
        for (int i = 0; i < nfrom; i++) {
            if (rows[i].from_cp == from_cp) {
                int ti = rows[i].trans_count++;
                rows[i].trans[ti].cp   = to_cp;
                /* 확률은 나중에 CDF로 변환 — 일단 빈도를 합으로 나눠 저장 */
                for (int j = 0; j < nfrom; j++) {
                    if (from_keys[j] == from_cp) {
                        rows[i].trans[ti].prob = (double)freq / (double)from_sum[j];
                        break;
                    }
                }
                break;
            }
        }
    }
    fclose(fp);

    /* 각 행의 전이 확률을 누적 확률(CDF)로 변환 */
    for (int i = 0; i < nfrom; i++) {
        double cum = 0.0;
        for (int j = 0; j < rows[i].trans_count; j++) {
            cum += rows[i].trans[j].prob;
            rows[i].trans[j].prob = cum;
        }
    }

    *out_n = nfrom;
    return rows;
}

/*
 * from_cp에 해당하는 BigramRow를 선형 탐색으로 반환한다.
 * 없으면 NULL 반환.
 */
static BigramRow *find_row(BigramRow *rows, int n, uint32_t from_cp) {
    for (int i = 0; i < n; i++)
        if (rows[i].from_cp == from_cp) return &rows[i];
    return NULL;
}

/*
 * 유니코드 코드포인트를 3바이트 UTF-8로 인코딩한다.
 */
static void encode_utf8(uint32_t cp, unsigned char out[4]) {
    out[0] = (unsigned char)(0xE0 | (cp >> 12));
    out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (unsigned char)(0x80 | (cp & 0x3F));
    out[3] = 0;
}

int main(int argc, char *argv[]) {
    const char *bigram_path = argc > 1 ? argv[1] : "step3/bigram_stats.txt";
    const char *out_path    = argc > 2 ? argv[2] : "step3/bigram_result.txt";
    const char *dict_path   = argc > 3 ? argv[3] : "data/korean-dict/kr_korean_simple.csv";

    int row_n = 0;
    BigramRow *rows = load_bigram(bigram_path, &row_n);
    if (!rows) return 1;

    Dict dict = load_dict(dict_path);
    if (!dict.words) return 1;

    srand((unsigned)time(NULL));

    FILE *out = fopen(out_path, "w");
    if (!out) { fprintf(stderr, "cannot open: %s\n", out_path); return 1; }

    BigramRow *start_row = find_row(rows, row_n, START_CP);
    if (!start_row) { fprintf(stderr, "no START transitions found\n"); return 1; }

    for (int s = 0; s < SEQ_COUNT; s++) {
        /*
         * 문장 생성:
         *   1. START 토큰의 전이 분포에서 첫 글자 샘플링
         *   2. 현재 글자의 전이 분포에서 다음 글자 샘플링
         *   3. END 토큰이 뽑히면 문장 종료
         *   4. MAX_SEQ_LEN 초과 시 강제 종료 (안전장치)
         */
        char seq[MAX_SEQ_LEN * 3 + 1];  /* 생성된 시퀀스를 UTF-8 문자열로 축적 */
        int  seq_bytes = 0;
        seq[0] = '\0';

        int ti       = sample_cdf(start_row->trans, start_row->trans_count);
        uint32_t cur = start_row->trans[ti].cp;

        for (int i = 0; i < MAX_SEQ_LEN; i++) {
            if (cur == END_CP) break;

            unsigned char g[4];
            encode_utf8(cur, g);
            memcpy(seq + seq_bytes, g, 3);
            seq_bytes += 3;
            seq[seq_bytes] = '\0';

            BigramRow *row = find_row(rows, row_n, cur);
            if (!row || row->trans_count == 0) break;

            int ni = sample_cdf(row->trans, row->trans_count);
            cur = row->trans[ni].cp;
        }

        /* 사전 검증 후 [O]/[X] 표기 */
        const char *mark = in_dict(&dict, seq) ? "[O]" : "[X]";
        fprintf(out, "seq %02d: %s %s\n", s + 1, seq, mark);
    }

    fclose(out);
    printf("saved: %s\n", out_path);

    for (int i = 0; i < row_n; i++) free(rows[i].trans);
    free(rows);
    for (int i = 0; i < dict.size; i++) free(dict.words[i]);
    free(dict.words);
    return 0;
}
