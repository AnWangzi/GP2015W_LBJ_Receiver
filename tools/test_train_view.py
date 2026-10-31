"""Compare the actual receiver's category row with user-requested labels."""
import ctypes as C
from pathlib import Path
from test_receiver import Message
root=Path(__file__).resolve().parent.parent
lib=C.CDLL(str(root/'keilproj/diagnostics/lcd_host.dll'))
lib.LCD_HostSnapshot.restype=C.POINTER(C.c_uint8)
samples=[('G','1234','高速动车组列车'),('D','1234','动车组列车'),
         ('Z','1234','直达特快'),('T','1234','特快'),('K','9999','普快'),
         ('X','80001','快速货运'),('K','3998','跨局普快'),('K','4001','跨局临客'),
         ('K','6998','管内临客'),('K','8515','管内普快'),
         ('','00123','动车组回送'),('','00398','普速客车底回送'),
         ('','11588','直达货物列车'),('','25230','跨局货物列车'),
         ('','35004','区段货物列车'),('','40001','摘挂货物列车'),
         ('','45001','小运转货物列车'),('','50001','客运单机'),
         ('','51001','货运单机'),('','52001','小运转单机'),
         ('','53001','补机返回'),('','55123','普通客货混合列车'),
         ('','55400','试运转列车'),('','56001','轨道车/接触网作业车'),
         ('','57001','路用列车'),('','58001','救援列车'),
         ('','60001','企业自备货车'),
         ('','70001','重载货物列车/超限货物列车/保温列车'),('','80001','普快货物班列'),
         ('','90001','军用列车'),('','55000','类别待识别')]
for prefix,number,label in samples:
    message=Message(); message.prefix=prefix.encode(); message.trainNumber=number.encode()
    message.train=(prefix+number).encode(); message.speed=b'036'; message.position=b'0610.8'
    lib.LCD_HostReset(); lib.AppView_Train(C.byref(message),0)
    actual=bytes(lib.LCD_HostSnapshot()[768:1024])
    lib.LCD_HostReset()
    expected_text=(C.c_uint16*(len(label)+1))(*(ord(c) for c in label),0)
    lib.LCD_DrawStr16(6,0,expected_text)
    expected=bytes(lib.LCD_HostSnapshot()[768:1024])
    assert actual==expected,(prefix,number,label)
    # No direction label remains: the complete train ID starts at column zero.
    lib.LCD_HostReset(); lib.AppView_Train(C.byref(message),0)
    header=bytes(lib.LCD_HostSnapshot()[:256])
    lib.LCD_HostReset(); lib.LCD_DrawAscii8(0,0,message.train)
    assert header==bytes(lib.LCD_HostSnapshot()[:256])
    if len(label)>8:
        # Reach the last window and then a new cycle; no suffix is clipped forever.
        for age,window in ((2000+(len(label)-8)*600,label[-8:]),
                           (4000+(len(label)-8)*600,label[:8])):
            lib.LCD_HostReset(); lib.AppView_Train(C.byref(message),age)
            actual=bytes(lib.LCD_HostSnapshot()[768:1024])
            lib.LCD_HostReset()
            text=(C.c_uint16*(len(window)+1))(*(ord(c) for c in window),0)
            lib.LCD_DrawStr16(6,0,text)
            assert actual==bytes(lib.LCD_HostSnapshot()[768:1024]),(label,age)
lib.LCD_HostReset(); lib.AppView_Waiting(1,0)
actual=bytes(lib.LCD_HostSnapshot()[:1024])
assert not any(actual[:256]+actual[512:]) # no R/RX, uptime or menu hint
lib.LCD_HostReset()
text=(C.c_uint16*8)(*(ord(c) for c in '等待列车信息'),0)
lib.LCD_DrawStr16(2,8,text)
assert actual==bytes(lib.LCD_HostSnapshot()[:1024])
print('PASS actual C view: all category names, long-label scroll, no direction, clean waiting page')
