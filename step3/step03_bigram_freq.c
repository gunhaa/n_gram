#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HANGUL_START  0xAC00
#define HANGUL_END    0xD7A3
#define HANGUL_COUNT  11172

/*
 * START/END 토큰을 HANGUL_COUNT 이후 인덱스로 표현한다.
 *   START_IDX = 11172  → 출력 시 '^'
 *   END_IDX   = 11173  → 출력 시 '$'
 * TOTAL_TOKENS = 11174 를 key 인코딩의 기저로 사용한다.
 */
#define TOTAL_TOKENS  (HANGUL_COUNT + 2)
#define START_IDX     HANGUL_COUNT
#define END_IDX       (HANGUL_COUNT + 1)

/*
 * 해시 테이블 크기: 2^21 = 2,097,152
 * 바이그램(+시작/끝 포함) 약 17만 개 → 로드 팩터 ~0.08
 */
#define TABLE_SIZE    (1 << 21)
#define TABLE_MASK    (TABLE_SIZE - 1)

#define MAX_WORD_LEN  256

typedef struct {
    uint32_t key;   /* from_idx * TOTAL_TOKENS + to_idx + 1, 0이면 빈 슬롯 */
    long     count;
} HashEntry;

typedef struct {
    uint32_t key;
    long     count;
} BigramEntry;

static int cmp_freq(const void *a, const void *b) {
    const BigramEntry *ea = (const BigramEntry *)a;
    const BigramEntry *eb = (const BigramEntry *)b;
    if (eb->count != ea->count)
        return (eb->count > ea->count) - (eb->count < ea->count);
    return (int)(ea->key - eb->key);
}

/*
 * UTF-8 바이트 시퀀스에서 유니코드 코드포인트 1개를 읽는다.
 */
static uint32_t read_utf8(const unsigned char *s, int *bytes) {
    if ((s[0] & 0x80) == 0x00) { *bytes = 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0) { *bytes = 2; return ((s[0] & 0x1F) << 6)  | (s[1] & 0x3F); }
    if ((s[0] & 0xF0) == 0xE0) { *bytes = 3; return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
                                  *bytes = 4; return ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
}

/*
 * 오픈 어드레싱(선형 탐사) 해시 테이블에 key를 삽입하거나 count를 1 증가시킨다.
 */
static void hash_increment(HashEntry *table, uint32_t key) {
    uint32_t h = (key ^ (key >> 16)) & TABLE_MASK;
    while (table[h].key != 0 && table[h].key != key)
        h = (h + 1) & TABLE_MASK;
    table[h].key = key;
    table[h].count++;
}

/*
 * 유니코드 코드포인트를 3바이트 UTF-8로 인코딩한다.
 * 한글 완성형(U+AC00~U+D7A3)은 항상 3바이트이다.
 */
static void encode_utf8(uint32_t cp, unsigned char out[4]) {
    out[0] = (unsigned char)(0xE0 | (cp >> 12));
    out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (unsigned char)(0x80 | (cp & 0x3F));
    out[3] = 0;
}

int main(int argc, char *argv[]) {
    const char *in_path  = argc > 1 ? argv[1] : "data/korean-dict/kr_korean_simple.csv";
    const char *out_path = argc > 2 ? argv[2] : "step3/bigram_stats.txt";

    HashEntry *table = calloc(TABLE_SIZE, sizeof(HashEntry));
    if (!table) { fprintf(stderr, "out of memory\n"); return 1; }

    FILE *fp = fopen(in_path, "r");
    if (!fp) { fprintf(stderr, "cannot open: %s\n", in_path); free(table); return 1; }

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        /* 한 줄(단어)을 한글 코드포인트 배열로 파싱 */
        uint32_t chars[MAX_WORD_LEN];
        int len = 0;
        const unsigned char *p = (unsigned char *)line;

        while (*p && *p != '\n' && len < MAX_WORD_LEN) {
            int n;
            uint32_t cp = read_utf8(p, &n);
            p += n;
            if (cp >= HANGUL_START && cp <= HANGUL_END)
                chars[len++] = cp;
        }
        if (len == 0) continue;

        /*
         * 단어 경계를 포함한 바이그램 생성:
         *   ^ → 첫 글자, 글자 쌍, 마지막 글자 → $
         *
         * key = from_idx * TOTAL_TOKENS + to_idx + 1
         * (+1로 key=0을 빈 슬롯 표시로 예약)
         */
        hash_increment(table,
            (uint32_t)(START_IDX * TOTAL_TOKENS + (chars[0] - HANGUL_START) + 1));

        for (int i = 0; i < len - 1; i++)
            hash_increment(table,
                (uint32_t)((chars[i] - HANGUL_START) * TOTAL_TOKENS
                         + (chars[i+1] - HANGUL_START) + 1));

        hash_increment(table,
            (uint32_t)((chars[len-1] - HANGUL_START) * TOTAL_TOKENS + END_IDX + 1));
    }
    fclose(fp);

    /* 유효 항목 수집 */
    int total = 0;
    for (int i = 0; i < TABLE_SIZE; i++)
        if (table[i].count > 0) total++;

    BigramEntry *res = malloc(total * sizeof(BigramEntry));
    int idx = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (table[i].count > 0) {
            res[idx].key   = table[i].key;
            res[idx].count = table[i].count;
            idx++;
        }
    }
    free(table);

    qsort(res, total, sizeof(BigramEntry), cmp_freq);

    /*
     * 출력 형식:
     *   ^가\t빈도   — 문장 시작 → 가
     *   다$\t빈도   — 다 → 문장 끝
     *   가나\t빈도  — 일반 바이그램
     */
    FILE *out = fopen(out_path, "w");
    if (!out) { fprintf(stderr, "cannot open: %s\n", out_path); free(res); return 1; }

    for (int i = 0; i < total; i++) {
        uint32_t k        = res[i].key - 1;
        uint32_t from_idx = k / TOTAL_TOKENS;
        uint32_t to_idx   = k % TOTAL_TOKENS;

        unsigned char g1[4], g2[4];

        if (from_idx == START_IDX) {
            encode_utf8(HANGUL_START + to_idx, g2);
            fprintf(out, "^%s\t%ld\n", g2, res[i].count);
        } else if (to_idx == END_IDX) {
            encode_utf8(HANGUL_START + from_idx, g1);
            fprintf(out, "%s$\t%ld\n", g1, res[i].count);
        } else {
            encode_utf8(HANGUL_START + from_idx, g1);
            encode_utf8(HANGUL_START + to_idx,   g2);
            fprintf(out, "%s%s\t%ld\n", g1, g2, res[i].count);
        }
    }

    fclose(out);
    printf("고유 바이그램: %d개 (시작/끝 포함) → %s\n", total, out_path);

    free(res);
    return 0;
}
