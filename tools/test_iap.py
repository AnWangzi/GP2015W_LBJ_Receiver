"""Exercise real IAP C code, UART Python client, and simulated power loss."""
import ctypes as C
from pathlib import Path
import random
import struct
from serial_update import (APP_BASE,APP_TAG,BOARD,MAX_IMAGE,BAUD,Updater,crc,
                           frame,package,unpack,validate_image,PING,BEGIN,DATA,COMMIT,RUN,ABORT)
root=Path(__file__).resolve().parent.parent
lib=C.CDLL(str(root/'keilproj/diagnostics/iap_host.dll'))
lib.Iap_FlashAt.restype=C.POINTER(C.c_ubyte)
lib.Iap_Crc32.restype=C.c_uint32
lib.Sim_Response.restype=C.POINTER(C.c_ubyte)
class Transfer(C.Structure):
    _fields_=[('length',C.c_uint32),('crc',C.c_uint32),('received',C.c_uint32),('receiving',C.c_ubyte)]
def image(seed,size=1301):
    rng=random.Random(seed)
    data=bytearray(rng.randrange(256) for _ in range(size))
    struct.pack_into('<II',data,0,0x20001000,APP_BASE+209)
    struct.pack_into('<4I',data,192,APP_TAG,BOARD,APP_BASE,MAX_IMAGE)
    return bytes(data)
old,new=image(1),image(2)
def record(data):
    fields=struct.pack('<III',0x314a424c,len(data),crc(data))
    return fields+struct.pack('<I',crc(fields))
def reset():
    lib.Sim_Init(); lib.Sim_Copy(APP_BASE,old,len(old)); lib.Sim_Copy(0x1e000,record(old),16)
    assert lib.Iap_ActiveValid()
def flash_bytes(address,size): return bytes(lib.Iap_FlashAt(address)[:size])
def command(t,cmd,payload=b''):
    response=(C.c_ubyte*32)()
    n=lib.Iap_Command(C.byref(t),cmd,payload,len(payload),response)
    return bytes(response[:n])
def stage(data):
    t=Transfer(); lib.Iap_Init(C.byref(t))
    assert command(t,BEGIN,struct.pack('<4I',len(data),crc(data),APP_BASE,BOARD))==b'\0'
    for offset in range(0,len(data),512):
        block=struct.pack('<I',offset)+data[offset:offset+512]
        assert command(t,DATA,block)==b'\0'
        assert command(t,DATA,block)==b'\0' # idempotent duplicate DATA
    return t
assert lib.Iap_Crc32(b'123456789',9)==crc(b'123456789')
reset(); t=stage(new)
assert flash_bytes(APP_BASE,len(old))==old
assert command(t,COMMIT)==b'\0' and lib.Iap_ActiveValid()
assert flash_bytes(APP_BASE,len(new))==new and lib.Iap_Recover()
print('PASS staged upload, CRC verification, activation, idempotent DATA')

# Fail at every mutation during commit: pending descriptor, every application
# word and active descriptor. Startup must recover either intact old or new.
reset(); t=stage(new); lib.Sim_FailAfter(-1); assert command(t,COMMIT)==b'\0'
count=lib.Sim_Mutations()
for cut in range(count+1):
    reset(); t=stage(new); lib.Sim_FailAfter(cut); command(t,COMMIT)
    lib.Sim_FailAfter(-1)
    assert lib.Iap_Recover(),('recovery failed',cut)
    actual=flash_bytes(APP_BASE,len(new))
    assert actual in (old,new),('mixed application',cut)
    assert lib.Iap_ActiveValid()
print('PASS power loss at all %d commit/copy/metadata mutation boundaries'%(count+1))

for cut in range(132):
    reset(); t=Transfer(); lib.Iap_Init(C.byref(t))
    lib.Sim_FailAfter(cut)
    if command(t,BEGIN,struct.pack('<4I',len(new),crc(new),APP_BASE,BOARD))==b'\0':
        command(t,DATA,struct.pack('<I',0)+new[:512])
    lib.Sim_FailAfter(-1); assert lib.Iap_Recover()
    assert flash_bytes(APP_BASE,len(old))==old
print('PASS interrupted BEGIN/staging preserves running image')

reset(); t=Transfer(); lib.Iap_Init(C.byref(t))
assert command(t,COMMIT)!=b'\0'
assert command(t,BEGIN,struct.pack('<4I',MAX_IMAGE+1,0,APP_BASE,BOARD))!=b'\0'
assert command(t,BEGIN,struct.pack('<4I',len(new),crc(new),0,BOARD))!=b'\0'
assert command(t,BEGIN,struct.pack('<4I',len(new),crc(new),APP_BASE,0))!=b'\0'
assert command(t,BEGIN,b'x')!=b'\0'
t=stage(new)
assert command(t,DATA,struct.pack('<I',0xffffffff)+b'abcd')!=b'\0'
assert command(t,DATA,struct.pack('<I',1)+b'abcd')!=b'\0'
# Mutate staging without changing expected complete-image CRC.
lib.Sim_Copy(0x10000+700,b'\x77',1)
assert command(t,COMMIT)!=b'\0'
assert flash_bytes(APP_BASE,len(old))==old
assert command(t,ABORT)==b'\0'
assert command(t,DATA,struct.pack('<I',0)+new[:512])!=b'\0'
print('PASS invalid target/length/offset, incomplete/corrupt image, abort')

