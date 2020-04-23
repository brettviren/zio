#!/usr/bin/env python3

import json
import zmq
import zio
from zio.flow import objectify, Broker, Flow, TransmissionEnd

from pyre.zactor import ZActor

from zio.util import modlog, mainlog, DEBUG, INFO
log = modlog("test_flowbroker")


def client_actor(ctx, pipe, address):
    'An actor function talking to a broker on given address'
    pipe.signal()
    
    port = zio.Port("client", zmq.CLIENT,'')
    port.connect(address)
    port.online(None)       # peer not needed if port only direct connects
    log.debug ("made flow")

    direction='extract'
    credit=2
    cflow = Flow(port, direction, credit)

    bot = cflow.bot()
    log.debug (f'client did BOT: {bot}')
    assert(bot)
    cflow.begin()

    msg = zio.Message(form='FLOW', label_object={'flow':'DAT'})
    log.debug (f'client put DAT with {cflow.credit}/{cflow.total_credit} {msg}')
    cflow.put(msg)
    log.debug (f'client did DAT')
    cflow.eot()
    log.debug (f'client did EOT')

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

    port = zio.Port("dumper", zmq.CLIENT,'')
    port.connect(address)
    port.online(None)       # peer not needed if port only direct connects

    fobj = bot.label_object
    direction=fobj["direction"]
    credit=fobj["credit"]
    flow = Flow(port, direction, credit)
    poller.register(flow.port.sock, zmq.POLLIN)

    log.debug (f'dumper: send {bot}')

    bot = flow.bot(bot)
    assert(bot)

    flow.begin()

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
            try:
                msg = flow.get()
            except TransmissionEnd:
                log.debug ("dumper: get gives EOT")
                flow.eotsend()
                poller.unregister(sock)
                keep_going = False
                break

            log.debug (f'dumper: sock hit: {msg}')

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
        fobj = bot.label_object
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
            log.info('broker is lonely, quitting')
            
            break

    log.debug (f"main: stop broker")
    broker.stop()
    log.debug (f"main: node offline")
    node.offline()
    log.debug (f"main: stop client")
    client.pipe.signal()
    

if '__main__' == __name__:
    mainlog()

    # logging.getLogger('transitions').setLevel(logging.INFO)
    # logging.getLogger('zio').setLevel(logging.DEBUG)
    # # logging.getLogger('zio.flow.proto').setLevel(logging.DEBUG)
    # # logging.getLogger('zio.flow.sm').setLevel(logging.DEBUG)
    # # logging.getLogger('zio.flow.broker').setLevel(logging.DEBUG)
    # logging.getLogger('test_flowbroker').setLevel(logging.DEBUG)
    test_dumper()
