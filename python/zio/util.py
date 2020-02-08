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
