#!/usr/bin/env python3
'''
Python interface to ZIO messages
'''

import struct

def encode_multipart(parts):
    '''
    Return a binary encoded concatenation of parts in the input
    sequence.  Result is suitable for use as a single-part message if
    first two parts are encoded header prefix and coords,
    respectively.  Subseqent parts can be payload of arbitrary
    encoding.
    '''
    ret = b''
    for p in parts:
        ret += p
    return ret

def decode_multipart(encoded):
    '''
    Unpack an encoded single-part ZIO message such as returned by
    socket.recv() on a SERVER socket.  It's the moral opposite of
    zio::Message::encode().  What is returned is sequence of message
    parts.
    '''
    tot = len(encoded)
    print ("tot",tot)
    ret = list()
    beg = 0
    while beg < tot:
        end = beg + 4
        print("[%d:%d]" %(beg,end))
        if end >= tot:
            print ("corrupt")
            break
        size = struct.unpack('i',encoded[beg:end])[0]
        beg = end
        end = beg + size
        print("[%d:%d]" %(beg,end))
        if end > tot:
            print ("corrupt")
            break
        ret.append(encoded[beg:end])
        beg = end
    return ret

def encode_header_prefix(mform="FLOW", level=0, label=""):
    '''
    Return a binary encoded header prefix suitable for use as a
    message part.
    '''
    pre = 'ZIO%d%-4s' % (level, mform[:4])
    pre += label
    pre = pre.encode()
    return struct.pack('I', len(pre)) + pre

def decode_header_prefix(henc):
    '''
    Parse the bytes of one encoded message part into a ZIO message
    header prefix.  This is usually the first part of a multipart
    message or as returned by decode().  Returns tuple (level, format,
    label) or None if parse error.
    '''
    if henc[:3] != b'ZIO':
        return None
    if len(henc) < 8:
        return None
    level = henc[3]-ord('0')
    mform = henc[4:8].decode()
    label = henc[8:].decode()
    return (level, mform, label)

def encode_header_coord(origin, granule, seqno):
    '''
    Return a binary encoded header coord suitable for use as a message
    part.  Arguments are taken to be 64 bit unsigned ints.
    '''
    return struct.pack('LLL', origin, granule, seqno);
    

def decode_header_coords(henc):
    '''
    Parse the bytes of one encoded message part into a ZIO message
    header coord.  This is ususally the second part of a multipart
    message or as returend by decode().  Returns tuple (origin,
    granule, seqno) or None if parse error.
    '''
    if len(henc) != 24:
        return None
    return struct.unpack('LLL', henc);

