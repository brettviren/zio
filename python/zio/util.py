#!/usr/bin/env python3
'''
Utilities
'''

import zmq
import struct

socket_names = [
    "PAIR",
    "PUB",
    "SUB",
    "REQ",
    "REP",
    "DEALER",
    "ROUTER",
    "PULL",
    "PUSH",
    "XPUB",
    "XSUB",
    "STREAM",
    "SERVER",
    "CLIENT",
    "RADIO",
    "DISH",
    "GATHER",
    "SCATTER",
    "DGRAM",
    0
]


def needs_codec(stype):
    """Determine if socket needs codec

    :param stype: a ZeroMQ socket type number
    :returns: True if socket type stype requires single-part messages
    :rtype: bool

    """
    return \
        stype == zmq.SERVER or \
        stype == zmq.CLIENT or \
        stype == zmq.RADIO or \
        stype == zmq.DISH

def guess_hostname():
    """Guess the local hostname

    :returns: host name
    :rtype: string

    """
    import socket
    return socket.getfqdn()
        
def byteify_list(lst):
    """Encode to bytes elements of a list

    :param lst: list of things that can be converted to bytes
    :returns: list of bytes
    :rtype: list

    """
    ret = list()
    for el in lst:
        if type(el) is str:
            el = bytes(el, encoding='utf-8')
        ret.append(el)
    return ret



def zpipe(ctx):
    """build inproc pipe for talking to threads

    mimic pipe used in czmq zthread_fork.

    Returns a pair of PAIRs connected via inproc
    """
    a = ctx.socket(zmq.PAIR)
    b = ctx.socket(zmq.PAIR)
    a.linger = b.linger = 0
    a.hwm = b.hwm = 1
    iface = "inproc://%s" % binascii.hexlify(os.urandom(8))
    a.bind(iface)
    b.connect(iface)
    return a,b

def dump(msg_or_socket):
    """Receives all message parts from socket, printing each frame neatly"""
    print(msg_or_socket)
    if isinstance(msg_or_socket, zmq.Socket):
        # it's a socket, call on current message
        if msg_or_socket.type == zmq.SERVER:
            msg = serverish_recv(msg_or_socket)
        else:
            msg = msg_or_socket.recv_multipart()
    else:
        msg = msg_or_socket
    print("----------------------------------------")
    for part in msg:
        print("[%03d]" % len(part), end=' ')
        is_text = True
        try:
            print(part.decode('ascii'))
        except UnicodeDecodeError:
            print(r"0x%s" % (binascii.hexlify(part).decode('ascii')))



def encode_message(parts):
    '''Return an encoded byte string which is the concatenation of all
    parts of the input sequence with suitable size prefixes.  The
    encoding is compatible with CZMQ's zmsg_encode().
    '''
    ret = b''
    for p in parts:
        if isinstance(p, zmq.Frame):
            p = p.bytes
        siz = len(p)
        if siz < 255:
            s = struct.pack('>B', siz)
        else:
            s = struct.pack('>BI', 0xFF, siz)
        one = s + p
        ret += one
        
    return ret

def decode_message(encoded):
    '''Unpack an encoded byte string to a list of multiple parts.  This
    is the inverse of encode_message().
    '''
    tot = len(encoded)
    ret = list()
    beg = 0
    while beg < tot:
        end = beg + 1           # small size of 0xFF
        if end >= tot:
            raise ValueError("corrupt message part in size")
        size = struct.unpack('>B',encoded[beg:end])[0]
        beg = end

        if size == 0xFF:        # large message
            end = beg + 4
            if end >= tot:
                raise ValueError("corrupt message part in size")
            size = struct.unpack('>I',encoded[beg:end])[0]
            beg = end

        end = beg + size
        if end > tot:
            raise ValueError("corrupt message part in data")
        ret.append(encoded[beg:end])
        beg = end
    return ret

def serverish_recv(sock, *args, **kwds):
    '''Return a message from a serverish socket.

    The socket may be of type ROUTER or SERVER.  Return list of
    [routing id, (message parts)]

    '''
    if sock.type == zmq.SERVER:
        frame = sock.recv(copy=False)
        cid = frame.routing_id
        msg = decode_message(frame.bytes)
        return [cid,  msg]
                
    if sock.type == zmq.ROUTER:
        msg = sock.recv_multipart(*args, **kwds)
        cid = msg.pop(0)
        # REQ implicitly adds an empty, DEALER must explicitly add one
        empty = msg.pop(0)
        if empty != b'':
            raise RuntimeError(f'expect empty frame, got "{empty}" following {cid}')
        return [cid, msg]

    raise ValueError(f'unsupported socket type {sock.type}')

def serverish_send(sock, cid, msg, *args, **kwds):
    '''Send a message via a serverish socket.

    The cid is a routing ID for the client.

    The msg may be single or multipart.
    '''
    if not isinstance(msg, list):
        msg = [msg]

    if sock.type == zmq.SERVER:
        frame = zmq.Frame(data=encode_message(msg))
        if isinstance(cid, bytes):
            # hope-and-pray oriented programming
            cid = int.from_bytes(cid, sys.byteorder)
        frame.routing_id = cid
        return sock.send(frame)

    if sock.type == zmq.ROUTER:
        msg = [cid,b''] + msg
        return sock.send_multipart(msg)
    
    raise ValueError(f'unsupported socket type {sock.type}')
    
def clientish_recv(sock, *args, **kwds):
    '''Receive a message via a clientish socket'''

    if sock.type == zmq.CLIENT:
        frame = sock.recv(copy=False, *args, **kwds)
        msg = decode_message(frame.bytes)
        return msg

    if sock.type == zmq.DEALER:
        msg = sock.recv_multipart(*args, **kwds)
        msg.pop(0)              # delimiter
        return msg

    raise ValueError(f'unsupported socket type {sock.type}')
    
def clientish_send(sock, msg, *args, **kwds):
    '''Send a message via a clientish socket'''
    if not isinstance(msg, list):
        msg = [msg]

    if sock.type == zmq.CLIENT:
        frame = zmq.Frame(data = encode_message(msg))
        return sock.send(frame, *args, **kwds)

    if sock.type == zmq.DEALER:
        msg = [b''] + msg
        return sock.send_multipart(msg, *args, **kwds)

    raise ValueError(f'unsupported socket type {sock.type}')

