#!/usr/bin/env python3
'''
Actor functions that can work as broker backends.
'''
import zmq
from ..message import Message

def spawner(ctx, pipe, factory, *args):
    '''Broker backend that spawns handlers.

    Parameters
    ----------
    factory : a callable
        Called with new BOT, return False if reject.

    '''
    poller = zmq.Poller()
    poller.register(pipe, zmq.POLLIN)
    pipe.signal()               # ready

    s2f = dict()                # socket to its flow

    while True:

        which = poller.poll()

        # how I might bail, let me count the ways
        if not which:
            return
        data = pipe.recv()
        if len(data) == 0 or data == b'STOP':
            return

        bot = Message(encoded=data)
        if factory(bot):
            pipe.send_string('OK')
            continue
        pipe.send_string('NO')
        continue

    return
