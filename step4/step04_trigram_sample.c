#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define HANGUL_START  0xAC00
#define HANGUL_END    0xD7A3

#define START_CP      0x0001u   /* 내부 START 토큰 */
#define END_CP        0x0002u   /* 내부 END 토큰   */

#define SEQ_COUNT     1000
#define MAX_SEQ_LEN   50
#define MAX_DICT_SIZE 520000

typedef struct {
    uint32_t cp;
    double   prob;   /* 누적 확률(CDF) */
} TransEntry;

/*
 * 트라이그램 전이 행: (from1_cp, from2_cp) → 다음 글자 분포
 * 정렬 키: from1_cp * SORT_BASE + from2_cp (이진 탐색용)
 */
#define SORT_BASE 0x100000u

typedef struct {
    uint64_t    sort_key;   /* from1_cp * SORT_BASE + from2_cp */
    uint32_t    from1_cp;
    uint32_t    from2_cp;
    TransEntry *trans;
    int         trans_count;
} TrigramRow;

/* ── 유틸 ─────────────────────────────────────────────────────────────── */

static double rand_unit(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

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

static void encode_utf8(uint32_t cp, unsigned char out[4]) {
    out[0] = (unsigned char)(0xE0 | (cp >> 12));
    out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (unsigned char)(0x80 | (cp & 0x3F));
    out[3] = 0;
}

/* ── 사전 검증 ─────────────────────────────────────────────────────────── */

typedef struct { char **words; int size; } Dict;

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

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

/* ── 트라이그램 로딩 ─────────────────────────────────────────────────────── */

static int cmp_row(const void *a, const void *b) {
    const TrigramRow *ra = (const TrigramRow *)a;
    const TrigramRow *rb = (const TrigramRow *)b;
    return (ra->sort_key > rb->sort_key) - (ra->sort_key < rb->sort_key);
}

/*
 * trigram_stats.txt 파싱 규칙:
 *   ^^글자\t빈도    → from1=START, from2=START, to=글자
 *   ^글자글자\t빈도 → from1=START, from2=글자, to=글자
 *   ^글자$\t빈도    → from1=START, from2=글자, to=END
 *   글자글자글자\t빈도 → 일반
 *   글자글자$\t빈도  → to=END
 */
static TrigramRow *load_trigram(const char *path, int *out_n) {
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "cannot open: %s\n", path); return NULL; }

#define MAX_CTX 600000
    uint64_t *ctx_keys = malloc(MAX_CTX * sizeof(uint64_t));
    int      *ctx_cnt  = malloc(MAX_CTX * sizeof(int));
    long     *ctx_sum  = malloc(MAX_CTX * sizeof(long));
    int       nctx = 0;
    if (!ctx_keys || !ctx_cnt || !ctx_sum) {
        fprintf(stderr, "out of memory\n"); return NULL;
    }

    char line[128];

    /* 1차 패스: (from1,from2) 맥락별 전이 수/빈도 합 집계 */
    while (fgets(line, sizeof(line), fp)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        long freq = atol(tab + 1);

        uint32_t from1, from2;
        const unsigned char *s = (unsigned char *)line;

        if (s[0] == '^' && s[1] == '^') {
            from1 = START_CP; from2 = START_CP;
        } else if (s[0] == '^') {
            from1 = START_CP;
            from2 = ((s[1]&0x0F)<<12)|((s[2]&0x3F)<<6)|(s[3]&0x3F);
        } else {
            from1 = ((s[0]&0x0F)<<12)|((s[1]&0x3F)<<6)|(s[2]&0x3F);
            from2 = ((s[3]&0x0F)<<12)|((s[4]&0x3F)<<6)|(s[5]&0x3F);
        }

        uint64_t k = (uint64_t)from1 * SORT_BASE + from2;
        int found = -1;
        for (int i = 0; i < nctx; i++)
            if (ctx_keys[i] == k) { found = i; break; }
        if (found == -1) {
            found = nctx++;
            ctx_keys[found] = k;
            ctx_cnt[found]  = 0;
            ctx_sum[found]  = 0;
        }
        ctx_cnt[found]++;
        ctx_sum[found] += freq;
    }

    /* TrigramRow 배열 할당 */
    TrigramRow *rows = malloc(nctx * sizeof(TrigramRow));
    for (int i = 0; i < nctx; i++) {
        rows[i].sort_key    = ctx_keys[i];
        rows[i].from1_cp    = (uint32_t)(ctx_keys[i] / SORT_BASE);
        rows[i].from2_cp    = (uint32_t)(ctx_keys[i] % SORT_BASE);
        rows[i].trans       = malloc(ctx_cnt[i] * sizeof(TransEntry));
        rows[i].trans_count = 0;
    }

    /* 2차 패스: TransEntry 채우기 */
    rewind(fp);
    while (fgets(line, sizeof(line), fp)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        long freq = atol(tab + 1);

        uint32_t from1, from2, to;
        const unsigned char *s = (unsigned char *)line;

        if (s[0] == '^' && s[1] == '^') {
            from1 = START_CP; from2 = START_CP;
            to    = ((s[2]&0x0F)<<12)|((s[3]&0x3F)<<6)|(s[4]&0x3F);
        } else if (s[0] == '^') {
            from1 = START_CP;
            from2 = ((s[1]&0x0F)<<12)|((s[2]&0x3F)<<6)|(s[3]&0x3F);
            to    = (s[4] == '$') ? END_CP
                  : ((s[4]&0x0F)<<12)|((s[5]&0x3F)<<6)|(s[6]&0x3F);
        } else {
            from1 = ((s[0]&0x0F)<<12)|((s[1]&0x3F)<<6)|(s[2]&0x3F);
            from2 = ((s[3]&0x0F)<<12)|((s[4]&0x3F)<<6)|(s[5]&0x3F);
            to    = (s[6] == '$') ? END_CP
                  : ((s[6]&0x0F)<<12)|((s[7]&0x3F)<<6)|(s[8]&0x3F);
        }

        uint64_t k = (uint64_t)from1 * SORT_BASE + from2;
        for (int i = 0; i < nctx; i++) {
            if (ctx_keys[i] == k) {
                int ti = rows[i].trans_count++;
                rows[i].trans[ti].cp   = to;
                rows[i].trans[ti].prob = (double)freq / (double)ctx_sum[i];
                break;
            }
        }
    }
    fclose(fp);

    /* CDF 변환 및 sort_key 기준 정렬 (이진 탐색용) */
    for (int i = 0; i < nctx; i++) {
        double cum = 0.0;
        for (int j = 0; j < rows[i].trans_count; j++) {
            cum += rows[i].trans[j].prob;
            rows[i].trans[j].prob = cum;
        }
    }
    qsort(rows, nctx, sizeof(TrigramRow), cmp_row);

    free(ctx_keys); free(ctx_cnt); free(ctx_sum);
    *out_n = nctx;
    return rows;
}

