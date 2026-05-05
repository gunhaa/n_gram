#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define HANGUL_START 0xAC00  /* 가 */
#define HANGUL_END   0xD7A3  /* 힣 */
#define HANGUL_COUNT (HANGUL_END - HANGUL_START + 1)  /* 11172개 */

typedef struct {
    uint32_t codepoint;
    long count;
} CharFreq;

/* 빈도 내림차순, 같으면 코드포인트 오름차순으로 정렬 */
static int cmp_freq(const void *a, const void *b) {
    const CharFreq *fa = (const CharFreq *)a;
    const CharFreq *fb = (const CharFreq *)b;
    if (fb->count != fa->count)
        return (fb->count > fa->count) - (fb->count < fa->count);
    return (int)(fa->codepoint - fb->codepoint);
}

/*
 * UTF-8 바이트 시퀀스에서 유니코드 코드포인트 1개를 읽는다.
 * s    : 읽기 시작할 바이트 포인터
 * bytes: 읽은 바이트 수를 저장 (1~4)
 * 반환값: 유니코드 코드포인트
 */
static uint32_t read_utf8(const unsigned char *s, int *bytes) {
    if ((s[0] & 0x80) == 0x00) { *bytes = 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0) { *bytes = 2; return ((s[0] & 0x1F) << 6)  | (s[1] & 0x3F); }
    if ((s[0] & 0xF0) == 0xE0) { *bytes = 3; return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
                                  *bytes = 4; return ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
}

/*
 * 유니코드 코드포인트를 3바이트 UTF-8로 인코딩한다.
 * 한글 완성형(U+AC00~U+D7A3)은 항상 3바이트이므로 4번째 바이트는 null terminator.
 */
static void encode_utf8(uint32_t cp, unsigned char out[4]) {
    out[0] = (unsigned char)(0xE0 | (cp >> 12));
    out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (unsigned char)(0x80 | (cp & 0x3F));
    out[3] = 0;
}

int main(int argc, char *argv[]) {
    const char *path = argc > 1 ? argv[1] : "data/korean-dict/kr_korean_simple.csv";

    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "cannot open: %s\n", path); return 1; }

    /* 완성형 한글 11172자 각각의 출현 빈도를 집계 */
    long freq[HANGUL_COUNT] = {0};
    char line[4096];

    while (fgets(line, sizeof(line), fp)) {
        const unsigned char *p = (unsigned char *)line;
        while (*p && *p != '\n') {
            int n;
            uint32_t cp = read_utf8(p, &n);
            if (cp >= HANGUL_START && cp <= HANGUL_END)
                freq[cp - HANGUL_START]++;
            p += n;
        }
    }
    fclose(fp);

    /* 빈도가 1 이상인 글자만 결과 배열에 수집 */
    int total = 0;
    for (int i = 0; i < HANGUL_COUNT; i++)
        if (freq[i] > 0) total++;

    CharFreq *res = malloc(total * sizeof(CharFreq));
    int idx = 0;
    for (int i = 0; i < HANGUL_COUNT; i++) {
        if (freq[i] > 0) {
            res[idx].codepoint = (uint32_t)(HANGUL_START + i);
            res[idx].count     = freq[i];
            idx++;
        }
    }

    qsort(res, total, sizeof(CharFreq), cmp_freq);

    /* 글자<TAB>빈도 형식으로 출력 */
    for (int i = 0; i < total; i++) {
        unsigned char utf8[4];
        encode_utf8(res[i].codepoint, utf8);
        printf("%s\t%ld\n", utf8, res[i].count);
    }

    free(res);
    return 0;
}
