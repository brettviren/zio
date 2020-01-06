#!/usr/bin/env python3
'''
Utilities
'''

import zmq

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
    '''
    Return True if socket type stype requires single-part messages
    '''
    return \
        stype == zmq.SERVER or \
        stype == zmq.CLIENT or \
        stype == zmq.RADIO or \
        stype == zmq.DISH

def guess_hostname():
    import socket
    return socket.getfqdn()
        
def byteify_list(lst):
    ret = list()
    for el in lst:
        if type(el) is str:
            el = bytes(el, encoding='utf-8')
        ret.append(el)
    return ret
