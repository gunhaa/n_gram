#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HANGUL_START  0xAC00
#define HANGUL_END    0xD7A3
#define HANGUL_COUNT  11172

/*
 * START/END 토큰 인덱스 (바이그램과 동일한 설계)
 *   START_IDX = 11172  → 출력 시 '^'
 *   END_IDX   = 11173  → 출력 시 '$'
 *
 * 트라이그램 키: from1_idx * T² + from2_idx * T + to_idx + 1
 * T = TOTAL_TOKENS = 11174
 * 최대 키 ≈ 11174³ ≈ 1.4×10¹² → uint64_t 필요
 */
#define TOTAL_TOKENS  (HANGUL_COUNT + 2)
#define START_IDX     HANGUL_COUNT
#define END_IDX       (HANGUL_COUNT + 1)

/*
 * 해시 테이블 크기: 2^22 = 4,194,304
 * 예상 고유 트라이그램 ~40만 개 → 로드 팩터 ~0.10
 * 각 슬롯 16바이트 → 64MB
 */
#define TABLE_SIZE    (1 << 22)
#define TABLE_MASK    ((uint64_t)(TABLE_SIZE - 1))

#define MAX_WORD_LEN  256

typedef struct {
    uint64_t key;
    long     count;
} HashEntry;

typedef struct {
    uint64_t key;
    long     count;
} TrigramEntry;

static int cmp_freq(const void *a, const void *b) {
    const TrigramEntry *ea = (const TrigramEntry *)a;
    const TrigramEntry *eb = (const TrigramEntry *)b;
    if (eb->count != ea->count)
        return (eb->count > ea->count) - (eb->count < ea->count);
    return (ea->key > eb->key) - (ea->key < eb->key);
}

static uint32_t read_utf8(const unsigned char *s, int *bytes) {
    if ((s[0] & 0x80) == 0x00) { *bytes = 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0) { *bytes = 2; return ((s[0] & 0x1F) << 6)  | (s[1] & 0x3F); }
    if ((s[0] & 0xF0) == 0xE0) { *bytes = 3; return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
                                  *bytes = 4; return ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
}

/*
 * 64비트 키에 대한 오픈 어드레싱(선형 탐사) 해시 테이블.
 * Murmur-inspired 믹싱으로 충돌 최소화.
 */
static void hash_increment(HashEntry *table, uint64_t key) {
    uint64_t h = key ^ (key >> 33);
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h &= TABLE_MASK;
    while (table[h].key != 0 && table[h].key != key)
        h = (h + 1) & TABLE_MASK;
    table[h].key = key;
    table[h].count++;
}

static void encode_utf8(uint32_t cp, unsigned char out[4]) {
    out[0] = (unsigned char)(0xE0 | (cp >> 12));
    out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (unsigned char)(0x80 | (cp & 0x3F));
    out[3] = 0;
}

int main(int argc, char *argv[]) {
    const char *in_path  = argc > 1 ? argv[1] : "data/korean-dict/kr_korean_simple.csv";
    const char *out_path = argc > 2 ? argv[2] : "step4/trigram_stats.txt";

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
         * 트라이그램 생성 (START 토큰 2개 + 단어 + END 토큰):
         *   (^, ^, c0)        ← ^^첫글자
         *   (^, c0, c1)       ← ^첫글자두번째글자
         *   (ci, ci+1, ci+2)  ← 중간
         *   (cn-2, cn-1, $)   ← 마지막글자$
         *
         * key = f1_idx * T² + f2_idx * T + to_idx + 1
         */
        uint64_t T  = TOTAL_TOKENS;
        uint64_t si = START_IDX, ei = END_IDX;

        /* (^, ^, c0) */
        hash_increment(table,
            si*T*T + si*T + (chars[0] - HANGUL_START) + 1);

        if (len >= 2) {
            /* (^, c0, c1) */
            hash_increment(table,
                si*T*T + (chars[0]-HANGUL_START)*T + (chars[1]-HANGUL_START) + 1);
        } else {
            /* 1글자 단어: (^, c0, $) */
            hash_increment(table,
                si*T*T + (chars[0]-HANGUL_START)*T + ei + 1);
        }

        /* 중간 트라이그램 */
        for (int i = 0; i + 2 < len; i++)
            hash_increment(table,
                (uint64_t)(chars[i]  -HANGUL_START)*T*T
              + (uint64_t)(chars[i+1]-HANGUL_START)*T
              + (uint64_t)(chars[i+2]-HANGUL_START) + 1);

        /* (cn-2, cn-1, $) — len >= 2 일 때만 */
        if (len >= 2)
            hash_increment(table,
                (uint64_t)(chars[len-2]-HANGUL_START)*T*T
              + (uint64_t)(chars[len-1]-HANGUL_START)*T
              + ei + 1);
    }
    fclose(fp);

    /* 유효 항목 수집 */
    int total = 0;
    for (int i = 0; i < TABLE_SIZE; i++)
        if (table[i].count > 0) total++;

    TrigramEntry *res = malloc(total * sizeof(TrigramEntry));
    int idx = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (table[i].count > 0) {
            res[idx].key   = table[i].key;
            res[idx].count = table[i].count;
            idx++;
        }
    }
    free(table);

    qsort(res, total, sizeof(TrigramEntry), cmp_freq);

    /*
     * 출력 형식:
     *   ^^가\t빈도   — (START,START) → 가
     *   ^가나\t빈도  — (START,가) → 나
     *   가나다\t빈도 — 일반 트라이그램
     *   가나$\t빈도  — 나 → END
     */
    FILE *out = fopen(out_path, "w");
    if (!out) { fprintf(stderr, "cannot open: %s\n", out_path); free(res); return 1; }

    uint64_t T = TOTAL_TOKENS;
    for (int i = 0; i < total; i++) {
        uint64_t k      = res[i].key - 1;
        uint64_t f1_idx = k / (T * T);
        uint64_t f2_idx = (k / T) % T;
        uint64_t to_idx = k % T;

        unsigned char g1[4], g2[4], g3[4];

        if (f1_idx == (uint64_t)START_IDX && f2_idx == (uint64_t)START_IDX) {
            encode_utf8(HANGUL_START + (uint32_t)to_idx, g3);
            fprintf(out, "^^%s\t%ld\n", g3, res[i].count);
        } else if (f1_idx == (uint64_t)START_IDX) {
            encode_utf8(HANGUL_START + (uint32_t)f2_idx, g2);
            if (to_idx == (uint64_t)END_IDX)
                fprintf(out, "^%s$\t%ld\n", g2, res[i].count);
            else {
                encode_utf8(HANGUL_START + (uint32_t)to_idx, g3);
                fprintf(out, "^%s%s\t%ld\n", g2, g3, res[i].count);
            }
        } else {
            encode_utf8(HANGUL_START + (uint32_t)f1_idx, g1);
            encode_utf8(HANGUL_START + (uint32_t)f2_idx, g2);
            if (to_idx == (uint64_t)END_IDX)
                fprintf(out, "%s%s$\t%ld\n", g1, g2, res[i].count);
            else {
                encode_utf8(HANGUL_START + (uint32_t)to_idx, g3);
                fprintf(out, "%s%s%s\t%ld\n", g1, g2, g3, res[i].count);
            }
        }
    }

    fclose(out);
    printf("고유 트라이그램: %d개 (시작/끝 포함) → %s\n", total, out_path);

    free(res);
    return 0;
}
