# encoding: utf-8
"""
Helper module for example applications. 
Mimics ZeroMQ Guide's zhelpers.h.
Extended to generalize SOCKET/ROUTER and CLIENT/DEALER 
"""

import sys
import binascii
import os
from random import randint
import struct
import zmq

def socket_set_hwm(socket, hwm=-1):
    """libzmq 2/3/4 compatible sethwm"""
    try:
        socket.sndhwm = socket.rcvhwm = hwm
    except AttributeError:
        socket.hwm = hwm


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


def set_id(zsocket):
    """Set simple random printable identity on socket"""
    identity = u"%04x-%04x" % (randint(0, 0x10000), randint(0, 0x10000))
    zsocket.setsockopt_string(zmq.IDENTITY, identity)


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


# send/recv message for server-like (ROUTER, SERVER) and client-like
# (DEALER, CLIENT) sockets which does type-erasure on the socket type.
# Serverish deals in a 2-tuple (pid, msg) while clientish deals with
# just a msg.  In all cases a msg is a list of zmq.Frame elements.
# SERVER and CLIENT inherently pass only single-part messages and so
# this list of frames is packed/unpacked (even if list is length 1).
# For ROUTER/DEALER it is sent as a multipart.

def encode_message(parts):
    '''
    Encode parts in a CZMQ-compatible (zmsg_decode()/zmsg_encode())
    way.

    A part may be either a PyZMQ zmq.Frame or bytes.

    Parts are serialized to bytes and prefixing with a size.  Parts
    smaller than 255 bytes are prefixed with a 1-byte size value.
    Larger parts are prefixed by a fixed 1-byte value of 0xFF and a
    4-byte size value.
    '''
    ret = b''
    for p in parts:
        if isinstance(p, zmq.Frame):
            p = p.bytes
        siz = len(p)
        if siz < 255:
            s = struct.pack('=B', siz)
        else:
            s = struct.pack('=BI', 0xFF, siz)
        one = s + p
        ret += one
        
    return ret

def decode_message(encoded):
    '''
    Decode parts in a CZMQ-compatible way.

    This provides the inverse function of encode_message() with the
    exception that each part in the returned list is of type bytes,
    an not zmq.Frame.
    '''
    tot = len(encoded)
    ret = list()
    beg = 0
    while beg < tot:
        end = beg + 1           # small size of 0xFF
        if end >= tot:
            raise ValueError("corrupt message part in size")
        size = struct.unpack('=B',encoded[beg:end])[0]
        beg = end

        if size == 0xFF:        # large message
            end = beg + 4
            if end >= tot:
                raise ValueError("corrupt message part in size")
            size = struct.unpack('=I',encoded[beg:end])[0]
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
    [id,msg]

    '''
    if sock.type == zmq.SERVER:
        frame = sock.recv(copy=False)
        cid = frame.routing_id
        msg = decode_message(frame.bytes)
        return [cid,  msg]
                
    if sock.type == zmq.ROUTER:
        msg = sock.recv_multipart(*args, **kwds)
        cid = msg.pop(0)
        empty = msg.pop(0)
        assert (empty == b'')
        return [cid, msg]

    raise ValueError(f'unsupported socket type {sock.type}')

def serverish_send(sock, cid, msg, *args, **kwds):
    '''Send a message via a serverish socket.'''
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
        msg = [cid, b''] + msg
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
        empty = msg.pop(0)
        assert empty == b''
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

