"""Compare actual C glyph renderer to original Unifont pixels, all glyphs."""
import ctypes as C
from pathlib import Path
root=Path(__file__).resolve().parent.parent
lib=C.CDLL(str(root/'keilproj/diagnostics/lcd_host.dll'))
lib.LCD_HostSnapshot.restype=C.POINTER(C.c_uint8)
count=C.c_uint16.in_dll(lib,'g_font_count').value
cps=(C.c_uint16*count).in_dll(lib,'g_font_cp')
source={int(cp,16):bytes.fromhex(bits) for cp,bits in
        (line.strip().split(':') for line in (root/'assets/fonts/unifont-subset.hex').read_text().splitlines() if ':' in line)}
for cp in cps:
    raw=source[cp]
    if len(raw)==16: raw=bytes(v for b in raw for v in (b,0))
    lib.LCD_HostReset()
    lib.LCD_DrawGlyph16(2,40,cp)
    screen=lib.LCD_HostSnapshot()
    for y in range(16):
        for x in range(16):
            actual=(screen[(2+y//8)*128+40+x]>>(y%8))&1
            expected=(raw[y*2+x//8]>>(7-x%8))&1
            assert actual==expected, (cp,x,y)
    if cp<128:
        lib.LCD_HostReset(); lib.LCD_DrawGlyph8(4,88,cp)
        screen=lib.LCD_HostSnapshot()
        for y in range(16):
            for x in range(8):
                assert ((screen[(4+y//8)*128+88+x]>>(y%8))&1)==((raw[y*2]>>(7-x))&1)
print('PASS font columns: all %d C-rendered glyphs match Unifont pixels; ASCII half width preserved'%count)
lib.LCD_HostReset()
text='\u4e1c\u98ce4D-0271'
lib.LCD_DrawUtf8(2,0,text.encode('utf-8'))
screen=bytes(lib.LCD_HostSnapshot()[:1024])
x=0
for char in text:
    raw=source[ord(char)]
    width=8 if ord(char)<128 else 16
    for y in range(16):
        for col in range(width):
            stride=1 if len(raw)==16 else 2
            expected=(raw[y*stride+col//8]>>(7-col%8))&1
            assert ((screen[(2+y//8)*128+x+col]>>(y%8))&1)==expected
    x+=width
print('PASS UTF-8 mixed Chinese/ASCII model rendering matches Unifont')
