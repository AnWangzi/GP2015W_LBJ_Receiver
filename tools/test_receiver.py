#!/usr/bin/env python3
"""Host regression: gcc -shared pocsag.c lbj_parse.c -> diagnostics/lbj_test.dll.
Run from the project root with python tools/test_receiver.py.
"""
import ctypes as C
from pathlib import Path
import random
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from radio_config import read_config, freq_bytes

SYNC, IDLE = 0x7CD215D8, 0x7A89C197
lib = C.CDLL(str(ROOT / 'keilproj/diagnostics/lbj_test.dll'))
CALLBACK = C.CFUNCTYPE(None, C.c_uint32, C.c_uint, C.c_char_p, C.c_uint)
received = []


@CALLBACK
def receive(addr, func, bcd, length):
    received.append((addr, func, bcd[:length].decode('ascii')))


def encode(data21):
    v = data21 << 10
    rem = v
    for n in range(30, 9, -1):
        if rem & (1 << n):
            rem ^= 0x769 << (n - 10)
    cw = (v | rem) << 1
    return cw | (bin(cw).count('1') & 1)


def send(word, inverse=False):
    for bit in range(31, -1, -1):
        lib.POCSAG_Bit(((word >> bit) & 1) ^ inverse)


def data_words(text):
    assert len(text) % 5 == 0
    table = '0123456789*U -)('
    words = []
    for p in range(0, len(text), 5):
        bits = 0
        for ch in text[p:p + 5]:
            nib = table.index(ch)
            bits = (bits << 4) | int(format(nib, '04b')[::-1], 2)
        words.append(encode((1 << 20) | bits))
    return words


def init():
    received.clear()
    lib.POCSAG_Init(receive)


class Stats(C.Structure):
    _fields_ = [(n, C.c_uint32) for n in
        ('bits', 'syncs', 'words', 'corrected', 'bad', 'messages',
         'discarded', 'discard_address')] + [
        ('discard_length', C.c_uint16), ('discard_reason', C.c_uint8),
        ('locked', C.c_uint8), ('inverted', C.c_uint8)]


def correction_tests():
    init()
    for cw in [SYNC, IDLE, encode(0), encode(0x1fffff)] + [
            encode(random.Random(i).randrange(1 << 21)) for i in range(12)]:
        v = C.c_uint32(cw)
        assert lib.POCSAG_Correct(C.byref(v)) == 0 and v.value == cw
        for i in range(32):
            v = C.c_uint32(cw ^ (1 << i))
            assert lib.POCSAG_Correct(C.byref(v)) == 1 and v.value == cw
            for j in range(i + 1, 32):
                v = C.c_uint32(cw ^ (1 << i) ^ (1 << j))
                assert lib.POCSAG_Correct(C.byref(v)) == 2 and v.value == cw
    for i in range(32):
        for j in range(i + 1, 32):
            for k in range(j + 1, 32):
                v = C.c_uint32(IDLE ^ (1 << i) ^ (1 << j) ^ (1 << k))
                assert lib.POCSAG_Correct(C.byref(v)) == -1
    print('PASS BCH: all single/double errors in 16 words; all triple errors in IDLE')


def stream_tests():
    short = '008717036 06108'
    for inverse in (False, True):
        for addr in (1233999, 1234000, 1234001, 1234002):
            init()
            send(0xaaaaaaaa, inverse)
            send(SYNC ^ 0x21, inverse)
            slot = 2 * (addr & 7)
            batch = [IDLE] * slot + [encode(((addr >> 3) << 2) | 3)]
            batch += data_words(short)
            batch += [IDLE] * (32 - len(batch))
            for p, cw in enumerate(batch):
                if p and p % 16 == 0:
                    send(SYNC, inverse)
                # two correctable errors, including occasional parity error
                send(cw ^ 0x80000001, inverse)
            assert received == [(addr, 3, short)], received
    init()
    send(SYNC)
    send(encode(((1234000 >> 3) << 2) | 1))
    for cw in data_words(short):
        send(cw ^ 7)  # three errors: drop report, never publish corrupt digits
    send(IDLE)
    assert not received
    # Only actual in-progress reports count as discarded; ordinary noise E
    # is not evidence of a lost long LBJ report.
    init()
    send(SYNC)
    send(encode(((1234000 >> 3) << 2) | 1))
    send(data_words(short)[0])
    send(data_words(short)[1] ^ 7)
    stats = Stats.in_dll(lib, 'g_pocsag_stats')
    assert (stats.discarded, stats.discard_address, stats.discard_length,
            stats.discard_reason) == (1, 1234000, 5, 1)
    assert not received
    init()
    send(SYNC)
    send(encode(((1234000 >> 3) << 2) | 1))
    send(data_words(short)[0])
    lib.POCSAG_Reset()  # simulate sample overrun
    stats = Stats.in_dll(lib, 'g_pocsag_stats')
    assert (stats.discarded, stats.discard_length, stats.discard_reason) == (1, 5, 2)
    for cw in data_words(short)[1:]:
        send(cw)
    send(IDLE)
    assert not received
    init()
    assert Stats.in_dll(lib, 'g_pocsag_stats').discarded == 0
    print('PASS stream: both polarities, frame address, batch continuation, corruption and sample loss')


class Message(C.Structure):
    _fields_ = [(n, C.c_char * size) for n, size in (
        ('train', 16), ('speed', 8), ('position', 12), ('loco', 24),
        ('locoCode', 8), ('route', 32), ('direction', 8))] + [
        (n, C.c_int) for n in ('isDetailed', 'isShort', 'isMerged', 'isStandalone')] + [
        (n, C.c_char * size) for n, size in (('trainNumber', 8), ('prefix', 3),
        ('locoModel', 24), ('locoNumber', 8), ('locoType', 5))]


