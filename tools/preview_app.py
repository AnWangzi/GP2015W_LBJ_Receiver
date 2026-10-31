"""Render the actual firmware view functions; sample data is synthetic."""
import ctypes as C
from pathlib import Path
from PIL import Image, ImageDraw
from test_receiver import Message
root=Path(__file__).resolve().parent.parent
lib=C.CDLL(str(root/'keilproj/diagnostics/lcd_host.dll'))
lib.LCD_HostSnapshot.restype=C.POINTER(C.c_uint8)
class Ui(C.Structure):
    _fields_=[('screen',C.c_int),('parent',C.c_int),('selected',C.c_uint8),
              ('light_mode',C.c_uint8),('sound_enabled',C.c_uint8),
              ('diagnostic_page',C.c_uint8),('history_index',C.c_uint8),('history_count',C.c_uint8)]
def snapshot():
    data=lib.LCD_HostSnapshot()
    out=Image.new('RGB',(128,64),(187,208,172))
    for y in range(64):
        for x in range(128):
            if (data[y//8*128+x]>>(y%8))&1: out.putpixel((x,y),(28,55,43))
    return out
samples=[]
msg=Message(); msg.train=b'K8515'; msg.trainNumber=b'8515'; msg.prefix=b'K'
msg.speed=b'013'; msg.position=b'0983.4'
msg.loco=b'HXD3C-0093'
lib.LCD_HostReset(); lib.AppView_Train(C.byref(msg),0)
lib.AppView_Battery(1,4,0,0,0)
samples.append(('RECEIVER - SYNTHETIC DATA',snapshot()))
msg.loco='\u4e1c\u98ce4D-0271'.encode()
lib.AppView_Train(C.byref(msg),0); lib.AppView_Battery(1,4,0,0,0)
samples.append(('CHINESE MODEL - SYNTHETIC',snapshot()))
msg.train=b'00123'; msg.trainNumber=b'00123'; msg.prefix=b''
lib.AppView_Train(C.byref(msg),0); lib.AppView_Battery(1,4,0,0,0)
samples.append(('ZERO-PREFIX RETURN SERVICE',snapshot()))
msg.train=b'11588'; msg.trainNumber=b'11588'
lib.AppView_Train(C.byref(msg),0); lib.AppView_Battery(1,4,0,0,0)
samples.append(('NUMERIC SERVICE CATEGORY',snapshot()))
for selected in (0,2):
    lib.LCD_HostReset()
    ui=Ui(3,0,selected,1,1)
    lib.AppView_Menu(C.byref(ui)); lib.AppView_Battery(1,4,0,0,0)
    first=bytes(lib.LCD_HostSnapshot()[:1024])
    lib.AppView_Menu(C.byref(ui)); lib.AppView_Battery(1,4,0,0,0)
    assert first==bytes(lib.LCD_HostSnapshot()[:1024]),'highlight toggles on redraw'
    samples.append(('SETTINGS - SCROLL / HIGHLIGHT',snapshot()))
lib.AppView_Waiting(1,0); lib.AppView_Battery(1,4,0,0,0)
samples.append(('CLEAN WAITING PAGE',snapshot()))
msg.train=b'G3483'; msg.trainNumber=b'3483'; msg.prefix=b'G'; msg.loco=b'CRH380BG-57000'
lib.AppView_History(C.byref(msg),2,100); lib.AppView_Battery(1,4,0,0,0)
samples.append(('HISTORY - SYNTHETIC 003/100',snapshot()))
msg.train=b'70001'; msg.trainNumber=b'70001'; msg.prefix=b''
for age in (0,8000):
    lib.AppView_Train(C.byref(msg),age); lib.AppView_Battery(1,4,0,0,0)
    samples.append(('LONG CATEGORY - %d ms'%age,snapshot()))
out=Image.new('RGB',(552,12+((len(samples)+1)//2)*164),'#18221e'); draw=ImageDraw.Draw(out)
for i,(caption,img) in enumerate(samples):
    x=12+(i%2)*276; y=12+(i//2)*164
    draw.text((x,y),caption,fill='#d8e2da')
    out.paste(img.resize((256,128),Image.Resampling.NEAREST),(x,y+20))
path=root/'keilproj/diagnostics/app_preview.png'; out.save(path)
print('PASS C-rendered view redraw stability; preview:',path)
