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
import zmq

def socket_set_hwm(socket, hwm=-1):
    """libzmq 2/3/4 compatible sethwm"""
    try:
        socket.sndhwm = socket.rcvhwm = hwm
    except AttributeError:
        socket.hwm = hwm



def set_id(zsocket):
    """Set simple random printable identity on socket"""
    identity = u"%04x-%04x" % (randint(0, 0x10000), randint(0, 0x10000))
    zsocket.setsockopt_string(zmq.IDENTITY, identity)