def parser_tests():
    msg = Message()
    short = b'008717036 06108'
    assert lib.parseLbj(short, len(short), 1233999, 3, C.byref(msg)) == 0
    assert (msg.train, msg.speed, msg.position) == (b'008717', b'036', b'0610.8')
    assert msg.direction.decode('utf-8') == '\u4e0a\u884c'
    # Explicit byte lengths, no NUL terminator required; previously read past short long reports.
    for n in range(50):
        data = C.create_string_buffer(b'0' * n, max(n, 1))
        assert lib.parseLbj(data, n, 1234001, 1, C.byref(msg)) == -1
    tail = b'54202390123400' + b'0' * 36
    assert lib.parseLbj(tail, 50, 1234001, 1, C.byref(msg)) == 0
    assert msg.isDetailed and msg.isStandalone and msg.locoCode == b'239'
    assert (msg.prefix, msg.locoModel, msg.locoType, msg.locoNumber) == (b'T', b'HXD3C', b'2390', b'01234')
    merged = short + tail
    assert lib.parseLbj(merged, 65, 1234000, 1, C.byref(msg)) == 0
    assert msg.isMerged and msg.train == b'T008717'
    assert lib.parseLbj(merged, 65, 1234002, 1, C.byref(msg)) == 0
    assert msg.train == b'T008717' and msg.trainNumber == b'8717'
    for prefix in b'GDCSZTKYX':
        encoded = ''.join('0123456789*U -)('[int(c, 16)] for c in '%02X20' % prefix)
        tail = encoded.encode() + b'23900093' + b'0' * 38
        assert len(tail) == 50
        assert lib.parseLbj(short + tail, 65, 1234000, 3, C.byref(msg)) == 0
        assert msg.prefix == bytes([prefix]) and msg.locoNumber == b'00093'
    tail = b'304U23900093' + b'0' * 38
    assert lib.parseLbj(short + tail, 65, 1234000, 3, C.byref(msg)) == 0
    assert msg.prefix == b'0K'
    # Three-digit EMU map in the old upstream was shifted by one. Four-digit
    # registry matches overlap number[0], and must never discard that digit.
    for registry, model, number, shown in (
            ('30757000', 'CRH380BG', '57000', '57000'),
            ('30735701', 'CRH380BL', '35701', '35701'),
            ('30705700', 'CRH380B', '05700', '5700'),
            ('30805700', 'CRH380C', '05700', '5700'),
            ('30602345', 'CRH380A', '02345', '2345'),
            ('31110001', 'CR400AF-Z', '10001', '10001'),
            ('31250001', 'CR400BF-G', '50001', '50001'),
            ('24700093', 'FXD1-J', '00093', '0093'),
            ('24800093', 'FXD3-J', '00093', '0093'),
            ('23400001', 'HXD1B', '00001', '0001'),
            ('24000002', 'HXD1D', '00002', '0002'),
            ('23912345', 'HXD3C', '12345', '12345')):
        tail = b'4720' + registry.encode() + b'0'*38
        assert lib.parseLbj(tail, 50, 1234002, 3, C.byref(msg)) == 0
        assert (msg.locoModel, msg.locoNumber, msg.loco) == (
            model.encode(), number.encode(), (model+'-'+shown).encode())
    print('PASS registry: EMU codes, specific-prefix priority, all five number digits, FXD unchanged')
    assert lib.parseLbj(short, 15, 999, 3, C.byref(msg)) == -1
    assert lib.parseLbj(short, 15, 1233999, 0, C.byref(msg)) == -1
    print('PASS LBJ: short, merged, standalone, truncated and unsupported reports')


def config_tests():
    # Independent published high-band 915 MHz / 30 MHz XO reference.
    assert freq_bytes(915000000, 30000000, 4)[:4] == (0x3c, 8, 0, 0)
    assert freq_bytes(821237500)[:4] == (0x3e, 9, 0x60, 0x7e)
    blob = read_config()
    props, cmds, p = {}, [], 0
    while p < len(blob):
        n = blob[p]
        cmd = blob[p + 1:p + 1 + n]
        assert len(cmd) == n
        cmds.append(cmd)
        if cmd[0] == 0x11:
            assert len(cmd) == cmd[2] + 4
            props.update({(cmd[1], cmd[3] + i): v for i, v in enumerate(cmd[4:])})
        p += n + 1
    assert p == len(blob) == 450
    assert props[(0x20, 0x51)] == 8 and props[(0x20, 0)] == 0x2a
    assert [c[0] for c in cmds].count(0x17) == 2
    assert not any(c[0] == 0x31 for c in cmds)  # receive-only
    assert all(len(c) == 8 for c in cmds if c[0] in (0x13, 0x32))
    f = [props[(0x40, i)] for i in range(4)]
    hz = (f[0] + ((f[1] << 16) | (f[2] << 8) | f[3]) / 524288.) * 52e6 / 4
    assert abs(hz - 821237500) < 13
    cfg1, cfg0 = props[(0x20, 0x1e)], props[(0x20, 0x1f)]
    dec = 2 ** (((cfg1 >> 6) & 3) + ((cfg1 >> 4) & 3))
    poly = 1 if cfg0 & 0x30 == 0x30 else 2 if cfg0 & 0x20 else 3
    fs = 26e6 / 8 / dec / poly
    nco = sum(props[(0x20, 0x24 + i)] << (16 - 8 * i) for i in range(3))
    baud = fs * nco / 2 ** 22
    assert abs(baud - 1200) < 0.1
    print('PASS radio: high-band table, IRCAL, 821.237499 MHz, %.6f bps' % baud)


if __name__ == '__main__':
    config_tests()
    correction_tests()
    stream_tests()
    parser_tests()
