#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""重写 keilproj/1.uvprojx 的 <Groups> 块，保持文件列表规范化

反斜杠统一用 BS = chr(92)，避免被 Python / re.sub 当转义处理。
"""
import os
import re

BS = chr(92)
PROJ = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'keilproj')
UV = os.path.join(PROJ, '1.uvprojx')

GROUPS = [
    ('src', [
        ('main.c', 1, '.', 'src', 'main.c'),
        ('lcd_8080.c', 1, '.', 'src', 'lcd_8080.c'),
        ('lcd_buffer.c', 1, '.', 'src', 'lcd_buffer.c'),
        ('lcd_text.c', 1, '.', 'src', 'lcd_text.c'),
        ('font_data.c', 1, '.', 'src', 'font_data.c'),
        ('si446x.c', 1, '.', 'src', 'si446x.c'),
        ('radio_config.c', 1, '.', 'src', 'radio_config.c'),
        ('radio_task.c', 1, '.', 'src', 'radio_task.c'),
        ('pocsag.c', 1, '.', 'src', 'pocsag.c'),
        ('lbj_parse.c', 1, '.', 'src', 'lbj_parse.c'),
        ('lbj_state.c', 1, '.', 'src', 'lbj_state.c'),
        ('train_class.c', 1, '.', 'src', 'train_class.c'),
        ('history.c', 1, '.', 'src', 'history.c'),
        ('power_sleep.c', 1, '.', 'src', 'power_sleep.c'),
        ('buttons.c', 1, '.', 'src', 'buttons.c'),
        ('board_io.c', 1, '.', 'src', 'board_io.c'),
        ('app_ui.c', 1, '.', 'src', 'app_ui.c'),
        ('app_view.c', 1, '.', 'src', 'app_view.c'),
        ('iap_entry.c', 1, '.', 'src', 'iap_entry.c'),
        ('battery.c', 1, '.', 'src', 'battery.c'),
        ('battery_model.c', 1, '.', 'src', 'battery_model.c'),
        ('battery_icon.c', 1, '.', 'src', 'battery_icon.c'),
    ]),
    ('device', [
        ('startup_hc32l17x.s', 2, '.', 'common', 'device', 'startup_hc32l17x.s'),
        ('system_hc32l17x.c', 1, '.', 'common', 'device', 'system_hc32l17x.c'),
    ]),
    ('ddl', [
        ('ddl.c', 1, '.', 'common', 'ddl', 'src', 'ddl.c'),
        ('gpio.c', 1, '.', 'common', 'ddl', 'src', 'gpio.c'),
        ('sysctrl.c', 1, '.', 'common', 'ddl', 'src', 'sysctrl.c'),
    ]),
]


def build_groups():
    out = ['      <Groups>']
    for name, files in GROUPS:
        out.append('        <Group>')
        out.append('          <GroupName>%s</GroupName>' % name)
        out.append('          <Files>')
        for fname, ftype, *parts in files:
            path = BS.join(parts)
            out.append('            <File>')
            out.append('              <FileName>%s</FileName>' % fname)
            out.append('              <FileType>%d</FileType>' % ftype)
            out.append('              <FilePath>%s</FilePath>' % path)
            out.append('            </File>')
        out.append('          </Files>')
        out.append('        </Group>')
    out.append('      </Groups>')
    return '\n'.join(out)


src = open(UV, encoding='utf-8').read()
new = build_groups()
src, n = re.subn(r'      <Groups>.*?      </Groups>', lambda m: new, src, flags=re.S)
assert n == 1, 'Groups 替换失败 n=%d' % n

open(UV, 'w', encoding='utf-8', newline='').write(src)

# 自检
chk = open(UV, encoding='utf-8').read()
for name, files in GROUPS:
    for fname, ftype, *parts in files:
        path = BS.join(parts)
        assert '<FilePath>%s</FilePath>' % path in chk, '缺少 %s' % path
print('工程文件列表已更新，共 %d 个源文件' % sum(len(f) for _, f in GROUPS))
