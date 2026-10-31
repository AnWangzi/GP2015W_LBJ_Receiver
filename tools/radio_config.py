"""Inspect the checked-in board RX configuration without a device backup."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent


def freq_bytes(hz, xo=26000000, outdiv=4):
    ratio = hz * outdiv / (2.0 * xo)
    inte = int(ratio) - 1
    frac = int(round((ratio - inte) * (1 << 19)))
    if frac >= (1 << 20):
        inte += 1
        frac -= 1 << 19
    assert 0 <= inte < 128 and (1 << 19) <= frac < (1 << 20)
    actual = (inte + frac / float(1 << 19)) * 2 * xo / outdiv
    return inte, frac >> 16, (frac >> 8) & 255, frac & 255, abs(actual - hz)


def read_config():
    source = (ROOT / 'keilproj/src/radio_config.c').read_text(encoding='utf-8')
    body = source.split('g_radio_cfg[', 1)[1].split('{', 1)[1].split('}', 1)[0]
    return bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{2})', body))
