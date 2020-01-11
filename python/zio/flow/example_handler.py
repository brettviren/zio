#!/usr/bin/env python3
'''
Example of handlers spawned from backend.spawner
'''
import zmq
import json
from ..port import Port
from ..message import Message
from .proto import Flow
from .util import objectify
from pyre.zactor import ZActor

def handshake(pipe, flow, bot):
    pipe.signal()
    flow.send_bot(bot)
    bot = flow.recv_bot()


def dump_actor(ctx, pipe, flow, bot, *args):
    '''
    Dump flow messages
    '''
    print ("spawn dumper")
    handshake(pipe, flow, bot)
    flow.flush_pay()

    while True:                 # fixme, check pipe
        msg = flow.get()
        if msg is None:
            flow.send_eot()
            print("eot")
            return
        print (msg)
    return

def gen_actor(ctx, pipe, flow, bot, *args):
    '''
    Generate flow messages
    '''
    print ("spawn genner")
    handshake(pipe, flow, bot)
    flow.slurp_pay(0)

    while True:                 # fixme, check pipe
        ok = flow.put(Message())
        if not ok:
            return
    pass


class Factory:

    def __init__(self, server_address):
        self.server_address = server_address
        self.ctx = zmq.Context()
        self.actors = list()

    def __call__(self, bot):
        '''
        Broker calls this to dispatch a BOT.

        Broker waits for return so don't dally.
        '''
        fobj = objectify(bot)
        direction = fobj["direction"]

        if direction == "inject":
            self.spawn(dump_actor, bot)
        else:
            self.spawn(gen_actor, bot)
        return True
    
    def spawn(self, actor_func, bot):
        '''
        Spawn actor function.

        Function must take a flow and the BOT.
        '''
        port = Port("handler", zmq.CLIENT,'')
        port.connect(self.server_address)
        port.online(None)
        flow = Flow(port)
        actor = ZActor(self.ctx, actor_func, flow, bot)
        self.actors.append(actor)
