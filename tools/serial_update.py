#!/usr/bin/env python3
"""LBJ UART IAP pack/probe/update tool. Python 3.7+, optional pyserial for hardware."""
import argparse
from pathlib import Path
import struct
import time
import zlib

MAGIC=b'LBJ1'
BOARD=0x36373148
APP_BASE=0x2000
MAX_IMAGE=0xE000
APP_TAG=0x31504149
BAUD=38400
HEADER=struct.Struct('<4s7I')
PING,BEGIN,DATA,COMMIT,RUN,ABORT=range(6)
ERRORS=('OK','bad command','bad length','bad image','bad offset','flash error','bad state')

def crc(data): return zlib.crc32(data)&0xffffffff

def validate_image(image):
    if not 210<=len(image)<=MAX_IMAGE: raise ValueError('Image must be 210..57344 bytes')
    stack,reset=struct.unpack_from('<II',image)
    if stack&7 or not 0x20000000<stack<=0x20003ff0:
        raise ValueError('Invalid stack pointer / reserved IAP mailbox overlap')
    if not reset&1 or not APP_BASE+208<=reset&~1<APP_BASE+len(image):
        raise ValueError('Wrong application address: build keilproj/iap/app.uvprojx')
    if struct.unpack_from('<4I',image,192)!=(APP_TAG,BOARD,APP_BASE,MAX_IMAGE):
        raise ValueError('Missing relocated LBJ IAP image tag; ordinary SWD BIN is not an update')

def package(image):
    validate_image(image)
    header=HEADER.pack(b'LBJU',1,BOARD,APP_BASE,len(image),crc(image),0,0)
    return header[:28]+struct.pack('<I',crc(header[:28]))+image

def unpack(blob):
    if len(blob)<HEADER.size: raise ValueError('Truncated update package')
    magic,version,board,base,size,checksum,flags,hcrc=HEADER.unpack_from(blob)
    if (magic,version,board,base,flags)!=(b'LBJU',1,BOARD,APP_BASE,0) or crc(blob[:28])!=hcrc:
        raise ValueError('Wrong update format, board, base address, flags or header CRC')
    image=blob[HEADER.size:]
    if len(image)!=size or crc(image)!=checksum: raise ValueError('Truncated/corrupt update image')
    validate_image(image)
    return image

def frame(command,sequence,data=b''):
    body=struct.pack('<BHH',command,sequence,len(data))+data
    return MAGIC+body+struct.pack('<I',crc(body))

class Updater:
    def __init__(self,port,timeout=10,retries=3):
        self.port=port; self.sequence=0; self.timeout=timeout; self.retries=retries
        self.buffer=bytearray()

    def _reply(self,command,sequence):
        until=time.monotonic()+self.timeout
        while time.monotonic()<until:
            self.buffer.extend(self.port.read(1))
            start=self.buffer.find(MAGIC)
            if start<0:
                self.buffer=self.buffer[-3:]
                continue
            if start: del self.buffer[:start]
            if len(self.buffer)<9: continue
            cmd,seq,size=struct.unpack_from('<BHH',self.buffer,4)
            if size>64:
                del self.buffer[0]; continue
            end=13+size
            if len(self.buffer)<end: continue
            packet=bytes(self.buffer[:end]); del self.buffer[:end]
            if crc(packet[4:-4])!=struct.unpack_from('<I',packet,end-4)[0]: continue
            if (cmd,seq)!=(command|0x80,sequence): continue
            payload=packet[9:-4]
            if not payload: raise RuntimeError('Empty bootloader response')
            if payload[0]:
                status=payload[0]
                raise RuntimeError('Bootloader: '+(ERRORS[status] if status<len(ERRORS) else str(status)))
            return payload[1:]
        raise TimeoutError('No matching IAP reply; check upgrade mode, mapped TX/RX, GND and 38400 baud')

    def exchange(self,command,data=b''):
        seq=self.sequence
        request=frame(command,seq,data)
        for attempt in range(self.retries):
            if self.port.write(request)!=len(request): raise IOError('Short serial write')
            self.port.flush()
            try: response=self._reply(command,seq)
            except TimeoutError:
                if attempt+1==self.retries: raise
            else:
                self.sequence=(seq+1)&0xffff
                return response

    def probe(self):
        info=self.exchange(PING)
        if len(info)!=16: raise RuntimeError('Unsupported bootloader capability reply')
        if struct.unpack('<4I',info)!=(BOARD,APP_BASE,MAX_IMAGE,BAUD):
            raise RuntimeError('Connected bootloader does not match LBJ H176 layout / baud')
        return info

    def flash(self,image,progress=lambda done,total: None):
        validate_image(image); self.probe()
        self.exchange(BEGIN,struct.pack('<4I',len(image),crc(image),APP_BASE,BOARD))
        for offset in range(0,len(image),512):
            block=image[offset:offset+512]
            self.exchange(DATA,struct.pack('<I',offset)+block)
            progress(offset+len(block),len(image))
        self.exchange(COMMIT)
        self.exchange(RUN)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('pack'); p.add_argument('binary',type=Path); p.add_argument('output',type=Path)
    sub.add_parser('ports')
    p=sub.add_parser('probe'); p.add_argument('--port',required=True)
    p=sub.add_parser('flash'); p.add_argument('--port',required=True); p.add_argument('update',type=Path)
    args=parser.parse_args()
    if args.command=='pack':
        args.output.write_bytes(package(args.binary.read_bytes()))
        print('Packed:',args.output); return
    image=unpack(args.update.read_bytes()) if args.command=='flash' else None
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.error('Install pyserial in this Python environment: python -m pip install pyserial')
    if args.command=='ports':
        for port in list_ports.comports(): print(port.device,port.description)
        return
    with serial.Serial(args.port,BAUD,timeout=0.1,write_timeout=3) as port:
        updater=Updater(port)
        if args.command=='probe': updater.probe(); print('LBJ H176 IAP ready, 38400 8N1')
        else:
            updater.flash(image,lambda done,total: print('\rSending %d/%d bytes'%(done,total),end='',flush=True))
            print('\nCRC verified, installed, application started')
if __name__=='__main__': main()