for seed,size in ((3,210),(4,512),(5,MAX_IMAGE)):
    data=image(seed,size)
    assert unpack(package(data))==data
for edit in ('header','payload','truncated','basezero','tag'):
    blob=bytearray(package(new))
    if edit=='header': blob[8]^=1
    elif edit=='payload': blob[-1]^=1
    elif edit=='truncated': del blob[-1]
    else:
        bad=bytearray(new)
        if edit=='basezero': struct.pack_into('<I',bad,4,0x101)
        else: bad[192]^=1
        try: validate_image(bad)
        except ValueError: continue
        raise AssertionError(edit)
    try: unpack(blob)
    except ValueError: continue
    raise AssertionError(edit)
print('PASS package boundaries, header/payload CRC, wrong linker image rejection')

class Port:
    def __init__(self,drop=True): self.rx=bytearray(); self.now=0; self.drop=drop; self.seen=set()
    def write(self,data):
        reply=b''
        for byte in data:
            self.now+=1
            n=lib.Sim_Byte(byte,self.now)
            if n: reply=bytes(lib.Sim_Response()[:n])
        key=bytes(data)
        if self.drop and key not in self.seen: self.seen.add(key)
        else: self.rx.extend(reply)
        return len(data)
    def read(self,n):
        result=bytes(self.rx[:n]); del self.rx[:n]; return result
    def flush(self): pass
reset(); port=Port(); updater=Updater(port,timeout=0.01)
updater.flash(new)
assert flash_bytes(APP_BASE,len(new))==new
assert lib.Iap_ActiveValid()
print('PASS actual Python updater <-> C wire/core with every first ACK dropped and retried')

reset(); port=Port(False)
for i in range(10000): lib.Sim_Byte(random.Random(i).randrange(256),i)
assert flash_bytes(APP_BASE,len(old))==old
lib.Sim_WireReset()
bad=bytearray(frame(PING,9)); bad[-1]^=1
port.write(bad); assert not port.rx
port.write(frame(PING,10)[:7]); port.now+=501
port.write(frame(PING,11)); assert port.rx
print('PASS wire garbage, bad CRC, truncated-frame timeout/resynchronization')

release=root/'keilproj/iap/release'
if (release/'first-install.bin').exists():
    install=(release/'first-install.bin').read_bytes()
    assert len(install)==0x20000
    lib.Sim_Init(); lib.Sim_Copy(0,install,len(install))
    assert lib.Iap_Recover() and lib.Iap_ActiveValid()
    actual=unpack((release/'lbj-update.lbju').read_bytes())
    assert flash_bytes(APP_BASE,len(actual))==actual
    reset(); Updater(Port(),timeout=0.01).flash(actual)
    assert flash_bytes(APP_BASE,len(actual))==actual and lib.Iap_ActiveValid()
    print('PASS generated first-install metadata and actual relocated ARM application UART update')

# Exercise the interactive launcher's handshake gate against the same real C
# bootloader. No actual serial port or hardware is opened by these tests.
from serial_flash_wizard import transfer, TracedPort
reset(); traced=TracedPort(Port(False))
transfer(traced,new,True,1,lambda text: None)
assert set(traced.commands)=={'0'} and flash_bytes(APP_BASE,len(old))==old
class SilentPort(Port):
    def write(self,data): return len(data)
reset(); traced=TracedPort(SilentPort(False))
try: transfer(traced,new,False,0.02,lambda text: None)
except TimeoutError: pass
else: raise AssertionError('Silent target must not reach Flash commands')
assert set(traced.commands)=={'0'} and flash_bytes(APP_BASE,len(old))==old
class WrongBoardPort(Port):
    def write(self,data):
        seq=struct.unpack_from('<H',data,5)[0]
        self.rx.extend(frame(PING|0x80,seq,b'\0'+struct.pack('<4I',BOARD^1,APP_BASE,MAX_IMAGE,BAUD)))
        return len(data)
reset(); traced=TracedPort(WrongBoardPort(False))
try: transfer(traced,new,False,1,lambda text: None)
except RuntimeError: pass
else: raise AssertionError('Wrong board must not reach Flash commands')
assert set(traced.commands)=={'0'} and flash_bytes(APP_BASE,len(old))==old
reset(); traced=TracedPort(Port(False))
transfer(traced,new,False,1,lambda text: None)
assert flash_bytes(APP_BASE,len(new))==new and lib.Iap_ActiveValid()
assert {'0','1','2','3','4'} <= set(traced.commands)
print('PASS launcher: probe-only, silent/wrong board no Flash, complete C-simulated installation')