/*
 * (from1_cp, from2_cp) 쌍으로 TrigramRow를 이진 탐색한다.
 */
static TrigramRow *find_row(TrigramRow *rows, int n,
                            uint32_t from1, uint32_t from2) {
    uint64_t k = (uint64_t)from1 * SORT_BASE + from2;
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (rows[mid].sort_key == k) return &rows[mid];
        if (rows[mid].sort_key  < k) lo = mid + 1;
        else                         hi = mid - 1;
    }
    return NULL;
}

/* ── main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    const char *trigram_path = argc > 1 ? argv[1] : "step4/trigram_stats.txt";
    const char *out_path     = argc > 2 ? argv[2] : "step4/trigram_result.txt";
    const char *dict_path    = argc > 3 ? argv[3] : "data/korean-dict/kr_korean_simple.csv";

    int row_n = 0;
    TrigramRow *rows = load_trigram(trigram_path, &row_n);
    if (!rows) return 1;

    Dict dict = load_dict(dict_path);
    if (!dict.words) return 1;

    srand((unsigned)time(NULL));

    FILE *out = fopen(out_path, "w");
    if (!out) { fprintf(stderr, "cannot open: %s\n", out_path); return 1; }

    for (int s = 0; s < SEQ_COUNT; s++) {
        char seq[MAX_SEQ_LEN * 3 + 1];
        int  seq_bytes = 0;
        seq[0] = '\0';

        /*
         * 트라이그램 문장 생성:
         *   컨텍스트 (prev1, prev2)를 START로 초기화하고
         *   매 스텝마다 (prev1, prev2) 조건부 분포에서 다음 글자 샘플링.
         *   END가 뽑히거나 MAX_SEQ_LEN 초과 시 종료.
         */
        uint32_t prev1 = START_CP, prev2 = START_CP;

        for (int i = 0; i < MAX_SEQ_LEN; i++) {
            TrigramRow *row = find_row(rows, row_n, prev1, prev2);
            if (!row || row->trans_count == 0) break;

            int ti       = sample_cdf(row->trans, row->trans_count);
            uint32_t cur = row->trans[ti].cp;
            if (cur == END_CP) break;

            unsigned char g[4];
            encode_utf8(cur, g);
            memcpy(seq + seq_bytes, g, 3);
            seq_bytes += 3;
            seq[seq_bytes] = '\0';

            prev1 = prev2;
            prev2 = cur;
        }

        const char *mark = in_dict(&dict, seq) ? "[O]" : "[X]";
        fprintf(out, "seq %04d: %s %s\n", s + 1, seq, mark);
    }

    fclose(out);
    printf("saved: %s\n", out_path);

    for (int i = 0; i < row_n; i++) free(rows[i].trans);
    free(rows);
    for (int i = 0; i < dict.size; i++) free(dict.words[i]);
    free(dict.words);
    return 0;
}
