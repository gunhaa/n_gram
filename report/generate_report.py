"""
바이그램(step3) vs 트라이그램(step4) 샘플링 비교 리포트 생성기
출력: report/comparison_report.md
"""

import re
from collections import Counter
from datetime import datetime

def parse_result(path):
    entries = []
    with open(path, encoding='utf-8') as f:
        for line in f:
            m = re.match(r'seq \d+: (.+) \[(O|X)\]', line.strip())
            if m:
                word, valid = m.group(1), m.group(2)
                entries.append({'word': word, 'valid': valid == 'O',
                                 'len': len(word)})  # Python str은 유니코드 문자 단위
    return entries

def stats(entries):
    total    = len(entries)
    valid    = sum(1 for e in entries if e['valid'])
    lengths  = [e['len'] for e in entries]
    avg_len  = sum(lengths) / total if total else 0
    len_dist = Counter(min(e['len'], 6) for e in entries)  # 6+ 묶음
    top_valid = Counter(e['word'] for e in entries if e['valid']).most_common(10)
    return {
        'total': total, 'valid': valid,
        'valid_rate': valid / total * 100 if total else 0,
        'avg_len': avg_len, 'len_dist': len_dist,
        'top_valid': top_valid,
    }

def bar(ratio, width=20):
    filled = round(ratio * width)
    return '█' * filled + '░' * (width - filled)

bi  = parse_result('step3/bigram_result.txt')
tri = parse_result('step4/trigram_result.txt')
bs  = stats(bi)
ts  = stats(tri)

lines = []
lines.append(f"# 바이그램 vs 트라이그램 샘플링 비교 리포트")
lines.append(f"\n생성일: {datetime.now().strftime('%Y-%m-%d %H:%M')}")
lines.append(f"샘플 수: 각 {bs['total']}개\n")

lines.append("## 1. 사전 일치율 (Validation)\n")
lines.append(f"| 모델 | 일치(O) | 불일치(X) | 일치율 |")
lines.append(f"|------|---------|----------|--------|")
lines.append(f"| 바이그램  | {bs['valid']:4d} | {bs['total']-bs['valid']:4d} | {bs['valid_rate']:.1f}% {bar(bs['valid_rate']/100)} |")
lines.append(f"| 트라이그램 | {ts['valid']:4d} | {ts['total']-ts['valid']:4d} | {ts['valid_rate']:.1f}% {bar(ts['valid_rate']/100)} |")

lines.append("\n## 2. 평균 생성 길이\n")
lines.append(f"| 모델 | 평균 글자 수 |")
lines.append(f"|------|------------|")
lines.append(f"| 바이그램  | {bs['avg_len']:.2f}자 |")
lines.append(f"| 트라이그램 | {ts['avg_len']:.2f}자 |")

lines.append("\n## 3. 글자 수 분포\n")
lines.append(f"| 길이 | 바이그램 | | 트라이그램 | |")
lines.append(f"|------|---------|---|----------|---|")
for l in range(1, 7):
    label = f"{l}자" if l < 6 else "6자+"
    bc = bs['len_dist'].get(l, 0)
    tc = ts['len_dist'].get(l, 0)
    bp = bc / bs['total']
    tp = tc / ts['total']
    lines.append(f"| {label} | {bc:4d} ({bp*100:4.1f}%) | {bar(bp,10)} | {tc:4d} ({tp*100:4.1f}%) | {bar(tp,10)} |")

lines.append("\n## 4. 사전 일치 상위 10개\n")
lines.append(f"| 순위 | 바이그램 [O] | 빈도 | 트라이그램 [O] | 빈도 |")
lines.append(f"|------|------------|------|-------------|------|")
max_len = max(len(bs['top_valid']), len(ts['top_valid']))
for i in range(max_len):
    bw, bc = bs['top_valid'][i] if i < len(bs['top_valid']) else ('—', 0)
    tw, tc = ts['top_valid'][i] if i < len(ts['top_valid']) else ('—', 0)
    lines.append(f"| {i+1:2d} | {bw} | {bc} | {tw} | {tc} |")

lines.append("\n## 5. 분석 요약\n")
rate_diff = ts['valid_rate'] - bs['valid_rate']
len_diff  = ts['avg_len'] - bs['avg_len']
lines.append(f"- **사전 일치율**: 트라이그램이 바이그램보다 "
             f"{'높음' if rate_diff > 0 else '낮음'} "
             f"({rate_diff:+.1f}%p)")
lines.append(f"- **평균 길이**: 트라이그램이 바이그램보다 "
             f"{'길다' if len_diff > 0 else '짧다'} "
             f"({len_diff:+.2f}자)")
lines.append(f"- 트라이그램은 2글자 앞 맥락을 사용하므로 "
             f"실제 사전 단어 패턴에 더 '구속'되어 "
             f"{'일치율이 높지만 다양성이 낮을 수 있다' if rate_diff > 0 else '예상과 달리 일치율이 낮을 수 있다'}.")

report = '\n'.join(lines) + '\n'
with open('report/comparison_report.md', 'w', encoding='utf-8') as f:
    f.write(report)

print("saved: report/comparison_report.md")
print(f"바이그램  일치율: {bs['valid_rate']:.1f}%  평균길이: {bs['avg_len']:.2f}자")
print(f"트라이그램 일치율: {ts['valid_rate']:.1f}%  평균길이: {ts['avg_len']:.2f}자")
