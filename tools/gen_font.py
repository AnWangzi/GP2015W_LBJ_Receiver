#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 GNU Unifont 抽取字形，生成固件用的 C 字库

Unifont 16x16：每个字形 32 字节，16 行 × 2 字节，MSB 在左。
"""
import os
import sys
import re
from gen_ui_text import UI_TEXT, TRAIN_LABELS

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEX = os.path.join(ROOT, 'assets', 'fonts', 'unifont-subset.hex')
OUT = os.path.join(ROOT, 'keilproj', 'src', 'font_data.c')

# 演示用字符（含 LBJ 常见字段）
TEXT = '列车接近预警上 下行韶山东风和谐型号线路公里速度时间设置注意安全京广'
TEXT += ''.join(UI_TEXT.values())
TEXT += ''.join(TRAIN_LABELS)
# Include every model name the actual parser can emit, including rare models.
parser = open(os.path.join(ROOT, 'keilproj', 'src', 'lbj_parse.c'), encoding='utf-8').read()
table = parser.split('LOCO_TABLE[] = {', 1)[1].split('};', 1)[0]
for literal in re.findall(r'"([^"\n]*)"', table):
    TEXT += literal.encode('ascii').decode('unicode_escape').encode('latin1').decode('utf-8')
TEXT += '未知型号'

codepoints = set(ord(c) for c in TEXT if c.strip())
codepoints |= set(range(0x20, 0x7F))          # ASCII 可见字符
codepoints = sorted(codepoints)

# 读 unifont
glyphs = {}
with open(HEX, 'r', encoding='utf-8', errors='ignore') as f:
    for line in f:
        line = line.strip()
        if not line or ':' not in line:
            continue
        cp_s, bits = line.split(':', 1)
        try:
            cp = int(cp_s, 16)
        except ValueError:
            continue
        if cp in codepoints and cp not in glyphs:
            glyphs[cp] = bits

missing = [cp for cp in codepoints if cp not in glyphs]
if missing:
    raise SystemExit('请先补充 assets/fonts/unifont-subset.hex，缺少字形: %s' %
                     ', '.join('U+%04X' % c for c in missing))

# 统一成 32 字节（16x16）
rows = []
for cp in codepoints:
    bits = glyphs[cp]
    raw = bytes.fromhex(bits)
    if len(raw) == 32:
        data = raw
    elif len(raw) == 16:          # 8x16 半角，右边补空白
        data = bytearray()
        for i in range(16):
            data += bytes([raw[i], 0])
        data = bytes(data)
    else:
        print('⚠ U+%04X 长度 %d，跳过' % (cp, len(raw)))
        continue
    # Store ready-to-copy LCD columns: page0 columns0..15, then page1.
    columns = bytes(sum(((data[(half * 8 + r) * 2 + c // 8] >>
                         (7 - c % 8)) & 1) << r for r in range(8))
                    for half in range(2) for c in range(16))
    rows.append((cp, columns))

print('收录字形: %d 个（其中汉字 %d）'
      % (len(rows), sum(1 for cp, _ in rows if cp > 0x7F)))
print('数据大小: %d 字节' % (len(rows) * 32))

lines = []
lines.append('/* 自动生成，勿手改 —— tools/gen_font.py')
lines.append(' * 来源: GNU Unifont 16.0.01 (GPLv2+ with font embedding exception)')
lines.append(' * 格式: 每字形32字节，两个8像素高LCD页，每页16列，bit0在上')
lines.append(' */')
lines.append('')
lines.append('#include <stdint.h>')
lines.append('')
lines.append('const uint16_t g_font_cp[%d] = {' % len(rows))
chunk = []
for cp, _ in rows:
    chunk.append('0x%04X' % cp)
for i in range(0, len(chunk), 12):
    lines.append('    ' + ', '.join(chunk[i:i + 12]) + ',')
lines.append('};')
lines.append('')
lines.append('const uint8_t g_font_columns[%d][32] = {' % len(rows))
for cp, data in rows:
    lines.append('    { ' + ', '.join('0x%02X' % b for b in data) + ' },  /* U+%04X */' % cp)
lines.append('};')
lines.append('')
lines.append('const uint16_t g_font_count = %d;' % len(rows))
lines.append('')

open(OUT, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
print('已生成:', OUT)
print('文件大小: %d 字节' % os.path.getsize(OUT))
