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

        
