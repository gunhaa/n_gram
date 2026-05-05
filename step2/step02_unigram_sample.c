#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_CHARS  3000
#define SET_COUNT  10   /* 세트 수 */
#define SET_SIZE   10   /* 세트당 글자 수 */

typedef struct {
    char glyph[4];  /* UTF-8 한글 글자 (3바이트 + null) */
    long freq;
    double prob;    /* 누적 확률 */
} CharEntry;

/*
 * step1 결과 파일(글자\t빈도)을 읽어 CharEntry 배열에 저장한다.
 * 반환값: 실제로 읽은 항목 수(n)
 *
 * MAX_CHARS(3000)는 배열의 최대 수용 크기일 뿐이다.
 * 확률 계산은 실제 읽은 n개의 항목만으로 이루어진다:
 *   - total = n개 항목의 빈도 합산
 *   - 각 항목의 누적 확률 = 자신까지의 빈도 합 / total
 * 따라서 현재 데이터가 2,467개든 2,800개든 n이 MAX_CHARS 이하이면
 * 항상 올바른 확률 분포(누적 확률 최종값 ≈ 1.0)가 만들어진다.
 */
static int load_freq(const char *path, CharEntry *entries) {
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "cannot open: %s\n", path); return -1; }

    long total = 0;  /* 전체 빈도 합 — 나중에 각 항목의 상대적 비율(확률)을 구하는 분모 */
    int n = 0;
    char line[64];

    while (fgets(line, sizeof(line), fp) && n < MAX_CHARS) {
        char *tab = strchr(line, '\t');  /* 탭을 기준으로 글자/빈도 분리 */
        if (!tab) continue;
        *tab = '\0';                             /* 탭을 null로 치환해 glyph 문자열 종료 */
        strncpy(entries[n].glyph, line, 3);      /* 한글 1글자 = UTF-8 3바이트 복사 */
        entries[n].glyph[3] = '\0';
        entries[n].freq = atol(tab + 1);         /* 탭 다음 숫자를 빈도로 파싱 */
        total += entries[n].freq;                /* 분모(total)에 누적 */
        n++;
    }
    fclose(fp);

    /*
     * 빈도를 누적 확률(CDF)로 변환한다.
     * 예) 다(90519), 하(58848), 기(28632) ... total=1,400,000 이라면
     *   entries[0].prob = 90519 / 1400000 ≈ 0.0647
     *   entries[1].prob = (90519+58848) / 1400000 ≈ 0.1068
     *   entries[2].prob = (90519+58848+28632) / 1400000 ≈ 0.1272
     *   ...
     *   entries[n-1].prob ≈ 1.0
     * 이렇게 만든 CDF 배열에 [0,1) 난수를 이진 탐색하면
     * 빈도에 비례한 글자를 O(log n)으로 샘플링할 수 있다.
     */
    double cum = 0.0;
    for (int i = 0; i < n; i++) {
        cum += (double)entries[i].freq / (double)total;
        entries[i].prob = cum;
    }

    return n;
}

/*
 * [0, 1) 균등 난수를 이진 탐색으로 누적 확률 배열과 대조해
 * 하나의 글자를 샘플링한다.
 */
static const char *sample_one(const CharEntry *entries, int n) {
    double r = (double)rand() / ((double)RAND_MAX + 1.0);

    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (entries[mid].prob < r)
            lo = mid + 1;
        else
            hi = mid;
    }
    return entries[lo].glyph;
}

/*
 * 유니그램 확률 분포로 SET_SIZE개의 글자를 샘플링해 out 스트림에 한 줄로 쓴다.
 */
static void write_set(FILE *out, const CharEntry *entries, int n, int set_no) {
    fprintf(out, "set %02d: ", set_no);
    for (int i = 0; i < SET_SIZE; i++)
        fprintf(out, "%s", sample_one(entries, n));
    fprintf(out, "\n");
}

int main(int argc, char *argv[]) {
    const char *freq_path = argc > 1 ? argv[1] : "step1/charfreq_result.txt";
    const char *out_path  = argc > 2 ? argv[2] : "step2/unigram_sample_result.txt";

    CharEntry *entries = malloc(MAX_CHARS * sizeof(CharEntry));
    int n = load_freq(freq_path, entries);
    if (n <= 0) { free(entries); return 1; }

    srand((unsigned)time(NULL));

    FILE *out = fopen(out_path, "w");
    if (!out) { fprintf(stderr, "cannot open: %s\n", out_path); free(entries); return 1; }

    for (int s = 1; s <= SET_COUNT; s++)
        write_set(out, entries, n, s);

    fclose(out);
    printf("saved: %s\n", out_path);

    free(entries);
    return 0;
}
