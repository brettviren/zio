#!/usr/bin/env python3

import zmq
import struct
from zio.util import encode_message, decode_message

def test_codec():
    lil_data = b"ddd"
    big_data = b"D"*512
    lil_frame = zmq.Frame(data = b"fff")
    big_frame = zmq.Frame(data = b"F"*512)

    mmsg = [lil_data, lil_frame, big_data, big_frame]

    enc = encode_message(mmsg)

    assert(len(enc) == 2*(1+3)+2*(5+512))
    ptr = 0
    assert(enc[ptr] == 3)
    ptr += 4
    assert(enc[ptr] == 3)
    ptr += 4

    assert(enc[ptr] == 0xFF)
    ptr += 1
    siz = struct.unpack('>I', enc[ptr:ptr+4])[0]
    ptr += 4 
    print ('big data size',siz)
    assert(siz == 512)
    assert(enc[ptr:ptr+siz] == big_data)
    ptr += 512

    assert(enc[ptr] == 0xFF)
    ptr += 1
    siz = struct.unpack('>I', enc[ptr:ptr+4])[0]
    assert(siz == 512)
    print ('big frame size',siz)
    ptr += 4
    assert(enc[ptr:ptr+siz] == big_frame.bytes)
    
    mmsg2 = decode_message(enc)
    for part,(m1,m2) in enumerate(zip(mmsg, mmsg2)):
        if isinstance(m1, zmq.Frame):
            m1 = m1.bytes
        assert(m1 == m2)


if '__main__' == __name__:
    test_codec()
    
