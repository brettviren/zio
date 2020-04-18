#!/usr/bin/env python3
'''
Python interface to ZIO messages
'''

import zmq
import json
import struct
from collections import namedtuple
from enum import Enum
from .util import byteify_list, encode_message, decode_message

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
            self.level = MessageLevel(int(phs[0]))
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

    def __str__(self):
        return "[0x%x,%ld,%ld]" %(self.origin,self.granule,self.seqno)

    def __repr__(self):
        return "<zio.message.CoordHeader %s>" % bytes(self)


class Message:
    '''
    A zio.Message fixes some of the message schema.

    It is equivalent to a C++ zio::Message.

    '''

    
    routing_id = 0
    prefix = None
    coord = None
    _payload = ()

    def __init__(self,
                 level=None, form=None, label=None, routing_id=None,
                 origin=None, granule=None, seqno=None,
                 prefix=None, coord=None, payload=None,
                 parts=None, encoded=None, frame=None):
        '''Construct a zio.Message.

        Construction applies arguments in reverse order.  Thus one
        may, eg, construct a message with a frame and then override
        the payload and label.  The ingredients may be considered
        deconstructed as:

            frame = encoding + routing_id

            encoding = packing of parts

            parts = [prefix, coord, ...payloads]
        
        A frame should be used when the zio.Message will be used with
        a SERVER socket.  Or else the routing_id must be explicitly
        set.

        '''
        self.prefix = PrefixHeader()
        self.coord = CoordHeader()

        if frame is not None:
            self.fromframe(frame)
        if encoded is not None:
            self.decode(encoded)
        if parts is not None:
            self.fromparts(parts)
        if payload is not None:
            self.payload = payload
        if coord is not None:
            self.coord = coord
        if prefix is not None:
            self.prefix = prefix
        if routing_id is not None:
            self.routing_id = routing_id
        else:
            self.routing_id = 0
        if label is not None:
            self.label = label
        if form is not None:
            self.form = form
        if level is not None:
            self.level = level
        if origin is not None:
            self.origin = origin
        if granule is not None:
            self.granule = granule
        if seqno is not None:
            self.seqno = seqno
        return

    @property
    def form(self):
        return self.prefix.form
    @form.setter
    def form(self, val):
        self.prefix.form = val

    @property
    def level(self):
        return self.prefix.level
    @level.setter
    def level(self, val):
        self.prefix.level = val

    @property
    def label(self):
        return self.prefix.label
    @label.setter
    def label(self, val):
        self.prefix.label = val

    @property
    def label_object(self):
        if not self.label:
            return dict()
        if type(self.label) is bytes:
            return json.loads(self.decode('utf-8'))
        return json.loads(self.label)

    @label_object.setter
    def label_object(self, val):
        self.label = json.dumps(val)
        
    @property
    def origin(self):
        return self.coord.origin
    @origin.setter
    def origin(self, val):
        self.coord.origin = val

    @property
    def granule(self):
        return self.coord.granule
    @granule.setter
    def granule(self, val):
        self.coord.granule = val

    @property
    def seqno(self):
        return self.coord.seqno
    @seqno.setter
    def seqno(self, val):
        self.coord.seqno = val


    @property
    def payload(self):
        return self._payload or list()

    @payload.setter
    def payload(self, pl):
        self._payload = byteify_list(pl)

    def toframe(self):
        '''
        Return self as a frame.
        '''
        frame = zmq.Frame(self.encode())
        if self.routing_id:
            frame.routing_id = routing_id
        return frame

    def fromframe(self, frame):
        '''
        Set self from a frame
        '''
        self.routing_id = frame.routing_id
        self.decode(frame.bytes)

    def encode(self):
        '''
        Return encoded byte array of self.

        It is suitable for use as the data arg to a zmq.Frame
        '''
        parts = self.toparts()
        return encode_message(parts)

    def decode(self, encoded):
        '''
        Decode to self.
        '''
        parts = decode_message(encoded)
        self.fromparts(parts)
        pass

    def toparts(self):
        '''
        Return self as a multipart set of encoded data
        '''
        return [bytes(self.prefix),bytes(self.coord)] + self.payload

    def fromparts(self, parts):
        '''
        Set self from multipart message / array of encoded data.
        '''
        if len(parts) < 2:
            raise ValueError("must have at least two parts")
        self.prefix = PrefixHeader(parts[0])
        self.coord = CoordHeader(parts[1])
        self.payload = parts[2:]

    def __str__(self):
        return "zio.Message: \"%s\" + %s + [%d]" % \
            (self.prefix, self.coord, len(self.payload))


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

