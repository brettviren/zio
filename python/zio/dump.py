#!/usr/bin/env python3

import zmq
import zio
from zio.flow import objectify, stringify

def handler(ctx, pipe, flow_factory, *args):
    '''
    A dump handler which may be used as an actor talking to a broker's botport.

    Parameters
    ----------
    flow_factory : a callable
        Each call shall return an online zio.Flow.
    '''
    poller = zmq.Poller()
    poller.register(pipe, zmq.POLLIN)
    pipe.signal()               # ready

    s2f = dict()                # socket to its flow

    while True:
        for sock,_ in poller.poll():
            if sock == pipe:
                data = pipe.recv()
                if data == b'STOP':
                    return
                if len(data) == 0:
                    return
                bot = zio.Message(encoded=data)
                fobj = objectify(bot)
                if fobj['direction'] != 'inject':
                    print("dump handler rejecting bot",bot)
                    pipe.send_string('NO')
                    continue
                pipe.send_string('OK')
                flow = flow_factory()
                flow.send_bot(bot)
                print ('handler sent bot',bot)
                poller.register(flow.port.sock, zmq.POLLIN)
                s2f[flow.port.sock] = flow
                continue
            flow = s2f[sock]
            msg = flow.get()
            if msg is None:
                print ("Got no message from client, buhbye")
                del s2f[sock]
                poller.unregister(sock)
                continue
            print ('handler flow',msg)
    return
