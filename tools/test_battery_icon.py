"""Verify and preview the exact firmware icon using its host-compiled C code."""
import ctypes as C
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parent.parent
lib = C.CDLL(str(root / 'keilproj/diagnostics/battery_icon.dll'))
column = lib.BatteryIcon_Column
column.argtypes = [C.c_uint, C.c_int, C.c_uint, C.c_int, C.c_int, C.c_int]
column.restype = C.c_uint16


def icon(valid, level, external=0, charging=0, phase=1):
    return [column(x, valid, level, external, charging, phase) for x in range(32)]


empty = icon(1, 0)
base = sum(bin(v).count('1') for v in empty)
for level in range(6):
    actual = icon(1, level)
    assert sum(bin(v).count('1') for v in actual) == base + level * 12
    assert all((a & b) == b for a, b in zip(actual, empty))
    assert actual[8:] == icon(1, level, 1, 1, 0)[8:] == icon(1, level, 1, 1, 1)[8:]
assert icon(0, 0) != empty
assert any(icon(1, 3, 1, 1, 1)[:6])
assert not any(icon(1, 3, 1, 1, 0)[:6])
assert icon(1, 3, 1, 0)[:6] != icon(1, 3, 1, 1)[:6]
assert column(32, 1, 5, 1, 1, 1) == 0
print('PASS icon: five distinct levels, fixed true bars while charging, unknown/empty, plug/lightning, bounds')

rows = [('EMPTY', 1, 0, 0, 0), ('1/5', 1, 1, 0, 0), ('2/5', 1, 2, 0, 0),
        ('3/5', 1, 3, 0, 0), ('4/5', 1, 4, 0, 0), ('FULL', 1, 5, 0, 0),
        ('CHARGING', 1, 3, 1, 1), ('EXTERNAL', 1, 5, 1, 0), ('UNKNOWN', 0, 0, 0, 0)]
canvas = Image.new('RGB', (460, len(rows) * 80 + 24), '#ecf0e5')
draw = ImageDraw.Draw(canvas)
for row, (label, valid, level, ext, charge) in enumerate(rows):
    tile = Image.new('RGB', (32, 16), '#bdc7a8')
    for x, bits in enumerate(icon(valid, level, ext, charge)):
        for y in range(16):
            if bits & (1 << y):
                tile.putpixel((x, y), (26, 39, 28))
    canvas.paste(tile.resize((128, 64), Image.NEAREST), (290, row * 80 + 16))
    draw.text((28, row * 80 + 40), label, fill='#1a271c')
canvas.save(root / 'keilproj/diagnostics/battery_icon_preview.png')
