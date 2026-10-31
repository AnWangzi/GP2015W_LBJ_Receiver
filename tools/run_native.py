"""Build and run firmware regressions without Keil, backups or hardware."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'keilproj/diagnostics'


def run(args):
    subprocess.run([str(x) for x in args], cwd=str(ROOT), check=True,
                   env=dict(os.environ, PYTHONIOENCODING='utf-8'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC', 'gcc'))
    parser.add_argument('--visual', action='store_true')
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if not compiler:
        parser.error('GCC not found; add it to PATH or pass --cc')
    OUT.mkdir(parents=True, exist_ok=True)
    src = 'keilproj/src/'
    cases = [
        ('buttons', ['buttons.c']),
        ('lbj_state', ['lbj_state.c', 'lbj_parse.c']),
        ('lbj_interleave', ['lbj_state.c', 'lbj_parse.c', 'history.c']),
        ('history', ['history.c', 'lbj_parse.c']),
        ('train_class', ['train_class.c', 'lbj_state.c', 'lbj_parse.c']),
        ('lcd_buffer', ['lcd_buffer.c']),
        ('radio_task', ['radio_task.c', 'radio_config.c']),
        ('ui_power_battery', ['app_ui.c', 'buttons.c', 'battery_model.c']),
        ('battery_driver', ['battery.c', 'battery_model.c']),
        ('board_light', ['board_io.c', 'buttons.c']),
        ('power_sleep', ['power_sleep.c', 'buttons.c']),
    ]
    common = [compiler, '-std=c99', '-Wall', '-Wextra', '-Werror', '-O2',
              '-Itools/mocks', '-Ikeilproj/src', '-Ikeilproj/iap']
    for name, files in cases:
        exe = OUT / (name + '_test.exe')
        run(common + ['tools/test_' + name + '.c'] + [src + f for f in files] + ['-o', exe])
        run([exe])
        if name == 'ui_power_battery':
            run(common + ['-DLBJ_IAP_APP', 'tools/test_' + name + '.c'] +
                [src + f for f in files] + ['-o', OUT / 'ui_iap_test.exe'])
            run([OUT / 'ui_iap_test.exe'])
    shared = {
        'lbj_test.dll': [src + 'pocsag.c', src + 'lbj_parse.c'],
        'iap_host.dll': ['tools/iap_host.c', 'keilproj/iap/iap_core.c', 'keilproj/iap/iap_wire.c'],
        'lcd_host.dll': ['tools/lcd_host.c'] + [src + f for f in
            ['lcd_buffer.c', 'lcd_text.c', 'font_data.c', 'app_view.c', 'train_class.c', 'battery_icon.c']],
        'battery_icon.dll': [src + 'battery_icon.c'],
    }
    for name, files in shared.items():
        flags = [] if os.name == 'nt' else ['-fPIC']
        run(common + flags + ['-shared'] + files + ['-o', OUT / name])
    for name in ['test_receiver.py', 'test_iap.py', 'test_font_columns.py', 'test_train_view.py']:
        run([sys.executable, 'tools/' + name])
    if args.visual:
        for name in ['test_battery_icon.py', 'preview_app.py']:
            run([sys.executable, 'tools/' + name])
    print('PASS all native regressions; no hardware was accessed')


if __name__ == '__main__':
    main()
