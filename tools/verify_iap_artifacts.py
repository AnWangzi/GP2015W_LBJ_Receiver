"""Validate final linked ARM vector tables, installer HEX and image metadata."""
from pathlib import Path
import struct
from serial_update import validate_image,unpack,APP_BASE,crc
root=Path(__file__).resolve().parent.parent
folder=root/'keilproj/iap'
boot=(folder/'ObjectsBoot/lbj_boot.bin').read_bytes()
app=(folder/'ObjectsApp/lbj_app.bin').read_bytes()
install=(folder/'release/first-install.bin').read_bytes()
vectors=struct.unpack_from('<48I',boot)
assert 0<len(boot)<=0x2000
assert 0x20000000<vectors[0]<=0x20003ff0 and not vectors[0]&7
assert vectors[1]&1 and 192<=vectors[1]&~1<len(boot)
forward=vectors[2]
assert forward&1 and forward&~1<len(boot)
for i,value in enumerate(vectors[2:],2):
    if value: assert value==forward,(i,hex(value),hex(forward))
assert vectors[15]==vectors[17]==forward,'SysTick / PORTB must forward into APP'
validate_image(app)
av=struct.unpack_from('<48I',app)
for i in (3,15,17): assert av[i]&1 and APP_BASE<=av[i]&~1<APP_BASE+len(app)
assert len(install)==0x20000
assert install[:len(boot)]==boot and install[APP_BASE:APP_BASE+len(app)]==app
assert install[0x1e200:0x1e400]==b'\xff'*512,'Pending descriptor must be blank on first install'
magic,size,checksum,rcrc=struct.unpack_from('<4I',install,0x1e000)
assert (magic,size,checksum)==(0x314a424c,len(app),crc(app))
assert rcrc==crc(install[0x1e000:0x1e00c])
decoded=bytearray(b'\xff'*0x20000); seen=set(); upper=0; eof=False
for line in (folder/'release/first-install.hex').read_text().splitlines():
    assert not eof and line.startswith(':')
    raw=bytes.fromhex(line[1:]); assert sum(raw)%256==0 and raw[0]+5==len(raw)
    count,high,low,kind=raw[:4]; data=raw[4:-1]
    if kind==4: upper=int.from_bytes(data,'big')<<16
    elif kind==0:
        address=upper+(high<<8)+low
        assert address+count<=len(decoded)
        for offset in range(count): assert address+offset not in seen; seen.add(address+offset)
        decoded[address:address+count]=data
    elif kind==1: eof=True
    else: raise AssertionError(kind)
assert eof and len(seen)==0x20000 and decoded==install
assert unpack((folder/'release/lbj-update.lbju').read_bytes())==app
print('PASS linked ARM boot/app vectors, exception forwarding, mailbox bounds, first-install BIN/HEX/metadata and UART package')
