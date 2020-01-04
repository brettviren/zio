#!/usr/bin/env python3
'''
Python interface to ZIO messages
'''

import struct
from collections import namedtuple

from enum import Enum
class MessageLevel(Enum):
    undefined=0
    trace=1
    verbose=2
    debug=3
    info=4
    summary=5
    warning=6
    error=7
    fatal=8

class PrefixHeader:
    '''
    A ZIO message prefix header.

    It is a triplet of (level, form, label) with a "ZIO" leading
    magic.
    '''
    level = MessageLevel.undefined # zio::level::MessageLevel 
    form=" "*4                  # up to 4 character format
    label = ""                  # free form string

    def __init__(self, *args, level=0, form=" "*4, label=""):
        '''
        Create a prefix header

        >>> PrefixHeader(0, "FLOW", json.dumps(dict(flow="DAT")))
        >>> PrefixHeader(form="TEXT", level=3)
        '''
        self.level = MessageLevel(int(level))
        self.form="%-4s"%form
        self.label=label
        if not args:            
            return
        # (level,form,label) constructor
        if type(args[0]) == int:
            self.level = MessageLevel(args[0])
            if len(args) > 1:
                self.form="%-4s"%args[1]
            if len(args) > 2:
                self.label=args[2]
            return
        # string constructor
        if type(args[0]) == str and len(args[0]) > 5:
            phs = args[0]
            if phs.startswith("ZIO"):
                phs = phs[3:]
            self.level(MessageLevel(int(phs[0])))
            phs = phs[1:]
            self.form="%-4s"%phs[:4]
            self.label = phs[4:]
        # bytes constructor
        if type(args[0]) == bytes:
            level, self.form, self.label = decode_header_prefix(args[0])
            self.level = MessageLevel(int(level))

    def __str__(self):
        return "ZIO%d%s%s" % (self.level.value, self.form, self.label)
    
    def __repr__(self):
        return "<zio.message.PrefixHeader %s>" % bytes(self)

    def __bytes__(self):
        a = b'ZIO%d' % self.level.value
        b = bytes(self.form, 'utf-8')
        c = bytes(self.label, 'utf-8')
        return a + b + c

class CoordHeader:
    origin=0                    # where a message came from
    granule=0                   # when a message came from
    seqno=0                     # which message

    def __init__(self, *args, origin=0, granule=0, seqno=0):
        self.origin = origin
        self.granule = granule
        self.seqno = seqno
        if len(args) == 0:
            return
        if len(args) == 3:
            self.origin,self.granule,self.seqno = args
            return
        if type(args[0]) == bytes:
            self.origin,self.granule,self.seqno = decode_header_coord(args[0])
            return

    def __bytes__(self):
        return encode_header_coord(self.origin, self.granule, self.seqno)

    def __repr__(self):
        return "<zio.message.CoordHeader %s>" % bytes(self)


def encode_message(parts):
    '''
    Return a binary encoded concatenation of parts in the input
    sequence.  Result is suitable for use as a single-part message.
    For ZIO messages the first two parts should be encoded header
    prefix and coord, respectively.  Subseqent parts can be payload of
    arbitrary encoding.
    '''
    ret = b''
    for p in parts:
        s = struct.pack('I', len(p))
        ret += s + p
    return ret

def decode_message(encoded):
    '''
    Unpack an encoded single-part ZIO message such as returned by
    socket.recv() on a SERVER socket.  It's the moral opposite of
    zio::Message::encode().  What is returned is sequence of message
    parts.
    '''
    tot = len(encoded)
    ret = list()
    beg = 0
    while beg < tot:
        end = beg + 4
        if end >= tot:
            raise ValueError("corrupt ZIO message in size")
        size = struct.unpack('i',encoded[beg:end])[0]
        beg = end
        end = beg + size
        if end > tot:
            raise ValueError("corrupt ZIO message in data")
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
    

def decode_header_coord(henc):
    '''
    Parse the bytes of one encoded message part into a ZIO message
    header coord.  This is ususally the second part of a multipart
    message or as returend by decode().  Returns tuple (origin,
    granule, seqno) or None if parse error.
    '''
    if len(henc) != 24:
        return None
    return struct.unpack('LLL', henc);

