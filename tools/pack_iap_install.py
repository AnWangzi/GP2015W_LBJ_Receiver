"""Export an explicit first-install image; never flashes hardware."""
from pathlib import Path
import struct
from serial_update import crc,package,validate_image,APP_BASE,MAX_IMAGE
root=Path(__file__).resolve().parent.parent
folder=root/'keilproj/iap'
boot=(folder/'ObjectsBoot/lbj_boot.bin').read_bytes()
app=(folder/'ObjectsApp/lbj_app.bin').read_bytes()
assert len(boot)<=APP_BASE
validate_image(app)
output=folder/'release'; output.mkdir(exist_ok=True)
image=bytearray(b'\xff'*0x20000)
image[:len(boot)]=boot; image[APP_BASE:APP_BASE+len(app)]=app
meta=struct.pack('<III',0x314a424c,len(app),crc(app))
image[0x1e000:0x1e010]=meta+struct.pack('<I',crc(meta))
(output/'first-install.bin').write_bytes(image)
(output/'lbj-update.lbju').write_bytes(package(app))
def record(address,kind,data):
    raw=bytes((len(data),address>>8,address&255,kind))+data
    return ':'+(raw+bytes((-sum(raw)&255,))).hex().upper()
lines=[]
for upper in range(2):
    lines.append(record(0,4,struct.pack('>H',upper)))
    for low in range(0,65536,32):
        start=upper*65536+low
        lines.append(record(low,0,image[start:start+32]))
lines.append(record(0,1,b''))
(output/'first-install.hex').write_text('\n'.join(lines)+'\n',encoding='ascii')
print('UNFLASHED first-install:',output/'first-install.hex')
print('UART update package:',output/'lbj-update.lbju')
print('boot=%d app=%d/%d bytes'%(len(boot),len(app),MAX_IMAGE))
