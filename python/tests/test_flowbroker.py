#!/usr/bin/env python3
import logging
logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s.%(msecs)03d %(levelname)s\t%(message)s',
                    datefmt='%Y-%m-%d %H:%M:%S')
log = logging.getLogger("zpb")


import json
import zmq
import zio
from zio.flow import objectify, Broker

from pyre.zactor import ZActor



def client_actor(ctx, pipe, address):
    'An actor function talking to a broker on given address'
    pipe.signal()
    
    port = zio.Port("client", zmq.CLIENT,'')
    port.connect(address)
    port.online(None)       # peer not needed if port only direct connects
    log.debug ("made flow")
    cflow = zio.flow.Flow(port)

    msg = zio.Message(seqno=0,
                      label=json.dumps(dict(direction='extract',credit=2)))
    cflow.send_bot(msg)
    log.debug (f'client sent BOT: {msg}')
    msg = cflow.recv_bot(1000)
    assert(msg)
    log.debug (f'client got BOT: {msg}')
    msg = zio.Message(seqno=1)
    cflow.put(msg)
    log.debug (f'client sent {msg}')
    eot = zio.Message(seqno=2)
    cflow.send_eot(eot)
    log.debug (f'client sent {eot}')
    eot = cflow.recv_eot()
    assert(eot)
    log.debug (f'client done with {eot}')
    pipe.recv()                 # wait for signal to exit


def dumper(ctx, pipe, bot, address):
    '''
    A dump handler which may be used as an actor talking to a broker's botport.

    Parameters
    ----------
    bot : zio.Message
        Our initiating BOT message
    address : string
        A ZeroMQ address string for a bound broker SERVER socket
    '''
    poller = zmq.Poller()
    poller.register(pipe, zmq.POLLIN)
    pipe.signal()               # ready

    port = zio.Port("client", zmq.CLIENT,'')
    port.connect(address)
    port.online(None)       # peer not needed if port only direct connects
    flow = zio.flow.Flow(port)
    poller.register(flow.port.sock, zmq.POLLIN)

    log.debug (f'dumper: send {bot}')

    flow.send_bot(bot)
    bot = flow.recv_bot(1000)
    assert(bot)

    # must explicitly break the deadlock of us waiting for DAT before
    # triggering the poll so that get() will implicitly send PAY so
    # that the other end can send DAT.  3rd base.
    flow.flush_pay()

    interupted = False
    keep_going = True
    while keep_going:

        for sock,_ in poller.poll():

            if sock == pipe:
                log.debug ("dumper: pipe hit")
                data = pipe.recv()
                if data == b'STOP':
                    log.debug ("dumper: got STOP")
                if len(data) == 0:
                    log.debug ("dumper: got signal")
                interupted = True
                return

            # got flow messages
            msg = flow.get()
            log.debug (f'dumper: sock hit: {msg}')
            if msg is None:
                log.debug ("dumper: null message from get, sending EOT")
                flow.send_eot()
                poller.unregister(sock)
                keep_going = False
                break

    log.debug("dumper: taking port offline")
    port.offline()
    if not interupted:
        log.debug("dumper: waiting for quit")
        pipe.recv()
    log.debug("dumper: done")
    return

class Factory:
    def __init__(self, address):
        self.handlers = list()
        self.address = address  # broker
        self.ctx = zmq.Context()

    def __call__(self, bot):
        fobj = json.loads(bot.label) 
        if fobj['direction'] == 'extract': # my direction.
            return                         # only handle inject
        actor = ZActor(self.ctx, dumper, bot, self.address)
        self.handlers.append(actor)
        return True

    def stop(self):
        for actor in self.handlers:
            log.debug("factory stopping handler")
            actor.pipe.signal()
            del(actor)
def test_dumper():
    
    ctx = zmq.Context()

    node = zio.Node("dumper")
    server = node.port("server", zmq.SERVER)
    server_address = "tcp://127.0.0.1:5678"
    server.bind(server_address)
    node.online()

    client = ZActor(ctx, client_actor, server_address)

    factory = Factory(server_address)
    broker = Broker(server, factory)

    for count in range(10):
        log.debug (f"main: poll [{count}]")
        try:
            broker.poll(1000)
        except TimeoutError as te:
            log.warning(te)
            # not the most elegant nor robust way to shutdown
            break

    log.debug (f"main: stop broker")
    broker.stop()
    log.debug (f"main: node offline")
    node.offline()
    log.debug (f"main: stop client")
    client.pipe.signal()
    

if '__main__' == __name__:
    test_dumper()
